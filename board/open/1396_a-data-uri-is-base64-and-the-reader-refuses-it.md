Type: task
Parent: 1382
Area: gltf
Tags: khronos

**A data uri is base64 and the reader refuses it**

`buffer N is a data: URI, which this reader does not decode` -- the one gap the reader names about base
glTF rather than about an extension. A `data:` URI is legal for a buffer and for an image, and a `.gltf`
that embeds its own bytes is the ordinary shape of a small asset.

- [ ] Both buffers and images decode
- [ ] The decode is bounded: a declared length that disagrees with the payload is a refusal, not a resize
- [ ] It happens at load and never on the frame path
