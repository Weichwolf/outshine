Type: bug
Area: core
Tags: instrument

**Four vector vocabularies, and the one in `core/` is the reason for the other three**

`src/render/stages/NormalFromMap.h` introduced `Direction`, `Dot`, `Cross`, `Sum`, `Scaled` and
`Normalised` at namespace scope in `outshine::Render`, which had no vector vocabulary before. The
developer flagged it as a collision surface. **Pulling the thread finds the larger defect: it is the
fourth such vocabulary in this repository, and two of the four spell one operation two ways.**

| where | the type | the operations |
|---|---|---|
| `src/core/Mat4.h` | **raw `float *` triples** | `Vec3Normalize`, `Vec3Cross` |
| `src/generators/draw/` (`TreeVec3`) | a struct with operators | `Dot`, `Cross`, **`Normalize`** |
| `src/gltf/Tangents.cpp` (`Vector`) | file-local, so it leaks nothing | — |
| `src/render/stages/NormalFromMap.h` (`Direction`) | a struct, free functions | `Dot`, `Cross`, **`Normalised`**, `Sum`, `Scaled` |

**`Normalize` and `Normalised` are the same operation under two spellings in one repository** (`NL.8`,
consistent naming style), and three of the four are the same arithmetic written out again (`ES.3`). The
new names are also **short and non-local** — namespace scope in a header, reachable unqualified from any
of the 49 translation units that open `outshine::Render`, and by ADL from any `Direction` argument
anywhere — which is `ES.7` and `NL.7` read backwards: length proportional to scope, and `Sum` and `Scaled`
are verbs a future accumulator or a future scale helper will want.

**THE CAVEAT, SOUGHT FIRST, AND IT IS WHY THIS IS FILED AGAINST `core/` RATHER THAN AGAINST THE NEW
HEADER.** The obvious repair — one shared vector type — looks forbidden by the layering: `generators` and
`render` are peers and peers never call each other, so neither may own it. But the layering also says
where it *does* belong, and something is already there: **`core/` never points up, and `core/Mat4.h`
already carries the vocabulary.** It carries it as `Vec3Normalize(float *v)` and
`Vec3Cross(float *o, const float *a, const float *b)` — **an array passed as a single pointer**, which is
`I.13` explicitly, plus `F.24`/`R.14` (a half-open sequence is a `span`) and `Bounds.1` (no pointer
arithmetic). *That* is why three layers each wrote their own instead of using it: the shared one is in a
shape the Guidelines forbid and nobody wanted to spread it.

**So the defect is not the new header, and a rename would leave the cause standing.** The new header's
shape is the good one — a value type, free functions in its own namespace where ADL finds them (`C.5`),
`[[nodiscard]]` throughout, `double` because the reference half is checked in f64 against a device that
answers in f32. **What is wrong is that `core/` holds the worse spelling of the same idea**, so every layer
that needs three floats and a cross product writes a fourth.

**What would be right instead.** A single value type in `core/` in the shape `NormalFromMap.h` already
demonstrates, with the four call sites moved onto it and `Vec3Normalize`/`Vec3Cross`'s pointer interface
deleted with them. **The names get their scope's length** — the operations are the type's helpers and stay
short where they are found by ADL; what must not stay short is anything at namespace scope that is not
about the type. **`Direction` is the name to argue about**: it is the right word for a unit vector and the
wrong one for a position, and a `core/` type used for both is a name that lies at half its call sites.

**What this must NOT become.** A maths library. The scope is the operations that exist in the tree today —
dot, cross, scale, sum, normalise — and nothing anticipated. A vector type that grows a `Lerp` nobody
called is the same defect in the other direction.

**Done when** one vector value type in `core/` serves the layers above it, no header outside it declares
`Dot`, `Cross`, `Sum`, `Scaled`, `Normalise*` at namespace scope, `Vec3Normalize` and `Vec3Cross` no longer
take a `float *` array, and one spelling of *normalise* survives.
