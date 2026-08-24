Type: bug
Parent: 1571
Area: test, tools
Tags: gate, runnability, named-only

# A proof nobody can run is not a proof

`tools/driver/window/AWindowShowsTheRoadTheCarIsDriving` draws every frame of Munich to
Hamburg. Measured on this machine, 2026-08-24:

```
DRIVEN 110.0 km of 753.6, frame 159894, p-so-far worst 18.03 ms
run.sh: tools/driver/window/AWindowShowsTheRoadTheCarIsDriving was killed after 560 s
```

**110 km in 560 s.** The full route needs roughly **3 800 s -- 63 minutes** of wall clock,
against `test/run.sh`'s default `TIMEOUT_S=120`. So:

| | |
|---|---|
| default gate | kills it at 120 s, at ~24 km of 753 |
| `--timeout 560` | kills it at 112 km |
| what it needs | `--timeout 4200`, and an hour of the machine |

`board:1571`'s second box -- attribute the 22.99 ms frame -- cannot be ticked by anyone who
does not know to pass a four-digit timeout and then wait an hour. That is not a proof anybody
runs; it is a proof that exists.

The same shape as `board:1765` (a suite whose corpus is unfetched judges nothing) and
`board:1766` (a suite the gate does not run does not compile): a case can be green, red, or
simply UNREACHABLE, and the third reads like the first.

## What will be true

1. The case declares what it costs, so a reader knows before starting it: a `Note` of the
   projected wall clock from the route length and the measured frame rate, printed BEFORE the
   drive rather than after.
2. Either the case has a bounded arm the gate can run -- a declared distance that fits a
   stated budget, with the full route behind an explicit name -- or the tree states plainly
   that this proof is an hour long and names who runs it and when.
3. `test/run.sh` says what it did NOT run for want of time, the way it now says what it did
   not compile (board:1766) and what it holds no corpus for (board:1765). A suite killed by
   its own timeout is a measurement that did not happen.

## Comments

- 2026-08-24 -- found while working board:1571, whose instrument box turned out to be already
  paid (the case prints `WORST %.3f ms at %.3f km, frame %ld, %s`), leaving only the box that
  needs the hour.
- The partial run is not worthless: at 112.200 km the worst frame was **18.375 ms, steady**
  -- not a relay, so not corridor-laying. That is a real attribution of a real outlier, and it
  came from 560 s rather than 3 800.

## Repaid (2026-08-24)

**The runner hands the case its budget** (`test/run.sh`, `export OUTSHINE_TIMEOUT_S`), and the
case spends half of it driving and reports. Half is the margin: the drive has to finish AND
publish inside the budget, and half leaves the report as much room as the drive took.

```
NOTE the route the scenario asks for                = 753.617 km
NOTE the wall clock the runner allows this case     = 120 s
NOTE of it this drive spends before it reports      =  60 s
SPENT the budget at 10.6 km of 753.6 after 60.0 s, frame 21603
NOTE what the whole route would cost at that rate   = 4276.479 s
NOTE in minutes                                     =   71.275 min
NOTE the whole route needs --timeout 8565; this arm drove 10.6 km of 753.6 and says so
```

**71.3 minutes, measured rather than estimated**, and the flag that buys it printed in the log.
That answers point 1 in the only honest way available: a projection needs a rate, a rate needs
a drive, and the drive publishes both.

**What it did not judge, it names.** Three claims are properties of the whole route -- the
handover at its middle third, arrival, and p99 over all of it. A bounded arm has reached none
of them. Asserting them anyway makes the arm red for what it never attempted; skipping them
silently is the defect `board:1765` exists against. So:

```
NOT JUDGED the handover at the route's middle third -- the drive stopped at 10.6 km of 753.6
NOT JUDGED arrival at Rathausmarkt -- same reason
NOT JUDGED p99 over the WHOLE route; what follows is p99 over 10.6 km of it
```

Point 3 was already paid before this round: `test/run.sh:1309` prints **MEASURED NOTHING** for
every case the timeout killed.

## What the budget arm immediately found

Two things nobody had seen, because nobody had ever seen this case reach its own report:

- **board:1800**: the case asserts a MAXIMUM frame time against a budget `CLAUDE.md` declares
  as p50/p95/p99. Measured: p50 1.725, p95 2.705, p99 4.075, worst single frame **16.698 ms**
  at 1.401 km -- 0.19 % over, once in 21 603 frames.
- The scenario's own asset had no owner in the rebuild of `board:1797`:
  `tools-driver-f31` is placed by `prepare.py scenario-assets`, not by a manifest. `RebuildOwner`
  falls back to that subcommand when no manifest owns a prepared directory, and the log line it
  reads had its path in the middle of a sentence rather than at the front -- both fixed here.

- **Proving test**: the case itself, `tools/driver/window/AWindowShowsTheRoadTheCarIsDriving`,
  which now finishes and reports inside the runner's default 120 s.
- **Negative control**, run: `OUTSHINE_TIMEOUT_S` unset -> `budgetS` is 0, the loop runs to the
  frame cap, and the case is killed with no report -- the state this item was filed against.
