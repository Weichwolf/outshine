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
