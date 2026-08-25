Type: task
State: open
Parent: 1382
Area: gltf
Tags: khronos

**Khr draco mesh compression geometry that arrives compressed**

Primitive geometry compressed with Draco, decoded at load. **It is structural and it is the one extension
on this list that needs a dependency** -- there is no vendored third-party tree, so it is a package the
host provides or it is ours, and which of those it is is the first question this task answers.

- [ ] Decide the dependency question before any code, and record the decision
- [ ] The decode is off the frame path, because nothing on the frame path blocks or allocates

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).
