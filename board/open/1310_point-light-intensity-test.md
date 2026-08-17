Type: feature
Area: corpus
Tags: khronos, extension

**PointLightIntensityTest has a case and it is not green**

*Point Light Intensity Test* -- tagged `extension`, `testing` at the pin, published as `glTF`, `glTF-Binary`.

**A case exists and does not meet the finish line.** One of **19 of 148** in that state.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/PointLightIntensityTest` | red | outside | `worst_disagreement_px`, `rgb_equals_white_channels_apart`, `gray_is_half_white_channels_apart`, `red_channel_equals_white_channels_apart`, `green_channel_equals_white_channels_apart`, `blue_channel_equals_white_channels_apart`, `picture_max_delta_code_alpha` |

**The failing metric names the mechanism, never the repair.** A bound widened to make this pass is a
number fitted to a case and is refused. The rungs are *fix the engine, reduce the oracle, patch the
asset, disqualify*, in that order, and disqualification is per `(case, metric)` rather than per case.
