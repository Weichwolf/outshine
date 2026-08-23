Type: issue
Area: test
Tags: gate-health

**The fast gate keeps its headroom as the mirror grows**

The gate's 90000 ms [SET] bound was re-derived when the suite held ~120 tests; it now holds
152 and cold runs (after wide header edits) land at 85-97 s -- headroom 2-20 s warm, and the
bound is CROSSED cold (94336 ms, 97047 ms observed on 2026-08-23 without an OVERRAN verdict
printed on those runs; whether the bound verdict fires on cold runs is itself worth a look).
Every closed item adds a proof, so the mirror grows monotonically; the headroom does not.

Candidates, in the order the tree's own rules suggest: (1) the bound re-derives against the
current population (measured, not chosen) with the derivation printed; (2) the build's cold
cost leaves the bound's population -- the bound holds the RUN, the compile is the nest's
business (measure the split; the runner knows both); (3) suites that grew past their weight
(unit/gltf 30 tests at ~11 s warm) parallelise within the one-process-per-test rule the
runner already holds. A gate that overruns its own bound without a red verdict is a gate
that measures the past -- if (1) confirms the missing OVERRAN print on cold runs, that is a
bug in the runner's own claim, filed here.

---

Corrected (same hour): the OVERRAN verdict is NOT missing -- it prints to stderr and exits 1
(run.sh:1101-1105), and the observing greps filtered it. Which means the cold runs at 94-97 s
DID fail the gate and the loop's own pipelines masked the exit code -- the observation error
was the fixer's, not the runner's. What stands of this item: the WARM headroom is shrinking
monotonically with the mirror (55-62 s at 133 tests when the bound was re-derived, 65-88 s at
152 now), and candidates (1) and (3) remain: re-derive the bound against the current
population with the derivation printed, and weigh in-suite parallelism within the
one-process-per-test rule. Candidate (2) sharpens: the bound's own comment says WARM
population -- the runner could EXEMPT builds from the measured span (compile time is the
nest's business) so cold runs stop crying wolf.
