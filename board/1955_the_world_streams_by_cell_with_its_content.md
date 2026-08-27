Type: feature
State: active
Parent: 1953
Area: world

# The world streams by cell, with its content

**Benchmark** — Unreal: World Partition — cell grid, data layers, HLOD. RAGE: map nodes, IMAP/ITYP, LOD hierarchy. **Both agree** — a non-resident cell is represented COARSER, never by nothing, and the horizon is the proof.

A cell brings its ground, its structures and its actors in and out TOGETHER, and nothing in the
tree holds the whole of anything.

**Both benchmarks are built around this rather than having added it.** Unreal's World Partition
replaced the hand-placed level and made the cell grid the world's own shape; RAGE streams map
nodes and an `fwEntity` belongs to the node it stands in. Neither has a code path that assumes the
world is resident, which is why neither has one to repair.

CURRENT streams ground tiles and nothing else: structures and actors are stood up whole. That is
the assumption both engines do not make, and it is cheaper to remove before the actor chain is
built on top of it than after.

- [ ] a non-resident cell is represented by a COARSER proxy and never by nothing (Unreal HLOD,
      RAGE LOD hierarchy). **BUILT, AND DELIBERATELY NOT TICKED.** `GroundStream` now keeps a
      coarse level at `Z - 3` -- one coarse tile covers 64 fine ones -- filled whenever a fine
      tile is built, so eviction leaves a proxy behind rather than a hole. `Resident()` falls
      back to it and the sample carries `CoarseBy()`, because a coarse value passing itself off
      as fine is a silent approximation and this tree makes failure loud.
      The capability was already half present and unreachable: `TilePool::Mesh(int z, ...)` and
      `GroundStream::BlockAt(int z, ...)` both carry a zoom, and the web-mercator tile pyramid
      IS the LOD chain -- what was missing was only the fallback.
      **It cannot be ticked because it cannot be exercised**, and the reason is the one below:
      the fallback fires after an EVICTION, and no route this tree can drive evicts anything.
      Two paths now hang on that single cause.
- [ ] a cell carries ground, structures and actors and they arrive and leave together
- [x] no resident term scales with the world's extent, BELOW THE BUDGET: 3.35 times the route
      costs 1.012 times the memory (871 m holds 58278432 bytes, 2916 m holds 58959040).
      proof: outshine/door/ScoreWhatAWiderWorldHolds
- [ ] **THE EVICTION PATH HAS NEVER RUN.** The tile pool's byte budget is 64 MB
      (`TerrainLoader.cpp:28`) and the longest route this tree can drive holds 58, so the loop
      that drops a victim is dead in practice -- disabling it entirely leaves the case above green.
      The limit is not the budget but the ROUTE: the connected road graph around the declared
      start refuses a destination much beyond three kilometres ("no chain of ways joins the two
      ends -- 57780 nodes of 59697 are joined to the start"). So streaming is unproven where it
      matters, and proving it needs a world wide enough to exceed the budget rather than a smaller
      budget, which would prove the guard rather than the streaming.
      **The headroom has since narrowed and the number is worth carrying**: the coarse level
      above costs about 2.7 MB, so the same two routes now hold 60729536 and 62383200 bytes
      against the 64 MB budget -- 1.6 MB of headroom where there were 5. The ratio the case
      checks moved from 1.012 to 1.027 and stands. Widening `kCoarseSlots` would cross the
      budget and fire the eviction path for the first time, and that is NOT the way to prove
      it: spending memory to reach a guard proves the guard.
- [ ] `apps/driver` drives out of the declared extent and the picture does not break
- [ ] a cell grows OFF the frame thread, and it does so on `Base::Graph` -- the task graph with
      explicit dependencies that TARGET takes because RAGE and Unreal agree on it. `RegionForge`
      was a bespoke thread and condition variable doing this for one caller and reachable from
      none; it was deleted with board:1975 rather than wired, because a dedicated thread beside a
      task graph is the shape TARGET already rejected
