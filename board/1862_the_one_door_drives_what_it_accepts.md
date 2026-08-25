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
Measured 2026-08-25 at d4c8784c on the SHIPPED scenario, unchanged from last round:

```
turns the search refused as too sharp for the car = 34618 turns
REFUSED the network holds both ends but no chain of ways joins them --
18374 nodes of 45248 were reachable from the start
NO DRIVE -- the picture is what stood without it
```

A 700 m hop up one street (`--from 48.14980,11.58680 --to 48.15600,11.58760`) refuses the same
way: 16783 of 46025. The endpoints are not the problem; the graph is.

**The turn filter is NOT the main cause, and that is now measured rather than argued.** The
filter at `src/actor/path/Wayfinding.cpp:555-579` refuses a transition when
`0.5 * shorter / tan(turn/2) < tightestM`, where `tightestM = sqrt((circle/2)^2 - wheelbase^2)`.
Running the SHIPPED scenario with `turningCircleM` lowered from 11.3 m to 5.65 m drops
`tightestM` from **4.90 m to 0.29 m**, a factor of 17 — a car that can turn on the spot:

| tightestM | reachable | share |
|---|---|---|
| 4.90 m | 18374 / 45248 | 40.61 % |
| 0.29 m | 22082 / 45248 | 48.80 % |

**8.19 percentage points.** Everything the turn filter can possibly explain is those eight
points; the other 51 % of the graph is unreachable for a reason nobody has measured. The snap is
the remaining candidate and it has evidence: `0.627022 m` against a tile quantum of `0.597164 m`
(`src/sim/DriveAssembly.cpp:161,164`), while the same real node quantised in two adjacent tiles'
local grids can sit `sqrt(2) * 0.597 = 0.845 m` apart — above the snap on the diagonal, so every
tile boundary is a candidate cut. **Count the components on the unfiltered graph before writing
another line of filter.**

**Of the five declarations the door accepted and never advanced, three now act.**

| declared | it stands | what the door does with it, at d5a562cd |
|---|---|---|
| `Views` | `ViewBook` | **advanced** — `Engine::Rides` (Engine.cpp:820-822) takes the active view every tick and the camera follows it |
| `Input` | `InputMap` + `InputPump` | **one binding of five acts.** The pump translates (`:405-412`), and then `Engine::Acts` (`:779`) is `if (named != "next-view" ...)` — a literal, in the engine. `throttle`, `brake`, `steer-left`, `steer-right` translate to an id, are un-interned back to a `std::string`, compared against that literal and dropped. A key does not move the car (board:1803) |
| `Volumes`/`Events` | `TriggerField` | **probed and never fires** — board:1891 has the measurement |
| `Tables` | `TableBook` | no host reads one |
| `Sounds`/`Buses` | `BusGraph` | no reference outside its own two files |

A declaration the engine ACCEPTS and does not execute is worse than one it refuses. So is an
action the engine names itself: the effect of `next-view` belongs in the declaration, beside the
binding, not in a string compare inside the door.

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
      At d4c8784c it writes one `refused.png`: a car on white, no ground, no sky, no shadow.
      The 136 m `--from`/`--to` variant now routes and keeps EIGHT stills, all eight distinct —
      so the corridor fit is no longer what blocks it (board:1887 closed). The search is.
- [ ] An ACTION carries its effect in the declaration. `Engine::Acts` names no action of its own.
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to a studio orbit and the case goes red.
