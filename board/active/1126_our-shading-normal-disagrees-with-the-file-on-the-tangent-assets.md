Type: bug
Area: render
Depends: 1122
Tags: oracle, khronos, instrument

**Our shading normal disagrees with the file on the tangent assets**

`board:1122` named the branch and this is the repair. Over the pixels where our shading normal and
Cycles' differ by more than the normal texture can express — 0.4°, derived from its 8-bit quantisation —
**the file's declared `NORMAL` is nearer Cycles in 60 065 of 60 065 pixels**: `normal-tangent-mirror`
39 029 of 39 029, `normal-tangent` 21 036 of 21 036. **Neither case has a single dissenting pixel.**

**What is ruled out, by measurement rather than by argument.** Fresnel and multiple-scattering GGX are
refuted by a pre-registered discriminator — the residual changes sign inside the disc at the highlight
and the rim is flat, and multiscatter dies twice since it can only make Cycles brighter while ours is
brighter over most of the dome. Handedness is refuted by a monotone ordering: the **tangent-free**
Geometry column is the *worst* and the two mirrored columns the *best*, where ignoring `w` would put the
mirrored ones far out ahead. Normal maps in general are refuted by `water-bottle`, which carries one and
agrees at p95 **0.064°**.

**The tension is resolved, and it resolves into TWO mechanisms rather than one.** The disputed pixels,
bucketed into four equal vertical bands across the frame:

| | band 0 | band 1 | band 2 | band 3 |
|---|---|---|---|---|
| `normal-tangent-mirror` | **840** | 18 617 | 12 371 | 7 201 |
| `normal-tangent` | **0** | 6 972 | 11 214 | 2 850 |

**Band 0 is clean** — a literal zero on one case, 2 % of the total on the other — and band 0 is where the
tangent-free Geometry column sits. *(Band is a position in the frame; the mapping to the asset's columns
is by layout and is not read from the file.)*

**So the earlier reading inverts, because the two measurements are about different quantities.** The
Geometry column was worst **in the picture** — appearance codes, 89.59 max non-flip over 1 125 over-bound
px. It is clean **in the shading normal**. Therefore:

- **where no tangent is used, our shading normal is right** to within what the texture can express;
- **the shading-normal defect is in the tangent path — the basis, not the map**;
- and whatever makes the tangent-free column worst *in the picture* is a **second mechanism**, since the
  normal there is correct. It needs its own item once this one is repaired.

That also explains `water-bottle`: carrying a normal map is not the same as stressing handedness the way
these two assets were built to.

**The remaining lead.** The disagreement is concentrated on the two assets
Khronos built for tangent handedness, while the per-column measurement found the tangent-free column
worst. Those two facts have not resolved into one mechanism, and **the shading normal is now a named
local at every arm**, so the value can be read at any point in its construction rather than inferred
from a highlight.

**Done when** the mechanism is named with a site, the repair is made, and the disputed population falls —
`p95 ours vs Cycles = 9.4786°` and the 60 065 unanimous pixels are the numbers stated before, and both
are restated after. **Five of the thirteen cases outside the picture bound are shading-normal cases**, so
this is where the bound is most likely to move.


## Comments

**A candidate found by reading, and it is algebra rather than a hypothesis.** `mappedNormal` in
`src/render/stages/SubjectDraw.cpp` carries the comment *a back face turns the whole frame around and
not only the normal … which is what the sign below does to all three.* **The code negates two axes, not
three:**

```
float3 n   = normalize(in.n) * side;
float3 raw = in.t.xyz * side;
float3 t   = normalize(raw - n * dot(n, raw));
float3 b   = cross(n, t) * in.t.w;
```

`side` flips `n` and `t`; the cross product is bilinear, so `cross(-n, -t) = cross(n, t)` and **`b` comes
out unnegated.** The back-face frame is `(-n, -t, +b)` where Khronos's sample viewer produces
`(-n, -t, -b)` by flipping `t`, `b` and the geometric normal explicitly rather than deriving `b` after
the flip. That is a left-handed frame on back faces, mirroring the map's y axis exactly where the comment
says it prevents that.

