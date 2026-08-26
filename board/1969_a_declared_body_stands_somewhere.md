Type: bug
State: open
Parent: 1953
Depends: 1897

# A declared body stands somewhere

`Scenario::Body` carries mass, centre of mass, inertia, aerodynamics, contacts, actuators and
slots -- everything about WHAT it is and nothing about WHERE it is. A body gets its first position
from one place only: `src/sim/DriveAssembly.cpp:303`, which computes it from the route's start.

    body.PositionM[0] = start.EastM - sin(start.HeadingRad) * startAsideM;
    body.PositionM[1] = under0.HeightM + stood.CentreM[1];
    body.PositionM[2] = -(start.NorthM + cos(start.HeadingRad) * startAsideM);

So a body that declares no drive has nowhere to be, and `Engine::State::Routes` says so by
returning early on `if (!declared.Driven.Declared)`. **A body without a journey cannot stand**,
which is a car's assumption wearing an engine's clothes: a crate, a fallen tree, a parked lorry and
a swinging door all have a place and no route.

The pieces exist and do not meet. `Instance` carries `TranslationM` and `RotationXyzw` and belongs
to a `Kind`; `Kind` carries an `Asset`, which is glTF, and no body. So the declaration has a scene
half with placements and a physics half without, and nothing joins them.

**Both benchmarks join them at the actor.** Unreal's `AActor` has a transform and its
`UPrimitiveComponent` carries the `FBodyInstance`; there is one placement and the physics hangs off
it. RAGE's `fwEntity` has a matrix and its `phInst` binds the physics to that entity. Neither has a
body that can only be placed by being sent somewhere.

This blocks board:1897's own proving test -- *a body with a thrust actuator and NO drive stands, is
placed by integration, and moves under a control command* -- which cannot be written while a body
without a drive has no start.

- [ ] a declared body carries its own placement, or an instance carries its body
- [ ] a body with no drive stands, falls under gravity and is placed by integration
- [ ] `Engine::State::Rides` places a body it is given rather than reaching into `Drive.State.Body`
