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

**The unresolved tension, and it is the lead.** The disagreement is concentrated on the two assets
Khronos built for tangent handedness, while the per-column measurement found the tangent-free column
worst. Those two facts have not resolved into one mechanism, and **the shading normal is now a named
local at every arm**, so the value can be read at any point in its construction rather than inferred
from a highlight.

**Done when** the mechanism is named with a site, the repair is made, and the disputed population falls —
`p95 ours vs Cycles = 9.4786°` and the 60 065 unanimous pixels are the numbers stated before, and both
are restated after. **Five of the thirteen cases outside the picture bound are shading-normal cases**, so
this is where the bound is most likely to move.
