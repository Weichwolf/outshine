Type: bug
Parent: 1843
Area: test
Tags: trailer, telemetry, coverage

# A case that judged nothing of its subject says so without reddening the gate

CLAUDE.md: *"a corpus is fetched and a green trailer must not read as coverage it never had"*.
`board:1843` gave `TheCorpusIsPrunedByOneRunnerOnly` an early return when another nest holds the
corpus claim -- the right RULE with the wrong REPORT: the case returned through `Report()`
carrying one `CHECK` about an environment variable and none about its subject, so the trailer
counted it PASS.

Measured, this review's gate at `ae926bbc`, in the worktree CLAUDE.md mandates:

```
log/harness-claims-TheCorpusIsPrunedByOneRunnerOnly.log
NOTE the corpus lock stands: yes, naming pid 89254, and this run is 11664
NOTE another nest holds the corpus while this ran ... so this run judges none of them
CHECKS 1 FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0
```

`CHECKS 1` is `test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:26` -- *"this test runs
under a runner that holds the nest"*. Not one of IV.15's own claims ran, and the verdict was
PASS.

## The repair landed and it overshot

`4eae57bc` put `Unprepared(...)` on both vacuous exits (`:49`, `:73`). UNPREPARED is the right
WORD and the wrong REGISTER, because `run.sh` counts it as red:

```sh
test/run.sh:1458  red=$((failed + timedout + signalled + unbuilt + undeclaredSkips + unprepared + compileBlind))
test/run.sh:1459  [ "$red" -eq 0 ] || exit 1
```

Measured at `4eae57bc`, in a parallel worktree, while the main nest held the corpus lock (pid
92848, alive):

```
UNPREP  harness/claims/TheCorpusIsPrunedByOneRunnerOnly    191 ms
33 tests: 32 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  1 UNPREPARED  in 49133 ms
exit=1
```

The SAME suite, the same worktree, one commit later, run when the main nest happened NOT to hold
the lock:

```
33 tests: 33 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  in 42737 ms
exit=0
```

Two runs of one tree, two verdicts, decided by which process held a lock file. So the fast gate
now goes RED whenever a second checkout is running -- which is the review's
mandated configuration and the exact situation `board:1843` was filed about. The main nest never
sees it, because the main nest holds the lock and the case runs in full. A guard that is green
for whoever happens to be first and red for everyone else has not been repaired, it has been
inverted.

`PARTIAL` is the register that carries the fact without the verdict:

```cpp
test/harness/shared/Check.h:78   inline void Partial(double share, const char *ofWhat)
```

`board:1810` built it for this sentence -- *"UNPREPARED already means 'this run judged nothing
here'; PARTIAL is the third member of that family"* -- `run.sh:1041` collects it, `run.sh:1398`
names the case and its share in the trailer, and it does NOT enter `red`. Its two users are
`apps/driver/test/window/AWindowShowsTheRoadTheCarIsDriving.cpp:314` and
`apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:543`.

The distinction the tree needs is three-valued, not two:

| state | what it means | verdict |
|---|---|---|
| the subject is missing and should not be | UNPREPARED | red, and a rebuild is attempted |
| the subject is LEGALLY unavailable this run | another nest holds the claim | not red, and the trailer names it |
| the subject was judged | | PASS or FAIL |

## What will be true

- [ ] The "another nest holds the claim" exit publishes that it judged nothing WITHOUT
      reddening the gate -- `Partial(0.0, ...)`, or a fourth verb whose name says "legally
      absent", so `test/run.sh:1458` keeps its meaning.
- [ ] The "no corpus on disk" exit is decided the same way, deliberately: `board:1796` argued it
      is vacuously true and should not shout, and `4eae57bc` made it red.
- [ ] The trailer names the case and the reason, the way it names PARTIAL cases today, so a
      reader of the FIRST line knows IV.15 was not judged this run.
- [ ] Proving test: a `harness/claims` run in a second checkout while the first holds the corpus
      lock exits 0 and its trailer names the unjudged claim. Negative control: `4eae57bc` ->
      `exit=1`, `1 UNPREPARED`, measured above.

## Comments

- 2026-08-25 -- filed by the hourly review, measured in its own run at `ae926bbc`, and reopened
  the same hour against `4eae57bc`, measured again. Both measurements are in this body.
