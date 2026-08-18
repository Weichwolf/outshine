Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr animation pointer an animation channel may target any property**

An animation channel whose target is a JSON pointer rather than one of the four node paths, so a file may
animate a base colour factor, a light intensity, a texture transform or a node's visibility.

**It is a SELECTION shape and the widest one**: the reader must resolve a pointer to a property it
already holds, which means the set of animatable properties becomes an enumeration the reader publishes
rather than a switch it hides.

**Five models use it**, and it is what `KHR_node_visibility` is specified to be used with.

- [ ] The set of pointers this reader resolves is ENUMERATED and published, and a pointer outside it is
  a named refusal rather than a silent no-op
- [ ] A pointer that resolves is animated by the same sampler machinery the four node paths use

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_animation_pointer>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: selection** (see the parent's table).
