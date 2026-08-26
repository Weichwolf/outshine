Type: feature
State: open
Progress: streaming
Area: world
Tags: benchmark, target

# What the generators build reaches the scene a scenario declares

Both benchmarks are world-streaming engines first and everything else second: Unreal's World
Partition streams cells with their actors, RAGE streams its map by node. outshine has the
generators and no path from them to a picture.

Measured: `src/world/generators/` plus `draw/`, `RegionForge` and `Clients::Sim` are 6528 of the
tree's 49769 lines -- buildings, tree growers with leaf-angle distributions, forests, roof
surfaces, water, infrastructure. `RegionForge` is held only by `Clients::Sim`, and
`grep -rn '"Sim.h"'` finds exactly ONE line: its own `.cpp`. Nothing else in the tree includes it.

- [ ] `Clients::Sim` has a consumer, or its capability moves to something that does (board:1805)
- [ ] `Stage::Terrain`, `Stage::Buildings` and `Stage::Water` execute, so what a generator makes
      can land in a pass (board:1805)
- [ ] a generator is a LIBRARY emitting the representation the compositor consumes, rather than
      a program with its own loop (board:1197)
- [ ] a building, a tree and a water surface stand beside the road in the still (board:1936)
- [ ] the world composed for a drive and the world composed for a viewer are ONE path
      (board:1805)
- [ ] a route crosses a continent over a graph that STREAMS rather than one held whole
      (board:1503)
- [ ] cut and fill meet the road, and the road is drawn (board:1505)
