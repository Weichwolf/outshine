Type: bug
Area: harness
Tags: instrument, corpus

**The runner scores a COPY of the manifest, and an edited declaration is silently stale**

`board:1364` moved every prepared file under the system temp root. The runner's contract is *one
directory*, so `prepare.py` copies the manifest in beside the products it made. **That copy can drift
from the tracked file, and it drifted within the hour it was written.**

[MEASURED] `expected.subjectFrameFraction` was added to five manifests in the tree;
`ADerivedCameraIsTheFramingRuleAndNotAQuotation` went on reporting *the case states the projected frame
fraction its boundary bound is applied under* as **FAIL**, because it was reading the copy made before
the edit. **Re-preparing the five cases turned it green with no other change.** The failure was correct
about the copy and wrong about the tree, which is the worst direction: it names the right claim and the
wrong file.

**`CLAUDE.md` states the rule this violates**: duplication is a defect exactly when the copies can
drift. An id may sit in a path because a path cannot disagree with itself; a manifest copy can.

## The candidates

- [ ] **The runner takes two paths** — the tracked manifest and the prepared directory. It ends the
  duplication outright, and it costs the *one directory* contract that `run.sh`, the prune and six tests
  currently share
- [ ] **The copy carries the tracked file's digest and the runner refuses a mismatch** *(recommended)*.
  The contract survives, the drift becomes a **named refusal** instead of a wrong verdict, and
  `provenance.json` is already the place that records what a preparation was made from. **A stale copy
  then reads as UNPREPARED rather than as a failing claim**
- [ ] **The copy is a symlink to the tracked file.** Cheapest to write; it makes the prepared directory
  no longer self-contained, which is the property that lets it be archived or copied to another machine

## What must not be concluded

**That the relocation caused this.** The hazard is the COPY and not its location — the same drift would
exist if the copy sat anywhere. What the relocation did was make the copy necessary, and make the drift
happen quickly enough to be found on the same day rather than in a round nobody could attribute.
