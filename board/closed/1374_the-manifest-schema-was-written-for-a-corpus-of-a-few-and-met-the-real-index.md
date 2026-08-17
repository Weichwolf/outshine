Type: bug
Area: corpus
Tags: khronos, instrument

**The manifest schema was written for a corpus of a few, and met the real index**

Seven of the ninety-four cases refused on the SCHEMA rather than on anything about their subject. Both
rules were correct for the cases that existed when they were written and wrong about glTF.

## `as` demanded a plain file name, and glTF references by relative URI

```
expected : a plain file name in the case directory
observed : 'EnvironmentTest_images/roughness_metallic_0.png'
observed : 'MODEL_ROUNDED_CUBE_PART_1/positions.bin'
```

**A glTF resolves its buffers and images by RELATIVE URI**, and models in the pinned index put theirs in
a subdirectory. Flattening the name would break the file's own reference — the `.gltf` would look for
`X_images/y.png` and find `y.png` beside it.

**The rule keeps what it was for and stops forbidding what the format requires**: nothing may escape the
case directory. No leading slash, no `.` or `..` segment, no empty segment — **and the pattern was
exercised rather than trusted**: `scene.gltf`, `X_images/y.png`, `a/b/c.png` pass; `/abs.png`,
`../escape.png`, `a/../b.png`, `./here.png`, `a//b.png`, `dir/` and the empty string are all held.

**`./x` is normalised rather than admitted.** `SheenChair` states `./chair_fabric_normal.png`, which
names the same file as `chair_fabric_normal.png` to glTF's own resolver — so the MANIFEST states the
normalised form and the schema stays strict about dot segments. *Loosening the rule to admit `.` would
have bought one case and cost the rule its edge.*

## `licence.year` demanded a string, and upstream sometimes writes a number

`GlassBrokenWindow` and `PlaysetLightTest` state `"year": 2023` and `2024` in their own `metadata.json`.
**A year is a label rather than a quantity** — nothing computes with it — so the authoring pass coerces
it and the schema keeps its type.

## What both have in common

**Neither was findable with a corpus of a few dozen hand-made cases.** The schema described what the
first cases happened to look like, and every case since had been shaped to fit it. *The first pass over
the whole index is the first time the schema was asked about assets nobody chose.*
