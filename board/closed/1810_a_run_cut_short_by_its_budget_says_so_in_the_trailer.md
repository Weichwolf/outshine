Type: bug
Area: test
Tags: process, gate, measured

# A run cut short by its budget says so in the trailer

`board:1778` gave a case its own wall clock so that a suite killed at the timeout reports
instead of measuring nothing. That was right, and it landed with one half missing: the case
reports the truncation in its LOG and returns PASS, so the run's trailer -- the line
`CLAUDE.md` tells the reader to read FIRST -- cannot tell a whole route from a seventh of one.

```
test/run.sh:411       export OUTSHINE_TIMEOUT_S="$TIMEOUT_S"
apps/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:514-532
apps/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:296-308
```

```cpp
const double budgetS = budgetSaid == nullptr ? 0.0 : 0.5 * std::atof(budgetSaid);
...
if (budgetS > 0.0 && spentS >= budgetS) {
  std::printf("SPENT the budget at %.1f km of %.1f after %.1f s, with %ld of %zu stills "
              "written\n", rode.ReachedM / 1000.0, routeM / 1000.0, spentS, wrote, atM.size());
  break;
}
```

Measured, from `board:1804`'s own record:

```
SPENT the budget at 113.9 km of 753.6 after 60.9 s, with 6 of 12 stills written
NOT JUDGED the 9 stations beyond 113.9 km -- the budget was spent there
```

**15.1 % of the route, half the stills, verdict PASS.** The remaining claims are then asserted
over what was reached, and the case is counted in `N tests: … PASS` exactly like a case that
drove to Hamburg.

## Why this is the rule the tree already wrote

`CLAUDE.md` states it twice, for two other reasons, in the same paragraph:

> *The fast gate also publishes **what it did not judge** … every declared case family holding
> no fetched subject is named, because a corpus is fetched and a green trailer must not read as
> coverage it never had (board:1765)*

and the runner already has the vocabulary: `UNPREPARED` means *this run judged nothing here*,
against `FAIL` which means *the code is wrong* (`board:1798`). A budget-truncated drive is the
third member of that family and has no word.

Worse, the truncation point moves with the MACHINE: `TIMEOUT_S` times the case's own 0.5, on
whatever hardware runs it. Two runs of the same commit judge different amounts of road and both
say PASS -- so a regression that first shows at km 400 is invisible on a slow machine and red on
a fast one, and nothing in the output distinguishes the two runs.

## What will be true

- [x] A case that stops on its budget publishes a machine-readable partiality -- the word, the
      share reached, and what it therefore did not judge -- and `test/run.sh` carries it into
      the trailer beside `UNPREPARED`, so a run that judged 15 % of its subject cannot read as
      one that judged all of it.
- [x] The share is a NUMBER in the trailer, not prose in a log: `PARTIAL n case(s) judged
      x % of their declared subject` or the runner's own preferred spelling.
- [x] Negative control: the budget forced to a tenth -> the trailer names the case and its
      share; the budget removed -> the line is absent.

## Comments

- 2026-08-24, reviewer round. This does not argue against the budget arm -- `board:1778` is
  right that a killed case measures nothing. It argues that a case which measured a seventh
  must not be counted as one that measured everything, and the runner is where that is counted.

## Repaid (2026-08-24)

`Check.h` gains `Partial(share, ofWhat)` beside `Unprepared`, the verdict line carries a
`PARTIAL` count, and `run.sh` names the case and the share it reached:

```
run.sh: apps/driver/stills/StillsAreTakenAlongTheDriveForTheEye JUDGED PART OF ITS SUBJECT
        -- 0.151199 of the route it was asked to drive, so this trailer says nothing about
        the rest and a run on another machine will stop somewhere else (board:1810)
run.sh: apps/driver/window/AWindowShowsTheRoadTheCarIsDriving JUDGED PART OF ITS SUBJECT
        -- 0.013192 of the route it was asked to drive, ...
```

**15.1 % and 1.3 %**, named, with the reason the number matters written into the line: the
truncation point moves with the machine, so two runs of the same commit judge different amounts
of road. That is why the share is a number in the trailer rather than prose in a log.

- **Proving test**: `run.sh apps/driver`, whose trailer now carries both lines.
- **Negative control**: the budget removed (`OUTSHINE_TIMEOUT_S` unset) -> the drive runs to
  arrival, `Partial` is never called, and the line is absent -- which is the state
  `board:1778` left and this item found.
- The fast gate is untouched: `apps/` is a named-only suite, so no partial line stands in it.
