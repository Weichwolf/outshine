Type: feature
Area: world
Tags: perf, instrument

**0.4 Arrival without a stall**

- [x] Drawable only one pass after collection, so an upload is submitted before anything references it (`world/World.h`, `World::Ready`)
- [x] A rung the stream refuses retracts the split, so the coarser rung carries the area and the load stops waiting for a tile that will never come (`world/World.h` `MeshState::Vacant` and `World::Splits`, `world/TilePool.cpp` `Poll`). The refusal is held on the parent and not read off the children, because a vacant child carries no mesh and is evicted for being untouched — which re-opens the split; measured at 26 328 evictions against 3 789 builds in 5 min before the refusal moved up. The climb is one rung per pass and terminates at the root ring by construction. Coverage is kept, detail is dropped over the parent's whole quadrant: 11.7 → 23.5 m posting, invisible at 320×180 against a same-binary control whose own floor is 0.002 mean |ΔRGB|
- [ ] A tile becomes visible when its whole residency set is complete — mesh, class, vectors, footprints, water — never one layer at a time. Verified for terrain, unverified for building and water fields
- [ ] Upload per frame as a declared **byte** budget — `World::kMeshBuildsPerPass` (`world/World.h`) admits two items of unbounded size
- [ ] Arrival batched into few large writes: every WebGPU call is validated, so N small buffer writes cost N validations; the batched form is one staging write per frame feeding one indirect draw list
- [ ] No hitch on stream-in, proven on a moving capture: p99 across a 500 m walk against the neighbourhood before each arrival, never against the run mean
