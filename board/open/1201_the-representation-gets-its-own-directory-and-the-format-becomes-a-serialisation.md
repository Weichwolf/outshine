Type: task
Parent: 1197
Depends: 1202
Area: gltf
Tags: scope, instrument

**The representation gets its own directory, and the format becomes a serialisation of it**

Owner's question: *where in `src/` lives the internal glTF data representation of the scene? I would
like a dedicated folder for it.* It lives in `src/gltf/`, mixed with the file format, and the mixing is
what makes `board:1197`'s ruling a convention rather than a compile error.

**`board:1202` is this instruction applied to the whole tree**, and it subsumes the shape question: the
representation gets *src/scene/* there for the same reason every other subsystem gets its own. This item
stays because the **dependency inversion below is specific to these two directories** and is the only
part of the move that is not a rename.

## Why not `compositor/`

**The representation is the EDGE between generator and compositor, not a layer.** `CLAUDE.md` assigns the
compositor to `src/world` and `src/generators` and states it produces *one draw list, never geometry* —
and the representation **is** geometry. Under `compositor/`, a generator would have to include the
compositor's directory to emit a part, which inverts the dependency the decomposition rests on.

## What is actually there, measured

`src/gltf/` is **4925 lines doing two jobs**, and they are almost exactly half each:

| | lines | files |
|---|---|---|
| **the representation** | ≈2445 | `Subject` · `Pose` · `Transform` · `Camera` · `Framing` · `Track` · `Tangents` · `Variant.h` |
| **the format** | ≈2480 | `Document` · `Types` · `Emit` · `Variant.cpp` |

**And the split is already true in the headers — only the directory boundary is missing:**

```
Subject.h includes   PunctualLight · Span · Camera · Transform · Variant      -- no Document.h
Document.h included by   Document.cpp · Variant.cpp · Pose.cpp · Subject.cpp  -- .cpp only
outside src/gltf/    Document.h is included by NOTHING; Subject.h by one file
```

So the format reader is **already private in practice**, and the directory does not enforce it. That is
the whole argument: `CLAUDE.md` states layering is the build and not a checker, so a boundary that holds
only because nobody has crossed it is the kind this tree converts into an include set.

## The one structural wrinkle, and it is the real work

**`Subject::Build(const Document &, ...)` puts the flatten on the representation and takes the format as
its input**, which points the dependency the wrong way. It moves to the format side, and then:

| | depends on | and therefore cannot spell |
|---|---|---|
| *src/scene/* | `src/core/` | anything about a file — JSON, accessors, buffer views, extensions |
| `src/gltf/` | *src/scene/* · `src/core/` | — it is the serialisation, in both directions |
| a generator | *src/scene/* | `Document` |

**That is `board:1197` made structural**: *a generator replies a `Subject`, and glTF is a serialisation of
it where something outside this engine has to read it.* Today that sentence is true and nothing holds it.

## Done when

- [ ] *src/scene/* exists and holds the record; `src/gltf/` holds the format and depends on it
- [ ] The flatten is `Gltf`'s, not `Subject`'s, so *src/scene/* names nothing about a file
- [ ] `CLAUDE.md`'s layer table carries the row, and `Area: scene` joins the board vocabulary — **that
  vocabulary is the tree's own layering, so adding one means adding a directory**, which is what this is
- [ ] One compile group per layer in the `Makefile` and the same sets in `test/run.sh`, and
  `test/outshine/unit/` mirrors the new directory — **which is what turns the boundary into a compile
  error rather than a rule**
- [ ] `Tangents` is placed deliberately rather than by where it sits today: it operates on the flattened
  record, so it is `scene`'s, but it is *invoked* during the build and that is not the same argument

## Comments

**The cheap half is already done and nobody did it.** The expectation before measuring was that the two
concerns were interleaved through the headers and that separating them would be a rewrite. They are not:
`Subject.h` never names `Document`, and nothing outside the directory names `Document` at all. **The
refactor is a move plus one function relocation**, and the reason it looks bigger is the tree-wide
paperwork — the layer table, the compile groups, the unit mirror — not the code.
