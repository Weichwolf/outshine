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
