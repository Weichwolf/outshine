Type: issue
Area: clients, sim
Tags: architecture, measured, driver, door

# The one door drives what it accepts

`2e779901` wrote the measurement into its own commit body, and it is the sharpest statement of
the distance the tree has produced:

> *Engine::Assemble and Advance accept Scenario::Driven and never drive it: AssembleDrive,
> CorridorLay and DriveTick are not on the path Engine walks. A declaration the engine ACCEPTS
> and does not execute is worse than one it refuses.*

Confirmed by walking the tree at `2e779901`:

```
$ grep -rn 'AssembleDrive' src --include=*.cpp --include=*.h
src/sim/DriveAssembly.h:  ... bool AssembleDrive(const Store &scene, const Assembled &cast,
src/sim/DriveAssembly.cpp: ... (the definition)
```

**Every call site of `AssembleDrive` in the tree is a TEST**: five under `apps/driver/test/`,
two under `test/unit/sim/`. Nothing in `src/` calls it, so nothing a user can run calls it.
`include/outshine/Scenario.h:323` declares `Drive Driven;`, `ScenarioRead` parses it, `Declare`
stores it, `Assemble` (src/clients/Engine.cpp:101) walks past it, and the driver's own entry
point -- `apps/driver/src/main.cpp`, which prints `DRIVING lat,lon -> lat,lon` before it does
so -- renders a studio orbit of a car on a white background.

The consequence for the map: `Engine` is recoloured green -> AMBER in the CURRENT class
structure this round. Its SHAPE is right and TARGET keeps it; what is in question is a door
whose `Assemble` silently drops the half of the declaration the product exists for.

This is board:1805's defect one layer up. There the world composition path (`Forest`,
`Buildings`, `Water`, `Infrastructure`, `DrawList`) has no consumer outside `src/`; here the
DRIVE has none inside `src/` at all. Both say the same thing: the tree's five best subsystems
are wired together by test files, and the one program a user runs reaches none of them.

## What will be true

- [ ] `Engine::Assemble` lays the drive the scenario declares -- `AssembleDrive` reached from
      `src/`, once, through the door -- or it REFUSES `Scenario::Driven` by name at assembly
      time. Silence is the one answer that is not allowed.
- [ ] `apps/driver/src/main.cpp --from ... --to ...` writes a still of the ROAD, not of a
      studio orbit, and a case runs the entry point and asserts that the frame it wrote is the
      one the stills case draws by hand.
- [ ] The 1281 lines of terrain, far ring and ribbon that `apps/driver/test/stills` and
      `apps/driver/test/window` build in their own C++ shrink by what the door now supplies,
      and the number is published before and after (board:1805 holds the same box for the
      composition path).
- [ ] Negative control: the drive removed from `Assemble` -> the entry point's frame goes back
      to the studio orbit and the new case goes red.

## Comments

- 2026-08-25, hourly review -- filed on the queue's own measurement rather than on the
  reviewer's, which is the best kind. The recolour of `Engine` is the review's, and it is what
  turns the hour's distance from 66 % to 65 %: this is a defect that was always there and that
  nobody could see until somebody wrote the program that walks the door.