**It is a real defect and it is NOT yet shown to be THIS defect.** Whether these cases have back-facing
shaded fragments at all is unmeasured, and `normal-tangent` is a near-planar grid facing the camera.
**Measure the back-facing population before repairing** — if it is zero, this is a separate bug and the
tangent-path disagreement has another cause.

**A second candidate, unverified and cheaper to check than to argue about**: whether the normal map
reaches the sampler linear. `Image.h` states that a file's embedded gamma is ignored and *the slot
decides* — base colour and emissive sRGB, metallic-roughness and normal linear. If the normal slot is
uploaded through an sRGB path, the tap is wrong in exactly the arm that disputes and in no other, which
fits the band evidence as well as the frame does. **Read the upload path for the normal slot before
touching the shader.**

**The cheap falsifier fired and candidate 1 survives it.** Both assets declare **`doubleSided: true`** —
`NormalTangentTest` one material, `NormalTangentMirrorTest` one material — so back faces are not culled
and the back-face branch is reachable. Had they been single-sided this candidate would have been dead
without instrumentation.

**The decisive count is still owed: how many SHADED fragments are back-facing.** If it is zero the frame
defect is real and is about some other case; if it is a meaningful fraction of the 60 065 disputed
pixels, it is this one.

**The instrument for it is already attached and free.** The shading-normal target is `xyzw` and its
comment claims *the fourth channel marks whether a lobe was shaded at all* — **nothing reads `w`**; the
exclusion is by zero *length* of `xyz`, which the same comment's next clause states correctly. So `w` is
unused, the comment describes a role it does not play, and writing `front ? 1 : −1` into it costs one
term in a macro that already exists at all nine call sites. **Fix the comment in the round that uses the
channel**, since a channel documented as carrying one thing and carrying another is how this tree's
instruments have gone wrong three times today.

**Both candidates are refuted, and a deduction replaces them.**

- **Back-face frame** — real by algebra, **0 shaded back-facing fragments** on either asset. Filed as
  `board:1127`. `doubleSided: true` made the branch *reachable*; reachable is not exercised.
- **The normal map's colour space** — `BindSurface` uploads `slot.Normal` with `Transfer::Linear`,
  which is what glTF requires. Colour and emissive are `Srgb`. The slot decides, and correctly.

**The cause is downstream of the tangent source, and that follows from what the tree already tests.**
`test/unit/gltf/AGeneratedBasisIsTheOneTheExporterWrote.cpp` establishes that the two assets differ in
exactly one thing: **`NormalTangentMirrorTest` supplies `TANGENT` and is taken verbatim;
`NormalTangentTest` supplies none and is generated.** Khronos ships both to catch an engine that always
regenerates, and `Part::TangentSource` makes which one readable.

**Both disagree with Cycles.** If our generator were wrong only the second would; if our verbatim
handling were wrong only the first would. **Both, so it is neither** — unless there are two defects, one
per path, which is the less parsimonious reading and should be ruled out rather than assumed.

**And both sides start from the same basis.** The file's `TANGENT` *is* Blender's MikkTSpace output over
that exact mesh, and our generator is separately tested to reproduce the exporter's answer. So the two
sides agree on the basis and diverge in what is done with it: **the mechanism is inside `mappedNormal`'s
construction — the Gram-Schmidt, the scale, or the combination — and not in its inputs.**

**Next**: instrument the construction rather than reason about it. The shading normal is a named local at
every arm, so `t`, `b`, `n` and the sampled tap can each be published at a chosen pixel and compared
against the same quantities derived on the CPU from the file. **Two defects rather than one is a live
possibility and the per-asset split is what would show it.**

**The signature is measured and it is magnitude, not orientation.** Decomposing both legs about the
file's geometric normal over all 39 029 disputed pixels of `normal-tangent-mirror`:

| | |
|---|---|
| signed turn of the tangential part about `n` | p5 **−0.020°** · p50 **+0.000°** · p95 **+0.020°**, 50.13 % positive |
| `\|z_ours − z_cycles\|` | median 0.1235 |
| `‖tangential‖` difference | median 0.0984 |

**The tangential direction is identical to a fiftieth of a degree and symmetric about zero.** A
green-channel flip is a reflection, a wrong tangent direction is a rotation, a handedness error is a
reflection — **all three would show a turn and there is none.** Both legs lie on the same great circle
through the geometric normal: same direction, **different tilt**. So the basis is right and the map is
applied at the wrong strength.

