Type: feature
State: open
Area: core
Tags: architecture, physics, actor
Parent: 1953
Depends: 1896, 1965

# The engine knows bodies, forces and control -- never a car

**Benchmark** — Unreal: `FBodyInstance` in the engine, wheeled movement in a plugin outside it. RAGE: `phInst` and `phConstraint` in physics, `CVehicle` and `CWheel` in the game layer. **Both agree** — the engine names bodies, forces and control, never a car.

**FIRST TWO FIELDS GONE, AND THEY WERE THE REDUNDANT ONES.** `wheelbaseM` and `trackM` stood in
`struct Vehicle` beside the four contacts that already fixed them: contacts at z = -1.405 and
+1.405 ARE a 2.810 m wheelbase, contacts at x = +-0.774 ARE a 1.548 m track. `Rigging` derived the
wheelbase, derived the track when it was not declared, and REFUSED when a declared wheelbase
disagreed by more than a millimetre -- a guarded second spelling, which is still a second spelling.
Both are deleted; the derivation is now the only place either number comes from, and the guard went
with the thing it guarded (the compiler said so by way of an unused `kContactResolutionM`).

The drive is unchanged to the digit: 10.5115 / 522.756 / 5.31713 before and after.
      proof: outshine/physics/ScoreWhatSetsTheSteeringSpan

**THE TYRE MOVED TO THE CONTACT IT BELONGS TO.** `<tyre grip radiusM corneringNPerRad
relaxationM/>` sat beside `<contact>` and applied to all of them -- a vehicle noun twice over: it
names a car's part, and it assumes a car's symmetry. `Rigging` already disagreed, copying those
four numbers into `mount.Sheds` PER MOUNT, so the physics had put them on the contact before the
declaration did. They are `Contact` fields now and the reader takes them off `<contact>`.
      proof: outshine/physics/ScoreWhatATyreBelongsTo

What the move buys is what proves it: a body can carry different rubber front and rear -- a
staggered set, a space-saver, a worn axle -- which one tyre per vehicle cannot express at all.

**DRIVE, BRAKE AND STEER ARE ACTUATORS NOW, WHICH IS THE WORD TARGET USES.**
`PeakTorqueNm`, `FinalDrive`, `BrakeTorqueNm` and `TurningCircleM` were fields of a vehicle, and
`src/engine/Assembly.cpp:200` INFERRED the body's capabilities from whether they were non-zero.
A capability guessed from a magnitude is wrong at exactly one place -- a body that HAS a drive and
currently delivers nothing -- and that place has a name: a dead engine, a disconnected motor, a
drivetrain a scenario means to build up. `std::vector<Actuator>` with a three-name catalogue
replaces them, the numbers are each actuator's strength, and the reader refuses a fourth name.
      proof: outshine/physics/ScoreWhatABodyDeclaresItCanDo

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
   proof: outshine/door/ScoreWhatCodeCanDeclare
   DONE -- the assembler is a TABLE the mode indexes: four rows, one filled, each carrying the
   name of the way it travels. Adding walking is filling a slot, and `Routes` names the empty one
   it was asked for rather than testing a boolean.
3. DONE -- `Along()` and `Whole()` are gone from the door. They were drive-specific verbs that a
   client had to call to learn a route's progress, so the door promised a JOURNEY to anyone who
   read it. They are measures now, on the return channel `include/Event.h` already provides:
   `how long the corridor is` and `how far along it the body has come`. the driver client (deleted) reads them
   like any other number and lost nothing.
4. RE-READ AND REDIRECTED. A `Column` is opened AGAINST the store -- `Bodies.Open(Scene)` -- so it
   is the store's sidecar and not the assembler's; the engine holds handles to them and nothing
   more. What this point actually asks for is that they go behind `Scene()` with the store, which
   is board:1896's work, not a move into `Sim`. Recorded rather than forced into the shape the
   point was first written in.

   What DID come out of looking: the word `vehicle` was still in the code's own SPEECH -- "the
   vehicle declares no width", "a vehicle with no mass cannot be pushed", "the m this vehicle can
   bend to", and `note(scenario.Bodies.size(), "vehicles")`, which a user READS. `src/actor`,
   `src/sim`, `src/engine`, `src/render`, `src/scenario`, `src/content` and `include` now carry the
   word nowhere at all.

**THE WORD IS NOW COUNTED AND MAY ONLY FALL.** This item claimed that `src/actor`, `src/sim`,
`src/engine`, `src/render`, `src/scenario`, `src/content` and `include` carry `vehicle` nowhere
at all -- and nothing was checking, so it was a sentence rather than a fact. Worse, `vehicle` was
only one word. Measured across `src/` and `include/` with the generator tier exempt:

    Car 6 · Seat 16 · Door 13 · Steering 8 · Brake 4 · Throttle 2 · Tyre 0 · Wheel 0 ·
    Chassis 0 · Axle 0 · Pedal 0 · Walker 0

`harness/claims/TheEngineNamesNoSubject` declares those counts and refuses in BOTH directions:
a word that arrives is caught, and a word that leaves has to be recorded where it left. The
zeroes are the ones board:1897 already cleared, and they are held there.

**`src/generators/` is exempt and that is the point rather than a loophole.** A generator's job
is to MAKE one concrete thing, so a tree grower called `Tree` is named for what it produces and
could not honestly be called anything else. CLAUDE.md names them that way itself -- forest,
buildings, water, infrastructure -- and the tier links with none of the engine behind it, so a
subject noun there decides nothing about the laws. Unreal draws the same line: PCG sits outside
the physics and renderer modules that have to stay generic.

- [x] no subject noun GROWS in the engine, and every one that leaves is recorded where it left
      proof: harness/claims/TheEngineNamesNoSubject

**THE PROVING TEST WAITS ON board:1965, AND SAYS SO RATHER THAN BEING BUILT AROUND.** Two thirds
of it stand: board:1969 shows a body with no drive standing, integrated and placed. The last third
-- *moves under a control command* -- needs a FORCE actuator, and the catalogue is `{Torque,
Steer}`. Adding a thrust to that catalogue would be building on the model board:1965 replaces: an
actuator is not a thing beside a joint, it is a constraint with a target and a force limit, and a
thrust is EFFORT on a free body's translational degree of freedom. Building it twice is what
board:1953 forbids.

Proving test when it lands: a scenario that declares a body with a thrust actuator and NO drive
at all stands, is placed by integration, and moves under a control command. Negative control:
the same scenario against an engine whose `Advance` places only `Drive.State.Body` -- the body
never moves.


**FOLDED IN 1944**, which said the same in 29 lines: RAGE keeps `CVehicle` in the game layer, Unreal keeps wheeled movement in a plugin, so no vehicle noun stands inside the engine. Same benchmark, same claim, one item.
