Type: task
Parent: 1382
Area: gltf
Tags: khronos

**A data uri is base64 and the reader refuses it**

`buffer N is a data: URI, which this reader does not decode` -- the one gap the reader names about base
glTF rather than about an extension. A `data:` URI is legal for a buffer and for an image, and a `.gltf`
that embeds its own bytes is the ordinary shape of a small asset.

- [x] Both buffers and images decode
- [x] The decode is bounded: a declared length that disagrees with the payload is a refusal, not a resize
- [x] It happens at load and never on the frame path

**Closed.**

```cpp
src/gltf/Document.cpp:107   [[nodiscard]] int SixBitsOf(char one);
src/gltf/Document.cpp:115   [[nodiscard]] bool Base64Payload(std::string_view uri, std::string_view &payload);
src/gltf/Document.cpp:124   [[nodiscard]] bool DecodeBase64(std::string_view payload, std::vector<uint8_t> &out);
```

**Both buffers and images.** A buffer's bytes decode into the buffer it declares. An IMAGE
becomes a buffer view the reader owns -- `image.View` set, `image.Uri` cleared -- so nothing
downstream sees a URI at all and there is no second path for an image to arrive by.

**The decode is bounded**, and each bound is a named refusal rather than a resize:

| what arrives | what the reader says |
|---|---|
| `data:...,AAAA` with no `;base64` | *declares no ;base64 payload, and this reader carries no other encoding* |
| `...;base64,AA*A` | *holds a character the alphabet does not, or a length no whole byte count can come from* |
| `byteLength 8` over a payload of 4 | *declares 8 bytes and its data: URI decodes to 4 -- a declared length that disagrees with its payload is a refusal rather than a resize* |

**At load, never on the frame path**: the decode happens inside `Read`, the bytes land in
`Buffers_`, and what leaves is a `BufferView` like any other.

Proving tests: `unit/gltf/AnEmbeddedAssetCarriesItsOwnBytes` -- a triangle whose buffer is a
data: URI and whose image is the eight-byte PNG signature, asserting three buffer views where
the file declares two and that the made one is exactly 8 bytes long; and
`unit/gltf/AFileThatCannotMeanAnythingIsRefusedByName`, whose data: case used to assert *"this
reader does not decode"* and now asserts the three refusals above. Both run under the sanitiser.

Negative control: the length check replaced by `if (false)` -> the refusal case goes red at :33
naming what it expected.
