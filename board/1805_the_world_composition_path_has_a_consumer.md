Type: issue
State: open
Area: generators, render, clients
Tags: architecture, measured
Supersedes: 1537

# The world composition path has a consumer

Four generators are complete, correct and unreachable: `Forest`, `Buildings`, `Water`,
`Infrastructure`. Nothing outside `src/` walks them, so no drive has ever seen a tree, a
building or a lake. This is the largest block of present-but-unreachable capability in the tree
and it is what the picture is missing.

## Measured at bb9472db

`grep -rn '"Sim.h"' src apps test include` finds **one line**, `src/engine/Sim.cpp:1` -- the
facade's own implementation. `src/engine/Sim.h:9,12,13,17,25` is the only place `Buildings.h`,
`Forest.h`, `GeneratorSet.h`, `Infrastructure.h` and `Water.h` are included from, so the whole
generator tier hangs off a class with zero consumers. STATE.md's STRANDED block counts it:
**37 of 151 sources reach no suite and 30 of them are `world/generators`.**

Nothing else in `src/engine`, `src/scenario`, `src/compositor` or `apps/` names a generator;
only `src/scenario/ScenarioRead.cpp` mentions the directory at all.

## What has been repaid

**The nailed-shut branch is gone.** `const bool overADrive = Drove && !way.Fine.empty();`
(`src/engine/Engine.cpp:304`) is a measurement now, the ring lays over a corridor's frame, and
the refusal at `:305` no longer denies what the same run proved. That was this item's loudest
paragraph for four rounds and it is finished.

**The UI half is finished.** `Markup`, `Stylesheet`, `Layout`, `Painting`, `Typeface`,
`Pointer`, `ViewBook`, `InputMap`, `InputPump` and `TriggerField` are all reached from the door.

## What stands between here and a tree on screen

`GeneratorSet` takes its inputs from `Ground::World` (`src/engine/Sim.h:201`), and the drive path
opens `Ground::GroundStack`, not `Ground::World`. The two are different worlds -- board:1924
found the same seam for the class field and moved `ClassField` onto the stack. A generator needs
the same move: its input is the stack's stream and class field, its output is a part in the
content store, and the compositor takes the handle. `Sim` is the wrapper that made this look like
one problem; it is the door that must die, not the generators behind it.

## AND THE STAGES THAT WOULD DRAW THEM ARE EMPTY ROWS

Even with a reachable generator there is nothing to execute. `Stage::Terrain`,
`Stage::Buildings` and `Stage::Water` stand in the catalogue with their resource edges declared
-- `RenderCatalogue.h:248`, `:253`, `:256`, each reading `ShadowAtlas`, `IrradianceBuffer` and
`CascadeUniform`, so by declaration the ground and the buildings RECEIVE shadow. Measured at
bb9472db:

    grep -rn 'Stage::Terrain|Stage::Buildings|Stage::Water' src --include=*.cpp --include=*.h

finds them ONLY in `RenderCatalogue.h`. No plan names them, no executor implements them, and
`src/render/shaders/` holds 25 files with no `terrain.msl`, `buildings.msl` or `water.msl` among
them. Three catalogue rows a scenario can select and nothing can run.

That is why the stakeholder's ledger reads *no building on a boulevard walled with them, no tree,
no water where the route crosses the Isar* (board:1936) and *does it cast a contact shadow? NO*
(board:1575) in the same column: the ground the car stands on is not a stage, so there is nothing
to receive.

## What will be true

- [ ] A scenario DECLARES which surface fields it wants drawn, and the engine composes them:
      terrain, ring, ribbon, forest, buildings, water, infrastructure, in ONE draw list.
- [ ] A generator reads the stack the drive already opened, and emits a part by handle. No
      generator names `Ground::World` or `Sim`.
- [ ] Terrain and water arrive together -- a cut without its fill is a hole, and something is
      always drawn -- and two grids that meet share their boundary posts.
- [ ] `Sim` dies with the path it was the only door to; STATE.md's stranded count falls from 37
      by the 30 that are generators.
- [ ] `BusGraph` is reached from the door or deleted: a subsystem whose only caller was a test is
      a subsystem the product does not have.
- [ ] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` execute or leave the catalogue: a
      row a scenario can select and nothing can run is a declaration surface with nothing behind
      it, and the audit that refuses an unbuilt stage at plan time already exists (board:1549).
- [ ] Proving test: a still from a declared world in which each declared field is present and
      named, and a scenario case that counts the parts each generator contributed.