**`normalScale` is refuted by its own signature.** It scales `tap.xy` uniformly and preserves direction
exactly — so a wrong one is a **constant**. Measured, `tan(tilt)_ours / tan(tilt)_cycles` runs p1 **1.020**
→ p50 **1.388** → p99 **2.331**, mean 1.465, std 0.343. **A spread of more than 2× is not a constant.**

**And Blender's `Strength` cannot be it either**, which is worth recording so the next round does not
chase it: glTF's `scale` multiplies `tap.xy`, Blender's Normal Map `Strength` mixes between the geometric
and mapped normals — **different operations**, but **neither asset declares `normalTexture.scale`**, so
both default to 1 and both operations are the identity.

**So the difference is value-dependent: small perturbations agree, large ones diverge increasingly.**

**A prediction, recorded as a prediction and not a finding**: if our side uses `tap.z` verbatim while
Cycles reconstructs `z = sqrt(1 − x² − y²)`, the two agree when `x,y` are small and diverge monotonically
as they grow — which is exactly the ratio's shape. **The instrument that decides it is now on disk.**

**The round's durable product: all three legs, per pixel, per case** — `outshine.normal.raw`,
`oracle.normal.raw`, `file.normal.raw`. Three quantities compared inside one process and none of them
openable is an investigation that has to be re-run to be questioned; these can be taken apart by
anything.

**CORRECTION, measured from the three legs on disk: the defect is a function of TILT and not of column,
and *the defect is in the tangent path* — written in this item and in a commit message — is wrong.**

Over all 374 566 shaded pixels of `normal-tangent-mirror`, the ratio of `sin(tilt)` ours to Cycles is
**1.000 at the median** and departs only in the top decile:

| `sin(tilt)` decile, Cycles | ratio |
|---|---|
| bottom nine | 0.998 – 1.003 |
| top, `[0.704, 0.785)` | **1.129** |

*(A median of 1.000 is also a check on both frame maps: a wrong one would make nothing agree.)*

**And the column confound is dead.** Splitting by band and holding tilt fixed:

| band | fraction with `sin(tilt) > 0.7` | ratio **where** `sin(tilt) > 0.7` |
|---|---|---|
| **0 — tangent-free** | **0.015** | **1.137** |
| 1 | 0.149 | 1.105 |
| 2 | 0.098 | 1.165 |
| 3 | 0.131 | 1.177 |

**Band 0 disagrees at the same rate as every other column once it has the same tilt.** It read as clean
because it has a tenth as many steeply-perturbed pixels, not because it is right. The median `sin(tilt)`
is 0.0055 in *every* band — the typical pixel is nearly flat and agrees, and all the action is in the
tail.

**So the tangent path is not implicated at all**, which agrees with the turn measurement rather than
contradicting it: the basis is right everywhere and the magnitude is wrong everywhere the perturbation is
steep. **The earlier *second mechanism* reading is withdrawn too** — band 0 being worst in the *picture*
while right in the *normal* was an artefact of its tilt distribution, not a separate defect.

**What remains is one question with a sharp shape**: what makes our mapped normal tilt ~13 % further in
sine than Cycles' once the perturbation exceeds about 45°, uniformly across every column, with the
tangential direction identical to a fiftieth of a degree.

**The next measurement, and it cannot come from the raws already on disk.** They hold the three
*resulting* normals; the question is now about the **tap** — the sampled texel before the basis is
applied — and about the `uv` it was sampled at. **Dump both**, at the shading point, the way the normal
already is.

**What each candidate predicts, written before the measurement:**

- **`tap.z` verbatim against a reconstructed `z = √(1 − x² − y²)`** → our tap's `z` is measurably below
  the reconstruction, by the amount the ratio implies: at `sin_c = 0.75` and ratio 1.129, `z_s ≈ 0.471`
  against a reconstruction of 0.661, so `‖tap‖ ≈ 0.886` rather than 1.
- **A sampling difference** — a half-texel offset, a different filter — → the two taps differ at the same
  `uv`, and the difference tracks the texture-space gradient rather than the tilt.
