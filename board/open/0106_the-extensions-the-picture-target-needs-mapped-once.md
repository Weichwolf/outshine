Type: feature
Area: render
Tags: scope

**The extensions the picture target needs, mapped once so the foliage stage does not discover them**

- [ ] **`KHR_materials_diffuse_transmission` is the first one foliage needs, and it is why a canopy reads as a canopy.** Light through a leaf rather than off it; without it a crown is a dark mass at every sun angle behind it, which is the single largest difference between a vegetation picture and a pile of geometry. **KCD is a vegetation picture, so this is not an enhancement**
- [ ] **`KHR_materials_transmission` + `KHR_materials_volume` + `KHR_materials_ior` for glass** — a window is the built world's most common non-opaque surface and GTA 5's built world is full of them
- [ ] **`KHR_materials_clearcoat` for car paint** · **`KHR_materials_sheen` for fabric** · **`KHR_materials_specular`, or roughness modulation, for a wet surface** — rain on a road is a roughness statement before it is anything else
- [ ] **Each lands with its behaviour and its ledger row in one round**, never as a parsed field waiting for a shader. *That is what the ledger's own comment already forbids and what this list must not be read as licensing*
