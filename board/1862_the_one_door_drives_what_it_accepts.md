Type: issue
State: active
Area: clients, scenario
Tags: architecture, measured, driver, door
Supersedes: 1486, 1488, 1489, 1490, 1491, 1494, 1863

# The one door ADVANCES what it accepts

The drive is through the door: `Engine::Assemble` runs `AssembleDrive` when the scenario declares
one and refuses by name (src/clients/Engine.cpp:170), `Engine::Advance` ticks it (:541),
`Engine::Drove()` answers, and the driver links `-Iinclude` alone.

**The route search is the product's first content-level result, and it is a refusal.** Measured
2026-08-25 at a3ebe3e0 over 48.137,11.576 -> 48.200,11.600, five kilometres inside Munich:

```
REFUSED the network holds both ends but no chain of ways joins them --
19406 nodes of 65615 were reachable from the start, so this is a network in pieces
and not a search that gave up
```

30 % of a dense urban graph reachable from its own start node. The ring does not join, and this
is a defect in the ring, not in the search — the search says so itself, with the count.

**Five declarations are still accepted and never advanced.** Each stands green in CURRENT and the
door does nothing with it; since `test/unit/` went, four of them have no caller in the tree at
all (board:1805):

| declared | it stands | what the door does with it |
|---|---|---|
| `Views` | `ViewBook` — one active view, clock scale, the ear | `Engine.cpp` includes no `Views.h`; the camera follows nothing |
| `Input` | `InputMap` + `InputPump` — bindings interned to ids | no SDL pump is wired; `InputPump` has no reference outside its own two files |
| `Volumes`/`Events` | `TriggerField` — enter · exit · dwell, allocation-free | nothing probes bodies against doors |
| `Tables` | `TableBook` — rows by first column, typed by column | no host reads one |
| `Sounds`/`Buses` | `BusGraph` — buses into buses, one master, falloff | no reference outside its own two files |

A declaration the engine ACCEPTS and does not execute is worse than one it refuses.

## What will be true

- [x] `Engine::Assemble` lays the DRIVE the scenario declares, or refuses it by name.
- [x] The ten stills are spaced by DISTANCE along the route, not by frame:
      `alongM * Stills >= (nextStill + 1) * routeM` (apps/driver/src/main.cpp:177).
- [ ] The corridor's tile ring joins, so a five-kilometre urban route exists to be driven. The
      count above is the measurement to beat.
- [ ] Every other row of the table is reached from `src/`, once, through the door — or refused
      by name at assembly.
- [ ] `apps/driver --from ... --to ...` writes ten stills of the ROAD and consecutive ones
      DIFFER. At a3ebe3e0 it writes one `refused.png`: a car on white, no ground, no sky, no
      shadow.
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
