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

## Comments

**CLOSES. The statement is true of the tree and the gap has a home that outlives the closure — which is
the rule, and the home had to be filed first.**

**The selection-extension precedent is proven rather than asserted**, and the diff is the proof: **six
files under `src/gltf/` plus two new ones, and zero files changed under `src/render/` or
`src/clients/`.** Both cases report `plan_passes 2 (at most 2) PASS`. **No fragment arm, pipeline, layout
or interpolant could have been added**, which is what *a selection resolves before the draw list is
compiled* means when it is true instead of intended.

**`VariantSelection` carries a name and not an index**, so a selection resolved against one document
cannot silently select in another — the same reasoning that made the `texCoord` refusal a refusal rather
than a fallback.

**THE PAIR EARNED ITS ACCEPTANCE AND THE MEASUREMENT SAYS SO.** Between variants, **19 918 of 29 973
covered pixels differ beyond the bound**, predicted beforehand from the file's own images at **19 925 —
a 0.04 % miss**. Under the silent-no-op mutation the beach render became **bit-identical to the correct
midnight render**, and **the midnight case could not see the mutation at all** — `203.48492 → 203.48492`,
byte for byte. **Either render alone was blind; the pair was not.** That is the clause this item argued
for, tested against the defect it was written about.

**Two red pixels per case remain and their home is `board:1191`.** They are **coincident surfaces inside
one pixel** — one at 1.95 µm, one at **0.113 µm, below one ulp of camera-relative f32 at 1.19 m** — and
`Routing.h` is blind to them **by construction**, binning by object and material index where both surfaces
share both. **The residual belongs to a defect that is not this row's**, and it now has an item rather
than a sentence in a report.

**The developer refused to build the depth instrument inside the round that needed it**, on the grounds
that it would move a population under a number. **That is the right call and it is why this closes
cleanly**: the round that would have built the instrument to explain its own residual is the round least
able to judge it.
