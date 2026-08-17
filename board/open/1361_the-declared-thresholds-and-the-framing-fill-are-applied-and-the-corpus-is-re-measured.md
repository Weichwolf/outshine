Type: feature
Area: render
Tags: oracle, instrument, khronos

**The declared thresholds and the framing fill are applied, and the corpus is re-measured**

Three decisions were taken on `board:1359` and `board:1360` and **not one of them is implemented**. This
is the dispatch, and it is deliberately one item because all three move the same numbers and a
before-and-after that mixed them with anything else would attribute nothing.

| change | where | decided in |
|---|---|---|
| a **perceptual floor of 1.0 code** under every case's derived bound | the picture bound's derivation | `board:1359` |
| the verdict on **p99.99** of the channel difference, the **maximum still reported** | the render harness's scoring | `board:1359` |
| **`kFramingFill` 0.6 -> 0.9**, every non-`exact` camera re-derived | `src/gltf/Framing.h` and every manifest | `board:1360` |

## The order matters and it is not the order they were decided in

**The fill goes LAST.** It re-renders every oracle product, so a threshold change measured across it
would have two causes in one diff. **Thresholds first, measured; then the fill, measured.** Two
before-and-afters, each with one cause.

## What must be true

- [ ] **The floor and the percentile are read from a declaration, not written as constants** in the
  scorer. A threshold spelled in code is a threshold nobody can see in a manifest diff
- [ ] **The maximum keeps its own row on every report.** It stops being the verdict and does not stop
  being a number
- [ ] **The before-and-after quotes the same population both times.** 44 cases, and the count of cases
  that changed verdict is named per id — *`CLAUDE.md`'s own rule that a number can be broken by moving
  the population underneath it applies hardest to a change that is about a population*
- [ ] **Every non-`exact` camera is re-derived from the rule** and
  `test/outshine/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp` is green again, which is
  what says the re-derivation was the rule's and not a hand edit
- [ ] **`exact` cases keep their cameras**, and the exemption is read from `acceptanceClass` rather than
  from a list

## What this item may NOT do

**It may not report a count of cases the thresholds bought until the run has produced one.** `board:1359`
declared the shape before the re-run precisely so that the number is a measurement, and quoting an
expectation here would spend that.

## The prediction, written down so it can be wrong

**The floor at 1.0 code should take in `BoxVertexColors`** — [MEASURED] 0.064038844 codes against a
derived bound of 0.000668135. *And that is the same case `board:1195` would fix by carrying the
interpolant term, so if both land the case is proven twice and the second proof is worth nothing; the
floor is the general rule and the term is the right answer for that case.*

**The percentile should take in `board:1136`'s four cases**, which exceed their bounds on fewer than ten
channels in 2.6 million. **It should take in nothing else**, and a case that goes green whose histogram
is broad is evidence the percentile is too generous rather than evidence of progress.
