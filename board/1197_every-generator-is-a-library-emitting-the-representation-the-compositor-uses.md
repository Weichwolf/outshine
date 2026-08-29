Type: feature
State: open
Area: generators
Tags: scope, contract

# Every generator is a library emitting the representation the compositor uses

**Benchmark** — Unreal: PCG emits into the same representation authored content uses. RAGE: the tool chain emits the cooked form. **Both agree** — a generator emits the representation the consumer already reads, never a private one.

One in-memory model, not two: a subject loaded from a file and a subject grown by a generator
are the SAME type, which makes *the compositor never learns what produced a part* structural
rather than conventional.

**THE REPRESENTATION THIS ITEM NAMED IS WITHDRAWN.** It read "`Gltf::Document` and `Gltf::Subject`
are the representation". They are not, and board:1949 carries the measurement that refutes it: an
interchange format's storage decisions reached inward, so the generators' float positions were
widened to double on the way in and narrowed back on the way to the device -- float to double to
float over 28 M vertices, 2 437 ms of assembly and 2 708 ms of packing on Shibuya, for a buffer that
is float either way.

**glTF is an IMPORT PATH.** The representation every producer emits is the one the DEVICE binds --
`RunsOf` declares it and a `static_assert` holds it -- and the glTF importer is one producer among
the generators rather than the language they all speak. `harness/claims/OnlyTheImporterSpellsGltf`
counts what still leaks past that door: 136, and it may only fall.

What this item still owns on its own, and what keeps it open beside board:1949: the generator
INTERFACE -- one signature every generator implements, and each one its own library.

## What will be true

- [ ] ONE interface every generator implements, taking `(kind, params, seed, budget)` and
      replying a part PLUS its capability.
- [ ] The reply is the internal representation, never bytes: a `.glb` is a serialisation for
      something outside this engine and never the currency between a generator and a compositor.
- [ ] Each generator compiles as its own library, so a test links the one it is about and a
      generator's include set is provable the way a layer's is (board:1582).
- [ ] A test drives a generator directly and scores what comes back, with no preparer and no
      Blender in the path for the parts of the claim that need no oracle.
