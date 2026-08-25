Type: bug
Parent: 1395
Area: gltf
Tags: metadata, refusal, expected

# An absent metadata key is not a key whose value is a structure

`board:1395` reads `KHR_xmp_json_ld` at load. Every property is taken as a string:

```cpp
src/gltf/Document.cpp:544
        held.Held.push_back(MetadataProperty{key, packet[key.c_str()].Str("")});
```

and every lookup answers a string that may be empty for two different facts:

```cpp
src/gltf/Types.h:176
  [[nodiscard]] std::string_view Of(std::string_view key) const {
    for (const MetadataProperty &one : Held) {
      if (one.Key == key) { return one.Value; }
    }
    return {};
  }
```

XMP JSON-LD properties are JSON-LD values, and the packet in the tree's OWN fixture carries one
that is not a string:

```json
test/unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName.cpp:23
    {"@context":{"dc":"http://purl.org/dc/elements/1.1/"},
```

`Of("@context")` answers `""`. So does `Of("dc:nothing")`. The case asserts the second as a
feature:

```cpp
test/unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName.cpp:70
  CHECK(document.Metadata().front().Of("dc:nothing").empty(),
        "and a key the packet does not carry answers empty rather than inventing one");
```

That claim is proven by a predicate that cannot tell "not carried" from "carried, and this
reader dropped it". `dc:creator` is an object (`{"@list":[...]}`) in the registry's own example
and would land the same way -- key present, value silently nothing -- while the reader's
`extensionsUsed` list says `KHR_xmp_json_ld` is HONOURED (`src/gltf/Document.cpp:219`).

This is not a frame-path defect -- `board:1395`'s core requirement, that no metadata string
reaches a part name, is proven and holds. It is a reader that reports a value it did not read.

## What will be true

- [x] `MetadataPacket::Of` distinguishes absent from present: `std::expected<std::string_view,
      ...>` or `std::optional<std::string_view>`, so the two facts are two answers.
- [x] A property whose JSON value is not a string is either kept in a form that says what it is,
      or the packet refuses at load naming the key -- the same rule every other index in this
      reader carries. Silently storing `""` is the one option that must go.
- [x] The unit twin asserts BOTH: `Of("@context")` on the existing fixture is not the same
      answer as `Of("dc:nothing")`. Negative control: HEAD -> red, because they are equal.

## Comments

- 2026-08-25 -- filed by the hourly review, judging `board:1395`'s closure. The extension is read,
  the out-of-range packet index refuses, and the frame path spells no name -- three of four.

**Closed.** Three facts, three answers:

```cpp
src/gltf/Types.h:168   enum class MetadataShape : uint8_t { Text, Structure };
src/gltf/Types.h:182   [[nodiscard]] std::optional<std::string_view> Of(std::string_view key) const;
src/gltf/Types.h:191   [[nodiscard]] bool Carries(std::string_view key) const;
src/gltf/Document.cpp:545   const bool spelled = said.GetKind() == Json::Kind::String || Number || Bool;
```

| the packet | `Carries` | `Of` |
|---|---|---|
| does not hold the key | false | `nullopt` |
| holds it as a string, number or bool | true | the text |
| holds it as an object or array | true | `nullopt` |

The reader reports what it READ. A JSON-LD property that is a structure is kept as one and says
so, rather than being stored as `""` and answered as though it were absent -- which is what the
fixture's own `@context` did while the claim beside it asserted absence as a feature.

Proving test: `unit/gltf/MetadataIsHeldAndTheFramePathSpellsNoName`, run under the sanitiser:

```
NOTE the packet carries @context     = 1 yes
NOTE and Of answers a string for it  = 0 yes
```

Negative control: every property stored as `MetadataShape::Text` again -> the structure claim
goes red at :81.

board:1395's core requirement is untouched and still holds: no metadata string reaches a part
name, and the case still proves it.
