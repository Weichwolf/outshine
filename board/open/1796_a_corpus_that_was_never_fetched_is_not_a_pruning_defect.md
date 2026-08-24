Type: bug
Area: test
Tags: process
Regresses: 1790

# A corpus that was never fetched is not a pruning defect

`test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly` opens with

```cpp
const bool held = std::filesystem::exists(lock);
CHECK(held, "**A RUNNER THAT PRUNES THE CORPUS HOLDS ITS CLAIM**: this very run is pruning, ...");
```

and `run.sh:59-60` takes that claim only when there is something to claim:

```sh
ClaimCorpus() {
  [ -d "$PREPARED" ] || return 0
```

The corpus lives in the system temp dir and is FETCHED, never versioned. When the machine's
temp cleaner removes it -- which it did between two gate runs tonight, along with
`outshine-content` -- the runner correctly takes no claim, and this claim then fails three
times over a premise that was never its subject:

```
NOTE the corpus lock stands: no
FAIL **A RUNNER THAT PRUNES THE CORPUS HOLDS ITS CLAIM**
FAIL and it names the pid that took it
FAIL and this run's own claim was never moved
```

Its subject is *a runner that prunes holds the claim*. With no corpus, no runner prunes, and
the statement is vacuously true. Asserting the lock exists asserts a PRECONDITION, and the
tree already reports an unfetched corpus where it belongs -- the `UNPREPARED` count in the
trailer and the named case families of `board:1765`. Two spellings of one fact, and the wrong
one is the loud one.

This is `board:1790` in a second place: *no claim in the gate turns red by the clock*. The same
rule holds for the disk -- a claim must go red for what it proves, never for what its
environment happened to hold.

## What will be true

- [ ] With no corpus on disk the claim says so and stands; with a corpus it proves exactly what
      it proves today.
- [ ] Proving test: the claim itself, run against an absent `OUTSHINE_PREPARED`. Negative
      control: the guard removed -> the three FAILs above return.
