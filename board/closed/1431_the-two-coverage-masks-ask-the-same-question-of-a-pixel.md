Type: bug
Area: harness
Tags: oracle, instrument

**The two coverage masks ask the same question of a pixel**

The comparison built its two masks from two different questions and called the difference a
disagreement. Ours asks *is this pixel's centre inside the subject*, which is what a rasteriser answers.
The oracle's asked *does the subject touch this pixel at all* -- `alpha > 0` -- and where Cycles renders
with more than one sample per pixel its alpha is an AREA, so that predicate makes the oracle's mask one
whole anti-aliased fringe fatter than any centre-sampling renderer's can be.

[MEASURED] on `PointLightIntensityTest`:

| | before | after |
|---|---|---|
| `coverage_fraction_oracle` | 0.44308485 | **0.43557292** |
| `coverage_fraction_outshine` | 0.43557617 | 0.43557617, untouched |
| `pixels_disagreeing` | 6920, **every one the oracle's** | **3** |
| `iou` | 0.98305363 | **0.99999253** |
| `worst_disagreement_px` | 0.47218609 against a floor of 0.005 | **0** |

## Half a pixel is derived and not chosen

**A straight edge crossing a pixel divides it in two, and the centre lies in the larger part.** So
`coverage >= 0.5` IS the centre predicate for a straight edge rather than an approximation of it, and
the cut carries no free parameter. Where an edge is curved enough to break the equivalence inside one
pixel, the subject is finer than the raster and no predicate rescues that.

**THE RULE CONSTRAINS THE ORACLE'S SIDE AND SAYS SO.** Our alpha is not an area estimate -- an opaque
arm writes 1 and a blended arm accumulates what it drew -- so applying the same cut to it would drop a
genuinely transparent surface that we do draw. `CLAUDE.md`'s rule that a comparison names which side it
constrains is the whole reason this is written in `FromOracle` and nowhere else.

## The population, quoted with the number

[MEASURED] **145 of the 146 prepared oracles in this tree have strictly binary alpha**, for which the two
predicates name exactly the same set -- so this is provably a no-op on all of them. The 146th,
`SimpleTexture/four-texels-per-pixel`, has two fractional pixels and the same count under both
predicates. **One case moves and it is the one the measurement came from.**

## The manifest's own claim was false and is corrected

`PointLightIntensityTest` declared 256 samples for its eight lights' selection estimator -- Cycles picks
one light per shading event -- and stated in the same sentence that *every one of the 256 samples is
taken at the pixel centre*. It is not: **the filter width weights a sample into its own pixel; it does
not place the sample.** [MEASURED] the alpha profile across a silhouette is `0, 0, 0.1016, 1, 1`, which
is 26 of 256 and an area. The note now says what the count actually produces, in both of its effects.

## Comments

Found while reading why `PointLightIntensityTest` reported 6920 one-sided coverage pixels. The first
instrument was wrong and said so: measuring the two masks from the written PNGs put the difference at
**3** pixels, because an 8-bit alpha of 0.0039 survives a `> 127` test as absent -- the harness reads the
f32 tap and the PNG is not it. The lesson is the general one: **quote the population the metric is
computed over, not one that looks like it.**

Two of the logs consulted while ranking this metric across the tree were **two days stale**, from a run
before the case moved to `test/khronos/glTF/PointLightIntensityTest/` -- `CLAUDE.md`'s warning that a
partial run leaves the previous run's logs in place, met in the field. Read the trailer first.
