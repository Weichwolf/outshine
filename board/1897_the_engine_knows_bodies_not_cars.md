Type: feature
State: open
Area: core
Tags: architecture, physics, actor
Parent: 1953
Depends: 1896

# The engine knows bodies, forces and control -- never a car

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
2. `Sim::AssembleDrive` is reached through a registered assembler chosen by what the scenario
   declares, not by `if (!declared.Driven.Declared) return true;`
3. `Along()`/`Whole()` go behind `Scene()` (board:1896) and stop being door verbs
4. `Column<Vehicle>` and `Column<Drive>` become columns the drive assembler owns

Proving test when it lands: a scenario that declares a body with a thrust actuator and NO drive
at all stands, is placed by integration, and moves under a control command. Negative control:
the same scenario against an engine whose `Advance` places only `Drive.State.Body` -- the body
never moves.
