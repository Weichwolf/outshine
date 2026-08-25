Type: issue
State: active
Area: clients, scenario
Tags: architecture, measured, driver, door
Supersedes: 1486, 1488, 1489, 1490, 1491, 1494, 1863

# The one door ADVANCES what it accepts

The drive is through: `Engine::Assemble` runs `AssembleDrive` when the scenario declares one and
REFUSES by name when no transport was handed over (src/clients/Engine.cpp:165), `Engine::Advance`
ticks it (:454), `Engine::Drove()` answers (:186), and the driver links `-Iinclude` alone. It
fetches 63 104 nodes, weaves them and reports *a network in pieces* — the first content-level
result the product has produced.

**Five more declarations are still accepted and never advanced.** Each stands green in CURRENT
because its own unit proves it, and the door does nothing with it:

| declared | it stands | what the door does with it |
|---|---|---|
| `Views` | `ViewBook` — one active view, clock scale, the ear | `ClockScale()` never multiplies into `Advance`; the camera follows nothing |
| `Input` | `InputMap` + `InputPump` — bindings interned to ids | no SDL pump is wired; no key reaches an action |
| `Volumes`/`Events` | `TriggerField` — enter · exit · dwell, allocation-free | nothing probes bodies against doors |
| `Tables` | `TableBook` — rows by first column, typed by column | no host reads one |
| `Sounds`/`Buses` | `BusGraph` — buses into buses, one master, falloff | the frame hands it no positions |

A declaration the engine ACCEPTS and does not execute is worse than one it refuses. This is
board:1805's defect one layer up: the tree's best subsystems are wired together by test files,
and the one program a user runs reaches them only where somebody walked the wire by hand.

## What will be true

- [x] `Engine::Assemble` lays the DRIVE the scenario declares, or refuses it by name. Silence is
      the one answer that is not allowed.
- [ ] Every other row above is reached from `src/`, once, through the door — or refused by name
      at assembly.
- [ ] `apps/driver --from ... --to ...` writes a still of the ROAD, and two consecutive stills
      DIFFER because the car moved.
- [ ] `test/run.sh --drive` spaces its ten stills by DISTANCE along the route rather than by
      frame — the same thing only while nothing moves.
- [ ] Three library functions the deleted driver tests held are library code with a unit twin
      each: `Lie` (the ground under the corridor — board:1805), `Standing` (a body's world
      matrix) and `Seen` (the chase eye, which was written twice and already diverging).
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
