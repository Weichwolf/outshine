Type: bug
Area: harness
Tags: instrument, scope, khronos

**The capability table stores what must be derived, and every error runs one way**

`board:1174`'s re-measurement found two things that are one defect: **`board:0079`'s table stores values
that are functions of the tree at a moment, as though they were properties of a row.** Stored derived
values go stale, and here they go stale **in one direction only**.

## The evidence, measured

**`Proven by` names a Khronos asset for 23 rows. For 16 of them the asset has no case in this tree.**
`Box` · `BoxVertexColors` · `RiggedSimple` · `AnimatedMorphCube` · `SimpleInstancing` ·
`MetalRoughSpheres` · `SpecularTest` · `ToyCar` · `SheenCloth` · `MaterialsVariantsShoe` ·
`DiffuseTransmissionTest` · `CubeVisibility` · `AnimatedColorsCube` · `InterpolationTest` ·
`SimpleSparseAccessor` · `MeshPrimitiveModes`. **A column called *Proven by* is a citation to an absent
asset in two rows out of three.**

**`MetalRoughSpheres` is the sharp instance**: its named prover is absent **and metalness demonstrably
works**, proven *incidentally* by cases that happen to contain it. **That is *a case passing its picture
bound does not prove the feature it happens to contain*, measured rather than argued.**

**And all three of the table's false assertions under-state the engine; none over-states it** — metalness
*"has no field at all"* while `Material.h` carries it, `TEXCOORD_1` unbuilt while `Uv1` is a layout,
sparse `REFUSED` while `HasSparse` exists. **Three instances is a property of maintenance, not three
coincidences: a row is written when the engine LACKS the feature and nothing moves it out when the engine
gains one.** The error direction is structural, and it is the direction that **sequences work already
done**.

## And tier is stale the same way, which is the third instance

**`COLOR_0` was a stream on a 5-layout enumeration when the order was set and is now a stream on an
8-layout one** — 8 → 16 layouts, 48 → 96 pipelines — because `TEXCOORD_1` moved underneath it. **Nothing
re-evaluated it, because tier was assigned at filing time and stored.** A tier assigned once and never
revisited is a stale population wearing a different hat.

## The repair is one rule, and this tree already applies it everywhere else

> **The table stores what is DECLARATIVE and derives what is a function of the tree, at dispatch.**

| stored | derived at dispatch |
|---|---|
| the capability, its specification reference, **the asset that WOULD prove it** | whether that asset has a case · whether a test cites the row · **impact** over the current uncovered set · **tier** against today's layout and pipeline counts · **provable** |

**`Proven by` is split rather than renamed**: *would be proven by* is a declarative choice and belongs in
the table; *is proven* is a query against `test/render/` and `git grep`. **A field's name is part of its
claim** (`board:1157`), and this column's name asserted the second while holding the first.

- [ ] **The derived columns are computed by the harness and printed beside the table, never written into
  it** — *anything derivable from the tree belongs to the harness* is this board's own rule, already
  applied to the next id, to `board/active/` as state, and refused three times for a counter file
- [ ] **The one-directional error is what the derivation removes**, and that is the acceptance rather than
  tidiness: a row cannot under-state an engine that gained the feature, because nobody edits the row
- [ ] **This is `board:1160` in a different column and it is filed separately for a different repair.**
  `1160` publishes ticked-versus-cited for the board's boxes; this splits a table's columns into stored
  and derived. **Same class — a claim whose evidence is absent — two instruments**

**Done when** `board:0079`'s ordering columns are computed from the tree at the moment of dispatch, the
table itself carries only what somebody chose, and a row that under-states the engine is unwritable rather
than merely wrong.
