Type: task
Parent: 1624
Area: sim

# DriveTick and CorridorLay carry unit twins in the fast gate

test/unit/sim mirrors DriveTick.cpp and CorridorLay.cpp; the gravity vector's sign, OffTheRoad
at surface loss, and the corridor product's derived numbers each have a check that would catch
the regression in the fast gate, not in a sporadic driver suite. run.sh's src/sim group
compiles them.
