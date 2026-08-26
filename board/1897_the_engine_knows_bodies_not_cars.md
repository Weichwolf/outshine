Type: feature
State: open
Area: core
Tags: architecture, physics, actor
Parent: 1953
Depends: 1896

# The engine knows bodies, forces and control -- never a car

**FIRST TWO FIELDS GONE, AND THEY WERE THE REDUNDANT ONES.** `wheelbaseM` and `trackM` stood in
`struct Vehicle` beside the four contacts that already fixed them: contacts at z = -1.405 and
+1.405 ARE a 2.810 m wheelbase, contacts at x = +-0.774 ARE a 1.548 m track. `Rigging` derived the
wheelbase, derived the track when it was not declared, and REFUSED when a declared wheelbase
disagreed by more than a millimetre -- a guarded second spelling, which is still a second spelling.
Both are deleted; the derivation is now the only place either number comes from, and the guard went
with the thing it guarded (the compiler said so by way of an unused `kContactResolutionM`).

The drive is unchanged to the digit: 10.5115 / 522.756 / 5.31713 before and after.
      proof: harness/outshine/physics/ScoreWhatSetsTheSteeringSpan

**THE TYRE MOVED TO THE CONTACT IT BELONGS TO.** `<tyre grip radiusM corneringNPerRad
relaxationM/>` sat beside `<contact>` and applied to all of them -- a vehicle noun twice over: it
names a car's part, and it assumes a car's symmetry. `Rigging` already disagreed, copying those
four numbers into `mount.Sheds` PER MOUNT, so the physics had put them on the contact before the
declaration did. They are `Contact` fields now and the reader takes them off `<contact>`.
      proof: harness/outshine/physics/ScoreWhatATyreBelongsTo

What the move buys is what proves it: a body can carry different rubber front and rear -- a
staggered set, a space-saver, a worn axle -- which one tyre per vehicle cannot express at all.

**DRIVE, BRAKE AND STEER ARE ACTUATORS NOW, WHICH IS THE WORD TARGET USES.**
`PeakTorqueNm`, `FinalDrive`, `BrakeTorqueNm` and `TurningCircleM` were fields of a vehicle, and
`src/engine/Assembly.cpp:200` INFERRED the body's capabilities from whether they were non-zero.
A capability guessed from a magnitude is wrong at exactly one place -- a body that HAS a drive and
currently delivers nothing -- and that place has a name: a dead engine, a disconnected motor, a
drivetrain a scenario means to build up. `std::vector<Actuator>` with a three-name catalogue
replaces them, the numbers are each actuator's strength, and the reader refuses a fourth name.
      proof: harness/outshine/physics/ScoreWhatABodyDeclaresItCanDo

**AND THE WORST OF IT IS IN THE DOOR.** `include/Scenario.h` carries `struct Vehicle` with 25
car-specific fields -- `TyreRadiusM`, `FinalDrive`, `BrakeTorqueNm`, `TurningCircleM`, `TrackM`,
`CorneringNPerRad`, `AssetWheelbase`. CLAUDE.md's own sentence is unambiguous: *a vehicle noun
inside the engine core is a finding wherever it stands*, and a public header is the furthest
inside it can stand, because what is public is what nobody can change later. RAGE keeps `CVehicle`
in the GAME layer above `fwEntity`; Unreal keeps wheeled movement in a plugin outside the engine
module. Neither puts a tyre radius in the engine's own interface.

An engine is an interactive physics simulation with a focus on graphics. It knows bodies,
forces, actuators and control. A control command comes from the player through bindings or from
a mind; it activates a force, and only integration places a body. RAGE keeps `CVehicle` in the
game layer, Unreal keeps wheeled movement in a plugin. outshine's engine core keeps it inline.

Measured at HEAD, `src/engine/Engine.cpp` alone:

| noun | count | where it bites |
|---|---|---|
| `Drive` | 29 | `#include "DriveAssembly.h"` (:24), `Column<Drive> Drives` (:104), `Sim::DriveProduct Drive` (:119), `Sim::AssembleDrive(...)` (:212) |
| `Vehicle` | 6 | `Column<Vehicle> Vehicles` (:103), `note(scenario.Vehicles.size(), "vehicles")` (:162) |
| `Rides` / `Routes` | 7 | `Engine::State::Rides` and `Engine::State::Routes` -- moved off the header, still in the engine |
| `Wheel` | 3 | `constexpr double kWheelStepPx = 48.0` (:74) -- this one is a mouse wheel, not a road wheel |
| `Corridor` | 1 | `const Sim::Corridor &way = Drive.Way;` (:280) |

Moving `Rides`/`Routes` into `Engine::State` (this session) took them off the door and changed
nothing structural: the engine still assembles a drive, still reads a `DriveProduct`, still
places one body from one route.

What TARGET says instead: the scenario declares a body with actuators; a controller -- a mind or
a binding -- acts on the actuators; forces meet the ground at the contacts; integration places
the body. A drive is then ONE assembler among several (walk, fly, rail) selected from the
catalogue, and the engine holds the list, not the call.

The order this is done in:

1. `Engine::State::Rides` becomes "place every body that moved" and reads a body pose, not
   `Drive.State.Body`
2. DONE -- the declaration names its MODE and a table answers it. `Scenario::Drive` was a route wearing a
   vehicle verb; it is `Journey` with a `Travels` of walk, drive, fly or rail, and the
   pathfinder's own `Route` -- the corridor that comes BACK -- keeps its name, because a declared
   intent and a computed corridor are two things. A mode nothing assembles is REFUSED by name
   instead of standing still and looking like a scenario that declared no journey.
   proof: harness/outshine/door/ScoreWhatCodeCanDeclare
   DONE -- the assembler is a TABLE the mode indexes: four rows, one filled, each carrying the
   name of the way it travels. Adding walking is filling a slot, and `Routes` names the empty one
   it was asked for rather than testing a boolean.
3. DONE -- `Along()` and `Whole()` are gone from the door. They were drive-specific verbs that a
   client had to call to learn a route's progress, so the door promised a JOURNEY to anyone who
   read it. They are measures now, on the return channel `include/Event.h` already provides:
   `how long the corridor is` and `how far along it the body has come`. `apps/driver` reads them
   like any other number and lost nothing.
4. `Column<Vehicle>` and `Column<Drive>` become columns the drive assembler owns

Proving test when it lands: a scenario that declares a body with a thrust actuator and NO drive
at all stands, is placed by integration, and moves under a control command. Negative control:
the same scenario against an engine whose `Advance` places only `Drive.State.Body` -- the body
never moves.
