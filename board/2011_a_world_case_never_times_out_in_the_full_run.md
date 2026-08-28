Type: bug
State: open
Area: test
Tags: measured, harness, intermittent

# a world case never times out in the full run, and one still does about half the time

**Benchmark** — Unreal: an automation test that is flaky is quarantined and the flake is the bug,
not the test. RAGE's farm the same. **Both agree** — an intermittent red is a red.

board:2009 found and fixed a DETERMINISTIC cause of this shape: a refused tile counted as pending,
so `DriveAssembly`'s `for(;;)` spun to `kPatienceS = 900.0` while the case's bound is 120 s. Two
full runs went `78 tests: 78 PASS` in 127 s and 137 s after it.

**A residual remains and it is not the same thing.** `sh test/run.sh outshine` has since read
`79 tests: 78 PASS 1 TIMEOUT` twice, on a DIFFERENT case each time -- first
`ScoreWhatAMovingSceneResends`, then `ScoreWhatAWiderWorldHolds` -- and passed clean in between.

What is measured, so the next attempt does not re-walk it:

| | |
|---|---|
| the cold path | NOT it. With `/tmp/outshine-drive-cache` moved aside `ScoreWhatAWiderWorldHolds` refuses in **3 s** with `none cached`, and so does `ScoreWhatAMovingSceneResends` |
| the runner's parallelism | there is none. `test/run.sh:2178` walks cases one at a time |
| orphaned children | board:2006 bounds them; `ps aux \| grep outshine-` is empty after a run |
| suite ORDER | board:2009 withdrew it: `outshine/audio outshine/door` reproduced once and passed on the next run of the same pair |
| the margin | these cases run in 3-8 s against a 120 s bound. A 15x margin does not get eaten by load; something stops |

- [ ] `sh test/run.sh outshine` reads 79 PASS ten times running
- [ ] whatever stops is named, with a `sample(1)` stack taken from the hung process IN the full run

**The measurement that would show I am wrong:** ten clean full runs would say the residual was the
two background shells this session left behind while measuring, and the item is withdrawn. That
walk has not been done and no cause may be written before it is.
