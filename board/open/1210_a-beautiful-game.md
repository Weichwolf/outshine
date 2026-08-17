Type: feature
Area: corpus
Tags: khronos, extension

**ABeautifulGame has a case and it is not green**

*A Beautiful Game* -- tagged `extension`, `showcase`, `video` at the pin, published as `glTF`, `glTF-Binary`, `glTF-Binary-KTX-ETC1S-Draco`.

**A case exists and does not meet the finish line.** One of **19 of 148** in that state.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/ABeautifulGame` | met | outside | `picture_max_delta_code` |

**The failing metric names the mechanism, never the repair.** A bound widened to make this pass is a
number fitted to a case and is refused. The rungs are *fix the engine, reduce the oracle, patch the
asset, disqualify*, in that order, and disqualification is per `(case, metric)` rather than per case.
