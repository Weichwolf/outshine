Type: feature
Area: corpus
Tags: oracle, perf

**A C++ reader for the oracle's EXR**

`oracle.raw` exists because C++ has no EXR reader; it is a flat float32 dump of what `oracle.exr`
already holds. **It is the blocker behind three separate decisions now** — the corpus retention rule,
the suite's disk footprint, and the cost of a full sweep.

**Without it, deleting `oracle.raw` means re-rendering in Blender every run**: 858 x 8.17 s is about
**two hours per suite run against 266 s today**. With it, the raws are derived on demand and the EXR is
the only oracle artefact that must survive.

**Done when** the runner reads `oracle.exr` directly and `oracle.raw` is generated on demand or not at
all.

**Groomed at audit: not blocked by `board:1120`, but scheduled with it.** Neither blocks the other — one
is a reader, one is a key — but **both invalidate the corpus and each costs a full re-prepare**, and
`test/run.sh` does not prepare: it reports *has no prepared input*. Run as one round they cost one
re-prepare instead of two. **That is a sequencing note and deliberately not a `Depends:` edge**, because
a false edge would make the readiness query lie about what can start.
