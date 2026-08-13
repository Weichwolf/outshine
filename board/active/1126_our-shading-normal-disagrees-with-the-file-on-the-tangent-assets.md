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
