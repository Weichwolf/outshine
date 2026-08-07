# Taa — the temporal resolve, and the sub-pixel jitter that makes it worth having

**Pass:** `TaaStage` (`sim/src/render/stages/TaaStage.{h,cpp}`), between the occlusion buffer and the
display curve. Its other half is **not** a pass: `render/TemporalJitter.h` moves the projection's
sample point, and without that the resolve averages the same samples over and over.

Neighbours: [`../renderer.md`](../renderer.md) §2.6 (the pass topology and the two frame-to-frame
quantities), [`terrain.md`](terrain.md) (the ground, whose every fragment is world-fixed and therefore
reprojects out of DEPTH alone — no stage writes a motion vector today), [`../lod.md`](../lod.md) (whose whole selection rule *assumes* this
file exists — sub-τ discrepancy is „the class of error TAA is built to absorb").

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| **The jitter is in the PROJECTION, not in a post filter.** A blade 11 mm wide at 8 m covers 0.9 px; a rasteriser asks one coverage question per pixel, so that blade is drawn in segments. No post filter can repair it because the coverage was never in the image | the 1 px and 2 px sky crossings of `wiese-b-frontlit`, and the count of enclosed fragments at a range of detection thresholds |
| **The jitter is a CAMERA property.** It may not move anything that is at a place | `FB_JITTER=1.0,0.0` must produce the picture `FB_JITTER=0,0` produces, shifted by exactly one pixel, IDENTICALLY at every range. Measured below |
| **Motion vectors for everything that moves.** World-fixed geometry is reprojected from its own depth; anything else writes the difference of its two clip positions | the ghost-free control: the same pose reached by a TELEPORT plus a settle, against the same pose reached by MOTION |
| The accumulation is in **linear radiance, before the display curve** | a history in display codes quantises at 8 bits and cannot carry the direct fraction the tonemap weights its occlusion by (`SurfaceLight.h`) |
| It is **armed by default and disarmable by one switch** | `FB_TAA=0` disarms the jitter and invalidates the history, so the resolve copies the frame through unchanged. The picture is the one a tree without this stage produces, off the same binary. It removes no pass — the pass count is a contract and a measurement gate may not move it |
| **Emptying the history means restarting the phase.** `ResetTemporal()` does both | a settle that starts at phase `n mod 8` visits the same eight sample positions in a rotated order and the feedback weights them unequally, so without it the settled picture is a function of how long the warm-up was |
| **A settled still is a function of the scene alone** | `tools/determinism.py`, several runs, forced through different tile arrival orders (`FB_TILEWORKERS=1/2/4/6`): ONE md5. Measured below, §7 |
| It costs a **fraction of a millisecond** at 720p | measured: **0.798 ms** for the resolve pass. The motion vector for 1.4 M blades is a separate and much larger number — see `## Knowledge` |

## State

**Built, 2026-08-07.** Pass count 7 → **8** (`passcount passes=8 … taa=1`), native and browser alike.

| Piece | Form |
|---|---|
| jitter | Halton(2,3), 8 phases, ±0.5 px, applied as a constant NDC offset on the z column of `MvpCamRel` — the same term the boresight shift rides on. `camRay()` subtracts it, so sky, sun and cloud sample the ray the jittered frustum sends through the pixel |
| motion attachment | a SECOND colour attachment on the scene pass, `rg16float`, cleared to the sentinel −1e4 = „world-fixed" (`stages/SceneTargets.h`). Every pipeline in that pass declares two targets; only the ground cover writes real motion, and the opaque stages drawn after it write the sentinel so a facade cannot inherit a blade's velocity |
| static reprojection | in the resolve, from depth through the previous frame's view-projection plus the eye's own step. Exact for anything world-fixed and costs no attachment write |
| resolve | 3×3 neighbourhood moments in YCoCg, clip toward the mean at γ = 1.5, Catmull-Rom history fetch (5 bilinear taps), Karis luminance-weighted blend, feedback 0.1 rising with pixel speed |
| history | two `rgba16float` targets, ping-ponged; a bind group pins a view at creation, so „read the other one" is two bind groups. `TonemapStage` carries the same parity |

### What it measures out at

Pinned binary `a386ccc0b3491d86ca1aa66a49d9e98a`, `mods/demo/scene.json`, 1280×720, `--warm 240`.
Both arms are the SAME binary under `FB_TAA=0/1`.

| Measurement | without | with | note |
|---|---|---|---|
| horizontal neighbour pairs jumping > 40 codes, scene | 7.740 % | **1.757 %** | 4.4× fewer |
| the same on `wiese-eye-frontlit` | 8.034 % | **1.091 %** | 7.4× fewer |
| mean \|Laplace\|, 8–15 m (the band the critic named as the maximum) | 0.2398 | **0.0850** | −65 % |
| the same, all ground | 0.2392 | **0.1074** | −55 % |
| share of ground pixels over 0.10 | 33.80 % | **20.49 %** | |
| RMSE against a 16× supersampled ground truth | 11.432 codes | **3.959** | 2.9× closer |
| Nyquist-octave power relative to that truth | 4.305× | **0.956×** | the excess IS the aliasing |
| mid-band (genuine detail) power relative to that truth | 2.242× | **0.924×** | 7.6 % softer than the truth |

The ground truth is the same frame rendered at 16 pinned sub-pixel phases (a regular 4×4 grid,
`FB_JITTER`) and averaged in **linear** light. It is not a higher-resolution render on purpose: a
supersample at another resolution also changes mip selection and the frustum cull, and then the
comparison carries two differences instead of one.

## Gaps

- **A pan smears.** At 21.8 px/frame the resolve holds only 0.719 of the still frame's mid-band detail
  even at the chosen velocity ramp, and at 3.8 px/frame only 0.299. The cause is structural — a history
  displaced every frame passes through ~1/α Catmull-Rom filters before it is forgotten — and the
  standard remedy this file does NOT yet carry is a sharpening pass on the resolve's output. The ramp
  and its measured curve are in `## Knowledge`; the sharpener is unbuilt and unmeasured.
- **The occlusion buffer is outside the accumulation.** `AoStage` runs on the jittered depth and the
  tonemap composites it AFTER the resolve, so whatever temporal noise a half-resolution occlusion
  buffer carries is not filtered. Unmeasured. Moving the composite into this stage would fix it and
  would cost `TonemapStage` its AO input.
- **No disocclusion test beyond the neighbourhood clip.** The usual second gate is a depth-based
  rejection of the reprojected sample; it is not built, and the clip alone is what carries the
  0.15-frame lag figure.
- **The history is not reset on a device loss**, only on the first frame and through
  `Renderer::ResetTemporal()`. A restored device would blend against a stale buffer for ~10 frames.
- **Nothing tests the resolve at another resolution.** The whole set is 1280×720.

## Knowledge

### 1 Why the jitter cannot move anything world-fixed, and the measurement that shows it

The offset enters `MvpCamRel` as `p[8] = −jx`, `p[9] = −(shift + jy)` — the z column, i.e. a term
multiplied by `z_eye` and divided by `w = −z_eye`. A world point's NDC therefore moves by exactly
`(jx, jy)` **whatever its depth**: it is a shear of the frustum, not a translation of the world, and
the rasteriser's coverage grid is what moves.

Measured rather than asserted. `FB_JITTER=1.0,0.0` against `FB_JITTER=0,0`, the second image compared
against the first **shifted by exactly one pixel**:

| range band | pixels | mean \|ΔY\| | share > 2 codes | median relative depth difference |
|---|---:|---:|---:|---:|
| 0–3 m | 46 851 | 0.000 | 0.000 % | 0 |
| 3–8 m | 294 441 | 0.011 | 0.124 % | 0 |
| 8–15 m | 68 754 | 0.003 | 0.010 % | 0 |
| 15–25 m | 23 094 | 0.006 | 0.022 % | 0 |
| 25–35 m | 12 151 | 0.191 | 3.004 % | 0 |
| 44–80 m | 3 967 | 0.691 | 4.437 % | 3.2e-6 |
| all ground | 482 263 | **0.020** | **0.207 %** | 0 |
| the same comparison WITHOUT the shift | 921 600 | 10.667 | 43.832 % | — |

The depth buffer is bit-identical out to 35 m. The residual in the far bands is the exposure meter and
the frustum's own edge cell, both of which see a frame shifted by one pixel and answer marginally
differently. **No parallax**: the shift is the same at three metres and at forty, which is the
statement [`../renderer.md`](../renderer.md) §1.9 asks for.

### 2 The clip width γ, derived and not taken from the source

γ is how many standard deviations of the 3×3 neighbourhood the history may sit outside before it is
pulled toward the mean. Karis reports 1.0 for the same estimator; this scene wants more, and the curve
is the reason (`FB_TAA_GAMMA`, against the 16× truth and against the ghost-free answer):

| γ | RMSE vs truth | mid-band | Nyquist octave | lag at 3.8 px/frame |
|---|---:|---:|---:|---:|
| 1.0 | 4.455 | 0.753× | 0.632× | 0.50 px |
| **1.5** | **3.959** | **0.924×** | **0.956×** | **0.63 px** |
| 2.0 | 3.929 | 0.948× | 1.016× | 0.66 px |
| no TAA | 11.432 | 2.242× | 4.305× | — |

At 1.0 the scene loses a quarter of its genuine detail; 2.0 buys 0.5 % more of the truth and lets the
Nyquist octave back over 1.0, i.e. starts re-admitting the aliasing. **γ = 1.5.**

### 3 The feedback is not a constant

A history displaced far every frame passes through ~1/α Catmull-Rom resamples before it is forgotten,
and at 21.8 px/frame that chain turns the sward into felt. So the new frame's weight rises with the
pixel's speed: `α = clamp(0.1 + |v|_px · kVelRamp, 0.1, 0.85)`. Measured against the settled TAA at the
same pose (mid = genuine detail, top = the Nyquist octave):

| kVelRamp | 3.8 px/frame RMSE / mid / top | 21.8 px/frame RMSE / mid / top |
|---|---|---|
| 0.000 | 10.02 / 0.245 / 0.305 | 10.56 / 0.288 / 0.408 |
| **0.015** | **9.42 / 0.299 / 0.373** | **9.23 / 0.719 / 1.118** |
| 0.030 | 8.90 / 0.370 / 0.476 | 10.80 / 1.444 / 2.504 |
| 0.060 | 8.15 / 0.557 / 0.784 | 11.15 / 1.563 / 2.734 |
| no TAA | — | 12.87 / 2.487 / 4.628 |

0.015 is the largest ramp that still keeps the promise at speed: it minimises the distance to the truth
on the fast pan and holds the Nyquist octave at 1.1×, where 0.030 lets it back to 2.5× — two thirds of
the way to having no antialiasing at all while panning. `kFeedMax = 0.85` `[SET]`: a pixel moving fast
enough to reach the cap has nothing usable in the history, and the remaining 0.15 keeps a slow object
inside a fast pan from re-aliasing.

### 4 Ghosting, and how it is asked without a biased metric

The obvious test — correlate `TAA_k − noTAA_k` against `noTAA_{k−1} − noTAA_k` — **is biased and was
discarded**: both terms contain the aliasing noise of frame k with the same sign, so the correlation is
positive (+0.63 measured) with zero ghosting. What replaces it needs no ground truth:

> The same pose reached two ways. **Moving:** frame k of a sequence. **Settled:** the camera placed at
> pose k outright, the history reset, 240 frames rendered with everything frozen. The settled frame is
> the ghost-free TAA answer at that pose, so their difference is the temporal lag and nothing else.

| arm | \|moving − settled\| mean | p99 | lag, frames |
|---|---:|---:|---:|
| yaw 0.35°/frame (3.8 px/frame), TAA | 4.00 codes | 41.4 | **+0.148** |
| the same pose without TAA at all | 5.01 codes | 60.0 | −0.252 |
| wind only, camera still, motion vector from `wave.w` | 4.16 | 43.0 | **+0.168** |
| wind only, motion vector from the camera alone (`FB_TAA_WINDMV=0`) | 4.51 | 41.8 | +0.204 |
| wind only, no TAA | 4.96 | 59.1 | −0.285 |

0.148 frames at 3.8 px/frame of image motion is **0.56 px of smear**, and the moving TAA frame is
closer to the ghost-free answer (4.00 codes) than an un-antialiased render of the same pose is (5.01).
The wind motion vector is worth 18 % of the lag, and by range band it is worth most where the blades
move most: 0.248 → 0.197 at 0–3 m, 0.220 → 0.186 at 3–8 m, 0.084 → 0.073 at 8–15 m.

### 5 Cost and memory

Min of 3 × 200 frames, pinned binary `a386ccc0`, 1280×720, all arms the same binary:

| arm | ms/frame |
|---|---:|
| `FB_GEOM=1` (no ground cover), TAA off | 2.848 |
| `FB_GEOM=1`, TAA on | 3.646 |
| ground cover, `FB_WIND=0`, TAA off | 69.169 |
| ground cover, `FB_WIND=0`, TAA on | 73.079 |
| ground cover + wind, TAA off | 83.308 |
| ground cover + wind, TAA on, camera-only motion vector | 91.409 |
| ground cover + wind, TAA on | 103.639 |

| item | ms | how it is isolated |
|---|---:|---|
| **the resolve pass itself** | **0.798** | the `FB_GEOM` pair — no blade in the frame |
| the motion attachment over 1.4 M blades, wind off | 3.112 | the `FB_WIND=0` pair, minus the resolve |
| everything except the wind's own motion vector | 8.10 | `FB_TAA_WINDMV=0` against `FB_TAA=0` |
| **the previous-frame blade station** | **12.23** | `FB_TAA=1` against `FB_TAA_WINDMV=0` |
| **total** | **20.33** | |

Memory: **17.578 MiB** (`walk terrain … temporalVramMB=17.5781`) = two `rgba16float` histories
(2 × 1280 × 720 × 8 B) plus the `rg16float` motion attachment (1280 × 720 × 4 B). It is GPU memory and
not WASM heap: the browser's `HEAPU8` stays at exactly the 256 MB `INITIAL_MEMORY` with the pass armed,
so the history buffer costs the WASM client nothing it had to grow for.

**The honest reading of that table:** the resolve is what was budgeted and the motion vector is not.
Twelve of the twenty milliseconds are one extra evaluation of the wind's own bending equation per
vertex, and they buy an 18 % reduction in temporal lag at the declared 6 m/s. A stronger wind, or a
tree, moves that ratio; the LOD round is where it should be revisited, because a blade that is drawn as
an aggregate has no station to evaluate twice.

**Rejected, with its measurement: the first-order previous station.** Rodrigues is analytic in its
angle, so the previous station is the current one plus `d/da` times the frame's change of tip angle,
reusing the sine, cosine, cross and dot the current station already computed. It saves 4.63 ms of a
98.6 ms frame and costs a motion vector wrong by up to **56 display codes** on the nearest blades (mean
0.26, p99 3.1, 2.2 % of pixels over 2 codes, over a moving sequence) — because the wave advances
0.48 rad of phase per frame at 30 Hz, which is not the small angle the expansion assumes. `FB_TAA_FO=1`
keeps it available so the trade stays measurable.

### 6 Switches

| Switch | Default | Effect |
|---|---|---|
| `FB_TAA` | **1 (armed)** | 0 disarms the jitter (`TemporalJitter::Disarm`) and forces `HistoryValid` false, so the resolve returns the current frame. Same pass count, same buffers, same binary. Three jobs, and the last two are the reason it exists: **show a critic the unfiltered frame** (a flickering shading defect is smoothed by this stage, and whoever judges the filtered frame judges the filter too), and **give the determinism work its baseline** — without TAA the settled frame is already byte-identical across warm-up lengths |
| `FB_JITTER` | off | `x,y` pins the sub-pixel phase in pixels instead of walking the sequence — the world-fixity measurement in §1 |

**Gone with the ground cover, and this table said otherwise until 2026-08-07:** `FB_TAA_GAMMA`,
`FB_TAA_VELRAMP`, `FB_TAA_FEEDMAX`, `FB_TAA_WINDMV`, `FB_TAA_FO`. `grep FB_TAA_ sim/src` is empty; the
three surviving numbers are `kTaaGamma` 1.5, `kTaaVelRamp` 0.015 and `kTaaFeedMax` 0.85, `constexpr` in
`TaaStage.cpp` with their measured curves still in §2 and §3. §4 and §5 describe a blade motion vector
that no longer has blades.

### 7 How many frames a reset costs, measured

`gpu_walk --settle N` against the same frame at `--settle 512`, 1280×720, the reference scene, one
resident world (`--warm` runs to residency, so the loading state is the same in every row):

| settle | px differing at all | > 2 codes | > 8 codes | max |
|---:|---:|---:|---:|---:|
| 0 | 78 118 (8.48 %) | 3924 | 357 | 39 |
| 16 | 68 305 (7.41 %) | 1788 | 15 | 35 |
| 32 | 15 437 (1.68 %) | 29 | 3 | 12 |
| 48 | 8494 (0.92 %) | 22 | 1 | 19 |
| 64 | 4790 (0.52 %) | 15 | 1 | 9 |
| **128** | **1399 (0.15 %)** | **7** | **1** | **15** |
| 192 | 3633 (0.39 %) | 7 | 0 | 7 |
| 256 | 4289 (0.47 %) | 7 | 2 | 19 |
| 384 | 4270 (0.46 %) | 8 | 1 | 20 |

**128 is the knee and is what `kTemporalSettleFrames` is set to.** Past it the > 2-codes population
does not fall any further; the surviving 7 px and the 0.15–0.47 % that differ by a single code are the
f16 history's last bit — the accumulation reaches a limit CYCLE of the jitter's eight phases, not a
fixed point, so a difference against a *different* settle length never becomes zero. The rows off the
multiples of 8 are much worse (settle 44: 7.91 % / 39 max) for the same reason, which is why the
number is a multiple of the phase count.

**What matters for an oracle is not that the difference reaches zero but that the SAME number gives
the same picture.** It does: 16 runs of `tools/determinism.py` at `FB_TILEWORKERS` 1, 2, 4 and 6 —
residency reached after 517, 437, 343 and 260 warm passes respectively, i.e. four genuinely different
tile arrival orders — produce **one** md5, `b9a48a34…`.
