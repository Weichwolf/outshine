Type: task
Parent: 1170
Area: harness
Tags: perf, oracle, instrument

**Prune test by test, with the loop inverted**

`board:1170` carries the design and nothing is built. **It is on the critical path rather than tidy**: at
**129 MB a case** the corpus grows with every ordered row completed, and the projected ~147-case in-scope
set is **19 GB against 53 GiB free** where pruned it is **~180 MB**. Every row landed before this does
makes the eventual conversion larger.

- [ ] **The loop inverts to case-outer, arm-inner.** [MEASURED] `JudgeEvery` is called three times, each
  looping every case, so a case's inputs must survive until the third arm reaches it and pruning inside an
  arm deletes what the next arm needs. **Same binaries, same set, same verdicts, different order**; the
  cost is process spawns, not rebuilds, because all three binaries are built before any case runs
- [ ] **Prune ALWAYS, per case, not on a pass.** The owner's rule, and it survives the objection it was
  written against: the store holds **1 070 objects at exactly 14 745 644 B**, so **pruning is declining to
  keep a second copy, not deletion**
- [ ] **The two proof classes are checked SEPARATELY and a file in neither stays, loudly, with the case
  reporting it.** Oracle products — `oracle.raw`, the quantity raws, the EXRs, **88.5 MB of a 137 MB
  case** — are proven by the store: key present, size and digest equal. **Our own outputs — `outshine.raw`,
  `outshine.normal.raw`, `file.normal.raw`, 44.2 MB — are NOT in the store** and are proven by their
  producer: re-running that one case regenerates them, with no Cycles in the path. *The store is
  authoritative for one half and not the other, and a single check would be wrong about the second*
- [ ] **`board:1154` is the live instance of the failure mode**: a cached case carries products whose
  mapping the store never recorded. **A prune that cannot prove its precondition does not proceed**
- [ ] **The keep set is the PNGs, the manifest and `provenance.json`.** Provenance is not decoration here —
  it carries the preparer digest and the product keys, and `board:1176` now depends on it surviving
- [ ] **The recovery step is named in the item and in the runner's own output**, so a diagnosis does not
  begin with *how do I see the pixels again*:
  `python3 test/corpus/prepare.py all --manifest test/render/<area>/<case>/manifest.json` for the oracle
  half, re-running the case for ours
- [ ] **The high-water mark is published as a number**, because *size doesn't grow* is only checkable
  against one. **Acceptance: the peak under `test/render/` never exceeds one materialised case plus the
  accumulated pictures — 162 MB (`scifi-helmet`) plus ~0.1 MB a finished case, ≈180 MB across the whole
  in-scope corpus**, against 4.4 GB today over 37 cases

**What it must not do.** It must not add a `Makefile` target — three, and no others — and it must not put
the verdict in the preparer, which *compares, scores and decides nothing*.

**Done when** a case is pruned as it finishes, both classes are proven by their own instrument, an
unprovable file stays and says so, and the runner publishes a high-water mark that a later round can watch.

## Comments

**THE PEAK ACCEPTANCE WAS WRONG ARITHMETIC AND IT IS CORRECTED HERE RATHER THAN LEFT IN A CLOSED ITEM.**
This item asked for *the peak under `test/render/` never exceeds one materialised case plus the
accumulated pictures — 162 MB plus ~0.1 MB a finished case, ≈180 MB*. **Both numbers were wrong and the
shape of the claim was wrong with them.**

| | this item said | measured |
|---|---|---|
| residue a finished case | ~0.1 MB | **≈2.6 MB** — pictures 3.4 MB over 74 files, **subjects 93.8 MB**, manifests 0.5, provenance 0.4 |
| what the runner bounds | the **peak** | **the END STATE.** `test/render/` fell 4821 MB → **139 MB**, and the run's peak was **3212 MB** |
| projected at 147 cases | ≈180 MB | **≈0.4 GB accumulated plus one live case**, against 19 GB unpruned |

**The residue was under-counted because I forgot the subject.** A pruned case keeps its `scene.glb` or
`.gltf` — 93.8 MB of the 139 — and that is correct: the subject is an input, not a product, and dropping
it would make every case a re-fetch. **~0.1 MB was the pictures alone.**

**And the shape error is the one worth keeping.** *The runner prunes test by test* bounds what is left
standing; **it does not bound the peak, because the preparer materialises the WHOLE CORPUS before the
runner sees a case.** So the owner's *so size doesn't grow* is met for the end state and **unmet for the
peak**, which is what that sentence is about. **The claim is still overwhelmingly true — 4821 MB that
never declined against 139 MB that does — and its numbers were not reproducible.** `board:1183` carries
the half this item cannot.

**Recovery cost, measured, for whoever reads this later**: the whole corpus from a warm store is
**14.3–17.6 s** over 37 manifests; **one case is 0.77 s**. No Cycles in the path — **but `prepare.py all`
still requires Blender to be present**, so a machine without it cannot re-materialise even from a warm
store.
