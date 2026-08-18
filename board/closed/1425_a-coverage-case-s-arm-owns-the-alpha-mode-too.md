Type: bug
Area: corpus
Tags: oracle, khronos

**A coverage case's arm owns the alpha mode too**

A case whose materials are the MANIFEST's replaces every surface with a flat emitter, and the preparer's
emission arm carries **no coverage at all** -- so the reference is opaque whatever the file's
`alphaMode` says. This engine honoured the file, and the two sides were then rendering different scenes.

**`GlassVaseFlowers` is the asset that shows it, and its two vases are the two ways to make glass**:

| material | how it is transparent |
|---|---|
| `GlassAlpha` | `alphaMode: BLEND`, base colour alpha **0.3** |
| `GlassTransmission` | `KHR_materials_transmission` + `KHR_materials_volume` |

**Our render drew the flower stems THROUGH the first vase and the reference did not**, and the two
pictures say so at a glance -- ours pale blue and see-through, the reference solid.

## It is the same repair as the transmissive one, one field further

`board:1386` established that a coverage case's arm owns the CLOSURE and not only the colour, and
cleared `Transmission` and `Thickness`. The alpha mode is the same statement about the same surface and
was left behind. It is cleared on the same condition and for the same reason.

## What it bought, and what it did NOT break

| case | after |
|---|---|
| `GlassVaseFlowers` | **green** -- IoU 0.99997307 with the coverage repair, and the picture agrees now |
| `CompareAlphaCoverage` | still green |
| `AlphaBlendModeTest` | still green |

**The two cases named for alpha were the risk and they were measured, not assumed.** Both take the same
`emission-per-material` arm, so both had the same reference all along; what changed is that this engine
stopped disagreeing with it.
