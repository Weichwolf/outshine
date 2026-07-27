# Current State vs. Sources — 5 Key Findings

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../flightbox/render/clouds.md).**

Cross-references `command_center/fb/FBRenderer.cpp` (the shipped cloud pass, `CreateClouds`/
`UpdateClouds`/`kCloudWGSL`, lines ~1171–1562) against the recipes in files 01–08. Ranked by how
likely each is to be the dominant cause of "looks like image static," most likely first.

## 1. No temporal accumulation at all — the quarter-res march is fully re-rendered every frame with per-frame-changing jitter

**Where**: `FBRenderer.cpp:1357` (`jit = fract(52.9829189 * fract(dot(in.pos.xy, ...) + C.p1.w))`,
`C.p1.w` advances every frame) into `FBRenderer.cpp:1358` (`t = tStart + dt * jit`); the resulting
`CloudLowTex` (`FBRenderer.cpp:1475-1483`) has no ping-pong history buffer, and the tonemap pass
(`FBRenderer.cpp:773`) just bilinear-upsamples whatever this frame's fresh march produced.

**Why it matters**: both primary sources ([05-temporal-reprojection.md](05-temporal-reprojection.md))
run the cloud march at reduced resolution specifically to spread cost across MULTIPLE frames via a
history buffer + reprojection (HZD: 1-in-16 pixels/frame over 16 frames; Frostbite: every pixel of a
half-res buffer blended against reprojected history). FlightBox has the resolution reduction (quarter
res) WITHOUT the temporal averaging that resolution reduction is supposed to be amortized by — it pays
the undersampling cost of quarter-res while getting none of the noise-cancelling benefit of averaging
across frames. A per-frame-uncorrelated jitter with no accumulation is architecturally close to a
definition of "video noise." **Fix: add a history buffer + reprojection + exponential blend, per
[05-temporal-reprojection.md §5](05-temporal-reprojection.md).**

## 2. Density-scale multiplies AFTER the `[0,1]` saturate, silently pushing "density" up to 5.0 and saturating the powder term to a flat constant

**Where**: `FBRenderer.cpp:1298`, `return clamp(d, 0.0, 1.0) * C.p2.w;` with `C.p2.w = 5.0`
(`FBRenderer.cpp:1556`, "density scale -> optically thick, solid cumulus"). The variable named `dens`
downstream (`FBRenderer.cpp:1381` onward) therefore ranges up to 5.0, not `[0,1]`.

**Why it matters**: `kExtinct = 0.06` alone (`FBRenderer.cpp:1263`) sits correctly in the physical
cumulus extinction range (0.04–0.12/m, [02-density-coverage.md §4](02-density-coverage.md)) — but
`sigma = dens * kExtinct` then reaches **0.3/m** (cumulonimbus-core density) uniformly across a
"pleasant default 0.4 coverage deck" (`FBRenderer.cpp:1540`). Worse, the powder term
(`FBRenderer.cpp:1394`, `1.0 - exp(-dens * 6.0)`) saturates to ≈1.0 for almost any `dens` above ~1.0 —
i.e. **the view-dependent edge-darkening term that is supposed to give clouds their shaped,
non-flat look ([03-lighting-model.md §3](03-lighting-model.md)) is very likely contributing no visible
gradient at all today**, collapsing to a constant multiplier. **Fix: keep density in `[0,1]` and apply
the "5.0-equivalent" thickness increase to `kExtinct` (or a separate absolute-thickness multiplier
applied only to `sigma`, never to the `dens` value used in `powder`/erosion math).**

## 3. No coarse/fine two-tier stepping and no adaptive step placement — every one of up to 160 steps runs the full (base+detail) density sampler

**Where**: `density()` (`FBRenderer.cpp:1280-1299`) is called unconditionally for every step of the
main march (`FBRenderer.cpp:1377-1403`) AND for every one of the 6 `sunOD()` sub-steps
(`FBRenderer.cpp:1304-1317`) called from inside that same per-step loop — there is no cheap/coarse
isosurface pre-pass ([04-raymarch-strategy.md §1](04-raymarch-strategy.md)) and no early-exit on
consecutive zero-density samples ([04-raymarch-strategy.md §6](04-raymarch-strategy.md) table).

