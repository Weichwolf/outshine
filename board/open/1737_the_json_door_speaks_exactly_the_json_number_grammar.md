Type: bug
Area: core
Tags: hostile-input, spec

# The json door speaks exactly the json number grammar, and Int cannot overflow

Residue of the 1732 repair (commit 403047ed). Empirically proven against today's
`src/core/Json.cpp` (UBSan build):

```
[01]    -> OK  num=1        RFC 8259: leading zero is not a number
[1.]    -> OK  num=1        fraction demands at least one digit
[-.5]   -> OK  num=-0.5     the integer part is not optional
[01.5]  -> OK  num=1.5
Int on {"n": 1e300} -> UBSan: 1e+300 is outside the range of 'int'  (Json.h:30)
```

## 1. The first-character gate is not the grammar

`src/core/Json.cpp:153` gates only `-` or a digit, then hands the whole tail to
`from_chars`, whose `chars_format::general` is laxer than JSON (leading zeros, `1.`,
`-.5` all consume). The comment at 151-152 claims the grammar; the code checks one byte
of it. Demand: pre-scan the token with the RFC 8259 pattern
(`-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`), hand `from_chars` exactly that span,
refuse anything shorter or longer. `[-]`, `[.5]`, `[1e]`, `[0x1F]`, `[1e999]` already
refuse correctly and must stay covered.

## 2. `Json::Ref::Int` is UB on any hostile large number

`src/core/Json.h:30` — `(int)Num(...)` is undefined for values outside int's range. Every
index accessor in the glTF reader funnels through it (104 call sites of `Int(`/`Num(` in
`src/gltf/Document.cpp` alone); a hostile `"mesh": 1e300` reaches the cast today, and the
sanitised gate layer would go red on the first such twin case. Demand: clamp inside
`Int()` (out-of-range or non-integral → the default, matching the DeclaredSize
philosophy at Document.cpp:400-408), plus twin cases in
`test/unit/core/AJsonDoorRefusesWhatIsNotJsonAndSurvivesWhatIsHostile.cpp` for both
defects — the four malformed numbers above and `Int` at 1e300 under the sanitiser.
