Type: issue
State: open
Area: generators, render, clients
Tags: architecture, measured
Supersedes: 1537

# The world composition path has a consumer

CURRENT draws `Ground -> Forest & Buildings & Water & Infrastructure -> DrawList -> SubjectDraw`
and colours all of it green — right responsibility, right layer. **Nothing outside `src/` walks
it, and since the cut nothing at all does.**

Measured 2026-08-25 at 235e3f47: `grep -rn '"Sim.h"' src apps test include` finds **one line**,
`src/clients/Sim.cpp:1`, the facade's own implementation. Last round it found one test as well;
that test went with `test/unit/`. 798 lines with ZERO consumers, and `Forest`, `Buildings`,
`Water`, `Infrastructure` and `RegionForge` hang off it.

**The consumer now EXISTS in the door and NOTHING calls it.** `bool Engine::Compose(void) {`
(src/clients/Engine.cpp:247) opens the ground stack, lays a 3x3 tile ring through
`const auto laid = LayPatchwork(S_->Stack.Pool(), over);` (:280) and restands the subject with
the ground appended. Measured 2026-08-25 at 235e3f47:
`grep -rn Compose src include apps test --include=*.cpp --include=*.h` finds **six lines and not
one of them a call**: the definition, its declaration at `include/Outshine.h:54`, and four
matches of the unrelated `Live::Compose`. So the public door publishes a verb that lays the
world's ground and no picture this engine has ever taken carries a tile. That is why the
product's still is a car on white.

The same cut moved NINE more green nodes off the reached side of the map, because their only
caller was a unit case: `BusGraph` and `InputPump` now have no reference anywhere outside their
own two files; `InputMap`, `TriggerField` and `ViewBook` reach no further than their own folder
(`src/clients/Engine.cpp` includes none of `Views.h`, `Triggers.h`, `Tables.h`, `InputPump.h`);
`Markup`, `Stylesheet`, `Layout` and `Painting` reach the picture only through `Live`, which the
map colours red. The cut did not break them — it revealed that nothing but a test ever called
them, which is this item's sentence one layer up.

That is why the one client that assembles a world picture built its own terrain grid, its own
far ring and its own road ribbon in its own C++ — and why the pictures it produced carried
defects the composition layer would have made unspellable: a lake cut out of the ground with no
water surface in it, a fine grid meeting the far ring with independently computed normals and
no shared vertex row, a ribbon edge on a 2 m polyline grading a 3 m post grid.

## What will be true

- [ ] A scenario DECLARES a world — the sphere, its ground, and which surface fields it wants
      drawn — and the engine composes them: terrain, ring, ribbon, forest, buildings, water,
      infrastructure, in one draw list, through one path.
- [ ] Terrain and water arrive together (a cut without its fill is a hole, and something is
      always drawn), and two grids that meet share their boundary posts.
- [ ] `apps/driver` loses its geometry construction to the library; the line count is published
      before and after.
- [ ] `apps/driver` calls the composition through the door — one still with a tile in it is the
      whole of the first step, and `Engine::Compose` is already written.
- [ ] `Sim` dies with the path it was the only door to.
- [ ] The nine nodes the cut stranded are reached from the door or deleted: a subsystem whose
      only caller was a test is a subsystem the product does not have.
- [ ] Proving test: a still from a declared world taken through the composition path, in which
      each declared field is present and named.