**Why it matters**: on the Iris Xe bandwidth budget (~50 GB/s vs. 176 GB/s PS4 /
[07-igpu-performance-budget.md §1](07-igpu-performance-budget.md)), this is likely the dominant cost
driver, not texture size — up to 16M texture fetches/frame in the worst case
([07-igpu-performance-budget.md §3](07-igpu-performance-budget.md)). This doesn't directly cause
"static," but it constrains how many steps/quality can be afforded once temporal accumulation (finding
#1) is fixed and the team wants to raise per-frame quality back up — the two fixes are complementary,
not substitutes. **Fix: add the cheap/full two-tier march ([04-raymarch-strategy.md §1](04-raymarch-strategy.md))
before spending more per-frame step budget.**

## 4. No haze/VIS coupling at all — a Koschmieder-driven ground-fog term is entirely absent from the terrain pass

**Where**: the terrain aerial-perspective term (`FBRenderer.cpp:446-455`) uses ONLY the Hillaire
Rayleigh/Mie/ozone atmosphere ratio (`tCamU`/`tFragU`, parametrized for the ~100 km-scale physical
atmosphere) — there is no separate boundary-layer haze/fog term anywhere in the shader, and
`HudState.vis`/telemetry visibility (referenced only in `flightsim.h:96` and the HUD readout,
`hud.h:287`) never feeds into any extinction coefficient.

**Why it matters**: per [06-haze-aerial-perspective.md §3](06-haze-aerial-perspective.md), a VIS
setting today cannot make the rendered scene hazier or clearer at all — the brief's explicit test case
("scene at VIS 5 km should not look like milk") can't currently be validated either way because there
is no mechanism connecting VIS to anything visual. This isn't the cause of the "static" symptom, but it
is a concrete, sourced, currently-100%-missing piece of the atmosphere/cloud rendering the brief asked
about. **Fix: add the `σ = 3.912/VIS` exponential height-fog term
([06-haze-aerial-perspective.md §1-2, §5](06-haze-aerial-perspective.md)), anchored to the existing
sky-view LUT horizon color so it doesn't introduce a second, differently-tinted haze.**

## 5. The base noise composite is architecturally a plain Perlin+Worley-erosion pair, not the Perlin-Worley DISPLACEMENT construction both primary sources use

**Where**: `kCloudBaseCS` (`FBRenderer.cpp:1225-1238`) stores plain `perlinFbm` in R and 3 independent
`worleyFbm` octaves in G/B/A — there is no step combining Worley as a **displacement of** the Perlin
field (the actual "Perlin-Worley" construction, [01-noise-construction.md §3](01-noise-construction.md)).
The `density()` function (`FBRenderer.cpp:1280-1299`) instead blends two frequency-scaled samples of
the plain-Perlin R channel directly, with Worley entering only later as edge erosion.

**Why it matters**: this is the mechanism Schneider 2015 explicitly identifies as the failure mode of
plain layered-Perlin fBm — "nice but very procedural looking... no larger governing shapes... lacks
those bulges and billows that give a sense of motion" (slides 24, 26,
[01-noise-construction.md §1](01-noise-construction.md)). It is a real, sourced gap, but ranked LAST
because it affects **shape quality** (does it look like a specific believable cumulus vs. a generic
soft blob), not the "static/interference" symptom the brief specifically flagged — that symptom is
much better explained by findings #1–#2 above. **Fix this only after #1 and #2 are resolved and the
image has stabilized enough to actually judge silhouette quality**, per the iteration order in
[08-experiment-protocol.md §5](08-experiment-protocol.md).

## Summary table

| # | Finding | File:line | Primary symptom it explains | Fix priority |
|---|---|---|---|---|
| 1 | No temporal accumulation, per-frame jitter | `FBRenderer.cpp:1357,1475` | **"Static"/noise itself** | Highest |
| 2 | Density-scale saturates powder to a flat constant | `FBRenderer.cpp:1298,1394,1556` | Flat/unshaped look, inflated extinction | High |
| 3 | No cheap/full two-tier march, no early-exit | `FBRenderer.cpp:1280,1377` | Perf headroom, not "static" directly | Medium (perf-enabling for #1) |
| 4 | No VIS→haze coupling | `FBRenderer.cpp:446-455` | Missing feature, not "static" | Medium (separate deliverable) |
| 5 | Plain Perlin+erode, not Perlin-Worley displacement | `FBRenderer.cpp:1225-1238` | Generic/blobby silhouette, not "static" | Lowest (do last) |
