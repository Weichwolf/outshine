Type: feature
Area: corpus
Tags: oracle, perf, instrument

**A validated case keeps its pictures, and nothing else**

**The owner:** *the test folders only need to keep the png files after validation.*

[MEASURED] over `test/khronos/glTF/`: **4.4 GB total**, of which `.raw` is **305 files ≈ 4.24 GB — 96.5 %**,
`.exr` 55 MB, **`.png` 34 MB**. A suite pruned to its pictures is **~75 MB, about 58× smaller.**

## The three things this rests on, each measured rather than assumed

- [ ] **THE STORE IS AUTHORITATIVE, so a case directory is a materialised view and pruning it loses
  nothing.** Verified by content, not by reading the code: `materials/scifi-helmet/oracle.raw` has a
  **byte-identical object in the content store**, and the store holds **1 070 objects at exactly
  14 745 644 B** — every raw product of every case and recipe. The store is **15 GB over 2 637 objects**
- [ ] **NOTHING PRUNED IS TRACKED.** `git ls-files test/khronos/glTF` is **35 `.json` · 14 `.h` · 1 `.cpp` ·
  1 `.gitignore` and nothing else** — no PNG, no EXR, no RAW. A clone is unaffected, and the PNGs that
  survive survive as working-tree artefacts for a person to look at, not as committed evidence
- [ ] **A RED CASE IS NEVER PRUNED, and "after validation" means "on a pass".** The first thing anyone
  does with a failing case is read its dumps — `board:1136`, `board:1138` and `board:1144` are each built
  on exactly those files, and `board:1136`'s four deciding pixels were found in `oracle.raw` and
  `outshine.raw`. **Pruning a red case deletes the evidence for the only cases anyone is working on.**
  A case keeps everything until its own verdict is PASS

## When it happens, and the constraint is the interesting part

**`CLAUDE.md` allows three `Makefile` targets and no others, and `test/run.sh` is the only runner**, so
there is no `make prune` and no fourth runner to put this in. Two candidates and one is refused:

| | |
|---|---|
| **the preparer prunes** | **refused.** `prepare.py` *compares, scores and decides nothing — that is C++, in the test*. Pruning on a pass needs a verdict, so this would hand the preparer a judgement it is defined not to have |
| **the runner prunes, per case, after that case's own PASS** | **taken.** It is the only thing that holds a verdict, it already owns the lifecycle, and the unit is one case rather than one run — so a suite that dies early prunes only what it actually greened |

**And the runner deleting files is a real hazard, named rather than waved past**: a runner whose verdict
logic is wrong now destroys the evidence that would show it. Two guards, both cheap: **prune only files
whose bytes the store provably holds** — key present, size and digest equal — and **never a `.png`**. What
cannot be reconstructed is never a candidate.

## What it unblocks, and this is why it is filed rather than swept

- [ ] **`board:1143`** — quantity passes declared per case. Today's argument against more channels is disk
  (18 channels measured at 293 MB a case, 19.9 GB across the corpus); with validated cases at their
  pictures, the ceiling moves and the per-case declaration becomes an ordinary choice
- [ ] **`board:0084`** — *two hundred cases, so the suite is generated rather than typed*, and the owner's
  ruling puts a render proof on ~865 content kinds. At today's **129 MB a case** that is **111 GB against
  53 GiB free**; pruned, it is single-figure gigabytes. **The constraint collision was disk and it was
  entirely this**
- [ ] **The other half was never the constraint**, and the number that has been quoted is the wrong one:
  [MEASURED] from 55 renders' own `provenance.json`, Cycles takes **p50 0.60 s, p95 2.75 s, max 36.99 s,
  83 s for the whole corpus**. `board:1129`'s **8.17 s** is **~14× the measured median** and is a different
  population. A cold rebuild of 865 cases is on the order of **half an hour**, not hours

## The hazard this creates, and it must be stated where the decision is

**Pruning makes the content store load-bearing, and the store is at
`/var/folders/…/T/outshine-content` — the OS temp directory.** *There is no second cache* is a design
rule; combined with pruned case directories it means **the only durable copy of every derived product
lives somewhere macOS may purge without asking.** It is recoverable — the whole corpus re-renders in
83 s today — but at 865 cases the recovery is half an hour and it will arrive unannounced.
**Where the store lives is now a decision and not a default**, and it is the owner's: this item does not
move it, it names that it must be chosen.

**Done when** a case that passes keeps its PNGs and its manifest, a case that fails keeps everything, no
file is removed whose bytes the store does not provably hold, the `Makefile` still has three targets, and
the suite's on-disk size is published so the 865-case claim is priced against a number rather than a
ratio.

## SHARPENED: always, and test by test

**The owner, verbatim:** *test runner must allways prune test by test so size doesn't grow.*

**The clause *a red case is never pruned* is STRUCK, and the objection behind it is answered rather than
overruled.** It rested on `board:1136`, `1138` and `1144` each being built on exactly those dumps. What it
missed is my own measurement two rounds earlier: **the store is authoritative — 1 070 objects at exactly
14 745 644 B, every oracle raw of every case and recipe.** So **pruning is not deletion; it is declining
to keep a second copy**, and a red case's dumps come back by a named step. *The rule I wrote was protecting
evidence that was never at risk.*

