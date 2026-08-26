Type: feature
State: open
Parent: 1953
Area: door, sim

# The door carries the laws, so a client can build a subject

The vehicle belongs in `apps/driver`; the engine is physics. Measured, that move is blocked by the
door, and the door is therefore the finding rather than the client.

**`include/Outshine.h` carries no physics verb at all.** Thirty-odd entries: canvas, scenario,
render, capture, park, restore, advance. Not one of them declares a body, applies a force, reads a
body's state, or steps the solver. `Advance()` runs the whole world internally and hands back
nothing a client could steer.

So `src/sim/` -- 1879 lines, and the vehicle words tell you which half is which --

    Rigging.cpp        44 vehicle words     wheelbase, axles, steer lock, drive and brake envelope
    DriveTick.cpp      26                   the tick that drives a car along a corridor
    DriveAssembly.cpp  15                   assembling a drive from a declaration
    CorridorLay.cpp    11                   fitting a corridor to a car's width
    GroundUnderfoot     0                   what a wheel stands on: the LAW side, and it stays

-- cannot leave the engine, because nothing outside it can do what it does. `Engine.cpp` is the
only user of `Sim::`, which makes the coupling small and the capability unreachable at once.

**Both benchmarks put the laws in the engine and the subject above it, and both make the laws
REACHABLE.** RAGE's game layer builds `CVehicle` on `phInst`, `phBound` and `phConstraint`, which
the physics library exposes. Unreal's ChaosVehicles plugin builds on `FBodyInstance`,
`FConstraintInstance` and the Chaos solver, which the engine module exposes. Neither hides the
solver and then keeps the car inside to reach it.

- [ ] the door declares a body with mass, inertia and a pose, and hands back a handle
- [ ] the door applies a wrench at a point and reads a body's state back
- [ ] the door steps the solver, and the fixed step of board:1959 is what it steps
- [ ] `apps/driver` builds its vehicle through those verbs and `src/sim`'s vehicle half is gone
- [ ] outshine may still SHIP a raycast-and-spring wheel -- as a declared assembly in the
      catalogue, which is where RAGE and Unreal keep theirs, never as an engine type
