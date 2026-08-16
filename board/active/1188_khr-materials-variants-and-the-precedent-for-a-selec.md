Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**`KHR_materials_variants`, and the precedent for a selection extension**

**Row 4.** Impact **7 of 146**, ratified — it stands under *Ratified Khronos Extensions* in the registry,
the same heading as `KHR_texture_transform` — so in scope by default under `board:1172`.

**It displaced `COLOR_0` at equal impact on this table's own tie-break**, *within a tier, fewest layers
moved*: `COLOR_0` adds a fifth boolean to the vertex layout — **8 layouts → 16, 48 pipelines → 96** — and
this adds **no vertex stream and no layout multiplication at all**.

## IT SETS A SECOND PRECEDENT, because it does not fit the first

`board:1177` established the **data** extension shape: *defaults are the identity, the data lives in the
material row, presence is signalled by values and never by a flag.* **`variants` breaks the middle clause
and must not be forced into it.** It is not material data — it is a **mapping from a variant to a
material**, per primitive. **Putting a mapping in the material row would put a table inside the thing it
selects.**

- [ ] **A selection extension lives in the draw list, not the material row.** The primitive already
  carries a material slot; a variant selection **changes which slot it wears** and changes nothing about
  what a slot contains. `DrawItem::Order.MaterialSlot` is the field this resolves into, and it resolves
  **before the draw list is compiled**, so the render path never learns that variants exist
- [ ] **Which is also why it costs nothing in the shader**: zero new fragment arms, zero new pipelines,
  zero new interpolants. **The cheapest row on the board that still changes the picture**
- [ ] **The default is the primitive's own `material` and it is not a special case.** With no variant
  selected the mapping is not consulted, which is the same *absence and default are one path* property
  `board:1177` established — reached by a different mechanism and worth saying so

## What must be declared, and by whom

- [ ] **A case declares which variant it renders, or none.** *None* is the default and is a real
  declaration rather than a silent fallback. **Two runs of one case must render one picture**, and a
  variant chosen by anything other than the declaration would make the picture a function of the runner
- [ ] **A named variant that the file does not define is a REFUSAL naming both** — the name asked for and
  the names available. Not the default, not the first: `board:1182`'s refusal invariant, applied to a
  selection instead of an attribute
- [ ] **A primitive mapped twice by one variant is a refusal at read time**, because the file has stated
  two answers to one question and neither is more correct

## The case, and it does not exist yet

**`MaterialsVariantsShoe` has no case in this tree** — it is one of the 16 rows whose named prover is
absent (`board:1189`). So this task carries a corpus case as well as a reader path.

- [ ] **The proof is a PAIR of renders of one subject**, the same camera, differing only in the declared
  variant. **A single render proves nothing**: a variant selection that silently did nothing would render
  the default and look entirely correct
- [ ] **The two renders must differ by more than the picture bound**, measured from the file's own
  materials before the case is scored — the same precondition `board:1182` established and the same reason
- [ ] **The mutation that must fail it**: ignoring the mapping and rendering the default. That is the
  defect a one-render case cannot see, and it is the whole reason the pair is the acceptance

**Done when** a declared variant selects its material before the draw list is compiled, an undefined
variant name is a refusal naming both sides, a pair of renders of `MaterialsVariantsShoe` differ by more
than the bound in the declared direction, and the selection-extension precedent is stated where the next
one will read it.
