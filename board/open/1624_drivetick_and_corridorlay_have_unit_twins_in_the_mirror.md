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
