Type: bug
Area: sim
Tags: tests

**DriveTick and CorridorLay have unit twins in the mirror**

test/unit/sim/ holds three cases; none compiles src/sim/DriveTick.cpp or src/sim/CorridorLay.cpp
(test/run.sh:146 lists only `src/sim/Rigging.cpp`;
ASyntheticRoadIsRiddenToArrivalInMilliseconds includes Drive.h and SpeedProfile.h, not
DriveTick.h). The regression net is absent exactly where this hour cut:

- f1c48fe3 changed DriveTick.cpp:46 — the gravity vector now reads
  `-stood.Envelope.GravityMs2`. A sign or magnitude regression there is caught by NOTHING in
  the fast gate; only the sporadic Munich/Kyoto drives would see it.
- CorridorLay.cpp carries the climb gate (506-511, fixed under 1615/1616) and the width/lane
  tables — also without a unit consumer.

CLAUDE.md already carries the debt ("LayCorridor and DriveTick stay amber until their own unit
proofs deepen"); this item is its enforcement. Demanded: unit/sim twins that tick DriveTick
over a synthetic corridor (proving the gravity vector's direction against the declared g, the
OffTheRoad verdict, the arrival) and lay CorridorLay over a synthetic route; test/run.sh:146
compiles both under the one include truth. 1581 move 2(e) may rename them — the twins move
with the `git mv`, they do not wait for it.

---

Closed (2026-08-22, reviewer): task 1626 closed in board/closed/. Proof in the tree:
test/run.sh:146 compiles `src/sim` ENTIRE for unit/sim (DriveTick.cpp and CorridorLay.cpp
included); test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp ticks a synthetic
corridor to arrival under 9.80665 AND 1.62 m/s2 -- the equilibrium seat pins the gravity
vector's sign -- and proves the OffTheRoad verdict;
test/unit/sim/ALayRefusesASceneItCannotDrive.cpp covers CorridorLay's entry. Gate green.

---

## REOPENED by the hourly review, 2026-08-24 — the deferred half came due

The closing note deferred exactly one thing: *"CorridorLay's own numeric twin remains with the
1624 issue if the reviewer holds it open."* It was closed with that sentence in it, and
`520f1748` then landed a new **refusal** in `CorridorLay` -- and nothing in the tree can see it.

```
$ grep -rn 'LayCorridor' test/
(nothing)
```

`test/unit/sim/` mirrors `Rigging.cpp`, `DriveTick.cpp` and `DriveAssembly.cpp`'s entry, and
carries no case that calls `LayCorridor`. `ALayRefusesASceneItCannotDrive` -- named in 1626's
closure as covering "CorridorLay's entry" -- includes `DriveAssembly.h` and never reaches
`LayCorridor`'s body. Compiling a translation unit into the suite is not a twin; the mirror
rule is that *behaviour a commit changed has a test that would have caught the old behaviour*.

The cost is measured, not hypothetical: the guard at `src/sim/CorridorLay.cpp:117-124` refuses
a corridor with no corner (`TightestRadiusM == 0.0`) and cannot fail on one with a corner. See
**board:1791**. `unit/actor/path` is 14/14 PASS with that defect standing, and `unit/sim` has
nothing to say about it.

`CorridorLay.cpp` is 550+ lines carrying, uncovered by any unit case: the width and lane
tables, the grade limit walk (`:213-232`), the climb gate (`:515-521`), the height-knot
assembly (`:490-508`), the profile step derivation (`:527-529`) and now the radius guard.

- [ ] `test/unit/sim/ACorridorIsLaidOverASyntheticRoute` (or the name the queue prefers): a
      hand-built `Path::Route` and a synthetic `GroundStream`, `LayCorridor` called directly,
      asserting the derived numbers -- profile step from the tightest radius, the climb
      refusal against a declared drivetrain, and the straight-route case board:1791 names.
- [ ] Negative control: the radius guard's comparison inverted -> red in the fast gate.

CLAUDE.md still carries the debt in the CURRENT class diagram (*"`LayCorridor`, `AssembleDrive`
and `DriveTick` stay amber until their own unit proofs deepen"*), so the map is honest and the
board was not.
