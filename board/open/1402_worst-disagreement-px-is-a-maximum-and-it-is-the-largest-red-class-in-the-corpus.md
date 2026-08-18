Type: issue
Area: harness
Tags: oracle, instrument, khronos

**`worst_disagreement_px` is a maximum, and it is the largest red class in the corpus**

**The decision is the owner's** because the metric's shape was chosen deliberately and its reason is
written down beside it: *a disagreement further from the silhouette than the oracle can resolve is not
a tie however few pixels of it there are*, and *a routed pixel that failed here while the picture read
`within` would be a picture defect reported as a feature defect*. Both are sound. **The evidence below
is that the population, not the direction, is what has gone wrong.**

## The measurement, on a case that was green before its camera was corrected

`MorphPrimitivesTest`, [MEASURED] over one frame:

| | |
|---|---|
| `worst_disagreement_px` | **36.458 px** against a floor of 0.005 -- FAIL |
| `boundary_p95_px` | **0 px** |
| `iou` | **0.99993573** |

**And the two renders are indistinguishable by eye**: the same red-and-blue plate, the same silhouette,
the same crease between the two materials, in the same pixels. **95 % of the boundary distances are
exactly zero and the masks overlap to 99.994 %**, so the 36 px is one isolated pixel that is nowhere
near a silhouette -- a sliver or a degenerate triangle, which is a different fact from *the geometry is
in the wrong place*.

## Why this is the same argument that already won once

`board:1367` moved the PICTURE verdict from a maximum to a p99 for exactly this reason, and the case
that made it visible was `WaterBottle` -- two renders the eye could not separate, scoring 149 codes on
one channel. **The geometric side kept its maximum**, and the same failure mode arrived there.

**The counter-argument is real and is why this is an issue rather than a task**: a hole is worse than a
wrong pixel, and a percentile over the boundary can hide a small hole in a large silhouette. *A
maximum cannot tell a hole from a sliver; a percentile cannot tell a sliver from a hole.* Neither
statistic alone is the question.

## The options

| | |
|---|---|
| **leave it** | 18 of the corpus's 40 red cases stay red, most of them on single pixels, and the count stops meaning *the picture is wrong* |
| **percentile it** | matches the picture side; **loses the ability to see a genuine 36 px displacement of a small feature** |
| **RECOMMENDED: keep the maximum and give it a population** | the metric stays a maximum over a CONNECTED COMPONENT of disagreement rather than over isolated pixels -- a hole has area and a sliver does not. It answers the hole question the maximum was for, and it does not fail on one pixel |

## What is NOT in question

**The threshold does not move.** A bound widened so a case passes is refused, and the recommendation
above changes what is measured rather than how much of it is allowed.

## Comments

**18 of 40 red cases fail on this metric** -- it is the largest single class, larger than the reader's
missing extensions and larger than every picture disagreement put together. *Two cases in the whole
corpus are red because the picture differs.*
