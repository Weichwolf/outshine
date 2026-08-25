Type: issue
State: active
Area: clients, scenario
Tags: architecture, measured, driver, door
Supersedes: 1486, 1488, 1489, 1490, 1491, 1494, 1863

# The one door ADVANCES what it accepts

The drive is through the door: `Engine::Assemble` runs `AssembleDrive` when the scenario declares
one and refuses by name (src/clients/Engine.cpp:192), `Engine::Advance` ticks it (:715),
`Engine::Drove()` answers, and the driver links `-Iinclude` alone.

**The route search is the product's first content-level result, and it is a refusal.** Measured
2026-08-25 at 235e3f47 on the SHIPPED scenario -- `apps/driver/src/f31.scenario` now declares its
own drive, 48.13720,11.57560 -> 48.15000,11.59000, 1.78 km inside Munich:

```
turns the search refused as too sharp for the car = 34618 turns
REFUSED the network holds both ends but no chain of ways joins them --
18374 nodes of 45248 were reachable from the start, so this is a network in pieces
and not a search that gave up
```

41 % of a dense urban graph reachable from its own start node, over 30 contiguous z14 tiles.

**And the refusal names a cause it did not measure.** `reached` is counted INSIDE the search
loop (`src/actor/path/Wayfinding.cpp:538-541`), after the turn filter at `:560-576` has already
refused 34618 turns with `continue`. So the number proves only "unreachable UNDER THIS SEARCH";
it cannot distinguish a graph in pieces from a filter that cuts it into pieces. The two
candidates both have evidence: the snap is `0.627022 m` against a tile quantum of `0.597164 m`
(`src/sim/DriveAssembly.cpp:161,164`), and the same real node quantised in two adjacent tiles'
local grids can sit up to one full quantum apart per axis -- above the snap -- so every tile
boundary is a candidate cut; and the turn filter refuses 41 % as many turns as the graph has
edges. Until the components are counted on the UNFILTERED graph, the sentence "this is a network
in pieces" is a guess wearing a measurement's clothes.

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
- [ ] Connected components are counted on the graph BEFORE any turn is filtered, and the
      refusal quotes that number. A diagnostic may name only what it measured.
- [ ] The tile ring joins across tile boundaries: the snap that makes two ways meet is derived
      from what the QUANTISATION can separate (two grids, two axes), not set just above one
      quantum. 18374 of 45248 is the measurement to beat.
- [ ] Every other row of the table is reached from `src/`, once, through the door — or refused
      by name at assembly.
- [ ] `apps/driver` with NO arguments writes ten stills of the ROAD and consecutive ones DIFFER.
      At 235e3f47 it writes one `refused.png`: a car on white, no ground, no sky, no shadow.
      A 400 m variant gets past the search and is refused by the corridor fit instead
      (board:1887).
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
