Type: issue
State: active
Area: clients, scenario
Tags: architecture, measured, driver, door
Supersedes: 1486, 1488, 1489, 1490, 1491, 1494, 1863

# The one door ADVANCES what it accepts

`Engine::Assemble` accepts `Scenario::Driven` and never drives it: `AssembleDrive`,
`CorridorLay` and `DriveTick` are not on the path `Engine` walks — every call site of
`AssembleDrive` in the tree is a test. `include/outshine/Scenario.h` declares `Drive Driven;`,
`ScenarioRead` parses it, `Declare` stores it, `Assemble` (src/clients/Engine.cpp) walks past
it, and `apps/driver/src/main.cpp` prints `DRIVING lat,lon -> lat,lon` and renders a studio
orbit of a car on a white background.

**It is not only the drive.** Six subsystems stand green in CURRENT because their own unit
proves them, and the door advances none of them:

| declared | it stands | what the door does with it |
|---|---|---|
| `Drive` | `AssembleDrive` · `CorridorLay` · `DriveTick` | nothing |
| `Views` | `ViewBook` — one active view, clock scale, the ear | `ClockScale()` never multiplies into `Advance`; the camera follows nothing |
| `Input` | `InputMap` + `InputPump` — bindings interned to ids | no SDL pump is wired; no key reaches an action |
| `Volumes`/`Events` | `TriggerField` — enter · exit · dwell, allocation-free | nothing probes bodies against doors |
| `Tables` | `TableBook` — rows by first column, typed by column | no host reads one |
| `Sounds`/`Buses` | `BusGraph` — buses into buses, one master, falloff | the frame hands it no positions |

A declaration the engine ACCEPTS and does not execute is worse than one it refuses. This is
board:1805's defect one layer up: the tree's best subsystems are wired together by test files,
and the one program a user runs reaches none of them.

## What will be true

- [ ] `Engine::Assemble` lays what the scenario declares and `Engine::Advance` ticks it — every
      row above reached from `src/`, once, through the door — or the door REFUSES that row by
      name at assembly. Silence is the one answer that is not allowed.
- [ ] `apps/driver/src/main.cpp --from ... --to ...` writes a still of the ROAD, and two
      consecutive stills DIFFER because the car moved.
- [ ] `test/run.sh --drive` spaces its ten stills by DISTANCE along the route rather than by
      frame — the same thing only while nothing moves.
- [ ] Three library functions the deleted driver tests held are library code with a unit twin
      each: `Lie` (the ground under the corridor — board:1805), `Standing` (a body's world
      matrix) and `Seen` (the chase eye, which was written twice and already diverging).
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
