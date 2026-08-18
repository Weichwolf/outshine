Type: bug
Area: harness
Tags: instrument, oracle

**The preparer's digest covers what prepares, and not what scores**

Every case's `provenance.json` records the digest of the preparer that made it, and
`EveryOracleWasPreparedByThisPreparer` holds the two against each other -- which is right and is how a
stale oracle is caught. **The population it digests is `test/harness` entire.**

**So changing how a case is SCORED invalidates every oracle that was RENDERED.** `Parity.cpp` decides
verdicts and lands nothing on disk; `Check.h` is a test harness; `Ties.h`, `PictureBound.h` and
`Acceptance.h` are all scoring. None of them can change a byte the preparer writes, and a change to any
of them makes 171 prepared cases read as prepared by somebody else.

**[MEASURED] this round it cost a full re-preparation of the corpus** -- a change to
`test/harness/outshine/render/prepare/fixtures.py`, which builds grown fixtures and is never read while
preparing a Khronos case, turned 163 cases red on the invariant. And the next scoring change will do it
again.

**This is `CLAUDE.md`'s named failure in its own words: the INPUT SET TOO WIDE.** *The number was right
and about something else.* The digest answers "did the same code produce this" over a population that
includes code which could not have produced it.

## What must be true

- [ ] **The digest's population is what the preparation RUNS** -- `test/harness/shared/corpus/` and the
  per-vendor `prepare/` directories -- and it is derived the same way on both sides, which is the
  property `board:1196` established and which must survive this narrowing
- [ ] **The scorer is out of it**, and a scoring change costs no re-render
- [ ] **A file that lands bytes on disk is IN it**, however it is spelled: the test is whether the file
  can change what a prepared case contains, not which directory it sits in
- [ ] **The narrowing itself costs one last full re-preparation**, and that is stated rather than
  discovered

## Why this is a bug and not a feature

**The code claims to do it.** The invariant's own comment says it exists so a stale oracle is caught,
and it names the divergence it was built for; what it does not say is that it also fires when nothing
about the oracle could have changed. **A test that goes red for a reason outside its own claim teaches
a reader to ignore it**, which is the one thing an invariant cannot survive.
