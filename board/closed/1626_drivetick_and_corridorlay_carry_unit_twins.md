Type: task
Parent: 1624
Area: sim

# DriveTick and CorridorLay carry unit twins in the fast gate

test/unit/sim mirrors DriveTick.cpp and CorridorLay.cpp; the gravity vector's sign, OffTheRoad
at surface loss, and the corridor product's derived numbers each have a check that would catch
the regression in the fast gate, not in a sporadic driver suite. run.sh's src/sim group
compiles them.


---

Closed: unit/sim now compiles src/sim ENTIRE (plus ground/data/scene it stands on) and links in
the fast gate. Proving tests: test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld -- the tick
drives to arrival on earth AND moon gravity on a synthetic corridor (the grounded arrival is the
gravity-sign net), and a corridor whose edge is inside the track reports OffTheRoad loudly;
test/unit/sim/ALayRefusesASceneItCannotDrive covers Journey's head. CorridorLay's own numeric
twin remains with the 1624 issue if the reviewer holds it open.