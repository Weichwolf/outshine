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
the table; *is proven* is a query against `test/khronos/glTF/` and `git grep`. **A field's name is part of its
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

## The tier question is answered by measurement, and the answer is a third thing

**I proposed that tier is not two values and that *moves a reader field* against *multiplies a global
count* are different decisions. `board:1187` measured it and the split is real but drawn elsewhere.**

**Pipelines are built in `Configure`, called only from `Renderer::Init` — none inside a frame.** 48
against 30 produced **no resolvable frame difference**, and the cost is **≈6.9 ms per pipeline cold, so
48 → 96 adds ≈330 ms to a cold `Init` and ~0 to the frame.** The four rows landed so far cost **nothing
this instrument can resolve**, bounded at **≈0.03 ms (1.4 %)** on the only arm their path reaches.

> **So the axis is not *frame cost versus reader field*. It is WHICH BUDGET THE MULTIPLICATION LANDS IN.**

**A row that doubles the pipeline set spends startup; a row that adds a fragment arm spends frame.** They
are both *multiplies a global count* and they are not comparable, because this engine's four constraints
name a **frame** budget and no startup one. **A tier that ranked them together would rank a cost the
project has a number for against a cost it does not.**

- [ ] **The derived tier column carries the budget, not a number**: *reader* · *startup* · *frame*. Three
  values, each naming where the cost lands, and **only one of them is measured against a declared
  ceiling**
- [ ] **Startup has no declared budget and that is now a visible gap.** 330 ms of cold `Init` is
  affordable and 3 s would not be, and **nothing in this tree says which** — so a row that spends startup
  is ordered against a ceiling nobody has written
- [ ] **The measurement is what made this decidable**, and it is worth recording as method: the two-value
  tier I rejected was right to reject, **and the replacement was not derivable from the rule either.** It
  came from an instrument built one round earlier for a different purpose
