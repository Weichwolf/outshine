Type: feature
Area: corpus
Tags: khronos, core

**TwoSidedPlane is green on both counts**

*Two Sided Plane* -- tagged `core` at the pin, published as `glTF`.

**Criteria met and the picture within the bound**, on the first run.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/TwoSidedPlane` | met | within | none |

**This case does NOT prove `doubleSided`, and it is named so it cannot be misread.** The framing rule puts the eye at elevation +20 degrees, so the camera sees the plane's front and the two-sided flag never decides a pixel. Proving it wants the eye below the plane, a declared camera, and its own case.

**It is `emission-per-material`, keyed by the name the FILE gives its material**, so the case states that
the material row reached the body. The colour is the manifest's and carries no claim about the asset's
own appearance.
