Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials transmission a surface light passes through**

A transmission factor and an optional texture, for glass and for anything a viewer sees THROUGH rather
than a surface a viewer sees. **The most common non-opaque surface in a built world is a window**, so
this is not an enhancement.

**Two models at the pin require it**; thirty-three use it -- the widest gap in the honoured list.

- [ ] What a transmissive surface does to the frame graph is decided BEFORE the lobe: it needs what is
  behind it, and that is a pass question and not a material one
- [ ] It composes with `KHR_materials_ior`, which the reader already honours
- [ ] A stall is worse than a wrong pixel: whatever this costs is priced by the frame suite

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_transmission>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## Measured, so the round that builds this does not start by counting

**It is the largest coherent red class in the corpus and it is worth six cases, not two.**

Four cases score today and disagree about COVERAGE because we draw glass opaque and the oracle sees
through it -- [MEASURED] by IoU against the oracle's silhouette:

| case | IoU | worst disagreement |
|---|---|---|
| `GlassVaseFlowers` | **0.70129** | 13.84 px |
| `DiffuseTransmissionTest` | 0.86663 | 6.94 px |
| `CompareTransmission` | 0.87801 | 25.46 px |
| `TransmissionOrderTest` | 0.99814 | 2.52 px |

and two more refuse at the reader because they REQUIRE the extension: `CommercialRefrigerator` and
`PotOfCoalsAnimationPointer`. **Thirty-three of the 148 models use it** -- the widest gap in the
honoured list by a wide margin.

## What is NOT wrong today, checked before it was assumed

**The reader does not read the extension at all.** `Material::Transmission` exists as a field and
nothing fills it from glTF; `Emit.cpp` refuses to WRITE a transmissive material without the extension.
So this is honestly *not built* rather than half-built, and there is no silent wrong picture to undo
first -- which is the state `kHonouredExtensions`' own rule exists to keep.

## Why this one is not a material row and cannot be done the way the others were

`KHR_texture_transform` and `KHR_materials_specular` are DATA extensions: numbers composed at the
reader that a fragment already in flight can use. **A transmissive surface needs what is behind it**,
which no fragment has. In a rasteriser that is a PASS -- the opaque scene resolved into something the
transmissive draw can read -- so this lands in the render plan of `CLAUDE.md`'s stage catalogue and not
in a material struct.

**The first question the building round answers is therefore which stage**, before any lobe:
what writes it, what reads it, and what it costs against 16.67 ms. *A stall is worse than a wrong
pixel, and a full-screen copy per transmissive draw is exactly the shape that stalls.*
