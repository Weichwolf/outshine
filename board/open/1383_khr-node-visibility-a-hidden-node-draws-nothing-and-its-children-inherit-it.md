Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr node visibility a hidden node draws nothing and its children inherit it**

A single boolean `visible` on a node, default `true`, and **it is inherited**: the specification's own
words are *a node is visible if and only if its own visible property is true and all its parents are
visible*. What is hidden is *all its visual features including but not limited to meshes, light sources
..., point clouds, particles, billboards, volumetric effects*; what is NOT hidden is cameras, and the
extension says so explicitly.

**It is structural and therefore the first of its shape.** A hidden node contributes no part and no
light to the flatten -- it is not a material field and not a draw-time branch, and a renderer that
learned about visibility would be a renderer that learned a content noun.

**Two models at the pin require it**: `CubeVisibility` and `LightVisibility`.

**The oracle cannot follow and that does not change the work.** [MEASURED] Blender 5.2.0's importer
refuses the file outright -- *Error: Extension KHR_node_visibility is not available on this addon
version*. So the picture is judged by eye and by an invariant over the drawn set, and the case declares
which instrument decided it (board:1381).

- [ ] Inheritance is the rule and a test states it over a chain deeper than two
- [ ] A hidden node's LIGHT does not reach the draw list either
- [ ] A camera on a hidden node still resolves
- [ ] The default is absence, and absence costs no branch

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_node_visibility>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).
