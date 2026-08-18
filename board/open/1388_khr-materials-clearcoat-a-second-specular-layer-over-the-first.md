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
