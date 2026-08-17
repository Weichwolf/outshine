Type: bug
Area: render
Tags: oracle, instrument, khronos

**The picture bound has no term for a quantity interpolated across a triangle**

`render/materials/box-vertex-colors` misses the bound at **`picture_max_delta_code` 0.064038844 codes
against 0.000668135**, and it is the only claim it misses: the Khronos criterion is **met**, coverage,
silhouette, tie margin, seed shift and frame fraction all pass, and `worst_disagreement_px` is
0.00156 against a 0.005 floor.

**The excess is not the feature and that is measured rather than argued.** A third reference was built
for it — an exact ray-triangle interpolation of the file's own colours in double, cross-checked against a
perspective-correct screen-space formulation of the same quantity, the two agreeing to **2.5e-15**.
Against it, over 3 819 sampled channels:

| | |
|---|---|
| **ours** | median deviation **1.1e-16**, 942 channels exactly equal, p95 `+8.19e-6`, range `−2.07e-5 … +9.82e-6` linear — the GPU's varying interpolator |
| **the oracle** | p50 **1.583e-4** relative, p95 **2.556e-4** — which is exactly the case's own reported `linear_p50_relative`/`p95` |

**Ours is the more accurate side.** The worst pixel (814, 250) sits 0.17 px from the edge where blue
reaches zero, where the display transfer's slope is `T' = 12.92`, so ~1e-5 linear on each side becomes
±0.03 codes.

**The bound's only term is f32 arithmetic order.** It has no term for a **per-vertex quantity
interpolated across a triangle** — a mechanism no case had before, because flat cases have no gradient
and textured cases are dominated by the sampler term. A derived width would be
`255 × 12.92 × 2.07e-5 = 0.068` codes, **above the 0.064 measured**.

**This is `board:1151`'s sibling and must not be repaired the same way twice.** That item derives the
sampler's *coordinate* term; this one is the *interpolant* term. Both are terms the bound is silent
about, and both are candidates for the failure this repository names: **a derivation that arrives at
exactly the gap it needed to close is evidence against itself.** So the number is written down here,
**before** the derivation, and the derivation must land where it lands.

**Widening the bound is not the developer's** — `PictureBound.h` says so — and it was not taken.

**Done when** the bound carries a derived interpolant term with its own origin, or the case is decided
by a route that does not widen the bound, and the prediction stated above is compared against what the
derivation produces.
