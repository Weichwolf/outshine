Type: bug
Parent: 1382
Area: gltf
Tags: khronos, xmp

# The reader keeps the XMP value it read, and every object that points at a packet reaches one

`board:1849` closed on *"the reader reports what it read"*, and what it now reports honestly is
that it read nothing: a property whose JSON value is an object or an array is stored with
`MetadataShape::Structure` and an EMPTY `Value`.

```cpp
src/gltf/Document.cpp:545   const bool spelled = said.GetKind() == Json::Kind::String ||
src/gltf/Document.cpp:546                        said.GetKind() == Json::Kind::Number ||
src/gltf/Document.cpp:547                        said.GetKind() == Json::Kind::Bool;
src/gltf/Document.cpp:548   held.Held.push_back(MetadataProperty{key, spelled ? said.Str("") : std::string(),
```

The distinction is repaired. The gap it made visible is not, and `KHR_xmp_json_ld` sits on
`kHonouredExtensions` (`src/gltf/Document.cpp:219`) -- the list `extensionsRequired` is refused
against (`src/gltf/Document.cpp:531`). Declaring an extension honoured is a promise.

## 1. The normative value shape is exactly the one that is dropped

An XMP property is a JSON-LD value: `dc:title` is an `rdf:Alt`, `dc:creator` an `rdf:Seq`,
`@context` a prefix-to-IRI map. The tree's own fixture proves the shape in one line --

```
test/unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName.cpp:23
    {"@context":{"dc":"http://purl.org/dc/elements/1.1/"},
```

-- and the case beside it now ASSERTS the drop as a feature (`:80`). Its `dc:title` is a bare
string, which is legal glTF and is not what a produced XMP packet looks like, so the reader is
proven against the easy half of its own extension. Without `@context` no consumer can resolve
`dc:` to its IRI at all: the keys the packet is "reachable by" are literal spellings and nothing
more.

## 2. Six of the seven carriers are never read

`board:1395`'s closure names them from the fetched registry: *"the objects that may carry it are
asset, scene, node, mesh, material, image and animation."* The reader reads one.

```cpp
src/gltf/Document.cpp:554   const Json::Ref onAsset = root["asset"]["extensions"]["KHR_xmp_json_ld"]["packet"];
src/gltf/Document.h:37      [[nodiscard]] int MetadataOfAsset() const { return AssetMetadata_; }
```

A node, mesh, material, image, scene or animation that points at a packet is silently ignored --
not refused, not held. The out-of-range refusal that guards the asset pointer guards nothing
else, because nothing else is read.

## 3. And the read is by string where it has an index

```cpp
src/gltf/Document.cpp:541   const std::string key = packet.Key(at);
src/gltf/Document.cpp:543   const Json::Ref said = packet[key.c_str()];
```

One `std::string` allocated per property, then a linear search of the same object for the key
just taken from it -- O(n^2) with an allocation per key, where `Json::Ref::operator[](size_t)`
(`src/core/Json.h:24`) already reaches the value at `at`. Duplicate keys resolve to the first, so
a packet carrying one twice stores one value twice.

## What will be true

- [ ] A property whose value is a structure keeps that value -- the packet's JSON text at
      minimum -- or `KHR_xmp_json_ld` leaves `kHonouredExtensions` and a file requiring it is
      refused by name, the way archived `KHR_xmp` already is.
- [ ] Every object the spec lets carry a packet pointer reaches one through the reader, or an
      unread carrier is REFUSED rather than ignored.
- [ ] The property loop reads by index in one pass, with no `std::string` per key.
- [ ] Proving test: `unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName` extended with a
      fixture in the produced shape (`dc:title` as an `rdf:Alt`) and a packet pointer on a node.
      Negative control: HEAD -> the title reads as carried-but-not-text and the node's packet is
      unreachable.
- [ ] Measured against the real subject: `khronos/glTF/XmpMetadataRoundedCube` is a declared case
      whose subject is NOT fetched -- its prepared directory holds `manifest.json` and nothing
      else -- so no run in this tree has ever put a produced XMP packet through this reader.

## Comments

- 2026-08-25 -- filed by the hourly review, checking `board:1849`'s closure rather than trusting
  its commit message. 1849's repair is right and complete for what 1849 said; this is what it
  made visible.
