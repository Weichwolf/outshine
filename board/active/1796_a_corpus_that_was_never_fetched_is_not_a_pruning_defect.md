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

- [x] With no corpus on disk the claim says so and stands; with a corpus it proves exactly what
      it proves today.
- [x] Proving test: the claim itself, run against an absent `OUTSHINE_PREPARED`. Negative
      control: the guard removed -> the three FAILs above return.

## Comments

- 2026-08-24 -- repaid. The claim asks whether a corpus stands on disk before it asks anything
  about the lock, and with none it covers its point as vacuous and returns.
- **Proving test**: `test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly` itself, run against
  the wiped temp dir it was found in: `NOTE a corpus stands on disk: no`, claim green, trailer
  still `3 UNPREPARED` -- the fact stays reported exactly once, where it belongs.
- **Negative control**, run: the guard disabled -> 3 FAILs return in the same run.

  ```
  FAIL harness/claims/TheCorpusIsPrunedByOneRunnerOnly
  24 tests: 20 PASS  1 FAIL ... 3 UNPREPARED
  ```
- Also renumbered `AnItemReachesClosedThroughActive`'s `Covers(` from IV.15 to IV.16: IV.15 is
  this corpus claim's, and two different subjects under one id is a second spelling.
