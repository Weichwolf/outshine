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

**The consumer exists, it is CALLED, and it refuses.** `Engine::Assemble` calls
`bool Engine::Compose(void) {` (src/clients/Engine.cpp:265) after it routes; Compose opens the
ground stack and lays the ring through `auto laid = LayPatchwork(S_->Stack.Pool(), over);`
(:297), giving the asynchronous pool the passes it needs. That is real and it is the movement
this item asked for.

**And no tile has reached a frame, because Compose refuses at :274 for every scenario in the
tree.** Measured 2026-08-25 at d5a562cd:

```
grep -rl '<ground' --include=*.scenario .     -> nothing
the ground did not compose: the scenario declares no sphere, so there is no ground to compose
```

The refusal is honest — board:1890 has the seam it names, the ring anchored on
`Patchwork::OriginEcef` while the vehicle stands on the corridor's origin, and a ground that
swallows the camera is worse than no ground. But the consequence for this item is unchanged:
`GroundPatchwork` is still stranded, the product's still is still a car on white, and the way
out is board:1890's one seam and not another caller.

**Six of the nine nodes the cut stranded are back.** `Markup`, `Stylesheet`, `Layout`,
`Painting`, `Typeface` and `Pointer` draw the viewer's own face — verified this round on
`frame003.png`, three faces at two sizes — and `ViewBook`, `InputMap`, `InputPump` and
`TriggerField` are stood and advanced by `Engine`. `BusGraph` alone still has no reference
outside its own two files. What the cut revealed was never a breakage: it was that nothing but
a test had ever called them, and this hour is what a real caller costs.

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
      only caller was a test is a subsystem the product does not have. Six are back; `BusGraph`,
      `Forest`, `Buildings`, `Water`, `Infrastructure` and `GroundPatchwork` are not.
- [ ] Proving test: a still from a declared world taken through the composition path, in which
      each declared field is present and named.
