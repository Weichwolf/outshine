Type: bug
Area: harness
Tags: instrument, oracle

**The preparer's digest covers what prepares, and not what scores**

Every case's `provenance.json` records the digest of the preparer that made it, and
`EveryOracleWasPreparedByThisPreparer` holds the two against each other -- which is right and is how a
stale oracle is caught. **The population it digests is `test/harness` entire.**

**CORRECTION -- THAT SENTENCE WAS WRONG AND IT WAS THE WHOLE OF THIS ITEM'S FIRST CLAIM.** It read that
a change to `Parity.cpp` invalidates every oracle. It does not: BOTH sides digest `**/*.py` and nothing
else -- `jobs.py` globs it, `EveryOracleWasPreparedByThisPreparer.cpp` filters
`entry.path().extension() == ".py"`, and the two say so in each other's words. **The scorer is not in
the population at all**, so a verdict change costs no re-render. *Filed on a reading of the directory
name instead of the glob, and corrected by reading both.*

## What survives the correction, and it is narrower and still true

**A change to ONE VENDOR'S fixture generator invalidates every OTHER vendor's oracles.** [MEASURED]
this round: `test/harness/outshine/render/prepare/fixtures.py` builds grown subjects and is never read
while preparing a Khronos case, and editing it turned **163 of 171** prepared cases red and cost a full
re-preparation of the corpus.

**And the file argues for exactly that conservatism, in its own words**: *a named list is a second copy
of a fact* -- it drifted once already, and `board:1196` widened the population after splitting the
preparer by vendor precisely because a narrower glob silently stopped covering `grown.py`. **So this is
a cost that was chosen, not one that was overlooked**, and the question is whether a PER-VENDOR digest
keeps the property without the cost.

## What must be true

- [ ] **A case's digest covers the shared preparer plus its OWN vendor's sources**, derived the same
  way on both sides -- never a named list, which is the trap `board:1196` was filed about
- [ ] **A file that lands bytes on disk is in the population of the cases it can reach**, and the test
  is what it can change rather than which directory it sits in
- [ ] **The narrowing itself costs one last full re-preparation**, stated rather than discovered
- [ ] **It is priced before it is built.** 163 cases is one re-preparation about every time a fixture
  moves; if that is twice a year the change is not worth its own risk, and this item closes as declined
  with the number beside it

## Why this is a bug and not a feature

**The code claims to do it** -- the invariant exists so a stale oracle is caught -- and it fires when
nothing about THAT oracle could have changed. **A test that goes red for a reason outside its own claim
teaches a reader to ignore it**, which is the one thing an invariant cannot survive.

*Kept as a bug and not closed, because the residual is real; narrowed to what the measurement supports,
because the first reading was not.*
