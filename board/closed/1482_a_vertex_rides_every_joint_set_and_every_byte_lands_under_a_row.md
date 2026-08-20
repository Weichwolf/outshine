Type: bug
Area: gltf
Tags: bug, instrument

**A vertex rides every joint set, and every byte lands under a row**

The owner asked for an audit against **a shape is 0 or 1..N**. Two places assumed exactly one and both
were silent about it.

## FOUR INFLUENCES WERE A CONSTANT AND THE FORMAT SETS NO BOUND

`Subject::BlendSkinFor` read `JOINTS_0` and `WEIGHTS_0` and looped `slot < 4`. **`JOINTS_1` had no
spelling anywhere in `src/` or `test/`** -- so a file binding a vertex to eight joints was read, drawn
and reported as fine with **half its influences dropped and no word said**. That is the worst shape a
shortfall can take: not a refusal, not a coarser answer, a WRONG one that looks right.

It reads every `JOINTS_n` / `WEIGHTS_n` the primitive declares now, blending across all of them, and a
set that carries one half without the other is a refusal naming which.

[MEASURED] a fixture of eight joints a metre apart, one vertex, an eighth of the weight on each:

| | |
|---|---|
| both sets, eight influences | **3.5 m** -- the mean of joints 0..7 |
| `JOINTS_0` alone | **1.5 m** -- the mean of joints 0..3 |
| the two answers | **2 m apart**, which is what makes it a discriminator rather than a tolerance |

**No case of the corpus at the pin carries more than four.** [MEASURED] 0 of the 34 generator animation
models, so this defect could not have been found by the corpus -- it was found by asking the format what
it permits and the tree what it spells. *That is the argument for the audit and not for another case.*

## THE LEDGER'S OVERFLOW ROW WAS SUBJECT TO THE BOUND IT EXISTS TO CATCH

`Heap` attributes bytes to 32 tag slots and sends what does not fit to a row named `other` -- except
`RowFor(kOverflow)` **needed a slot of its own**, so with the table full it returned null too and the
bytes vanished with `if (row != nullptr)`. Slot 0 is reserved for it now and the guard is gone: there is
always a row.

**This is the instrument `board:1463` was measured with**, and this round added ~20 tags to it -- one per
stage of the compiled plan. It did not overflow, but nothing said what would happen if it had.

[MEASURED] 64 distinct tags asked of a 32-slot table: **32 slots named, 262 144 bytes counted, none
lost**.

## What must be true

- [x] Every `JOINTS_n` set a primitive declares reaches the blend, and a half-declared set is refused
- [x] The tag table's overflow row always exists, so a bound on the ATTRIBUTION is never a bound on the
  MEASUREMENT
- [x] Both are proved by a test that exercises the bound rather than staying inside it

## What proves it

**`test/unit/gltf/AVertexRidesEveryJointSetTheFileDeclares.cpp`** -- eight influences land on the mean
of eight joints, four land on the mean of four, and the two are 2 m apart.

**`test/unit/core/EveryByteTheHeapTakesLandsUnderATagOrUnderOther.cpp`** -- 64 tags into 32 slots, every
byte still counted, and `other` is a row a reader can see rather than a difference they would compute.

## Comments

**The heap had NO test of its own** and was reached only through the scenario suite. Writing one found
a third thing, and it is a fact about the instrument rather than a defect: **at `-O2` clang elides an
allocation whose whole lifetime it can see** -- [MEASURED] a `std::vector<char>(4096)` taken and dropped
in one scope reads `LiveBytes() == 0` while it is alive, and the same code at `-O0` reads 4096. The
ledger is therefore RIGHT about the shipping build, and an instrument for the ledger has to take memory
the optimiser must keep. *A test that had not checked this would have read zero and called it a pass.*

## Where the audit stands

`TEXCOORD_2` is a **named refusal** with the bound in the sentence, and `COLOR_1` has no semantics in
the core format, so neither is this class. The remaining declared bounds -- `kMaxSubjectLights` 16,
`kMaterialSlots` 1, `kGroundSlots` 12, `kMaxColourAttachments` 8, `kUvSets` 2 -- each refuse by name
where they are reached. **The sweep is not finished**: it covered the glTF reader's attribute sets and
the ledger, and `board:1483` carries the rest.
