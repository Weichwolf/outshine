Type: feature
Area: corpus
Tags: oracle, perf, instrument

**A validated case keeps its pictures, and nothing else**

**The owner:** *the test folders only need to keep the png files after validation.*

[MEASURED] over `test/render/`: **4.4 GB total**, of which `.raw` is **305 files ≈ 4.24 GB — 96.5 %**,
`.exr` 55 MB, **`.png` 34 MB**. A suite pruned to its pictures is **~75 MB, about 58× smaller.**

## The three things this rests on, each measured rather than assumed

- [ ] **THE STORE IS AUTHORITATIVE, so a case directory is a materialised view and pruning it loses
  nothing.** Verified by content, not by reading the code: `materials/scifi-helmet/oracle.raw` has a
  **byte-identical object in the content store**, and the store holds **1 070 objects at exactly
  14 745 644 B** — every raw product of every case and recipe. The store is **15 GB over 2 637 objects**
- [ ] **NOTHING PRUNED IS TRACKED.** `git ls-files test/render` is **35 `.json` · 14 `.h` · 1 `.cpp` ·
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
