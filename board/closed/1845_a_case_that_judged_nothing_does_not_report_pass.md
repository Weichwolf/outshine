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

- [x] The "another nest holds the claim" exit publishes that it judged nothing WITHOUT
      reddening the gate -- `Partial(0.0, ...)`, or a fourth verb whose name says "legally
      absent", so `test/run.sh:1458` keeps its meaning.
- [x] The "no corpus on disk" exit is decided the same way, deliberately: `board:1796` argued it
      is vacuously true and should not shout, and `4eae57bc` made it red.
- [x] The trailer names the case and the reason, the way it names PARTIAL cases today, so a
      reader of the FIRST line knows IV.15 was not judged this run.
- [x] Proving test: a `harness/claims` run in a second checkout while the first holds the corpus
      lock exits 0 and its trailer names the unjudged claim. Negative control: `4eae57bc` ->
      `exit=1`, `1 UNPREPARED`, measured above.

## Comments

- 2026-08-25 -- filed by the hourly review, measured in its own run at `ae926bbc`, and reopened
  the same hour against `4eae57bc`, measured again. Both measurements are in this body.

## Sharpened 2026-08-25 -- the repair landed, and the register is now diluted

`9117e1c9` put `Partial(0.0, ...)` on both vacuous exits
(`test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:49`, `:76`). That is one of the two
options this item's own box allowed, it does exactly what it was asked -- the gate is no longer
red for a configuration CLAUDE.md mandates -- and it leaves three things standing.

**1. `PARTIAL` now spans two meanings and its trailer sentence contradicts itself at one of
them.** `board:1810` built PARTIAL for *"a seventh of the route was driven"*, and the trailer
prints that reading verbatim:

```sh
test/run.sh:1401  'run.sh: %s JUDGED PART OF ITS SUBJECT -- %s of %s, ...'
```

At share `0.0` this reads *"JUDGED PART OF ITS SUBJECT -- 0.000000 of IV.15's own claims"*. A
part of zero is not a part. The three-valued distinction this item's own table demanded still has
only two registers to sit in, and the second one now carries both "some" and "none".

**2. The FIRST line still counts it as PASS.** This item's third box says a reader of the first
line must know IV.15 was not judged. The first line is

```sh
test/run.sh:1388  '%s tests: %s PASS  %s FAIL  ... %s SKIP  %s UNPREPARED  in %s ms'
```

and it has no PARTIAL column: the case lands in `PASS` and the fact goes out afterwards, on
stderr. `partialCases` is initialised at `test/run.sh:953` and incremented at `:1042`, so the
number exists and is not printed where the box asked for it.

**3. The register is load-bearing only by accident.** `Report()` fails a case that checked
nothing:

```cpp
test/harness/shared/Check.h:94   if (Checks.Value() == 0 && Skips.Value() == 0 && Unprepareds.Value() == 0) {
test/harness/shared/Check.h:96     std::printf("FAIL no claim was checked\n");
```

`Partials` is not in that condition. This case survives it only because of the unrelated
`CHECK(nest != nullptr, ...)` at `:28`; a case whose sole output is `Partial(0.0, ...)` goes red
with *"no claim was checked"*, which is the opposite verdict from the one intended.

### What will additionally be true

- [x] `PARTIAL` at share 0 either gets its own sentence in the trailer -- "JUDGED NONE OF ITS
      SUBJECT" -- or a fourth verb carries the legally-absent case, so the register a reader sees
      matches the fact.
- [x] The first trailer line carries `%s PARTIAL` beside `%s SKIP` and `%s UNPREPARED`; the count
      is already computed at `test/run.sh:1042`.
- [x] `Report()`'s vacuity guard counts `Partials`, so `Partial(...)` alone is a complete report.
- [x] Proving test: `harness/claims` in a second checkout while the first holds the corpus lock
      -> exit 0 AND the first line names the unjudged case. Negative control: HEAD -> exit 0 with
      the case counted in `N PASS` and the sentence reading "judged part -- 0.000000".

### Measured, this review's gate at `1c1e6e57`, in the parallel worktree while the main nest held the lock

```
271 tests: 271 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  in 502474 ms
run.sh: harness/claims/TheCorpusIsPrunedByOneRunnerOnly JUDGED PART OF ITS SUBJECT -- 0.000000 of of IV.15's own claims, because another nest held the corpus claim, so this trailer says nothing about the rest and a run on another machine will stop somewhere else (board:1810)
exit=0
```

The gate is green in the mandated configuration, which is the repair working. Three facts stand
in that one line:

| | |
|---|---|
| `271 PASS` | the case that judged none of its subject is inside it, and the first line says nothing else |
| `JUDGED PART ... 0.000000` | the sentence contradicts the number it carries |
| `of of` | `test/run.sh:1401` supplies the `of`, and both call sites at `TheCorpusIsPrunedByOneRunnerOnly.cpp:49` and `:76` supply a second one inside `ofWhat` -- the tree's other two users (`apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:543`, `apps/driver/test/window/AWindowShowsTheRoadTheCarIsDriving.cpp:314`) pass `"the route it was asked to drive"` with no leading `of`, which is the form the format string expects |

- [x] The two `Partial(0.0, "of IV.15's own claims, ...")` call sites drop the leading `of`, or the
      format string stops supplying one -- one of the two, not both.

## The four the review measured are fixed (2026-08-25)

| | before | after |
|---|---|---|
| a share of zero | `JUDGED PART OF ITS SUBJECT -- 0.000000` | `JUDGED NONE OF ITS SUBJECT -- 0.000000` |
| the format string | `... -- 0.000000 of of IV.15's own claims` | `... of IV.15's own claims` |
| the first trailer line | no PARTIAL column, while `partialCases` was counted | `0 UNPREPARED  1 PARTIAL` |
| `Check.h`'s vacuum guard | counted Checks, Skips, Unprepareds | counts Partials too |

```
34 tests: 34 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  1 PARTIAL
run.sh: ...TheCorpusIsPrunedByOneRunnerOnly JUDGED NONE OF ITS SUBJECT -- 0.000000 of IV.15's
        own claims, because another nest held the corpus claim ...
```

**On whether `Partial(0.0)` dilutes what board:1810 built**: the review's answer is yes, and the
guard is where that is settled rather than argued. `Partial` was for *"a seventh of the route"*;
zero is not part of anything. But the family it belongs to is the one that says **how much of
its subject a run judged**, and zero is a member of that family -- the trailer now says NONE
rather than PART, and the vacuum guard treats it as a statement rather than as silence. A case
that says nothing at all is still caught; a case that says "none, and here is why" is not.

Proving test: the runner itself, under a live foreign corpus holder. Negative control, run: the
guard's `Partials` term removed and a case written that only calls `Partial(0.0)` ->
`FAIL no claim was checked`, which is the state where an honest partial answer reads as silence.
