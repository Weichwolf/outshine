Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument, scope

**The capability table loses its KCD/GTA column, and its three populations are re-measured**

**`board:0079` already IS the feature enumeration and no second one is filed.** It is a **50-row table** —
`| Capability | Proven by | KCD / GTA 5 | Verdict |` — one line per glTF capability, with the corpus asset
that proves each. What the owner's rule changes is not its existence but **its denominator**.

## The third column is now the wrong question

`0079`'s ruling was *on every feature ask whether Kingdom Come: Deliverance or GTA 5 uses it; if so
Outshine must implement it, and if not it is a `REFUSED` line with that reason.* The owner's new sentence —
*every glTF 2.0 feature must be supported and tested* — **deletes that filter for core features.** A row
refused because neither reference uses it is refused for a reason the specification does not recognise.

**[MEASURED] 13 of the 50 rows carry a `REFUSED`, and they split cleanly:**

| | |
|---|---|
| **core features refused for the old reason — now REQUIRED** | **`multiple scenes`** (*"neither — we render one"*) · **`POINTS`/`LINES`/`STRIP`/`FAN`** (*"debug drawing only"*) |
| already overturned before this ruling | **sparse accessors**, refused as *"a file compaction, no runtime capability"* and since marked **BUILT** — the same mistake caught once already |
| **extensions — stay refused, and the owner's sentence does not reach them** | `iridescence` · `dispersion` · `unlit` · `draco` · `meshopt` · `webp` · `xmp_json_ld` · `pbrSpecularGlossiness` |

**Two rows change verdict, and the pattern that produced all three is the finding**: a capability was
refused because *no reference game uses it*, which is a statement about two games and never about glTF.
**The sparse-accessor row proves the pattern is real rather than suspected** — it was refused on exactly
that reasoning and overturned on its own merits before anyone noticed the class.

## The three populations, and the middle one is what this task exists for

**They must be counted apart and they have never been:**

- [ ] **supported and proven** — the reader handles it *and* something under `test/` asserts **that
  feature**, with the citation
- [ ] **supported and unproven** — the reader handles it and nothing asserts it. **A case passing its
  picture bound does not prove the feature it happens to contain**: `WaterBottle` is inside the bound and
  that says nothing about occlusion strength. This is the dangerous population and the reason the column
  is *Proven by* rather than *Present in*
- [ ] **absent** — the reader does not handle it, which under the new rule is now a defect for every core
  row rather than a `REFUSED`

**`board:0079`'s last measurement is 2026-08-12 and is stale**: *the engine reads 10 of the 39 through
glTF — 25.6 %*, *6 of those 10 are held by an asserted claim and 4 cross untested*, *as raw capability the
engine expresses 18 of the 40 — 45 %*. Since then the tree gained generated tangents, a mip chain, index
channels, `KHR_materials_unlit`, an EXR reader and — this round — a posed hierarchy. **Re-measuring 50
rows against a moved tree is the work; quoting the old numbers as current would be the defect.**

## How each row is decided, so the re-measurement is reproducible rather than a reading

- [ ] **Supported** is decided by **exercising the reader**, never by a grep. `git grep` proves a string
  absent and never a capability, and this table's rows are exactly where that failure is cheapest to
  commit
- [ ] **Proven** is decided by a **named test citing the row**, and the citation lives in the source per
  this board's rule. A row whose *Proven by* names an asset but whose id appears nowhere under `test/` is
  **supported and unproven**, however green that asset's case is
- [ ] **The feature list itself is derived from the specification and not from this table**, because a
  table built from a corpus enumerates what somebody happened to author. Where the spec carries a feature
  no row names, the row is added — that is the only way the count can be a claim about glTF rather than
  about Khronos's sample set
- [ ] **The spec is not in this tree**, so the derivation names its source and version the way every other
  external claim here does — the glTF reader already cites `Specification.adoc` by line, so the convention
  exists and only the pin is missing

## Why this is one number and the model gap is another

**52 core models have no case** (`board:1172`). **That is not the same number as the features with no
test**, and neither implies the other: one model can carry six features and one feature can need three
models to be pinned down. **Both are published and neither is quotable as the other**, which is the same
discipline the picture bound and the criteria counts already keep.

**Done when** the table's third column is replaced by *glTF 2.0 core · extension implemented · extension
out*, every row carries one of the three populations with its citation, the two rows above are required
rather than refused, and the feature list is checked against the specification rather than against the
corpus that happens to exist.
