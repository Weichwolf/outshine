Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr texture basisu a texture that arrives supercompressed**

KTX2 textures with Basis Universal supercompression, transcoded at load to whatever the device wants.
**Same dependency question as Draco** and the same answer must be reached deliberately.

- [ ] The transcode target is the DEVICE's own compressed format, or the extension buys nothing
- [ ] It composes with the sampler and the texture transform the reader already honours

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_basisu>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: structural** (see the parent's table).
