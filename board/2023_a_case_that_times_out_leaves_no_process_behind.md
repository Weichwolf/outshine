Type: bug
State: open
Area: test
Tags: measured

# A case that times out leaves no process behind

**Benchmark** — Unreal: `UAutomationTest` runs a case in-process and a hung one takes the runner with it, which is visible. RAGE: `rage::sysTaskManager` bounds a task and reclaims its slot. **Taking RAGE** -- the bound is the runner's to enforce, and a verdict printed while the thing it judged is still running is not a verdict.

MEASURED: `outshine/door/ScoreWhatAMovingSceneResends` was reported TIMEOUT at 122 061 ms. Four
hours and twenty-three minutes later its process was still running, found only because it showed up
in a `ps` looking for something else. The runner printed TIMEOUT, moved on, and left the child
alive.

This is the THIRD time an orphan has been found in this tree by accident, and the second this
session. The earlier one was `outshine-driver` at 01:19:55, killed and its cause fixed with the SDL
process API and a bound. So a bound exists for a client and not for a case.

WHAT IT COSTS is not just a core. A live child holds the shared corpus lock and this checkout's
nest, so every later `run.sh` refuses or waits -- which is exactly what happened repeatedly while
board:2017 was measured, and each wait looked like a slow cold fetch rather than a held lock.

## What will be true

- [ ] a case declared TIMEOUT is DEAD before the verdict is printed, not after
- [ ] the runner refuses to print any verdict while a child it started is still alive
- [ ] the nest and corpus locks are released by the same path that kills the child, so a timeout cannot wedge the next run

## The measurements that would show I am wrong

1. **The negative control is a case that hangs on purpose.** Run one that sleeps past the bound: `ps` must show no child of the runner afterwards, and a second `run.sh` must start immediately rather than refuse on the nest
2. **The verdict's timestamp against the child's death.** If the child outlives the printed line by any measurable interval, the bound is advisory rather than enforced
3. `ScoreWhatAMovingSceneResends` itself: it may be hanging for a reason worth knowing, and killing the orphan is not the same as understanding why the case does not finish. That question is board:2011's
