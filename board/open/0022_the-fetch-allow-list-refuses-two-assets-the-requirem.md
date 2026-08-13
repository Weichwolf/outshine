Type: bug
Area: corpus
Tags: oracle

**The fetch allow-list refuses two assets the requirements already name**

`test/corpus/prep/fetch.py:19-25` lists four subdirectories of `download.blender.org/demo/` —
`cycles/`, `eevee/`, `bbb/`, `asset-bundles/`. Two assets `board/` § I.26 already requires
are outside all four and are therefore unfetchable today:

| Asset | URL | What it is |
|---|---|---|
| Barcelona Pavilion | a Blender demo archive | **rungs 19 and 21** — scene scale, and the film summit |
| Sprite Fright shot | `demo/sprite_fright_030_0020_A.zip` | **the forest rung's motion arm** (§ I.26.7) |

a Blender demo archive is in the same position. This is the enumeration-versus-invariant defect
again: the list names *what happened to be needed the day it was written*, and it was already stale
against the same document when it was committed.

**The wrong fix is one more line.** The allow-list's job is *is this host a source at all* — an index
that can be walked, a licence convention that can be read per file, no account — and that is true of
`download.blender.org/demo/` entire. Nothing under it is refused **by path**: `lone-monk`,
`mr_elephant`, `tree_creature` and `loft.blend` are refused by the named-subject table in `licence.py`,
which is the mechanism that reads licences and the only one that can. Widening therefore weakens
nothing, because the bytes are pinned by per-file SHA-256 either way.

**Right:** `https://download.blender.org/demo/` as one prefix. Fixed when the pavilion and the Sprite
Fright archive fetch under an unmodified `fetch.py`, and when `licence.py` still refuses `mr_elephant`.
