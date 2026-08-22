Type: bug
Area: harness
Tags: perf, instrument

**A frame measurement says when the machine under it was busy**

`outshine/frame/TheFrameCostIsPublishedAgainstItsOwnFloor` went red inside a full run and green on its
own, with no change to the tree between the two.

| | second ray | floor sum | resolved |
|---|---|---|---|
| inside a full run, machine also serving an agent | **1.940 ms** | **2.202 ms** | **no** |
| the same commit, run alone | -- | -- | **yes** |

The claim is *the instrument resolves one more shadow ray per fragment above its own floor*, and the
floor is computed from the SAME run's noise. **Load raises the measurement and the floor together but
not by the same factor**, so a busy machine can put a real effect underneath a floor that grew faster
than it did -- and the verdict then reports the machine rather than the tree.

**This is not the sanitiser rule and it is not covered by it.** `CLAUDE.md` already says a duration
measured through a bounds checker is not the shipping frame, and the frame suite links with no
sanitiser in the path for exactly that reason. What is missing is the sentence beside it: **a duration
measured while something else runs is not the shipping frame either.**

## What must be true

- [ ] **A frame arm says whether it was measured on a quiet machine**, and *quiet* is a number the run
  takes rather than an assumption it makes -- load average, or the run's own dispersion against its
  archive, or a named alternative
- [ ] **An unresolved instrument is distinguishable from a moved cost.** Today both are one red. They
  are different findings: one says *this measurement could not be taken*, the other says *the engine
  changed*, and only the second is about the tree
- [ ] **The archive already holds the answer and is not being asked.** 377 earlier measurements sit
  beside this arm; a run whose dispersion is far outside that history is a run that should say so
  before it delivers a verdict

## What this may NOT do

**It may not widen the floor until the red goes away.** The floor is what makes the pricing claim
falsifiable, and a floor chosen to admit a measurement is a floor fitted to a number -- which
`CLAUDE.md` names as its own defect. The repair is that the run knows whether it was in a position to
measure, not that it lowers what counts as measured.

## Comments

Found while re-running a full suite after `board:1392`: the failure appeared with a change to
`src/gltf/Pose.cpp` in the diff, and a causal path from an animation channel loop to the price of a
shadow ray on an arm with `lights=0` and an unanimated subject is not plausible. **The caveat was
sought first and it was the true one.** Every other timing arm in the same run reported *within-floor*
against its archive -- geometry -2.2 %, fill -13.2 %, fill-twice-lit +0.2 %, texture +1.1 % -- which is
itself the signature of a noisy run rather than a moved cost.

---

Closed -- the three demands stand, and the floor was not touched:

- QUIET IS A NUMBER THE RUN TAKES, twice: getloadavg against half this machine's threads
  (derived from hardware_concurrency, no literal), and the run's floor-sum against the
  archive's remembered median. Both published as NOTEs on every run.
- AN UNRESOLVED INSTRUMENT IS DISTINGUISHABLE FROM A MOVED COST: unresolved on a busy
  machine reports UNPREPARED ("this measurement could not be TAKEN") -- run.sh counts it
  apart from FAIL; unresolved on a QUIET machine stays the red it always was, now saying
  "this is a MOVED COST".
- THE ARCHIVE IS ASKED: the fill + fill-twice-lit floor-sums of every archived run feed a
  median; kBusyFloorInflation [SET] 2.0 from the filing incident (the busy floor stood at
  twice its quiet neighbours while every other arm sat within-floor).

Also repaid in the same sitting: the frame and scenario suites had gone UNBUILDABLE at HEAD
(-Isrc/data missing after GltfStudio grew its Wgs84 include) -- the include grants carry it
now, frame 4/4 (second-ray 1.852 ms over 0.188 ms floor, load 1.24 under quiet bound 3),
scenario 6/6. Proving test: TheFrameCostIsPublishedAgainstItsOwnFloor itself -- its verdict
now names which of the three findings it delivers.
