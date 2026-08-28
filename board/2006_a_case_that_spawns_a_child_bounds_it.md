Type: bug
State: active
Area: test
Tags: measured, harness

# a case that spawns a child BOUNDS it, and no process outlives the run that started it

**Benchmark** — Unreal: `FPlatformProcess::CreateProc` is paired with `WaitForProc` under a
timeout and the automation runner reaps what it starts. RAGE's build farm does the same. **Both
agree**, and the rule is older than either: a parent that cannot kill its child has not spawned a
process, it has leaked one.

`test/outshine/door/ScoreWhatTheDriveMeasures.cpp:31` runs the driver through `popen` and reads
until EOF. `popen` hands back no pid, so there is nothing to kill. When `test/run.sh` cuts the
CASE off at its 120 s bound, the driver child is orphaned and keeps running.

**MEASURED, and it had been running for an hour and twenty minutes:**

    26875 01:19:55 ./build/outshine-driver --headless --offline --frames 24

It cost a core the whole time, and the bill arrived as three door runs in a row where a DIFFERENT
case timed out each time -- `ScoreWhatTheDriveMeasures`, then `ScoreWhatAWiderWorldHolds`, then
`ScoreWhatCodeCanDeclare`, always one of the three longest, always with an EMPTY log because the
case never got enough of the machine to print. Killing the orphan took the suite straight back to
`35 tests: 35 PASS 0 TIMEOUT in 103067 ms`.

**This is the second time in this session.** Three driver orphans of 2h, 1.5h and 1h poisoned an
earlier run, and the cause was read then as "I used `kill` instead of `kill -9`". That was the
symptom. The cause is that nothing in the tree bounds a spawned child, so an orphan is one killed
case away at all times, and the failure it produces names an innocent case.

- [ ] the case spawns the driver with a bound and kills it when the bound passes
- [ ] a case killed by the runner leaves no process behind

**The measurement that would show I am wrong:** `ps aux | grep outshine-` after a door run must
print nothing. Negative control: cut the bound to something the drive cannot finish inside and the
case must FAIL with the bound named, not hang.
