Type: issue
Area: corpus
Tags: oracle, khronos, instrument

**Nine bodies, one material, and the emission colour that cannot separate them**

`InterpolationTest` puts **nine animated cubes in one picture** and gives all nine **the same glTF
material** — `Material`, a plain metallic-roughness with no texture. The tenth node, a backdrop plane,
carries the only other one. The animated tier's scene declaration is
`material.kind = emission-per-material`, so a colour is chosen **per glTF material** and all nine cubes
would emit the same one.

**They overlap, and it is not marginal.** Projected through this case's own derived camera at 320×180,
counting pixels covered by two or more cubes:

| | |
|---|---|
| pixels covered by **two or more cubes** | **2428** over the 17-frame grid |
| worst single frame | **234 px**, at frame 0 |
| pixels covered by any cube, for scale | **≈2006** in the last frame |

So roughly a tenth of the subject's own pixels have two cubes in them. Under one flat emission colour
those silhouettes **fuse**, which is exactly what `BoxAnimated`'s material note already warns about —
it gave its two bodies two colours for this reason, and here there are nine bodies and one material.

*Twelve of the 153 (node, frame) projections were degenerate and are excluded: a `scale` channel takes
its cube to zero, which is the asset moving as declared and not an instrument fault.*

## Why it matters, stated narrowly

Fusion does **not** make the case unable to fail — a wrong pose still moves pixels, and the comparison
is per pixel. What it costs is **attribution and one failure mode**: two same-coloured bodies can
overlap such that a wrong pose is hidden behind a right one, and `board:1198`'s own clause is that
**`STEP` is the mode that would pass by accident.** A case built to separate three interpolations
should not be the one place where two bodies are indistinguishable.

## The options, and they are both defensible

| | what it does | what it costs |
|---|---|---|
| **A — `emission-per-node`** | a new declared material kind: one colour per **node** rather than per material | schema, the preparer's validation, `apply_material` on the oracle side, and a per-node override in the runner. The asset is **untouched**, and any future case with one material over many bodies inherits it |
| **B — patch the asset** | rung 3 of the ladder: a named `subject-patch` splitting `Material` into nine, applied identically to both sides | cheap and local, but it puts nine materials in a file Khronos ships with one, and every later reader of this case has to know why |
| **C — accept the fusion** | declare it, measure it, and score the case anyway | free, and it leaves the one masking mode above unexcluded in the case built to exclude it |

**RECOMMENDATION: A.** `emission-per-material` was already a *declaration about the scene* rather than a
fact about the asset, so `emission-per-node` is the same declaration at the granularity the picture
actually has — and it is the only option that does not either edit an upstream asset or leave a known
masking mode in place. B is the cheaper answer to this one case and the worse answer to the next one.

**This blocks the `InterpolationTest` case directory and nothing else.** `board:1198`'s oracle half, its
animation set and its Hermite are all landed and proven against `BoxAnimated`; it is the manifest that
waits on this.

## Comments

The measurement is worth keeping whichever way it goes: **the overlap is a property of the asset and
this engine's framing rule together**, not of any material choice, so option C's cost is fixed at 2428
px over the grid and does not need re-deriving if the decision is revisited.
