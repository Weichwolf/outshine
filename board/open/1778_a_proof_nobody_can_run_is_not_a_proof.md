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
