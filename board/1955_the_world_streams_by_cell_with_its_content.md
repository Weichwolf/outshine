Type: feature
State: active
Parent: 1953
Area: world

# The world streams by cell, with its content

A cell brings its ground, its structures and its actors in and out TOGETHER, and nothing in the
tree holds the whole of anything.

**Both benchmarks are built around this rather than having added it.** Unreal's World Partition
replaced the hand-placed level and made the cell grid the world's own shape; RAGE streams map
nodes and an `fwEntity` belongs to the node it stands in. Neither has a code path that assumes the
world is resident, which is why neither has one to repair.

CURRENT streams ground tiles and nothing else: structures and actors are stood up whole. That is
the assumption both engines do not make, and it is cheaper to remove before the actor chain is
built on top of it than after.

- [ ] a non-resident cell is represented by a COARSER proxy and never by nothing (Unreal HLOD, RAGE LOD hierarchy)
- [ ] a cell carries ground, structures and actors and they arrive and leave together
- [x] no resident term scales with the world's extent, BELOW THE BUDGET: 3.35 times the route
      costs 1.012 times the memory (871 m holds 58278432 bytes, 2916 m holds 58959040).
      proof: harness/outshine/door/ScoreWhatAWiderWorldHolds
- [ ] **THE EVICTION PATH HAS NEVER RUN.** The tile pool's byte budget is 64 MB
      (`TerrainLoader.cpp:28`) and the longest route this tree can drive holds 58, so the loop
      that drops a victim is dead in practice -- disabling it entirely leaves the case above green.
      The limit is not the budget but the ROUTE: the connected road graph around the declared
      start refuses a destination much beyond three kilometres ("no chain of ways joins the two
      ends -- 57780 nodes of 59697 are joined to the start"). So streaming is unproven where it
      matters, and proving it needs a world wide enough to exceed the budget rather than a smaller
      budget, which would prove the guard rather than the streaming.
- [ ] `apps/driver` drives out of the declared extent and the picture does not break