**And *so size doesn't grow* is about the PEAK, not the total.** Pruning after a suite still lets the
corpus reach full size at some instant. Pruning as each test finishes bounds the working set to one case.
That is a different design and it is the one asked for.

## The safety argument now rests entirely on the store check, so its failure mode is stated

**Prune only files whose bytes the store provably holds — key present, size and digest equal.** Under
*prune on a pass* a mistake cost a re-render; under *always* a mistake on a product the store does **not**
hold is unrecoverable.

**When the check fails the file STAYS, loudly, and the case says so.** A prune that cannot prove its own
precondition does not proceed. **`board:1154` is the live example and it is not hypothetical**: the index
mapping is recorded only on the miss path, so a cached case carries products the store cannot vouch for —
exactly the shape this guard exists to catch.

## Two classes, two recovery steps, and only one of them is the store's

[MEASURED] over one case — `materials/water-bottle`, 137 MB — the products partition by **producer**, not
by extension:

| | bytes | recovered by |
|---|---|---|
| **oracle products** — `oracle.raw` + 5 quantity raws + the EXRs | **88.5 MB** | the store. `PRODUCTS = ("exr","raw") + quantity pairs` is exactly this set |
| **our own outputs** — `outshine.raw`, `outshine.normal.raw`, `file.normal.raw`, `1-outshine.png` | **44.2 MB** | **not the store — the runner.** They are outputs, so re-running that one case regenerates them, with no Cycles in the path |
| the subject — `scene.glb` | 9.0 MB | the store by SHA-256 if fetched, `prepare.py generate` if ours |
| kept | PNGs, manifest, provenance | — |

**So *the store is authoritative* is true of the oracle half and false of ours**, and the guard must know
the difference: our own dumps are pruned because **re-running the case is their producer**, not because
the store holds them. Two classes, two proofs, one rule — and a file in neither class stays.

**The named step, so a diagnosis does not begin with *how do I see the pixels again*:**

```sh
python3 test/harness/shared/corpus/prepare.py all --manifest test/khronos/glTF/<area>/<case>/manifest.json
```

idempotent, independently invocable, and it re-places every oracle product from the store without
touching Blender on a hit. **Our own dumps come back by re-running that one case.** Both are one command
and neither rebuilds anything.

## The three arms decide whether *test by test* means *per case* or *per invocation*, and the loop today forbids the smaller one

[MEASURED] in `test/run.sh`: `JudgeEvery` runs one binary over every case, and there are three binaries —
plain, sanitised, validated. **The loop is arm-outer, case-inner.** So a case's inputs must survive until
the third arm reaches it, and pruning inside an arm would delete what the next arm needs.

- [ ] **Invert the loop to case-outer, arm-inner** — for each case run the three arms, then prune. **Same
  binaries, same set, same verdicts, different order**, and the peak becomes one case. The cost is process
  spawns, not rebuilds, because the three binaries are already built before any case runs
- [ ] **The alternative — re-materialise per arm — is refused before it is measured**, and the reason is
  arithmetic rather than a preference: it pays the placement cost three times per case to buy a peak that
  inverting the loop gives for free
- [ ] **If the loop cannot invert**, then *test by test* means *case by case across all three arms*, the
  peak is one case either way, and that is stated rather than discovered

## The peak, as a number, because *size doesn't grow* is only checkable against one

| | |
|---|---|
| largest case materialised | **162 MB** (`materials/scifi-helmet`); `water-bottle` 137 MB |
| pruned residue per finished case | **~0.1 MB** — two PNGs, manifest, provenance |
| **peak over the whole in-scope corpus (~147 cases)** | **≈ 180 MB** — one live case plus every finished case's pictures |
| against | **4.4 GB today over 35 cases**, ~19 GB projected for 147 |

**That is the acceptance: the suite's high-water mark under `test/khronos/glTF/` never exceeds one case plus the
accumulated pictures**, published by the runner as a number so the claim is checkable rather than
asserted.

## The numbers this feature carried are corrected, and one half of the ruling is unmet

**`board:1181` landed and measured what this item estimated.** `test/khronos/glTF/` **4821 MB → 139 MB**, 893
files declined per run totalling 4570 MB, **2 left standing with the proof that refused them**. The
projection here — *≈180 MB across the whole in-scope corpus* — was built on **~0.1 MB a finished case**;
it is **≈2.6 MB**, because a pruned case keeps its **subject** (93.8 MB of the 139), which is an input and
not a product. **At 147 cases: ≈0.4 GB accumulated plus one live case, against 19 GB unpruned.**

**And the ruling's own words are met for one half only.** *So size doesn't grow* is about the **peak**;
the runner bounds the **end state**. **The preparer materialises the whole corpus before the runner sees a
case**, so the measured peak was **3212 MB** — and at 147 cases it scales with the corpus rather than with
one case. **`board:1183` is that half**, and this feature is not done until it lands.
