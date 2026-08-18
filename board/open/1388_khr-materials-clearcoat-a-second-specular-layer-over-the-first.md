Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr materials clearcoat a second specular layer over the first**

A clearcoat factor, roughness and an optional normal map: a thin dielectric layer over the base material
with its own normal. Car paint, lacquer, a varnished surface.

**One model at the pin requires it**; thirteen use it.

- [ ] The coat has its OWN normal and that is the part a naive implementation drops
- [ ] The base layer is attenuated by what the coat reflects, or energy is created

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_clearcoat>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

## The factors are built and the three textures are the rest

`ClearCoatCarPaint` -- the one model at the pin that REQUIRES the extension -- **scores green at 30
checks and within the picture bound**. It declares no clearcoat texture, so what is built decides it
honestly and completely.

**It was smaller than it looked, and the specification is why**: *the specular BRDF for the clearcoat
layer is computed using the specular term from the glTF 2.0 Metallic-Roughness material*. There is no
new lobe -- the same distribution and visibility, with the coat's own roughness, a fixed ior of 1.5 and
the extension's own layering operator `base * (1 - w*F) + layer * w*F`.

**What is left is the three textures**, and [MEASURED] at the pin they are not rare: of 40 clearcoat
materials, **18 declare `clearcoatTexture`, 16 `clearcoatRoughnessTexture` and 13
`clearcoatNormalTexture`**. A material declaring one is coated by its factor across the whole surface,
which is a plainer picture rather than a wrong one -- and it is named where the reader drops it.

*The coat's normal being the geometric one is NOT part of that shortfall: the extension states that a
coat with no normal texture takes no normal mapping at all, even where the base has one.*

- [x] The coat has its OWN normal and that is the part a naive implementation drops -- and the format's
  rule when the texture is absent is that it has none
- [x] The base layer is attenuated by what the coat reflects, or energy is created
- [ ] The three textures reach the shader, each on the material's own texture table
