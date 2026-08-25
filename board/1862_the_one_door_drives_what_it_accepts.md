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
      DIFFER because the car moved. **Measured 2026-08-25 by the review**: at a9a96a0c the drive
      wrote 14 840 stills over 11 minutes, 1.4 GB, and all of them hash to the same
      `d2cd33750477d24f965adc5340f28f8a` — a car on white, no ground, no sky, no shadow. The
      07:39 run of the queue's own gate produced the same ten bytes-identical files. Nothing in
      the picture moves and nothing in it is a road.
- [ ] `test/run.sh --drive` spaces its ten stills by DISTANCE along the route rather than by
      frame. The cadence is not merely coarse, it is broken: `ofFrames` is
      `(long)engine.Frames()` (apps/driver/src/main.cpp:158), `Engine::Frames()` hands back
      `S_->Standing->Frames()` (src/clients/Engine.cpp:540) and `Live` defaults `int Frames_ = 1;`
      (src/clients/Live.h:170), so `frames * Stills >= (nextStill + 1) * ofFrames` (:165) is true
      on EVERY frame and the drive keeps one still per frame until it is killed.
- [ ] The corridor's tile ring joins: at 1af2c00b a 5 km Munich route refuses with *a network in
      pieces* — 20 576 of 64 334 nodes reachable from the start. That is the first content-level
      result the product has produced and it is a defect in the ring, not in the search.
- [ ] The gate is green with the drive in the door. It is not: `8 BUILD` at b7ffe736, seven of
      them the `render/outshine/client` suite, which stopped linking the day `Engine.cpp` took
      `Sim::AssembleDrive`, `Sim::DriveTick` and `Data::ShippedProviders` (board:1582).
- [ ] Three library functions the deleted driver tests held are library code with a unit twin
      each: `Lie` (the ground under the corridor — board:1805), `Standing` (a body's world
      matrix) and `Seen` (the chase eye, which was written twice and already diverging).
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
