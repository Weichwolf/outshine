Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr xmp json ld metadata that must not reach the frame path**

XMP metadata packets attached to the asset, scenes, nodes, meshes, materials and images. **It carries no
picture and that is precisely the requirement**: it must parse, be reachable, and never put a string on
the frame path.

**Two models at the pin carry it**, and one of them -- `XmpMetadataRoundedCube` -- is red today.

- [x] Metadata is held where a name is held, and the frame path spells no name
- [x] The archived `KHR_xmp` is NOT implemented by this task, and the difference is stated

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_xmp_json_ld>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: data** (see the parent's table).

**Closed.** The spec was FETCHED from the Khronos registry this hour, not recalled: packets live
at `extensions.KHR_xmp_json_ld.packets` on the document root; an object points at one by index
through `extensions.KHR_xmp_json_ld.packet`; the objects that may carry it are asset, scene,
node, mesh, material, image and animation.

```cpp
src/gltf/Types.h:168    struct MetadataProperty { std::string Key, Value; };
src/gltf/Types.h:173    struct MetadataPacket { ... [[nodiscard]] std::string_view Of(std::string_view key) const; };
src/gltf/Document.h:36  [[nodiscard]] const std::vector<MetadataPacket> &Metadata() const;
src/gltf/Document.h:37  [[nodiscard]] int MetadataOfAsset() const;
```

**Metadata is held where a name is held** -- prefixed keys and their values, reachable by
`Of("dc:title")` -- and an index outside the packets array is a refusal carrying both numbers,
the same rule every other index in this reader follows.

**The frame path spells no name from it**: the proving case builds the tagged document as a
`Subject` and asserts that no part name carries any string from the packet. That is the
requirement stated as a measurement rather than as an intention.

**The archived `KHR_xmp` is NOT implemented**, and the difference is that it is a different
extension: `KHR_xmp` (archived) carries an XMP packet as a string blob per object, while
`KHR_xmp_json_ld` carries JSON-LD packets in an array the objects index. This reader implements
the second and does not honour the first, so a file requiring `KHR_xmp` is refused by name at
`extensionsRequired`.

Proving test: `unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName`, four claims, run under the
sanitiser. Negative control: the packet-index bound replaced by `if (false)` -> the
out-of-range case goes red at :77.
