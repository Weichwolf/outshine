Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr xmp json ld metadata that must not reach the frame path**

XMP metadata packets attached to the asset, scenes, nodes, meshes, materials and images. **It carries no
picture and that is precisely the requirement**: it must parse, be reachable, and never put a string on
the frame path.

**Two models at the pin carry it**, and one of them -- `XmpMetadataRoundedCube` -- is red today.

- [ ] Metadata is held where a name is held, and the frame path spells no name
- [ ] The archived `KHR_xmp` is NOT implemented by this task, and the difference is stated

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_xmp_json_ld>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).
