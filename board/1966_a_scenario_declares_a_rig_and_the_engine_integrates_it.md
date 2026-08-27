Type: feature
State: open
Parent: 1953
Area: door, sim

# A scenario declares a RIG, and the engine integrates it

**Benchmark** — Unreal: a vehicle is a data asset of constraints and drives that the plugin integrates. RAGE: handling data in the game layer. **Both agree** — the rig is DECLARED and the engine integrates it.

**Filed once on a wrong premise and corrected here.** The first version read: the door carries no
physics verb, so a client cannot build a vehicle, so the door must gain `AddBody`, `ApplyWrench`
and `Step`. That is the wrong answer and it would have been a second door for one truth -- an
imperative path beside the declarative one, which is the shape this tree spends its hours deleting.

**The client should almost be able to do it already: glTF plus a RIG in the scenario.** Geometry
comes from the file; mechanics come from the declaration; the engine integrates. No imperative
physics API, because content is data and the engine is verbs.

Half of it already stands. `Scenario::Body` carries mass, centre of mass, inertia, aerodynamics,
contacts and slots -- a rigid body declared, which is exactly right. What it cannot say:

    a JOINT              two bodies, a kind, its degrees of freedom
    a DRIVE on a freedom a target, a limit, a ratio -- what Chaos and PhysX call a joint drive
    a body BESIDE a body a wheel is a body on a revolute joint, and there is only ever one body

So a car is declarable only as one rigid mass with four springs to the ground, and a door, a crane,
a suspension arm and a walker are not declarable at all.

**Both benchmarks declare their rigs rather than building them in code.** RAGE's vehicles come from
handling and layout data read at load; Unreal's Chaos vehicle is a component tree configured in the
editor, and its wheels are `UChaosVehicleWheel` ASSETS. Neither writes a vehicle as engine C++, and
neither exposes a raw solver API to do it either.

Once this stands, `apps/driver`'s vehicle is a declaration in `apps/driver/src/` rather than
`src/sim/`'s 1879 lines, of which `Rigging` alone carries 44 vehicle words. What stays in the
engine is what has none: `GroundUnderfoot`, the query for what a wheel stands on.

- [ ] a scenario declares a joint between two bodies, with its degrees of freedom
- [ ] a scenario declares a drive on a degree of freedom: target, limit, ratio
- [ ] the engine integrates a rig it did not have to be told the shape of, proven by a case that
      declares something that is not a car -- a door, or an arm -- and moves it
- [ ] `apps/driver`'s vehicle is declared, and the vehicle half of `src/sim` is gone
