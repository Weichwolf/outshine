Type: bug
Parent: 1789
Area: test
Tags: claims, false-red, parallel, measured

# The pruner claim measures its OWN claim, not whichever lock stands

`test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly` ends with

```cpp
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:111   CHECK(std::filesystem::exists(lock),
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:112         "and this run's own claim was never moved, so the proof cannot open the window it "
test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:113         "exists to close (board:1789)");
```

`lock` is `$TMPDIR/outshine-prepared.lock` -- one path for every checkout. The case never asks
whether THIS run holds it. When another nest holds the claim, the case reads a lock it does not
own, and then asserts that lock still exists at the end; if the owning runner finishes in the
meantime, `ReleaseCorpus` removes it and the case goes RED for a thing it did not do.

## Measured, this round

The hourly review ran the fast gate in its own worktree while the main nest was running --
which is the arrangement CLAUDE.md MANDATES:

```
run.sh: another runner (pid 67076) is reading the shared corpus, so this run will NOT prune it
...
NOTE the corpus lock stands: yes
NOTE the claim names pid 34475
NOTE a child under this runner: ... this runner does not hold the corpus claim and would NOT prune
NOTE with no claim standing:    ... this runner holds the corpus claim and WOULD prune
FAIL test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:112
       CHECK(std::filesystem::exists(lock))
CHECKS 7 FAILURES 1

264 tests: 263 PASS  1 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  in 469772 ms
```

The gate reported 264/264 in the main nest and 263/264 in the review worktree, on the same
commit `e7de9c1e`, with no source difference. The pid the claim printed (34475) is not this
run's; the lock vanished because its real owner exited.

The first six checks are all correct and the guard they measure works. The seventh measures the
weather.

## What will be true

- [ ] The case asks whether THIS run holds the claim before it asserts anything about the
      claim's contents or survival. Where it does not hold it, the check is a NOTE, not a
      CHECK -- "another runner owns the corpus" is a legal state, and this item is the one that
      made it legal.
- [ ] Where the case must prove that its own proof moved nothing, it compares the lock's
      CONTENT before and after -- pid and mtime -- rather than mere existence, so a foreign
      runner's orderly exit is told apart from a claim this case borrowed.
- [ ] Proving test: the case itself, run with a foreign claim standing and released mid-run.
      Negative control: the existence check restored -> red, as measured above.

## Comments

- 2026-08-25 -- filed by the hourly review from its own gate run. A claim that goes red because
  a second, legitimate runner finished is a claim that trains its readers to ignore it, and it
  goes red in exactly the configuration this tree requires the review to use.
