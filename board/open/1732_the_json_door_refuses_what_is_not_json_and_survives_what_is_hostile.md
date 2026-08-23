Type: bug
Area: core
Tags: boundary, refusal, mirror

# The json door refuses what is not json and survives what is hostile

The parser under the whole gltf and provider surface crashes on depth and accepts
what the grammar forbids. Proven against the tree at 27caf0ca:

- `Json::ParseValue` recurses per nesting level with no depth bound
  (src/core/Json.cpp:44,68). A file of 200 000 `[` characters segfaults the
  process (measured, 8 MiB main stack; a 512 KiB worker stack falls around
  depth 12k). A hostile glTF is a crash, not a refusal — the reader's careful
  `Refuse` arms in Document.cpp sit BEHIND this door and never run.
- The grammar is not enforced: `[1 2 3]` parses as a 3-element array (no comma
  demanded, src/core/Json.cpp:56-78), `{"x":1} trailing garbage` parses (Parse
  never demands the text be consumed, src/core/Json.cpp:8-15), `truex` parses
  as true (src/core/Json.cpp:99). All measured.
- Kinds interconvert: `{"byteLength": true}` numifies to 1.0
  (src/core/Json.cpp:175-179), so `DeclaredSize` in Document.cpp accepts a
  boolean as a byte count — the 1727 checked-integer door is fed a value that
  was never a number.
- `\uXXXX` decoding emits raw surrogate halves as three-byte sequences
  (src/core/Json.cpp:127-136) — invalid UTF-8 out of a valid JSON pair.
- src/core/Json.cpp has NO unit twin under test/unit/core/ — the parser that
  guards every untrusted byte is proven only incidentally through gltf tests.
  Xml and Sha256 have theirs; Json does not.
- Mechanical bar: Json.h Ref queries `Size`, `operator[]`, `Key`, `Num`, `Int`,
  `Str`, `StoppedAt`, `Root` carry no `[[nodiscard]]` (src/core/Json.h:22-44).

Demanded: an explicit depth bound (a counter, not the C stack, or an iterative
parse) that REFUSES with the byte position; commas and full-text consumption
enforced; Number and Bool kept apart at the accessor; a unit twin
test/unit/core/ that proves the depth refusal, the grammar arms and the decode.
