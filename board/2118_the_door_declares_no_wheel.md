# The door declares no wheel

State: open

board:2117 took the autopilot out of `src/`. The DOOR still carries the shape of the vehicle it
drove, and the parts are not all the same kind of thing:

| in `include/scenario/Scenario.h` | what it is | verdict |
|---|---|---|
| `Drive`, `Drives::Effort \| Motion` | the drive of a degree of freedom | **LAW.** CLAUDE.md's own vocabulary. Stays |
| `Prismatic` (`ReachM`, `StiffnessNPerM`, `DampingNsPerM`, `TravelM`, `StopNPerM`, `LimitN`) | a sliding joint with a spring, a damper and a stop | **LAW.** A prismatic joint is a joint, not a strut. Stays |
| `Slip` (`Grip`, `CorneringNPerRad`, `RelaxationM`, `LoadFalloff`) | a TYRE model -- Pacejka's shape, by its field names | a wheel. `TheEngineNamesNoSubject` exists to catch exactly this |
| `Contact` (`At`, `AtM`, `Prismatic Strut`, `Slip Touches`) | a strut plus a tyre at a point -- **a wheel**, spelled without the word | the same |
| `AssetAnimation::Driven` | an animation a drive plays | read before deciding: it may be the LAW's word |

**Nothing in `src/` reads `Contact`, `Slip` or `Prismatic` any more** -- measured 2026-09-04, after
the cut. So the door declares three structures the engine cannot act on, which is the failure
CLAUDE.md names outright: accepting a declaration and doing nothing with it is worse than refusing
it.

## The question this item has to answer, and it is not "delete or keep"

A scenario declaring a car is a SUBJECT assembling laws -- body, joint, drive, contact -- which is
exactly what CLAUDE.md says a scenario is for. So the door is allowed a contact point and a joint.
What it is NOT allowed is a TYRE MODEL, because `CorneringNPerRad` decides the shape of everything
downstream: a cornering stiffness implies a wheel, a wheel implies an axle, an axle implies a pair,
and a machine with one driven wheel or a track can no longer be declared at all.

So the cut is between `Prismatic` (a joint, stays) and `Slip` (a tyre, goes), and `Contact` has to
be re-read as "a body touches the world at a point" rather than "a wheel meets the road".

## What Unreal does, what RAGE does

Unreal puts wheeled movement in a PLUGIN (`ChaosVehicles`) and keeps `FBodyInstance` in the engine.
RAGE puts `CVehicle` in the game layer above `phInst`. **They agree**, so the matter is closed: the
tyre belongs outside, and the joint belongs inside.

## What will show I was wrong

`TheEngineNamesNoSubject` walks `include/` for subject nouns. If `Slip` and `Contact` are laws
rather than a wheel, that walk should be able to say so without an exemption written for them.
Today the claim is RED for an unrelated reason (`Kerb` measures 6 against a declared 5), which is
board:2093's to clear first.