- **Neither** → the taps agree and the divergence is downstream of the sample, which would be the first
  real surprise in this investigation.

**`‖tap‖` is the single number that separates the first two**, and it is one dump away.

**The prediction is refuted by the texture itself, at no cost.** `‖tap‖` measured over all 4 194 304
texels: **p5–p95 = 1.000015**, and `|tap.z − √(1−x²−y²)|` has p50 **1.5e-5**. **The map is unit-length,
so the two forms of `z` are the same number.** No attachment, no plan change and no shader change were
needed — **`‖tap‖` is a property of the texture, not of the shader**, so a PNG decoder and arithmetic
settled what was scoped as a `Resource` row, a `Contributes` entry, a splice and a readback.

**The candidate that fits everything, stated as a candidate: we have no mipmaps.**
`src/render/stages/SubjectDraw.cpp:882` sets `num_levels = 1` and `:915` `mipmap_mode = NEAREST`. The
normal map is **2048×2048** on a subject spanning a few hundred screen pixels — heavy minification,
bilinear on level 0 — while **Cycles filters over the ray footprint**. Averaging a normal map toward its
mean **flattens** it, and that predicts every measurement without adjustment:

| measured | predicted by no-mipmaps |
|---|---|
| turn p50 **0.000°** | averaging taps that share a bearing changes **length, not direction** |
| ratio **1.129** top decile, up to **2.33** disputed | ours keeps the full perturbation, Cycles averages it away |
| ratio **1.000** at the median | where the map is smooth, averaging changes nothing |
| **band 0 not exempt** | this is the map's local **frequency**, not the tangent basis |
| `water-bottle` p95 **0.064°** | its normal map is low-frequency where it is visible |

**It is not called the mechanism, and the reason is precedent**: *everything fits* is how the
multiscatter hypothesis felt before its own signature killed it. **The test is direct**: at disputed
pixels, compare the level-0 tap against the tap averaged over the pixel's texel footprint and see which
reproduces Cycles' tilt. **It needs the `uv` channel**, which the three-channel trim dropped — one row in
`QUANTITY_PASSES` and a re-prepare, now measured at 228 s.

**And if it holds, the repair is a ladder question rather than an obvious fix.** Mipmapping a *normal*
map is not neutral: averaging unit vectors shortens them, which is why the literature normalises after or
carries the lost length as a roughness term. So *add mipmaps* is a shading change with a picture
consequence, and it lands on **fix the engine** only if our sampling is the non-conforming side —
Cycles filtering over a footprint against us point-sampling is a difference in **what a pixel means**.

**The mipmap candidate is refuted as the mechanism, by its own quantitative prediction.** The `uv`
channel is added — safe by construction now that `manifest.py` is digested — and the test needed no
attachment and no shader change: with `uv` per pixel and the map on disk, the footprint is arithmetic.

**Minification is real but modest**: `|du/dx|` p50 = 0.000692 over a 2048-square map = **1.42 texels per
screen pixel**, p95 2.54.

**And the disagreement is present where the map is smooth**, which filtering cannot explain — averaging a
smooth map changes nothing:

| local map roughness in the footprint | n | median ratio |
|---|---|---|
| **smooth `[0, 0.01)`** | **36 978** | **1.120** |
| mid `[0.01, 0.03)` | 2 213 | 1.173 |
| high-frequency | 1 281 | 1.177 |

correlation(roughness, ratio) over steep pixels = **+0.299**

**So filtering contributes and is not the bulk.** `board:1130` stands on its own merits — no mip chain is
a defect and a bandwidth problem — but it is **not** what `1126` is about.

**The residual, stated as precisely as it is now known**: ours tilts **≈1.12× further in sine** than
Cycles at steep perturbations, **even where the normal map is locally smooth**, with the tangential
direction identical to **a fiftieth of a degree** and the tap unit-length to **1.5e-5**.

**Eliminated with a number against each**: the tangent basis · handedness · a green-channel flip ·
`normalScale` · Blender's `Strength` · the tangent *source* · `tap.z` against a reconstructed `z` · and
filtering as the bulk explanation. **Whatever remains scales the perturbation's magnitude uniformly with
steepness and is not any of those.**
