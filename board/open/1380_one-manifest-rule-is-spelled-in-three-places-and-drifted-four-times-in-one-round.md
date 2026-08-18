Type: bug
Area: harness
Tags: instrument

**One manifest rule is spelled in three places, and drifted four times in one round**

A manifest is validated by `test/harness/shared/corpus/manifest-schema.json`, enforced in C++ by
`test/harness/shared/render/ManifestSchema.h` and enforced again in Python by
`test/harness/shared/corpus/prep/schema.py`. The JSON is supposed to be the single declaration. It is
not: the two enforcers carry rules of their own, and **the divergence is not theoretical**. [MEASURED]
in one round of work:

| the rule | who had it | who did not | cost |
|---|---|---|---|
| `filename` may carry a relative path | `prep/schema.py` | `ManifestSchema.h` | 6 manifests prepared and then refused at scoring |
| `emission-by-material-index` exists | C++ runner, Python applier | the JSON schema | every case using it refused before rendering |
| the origin enum is lower case | both schemas | the author | 70 manifests refused |
| percent-decoding a URI | `src/gltf/Document.cpp`, `in_blender_render.py` | the authoring helper | one model had no case for months |

**Three of the four point the dangerous way**: the preparer succeeds and the scorer refuses, or the
opposite -- so a case looks prepared and is unscorable, or looks unscorable and is fine.

## What the shape should be, and it is not more discipline

The JSON is a declaration a program has to *interpret*, and two interpreters are two implementations.
**The rule that cannot drift is the one only one side can spell.** Two candidates, and the second is
recommended:

| | |
|---|---|
| keep three places, add a cross-check test | a checker counts what a shape could forbid, and it is the weaker of the two -- `CLAUDE.md` says so directly |
| **the C++ enforcer is generated from the JSON, or the JSON is generated from the C++** | one authority, and the other side is a build product nobody edits. The predicate set is small -- eleven scalars -- so this is a table, not a compiler |

**The Python half is separate and stays**, because the preparer must refuse a manifest before Blender
starts and cannot link C++. What it may not carry is a rule of its own: it validates STRUCTURE against
the same JSON and nothing else.

## What must be true

- [ ] **One authority for the manifest's shape**, named, with the other side derived from it
- [ ] **No predicate is implemented twice.** A predicate the JSON cannot express is a reason to extend
  the JSON, not to write it in an enforcer
- [ ] **The four drifts above are regression cases**, so the instrument is exercised on the exact
  failures that produced it rather than on an invented one
