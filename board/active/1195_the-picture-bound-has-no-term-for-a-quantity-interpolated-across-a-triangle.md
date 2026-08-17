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

## The mechanism is TWO terms, and only the smaller one is arithmetic

**The deviation is not the interpolator. It is the rasteriser's subpixel grid**, and that is measured
rather than argued: the exact reference was rebuilt on each candidate grid and compared against the
same rendered image.

| grid | worst, affine arm | worst, perspective arm |
|---|---|---|
| unsnapped | 5.34653727e-06 | 3.42314443e-05 |
| 1/16 | 8.03656315e-05 | 5.14874205e-04 |
| 1/64 | 2.67834796e-05 | 1.71746312e-04 |
| 1/128 | 9.22138514e-06 | 3.95406712e-05 |
| **1/256** | **1.33373175e-07** | **2.08630491e-07** |
| 1/512 | 6.69475707e-06 | 3.34675280e-05 |
| 1/4096 | 5.02031315e-06 | 3.22191733e-05 |

**A factor of 40 on one arm and 164 on the other, with both neighbouring grids worse.** A minimum that
sharp measures the grid; it does not fit one. So [MEASURED] **this device snaps a projected vertex to
1/256 px**, and Metal's silence, Vulkan's `subPixelPrecisionBits` and D3D11's mandated 8 are all beside
the point — none of them is a statement about this device.

**What is left on that grid is f32 and nothing else.** 1.334e-7 and 2.086e-7 against a derived
`4 × 2^-24 = 2.384e-7` — four roundings of a quantity in [0, 1], which is what
`(lambda_i / w_i) / sum(lambda_j / w_j)` costs. **Both arms come in under the UNAMPLIFIED term**, though
the derivation only claims the perspective one within its own `w` ratio: the amplification applies to an
error in lambda and the final division's rounding happens after it. *The looser claim is the one kept,
because the tighter one would be the measurement wearing a derivation's clothes.*

## So the term is a function and a bare scalar was the wrong shape

An earlier draft of `test/harness/shared/InterpolantPrecision.h` was one number. **It cannot be**: the snap
term scales as `grid / triangle height`, so it is unbounded as a triangle grows thin, and a scalar is
only true beside a smallest height and a widest `w` ratio it does not carry. That is the *domain too
narrow* face of the failure `CLAUDE.md` names, and it was avoided by luck rather than by design — the
placeholder was zero, so the test was red rather than wrong.

> `error = 5 · (grid/2) / smallestHeight · widestWRatio + 4 · 2^-24`

Both constants of that derivation are stated in the header with their algebra. [MEASURED] the two arms
sit **4.675x** and **5.793x** under it, on a triangle of smallest height 394.458 px covering 94 844 px.
**That headroom is the price of a worst case over the snap phase**, which a rendered triangle does not
have: it carries one set of three offsets rather than the adversarial one.

## The prediction, compared as this item demanded

This item predicted **2.07e-5 linear** as the width needed to cover `box-vertex-colors`, and required
the derivation be compared against it rather than tuned to it.

**Inverted, 2.07e-5 is what this derivation gives a triangle of smallest height ~471 px at `w` ratio 1**
— larger than any triangle a box at that scale presents. So for that case the derived term is
**comfortably above** what was needed rather than equal to it, which is the direction that clears the
suspicion this item raised against itself. *It is an inversion of numbers already in hand and NOT a
measurement of that case's geometry, which is owed below.*

## What is still missing, and it is the item's own headline

**`PictureBound.h` still carries no interpolant term.** The derivation exists, its two device constants
are measured, and nothing reads it — because the term needs each case's own smallest triangle height and
widest `w` ratio, which the render harness does not compute today. **The headline of this item is
therefore still literally true**, and it stays open with the work named rather than closed on a header
nobody consumes.
