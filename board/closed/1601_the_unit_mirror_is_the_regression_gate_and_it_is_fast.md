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

---

Progress 2026-08-22: box 1 stands -- unit/sim/ASyntheticRoadIsRiddenToArrivalInMilliseconds
lays, plans and rides 1.45 km in ~17 ms, holding lane (0.74 m worst) and plan (0.94 m/s worst)
with Ride's own braking lookahead; the mirror claim EverySourceLayerHasItsUnitMirror is green
over the whole tree. The gate measured: 115 tests in 39.6 s. Open: the runner's default set and
the bounded-wall-time claim.

---

**Closed, all four boxes.** The synthetic drive gate rides 1.45 km in ~17 ms; `test/run.sh`
without arguments IS the fast gate (118/118 in 43.6 s), excluding the named-only suites loudly
by a declared list; the runner judges its own pace -- kFastGateBoundMs = 90000, [SET] at ~2x
the measured 115-test baseline of 39.6 s on this machine, and an overrun is a red run; the long
driver suites remain the sporadic full proof, run only when named. `make test` therefore runs
the gate. The mirror claim EverySourceLayerHasItsUnitMirror walks the whole tree green.
