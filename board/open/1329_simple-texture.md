Type: feature
Area: corpus
Tags: khronos, core

**SimpleTexture has a case and it is not green**

*Simple Texture* -- tagged `core`, `testing`, `written` at the pin, published as `glTF`, `glTF-Embedded`.

**A case exists and does not meet the finish line.** One of **19 of 148** in that state.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/SimpleTexture/four-texels-per-pixel` | red | outside | `picture_max_delta_code`, `picture_max_delta_code_alpha`, `linear_channels_differing` |
| `test/khronos/glTF/SimpleTexture/simple-texture` | red | within | `linear_channels_differing` |

**The failing metric names the mechanism, never the repair.** A bound widened to make this pass is a
number fitted to a case and is refused. The rungs are *fix the engine, reduce the oracle, patch the
asset, disqualify*, in that order, and disqualification is per `(case, metric)` rather than per case.
