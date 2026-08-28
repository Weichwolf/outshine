Type: bug
State: open
Area: test
Tags: measured, harness

# a case that passes alone passes in the run, and one does not

**Benchmark** — Unreal: an automation test is expected to be independent, and the runner reports
which test left the editor dirty. RAGE's build farm the same. **Both agree**, and the rule is
older than either: a suite whose verdict depends on what ran before it has no verdict.

`outshine/door/ScoreWhatAMovingSceneResends`:

    sh test/run.sh outshine/door     PASS in 3342 ms
    sh test/run.sh outshine          TIMEOUT at 122060 ms

Reproduced twice. **The runner is SEQUENTIAL** -- `test/run.sh:2178` walks cases one at a time
with a watchdog each -- so this is not contention for the machine or the GPU. Something an earlier
suite leaves behind makes this case hang, and `outshine/audio`, `outshine/content` and
`outshine/fuzz` run before `outshine/door` in that order.

It is not the orphaned-driver defect (board:2006): that one is fixed, `ps aux | grep outshine-`
prints nothing after the run, and the case that hangs is the same one every time rather than
whichever is longest.

- [ ] the case passes in `sh test/run.sh outshine` as it does alone
- [ ] whatever an earlier suite leaves behind is named, and the runner either clears it or refuses

**The measurement that would show I am wrong:** run the suites pairwise -- `outshine/audio
outshine/door`, then `outshine/content outshine/door`, then `outshine/fuzz outshine/door` -- and
whichever pair reproduces it names the leaver. If none does, the cause is the ORDER of more than
two and the walk widens from there.
