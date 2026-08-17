Type: feature
Area: corpus
Tags: khronos, core

**NormalTangentTest has a case and it is not green**

*Normal-Tangent Test* -- tagged `core`, `testing` at the pin, published as `glTF`, `glTF-Binary`.

**A case exists and does not meet the finish line.** One of **19 of 148** in that state.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/NormalTangentTest` | red | outside | `row1_pair2_geometry_matches_normalmap_p95_relative`, `row2_pair2_geometry_matches_normalmap_p95_relative`, `row3_pair2_geometry_matches_normalmap_p95_relative`, `row4_pair2_geometry_matches_normalmap_p95_relative`, `picture_max_delta_code` |

**The failing metric names the mechanism, never the repair.** A bound widened to make this pass is a
number fitted to a case and is refused. The rungs are *fix the engine, reduce the oracle, patch the
asset, disqualify*, in that order, and disqualification is per `(case, metric)` rather than per case.
