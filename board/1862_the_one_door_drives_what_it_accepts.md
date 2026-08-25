Type: issue
State: active
Area: clients, scenario
Tags: architecture, measured, driver, door
Supersedes: 1486, 1488, 1489, 1490, 1491, 1494, 1863

# The one door ADVANCES what it accepts

The drive is through the door: `Engine::Assemble` runs `AssembleDrive` when the scenario declares
one and refuses by name (src/clients/Engine.cpp:192), `Engine::Advance` ticks it (:715),
`Engine::Drove()` answers, and the driver links `-Iinclude` alone.

**The route search is the product's first content-level result, and it is STILL a refusal.**
Measured 2026-08-25 at 817ea333 on the SHIPPED scenario, after four commits of graph surgery:

```
nodes after snapping = 45248 nodes        junctions among them = 9147 nodes
loose ends tied onto an edge they end on = 2450 ends
pieces the graph falls into = 4193 pieces   nodes in the largest = 26807 nodes
pieces holding fewer than four nodes = 2455 pieces   nodes stranded in those = 5584 nodes
places two ways cross in plan without sharing a node = 6340 places
turns the search refused as too sharp for the car = 41196 turns
REFUSED no chain of ways joins the two ends -- 26853 of 45248 are joined to the start by ANY
edge, and the search settled 20158 of those, so what separates the ends is the graph itself
```

**The refusal now says what it measured and the graph counts its own pieces** -- that is this
hour's delivery and both boxes below are ticked for it. The axis moved 18374 -> 20158 settled,
40.6 % -> 44.6 %, and the shipped route still does not lay.

**What the piece census says the remaining work is.** 26807 of 45248 sit in ONE component and
5584 sit in pieces of three nodes or fewer; 6340 places have two ways crossing in plan without
sharing a node. That last number is the next candidate and it is not the snap: a crossing with
no shared node is either a junction the tile simplification dropped -- which the weld should
take -- or a bridge over a road, which must NOT be joined. The two cannot be told apart in plan,
and the tiles carry `KEY bridge` and `KEY tunnel` but NOT `layer` -- the 12 keys are printed on
every run and **`ways that declare a stacking layer = 0 ways`** is measured against them. So the
crossing census that separates a dropped junction from an overpass has `bridge` and `tunnel` to
work with and nothing else, and board:1894 has the defect the weld already carries.

**Of the five declarations the door accepted and never advanced, three now act.**

| declared | it stands | what the door does with it, at 817ea333 |
|---|---|---|
| `Views` | `ViewBook` | **advanced** — `Engine::Rides` (Engine.cpp:820-822) takes the active view every tick and the camera follows it |
| `Input` | `InputMap` + `InputPump` | **translated and handed to the client.** `Engine::Acts` is gone at 35829990; the pump translates (`:419-420`) and `Host::Calls` carries the declared name (`:428`), so the engine names no action of its own. `apps/driver` offers no host, so its four driving bindings still reach nothing (board:1803) |
| `Volumes`/`Events` | `TriggerField` | **probed and never fires** — board:1891 has the measurement |
| `Tables` | `TableBook` | no host reads one |
| `Sounds`/`Buses` | `BusGraph` | no reference outside its own two files |

A declaration the engine ACCEPTS and does not execute is worse than one it refuses.

## What will be true

- [x] `Engine::Assemble` lays the DRIVE the scenario declares, or refuses it by name.
- [ ] The ten stills are spaced by DISTANCE along the route, not by frame. The spacing is
      `alongM * Stills >= (nextStill + 1) * routeM` (apps/driver/src/main.cpp:196-198) and it
      is off by one at BOTH ends: the tenth still needs `alongM >= routeM`, which a drive that
      stops inside its arrival tolerance never reaches -- 302 m declared, 282 m driven, NINE
      stills -- and the first fires only at a tenth of the route, so the start of the drive is
      never pictured. Ten stills at `k/Stills` for `k = 0..Stills-1` picture the whole of it.
- [x] Connected components are counted on the graph BEFORE any turn is filtered, and the
      refusal quotes that number. `Network::Reaches` walks unfiltered; the census publishes
      pieces, the largest, and the nodes stranded in the small ones.
- [ ] The tile ring joins across tile boundaries: the snap that makes two ways meet is derived
      from what the QUANTISATION can separate (two grids, two axes), not set just above one
      quantum. 20158 settled of 45248 is the measurement to beat.
- [ ] Every other row of the table is reached from `src/`, once, through the door — or refused
      by name at assembly.
- [ ] `apps/driver` with NO arguments writes ten stills of the ROAD and consecutive ones DIFFER.
      At 817ea333 it writes one `refused.png`. A 302 m `--from`/`--to` variant routes and keeps
      NINE — nine, not ten, over a route the door was asked for ten of.
- [x] An ACTION carries its effect in the declaration. The engine names no action of its own.
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
