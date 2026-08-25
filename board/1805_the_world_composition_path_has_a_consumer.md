Type: issue
State: open
Area: generators, render, clients
Tags: architecture, measured
Supersedes: 1537

# The world composition path has a consumer

CURRENT draws `Ground -> Forest & Buildings & Water & Infrastructure -> DrawList -> SubjectDraw`
and colours all of it green — right responsibility, right layer. **Nothing outside `src/` walks
it.** Measured over the tree, files reaching each node from `tools/` + `apps/`: `Forest` 0,
`Buildings` 0, `WaterField` 0, `Infrastructure` 0, `DrawList` 0, `RegionForge` 0. Their only
path to a client runs through `Sim`, which `grep -rln '"Sim.h"'` finds in one src file and one
test: 798 lines of facade with no consumer.

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
- [ ] `Sim` dies with the path it was the only door to.
- [ ] Proving test: a still from a declared world taken through the composition path, in which
      each declared field is present and named.
