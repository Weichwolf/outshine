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

- [x] Inheritance is the rule and a test states it over a chain deeper than two
- [x] A hidden node's LIGHT does not reach the draw list either
- [x] A camera on a hidden node still resolves
- [x] The default is absence, and absence costs no branch -- the field defaults to `true` and the walk asks it once

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_node_visibility>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).

## Comments

**Implemented by NOT DESCENDING, which is the whole of it.** The extension's rule is a conjunction over
the path to the root, so an invisible node's entire subtree is invisible and the flatten simply stops:
no part, no light, no children pushed. That makes inheritance a property of the walk rather than a
field on a node, which is the only place the hierarchy exists.

**The camera exemption needed no arrangement and needed checking anyway.** Cameras are resolved by
their own scan over `document.Nodes()`, which never consulted this walk -- so the skip cannot reach
them today. The test states it because a later round that moved camera resolution INTO the walk would
inherit the skip silently, and a comment alone would not stop it.

**The two models this unblocks are still not scored, and the reason is the oracle rather than us.**
Blender 5.2.0's importer refuses `KHR_node_visibility` outright, so `CubeVisibility` and
`LightVisibility` cannot be prepared until `board:1381` gives a reduction a spelling. **The capability
is built regardless** -- that is the owner's ruling and it is why this item closes on its own test
rather than on a corpus case.
