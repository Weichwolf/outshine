Type: bug
Area: corpus
Tags: oracle, perf, instrument

**The corpus keeps 4.45 GB that a proven reader makes redundant**

[MEASURED] over `test/khronos/glTF/`:

| | |
|---|---|
| the whole render corpus | **4 521 MB over 35 cases — 129 MB a case** |
| `.raw` dumps: 305 files at 14 745 644 B each | **4.45 GB — 99.6 % of it** |
| `.exr` files carrying the same pixels: 200 files | **0.06 GB** |
| ratio | **79.5×** |

**The reason the `.raw` exists is gone.** `test/harness/shared/corpus/README.md` states it plainly — *C++ has no EXR
reader, SDL3 provides none* — and `board:1119` built one. `test/outshine/harness/TheOraclesExrReadsAsItsRaw.cpp`
holds the two to **bit-exactness**: *reading `oracle.exr` and stacking `R,G,B,A` must reproduce
`oracle.raw` sample for sample*. **So the tree already proves the larger file carries nothing the smaller
one does not**, and keeps both.

**Why this is filed now rather than as housekeeping: it is the whole of the corpus-scale question.** The
owner's ruling puts a render proof on every species, building and infrastructure type — roughly 865
content kinds. At today's 129 MB a case that is **111 GB against a 50 GB disk**, and the collision reads
as a reason to soften the ruling. **It is not.** Take the redundant dumps out and a case is its EXRs, its
PNGs and its subject; the 865 fit in single-figure gigabytes.

**And the other half of the price is already measured and is not the constraint anybody assumed.**
[MEASURED] from 55 oracle renders' own `provenance.json`: **p50 0.60 s, p95 2.75 s, max 36.99 s
(`fetched-triangle`), 83 s for the entire corpus.** `board:1129`'s **8.17 s** — the figure the scale
question has been argued with — is **~14× the measured median** and is a different population. A cold
rebuild of 865 cases at ~1.6 recipes each is on the order of **half an hour of Cycles, not hours.**
**Disk was the constraint and Cycles never was.**

**What must be checked before the dumps go, because the reader's domain is narrower than the file's.**
`outshine.raw` is **ours**, written so both sides of a comparison open through one reader — there is no
`outshine.exr`, so our side needs either an EXR writer or to keep its dump. And `file.normal.raw` is
rasterised by the runner, not produced by Blender. **Three producers, one format, and only the oracle's
half is proven replaceable** — so the saving is the oracle's ~200 dumps and the rest is a separate
question.

**Done when** no product is kept whose bytes another committed product provably contains, the per-case
cost of a content-kind proof is published as a number, and the 865-case corpus claim is priced against
that number rather than against today's.
