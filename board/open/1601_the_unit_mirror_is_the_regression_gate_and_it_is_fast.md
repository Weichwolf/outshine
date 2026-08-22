Type: issue
Area: test

**The unit mirror is the regression gate, and it is fast**

The owner's rule (2026-08-22): unit tests guarantee regression safety and run FAST; the full
driver run happens only sporadically because it is long. Tests fast = engine fast.

Today's gap: the drive itself -- Lay's corridor pipeline and Ride's integration loop -- is
proven ONLY by the long driver cases (Munich ~2-4 min with a warm cache, stills ~4.5 min, the
window drive ~1 h). A regression in the drive logic hides until someone pays those minutes.
The bricks below it (corridor, physics, pilot units) are already fast and green.

## What must become true

- [ ] a fast drive test exists in the unit mirror: a synthetic corridor built in code (no
      network, no tiles, no fixture files -- the unit/pilot pattern), laid and ridden to
      arrival in milliseconds, guarding resection, speed-plan holding, and the crash reads
- [ ] the runner's DEFAULT set is the fast gate: `test/run.sh` without arguments runs
      everything EXCEPT the long driver suites, which run only when named (and the trailer
      says which suites were excluded, so a green default is never mistaken for the whole)
- [ ] the fast gate's wall time is published per run and bounded by a claims test with a
      measured population (baseline 2026-08-22: 111 unit+claims tests in ~35 s on this
      machine) -- a slow test is a finding, exactly like a slow frame
- [ ] the driver suites keep their place as the sporadic full proof: run before a close of
      any drive-touching item, never per edit
