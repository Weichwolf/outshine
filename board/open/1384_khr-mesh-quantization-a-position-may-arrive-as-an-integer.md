Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr mesh quantization a position may arrive as an integer**

Positions, normals, tangents and texture coordinates may use integer component types the base format
forbids for those attributes. **It is structural** -- it changes how vertex data DECODES, which is
upstream of every material question -- and it is the compact form most shipped assets use.

**One model at the pin requires it.**

- [ ] The accessor decode is where this lives, not the draw path
- [ ] Normalised and unnormalised integer forms are both decided, and a test says which is which
- [ ] The bounds a case's camera is derived from are computed AFTER the decode, or the framing rule
  frames integers

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_mesh_quantization>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).
