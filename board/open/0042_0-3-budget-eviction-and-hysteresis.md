Type: feature
Area: world
Tags: perf, instrument

**0.3 Budget, eviction and hysteresis**

- [ ] A byte budget per pool declared in one place with its derivation beside it, and eviction against bytes — `kNodeCeil = 6000` (`world/World.cpp:42`) is a count of entries and stops splitting in silence
- [ ] The eviction policy is the reference's and it is neither LRU nor TTL: sort the streamables by importance descending, accumulate their content bytes, unload everything past the point where the sum crosses the budget (`ObjManStreaming.cpp:672-691`). It removes exactly the least important bytes, needs no per-entry timestamp, and costs one sort — which is why the sort is throttled to 10 Hz
- [ ] The eviction unit is one LOD rung, not a whole tile: what is far loses its fine rungs and keeps its coarse one, so evicting costs silhouette rather than coverage. The reference streams mips and mesh LODs and never whole assets
- [ ] A minimum residency in seconds and in metres of camera travel, never in frames — the reference holds a texture 10 s (`r_TexturesStreamingResidencyTime`) and a mesh record 8 priority rounds. `kGrace = 180` passes (`world/World.cpp:33`) is 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds
- [ ] Residency at the full radius rather than the frustum, with the reason on the line — the reference prioritises by visibility and never evicts by it, because a turn must not stall
- [ ] A measured re-fetch rate per tile with a published ceiling, so thrash is detected rather than assumed away — the reference measures it over a 5 s window (`r_TexturesStreamingResidencyTimeTestLimit`)
- [ ] The resident representation per tile declared and bounded — measured 1.56 MiB of device geometry per tile (202.5 MiB over 130) and 70.9 MiB of building heap over one block. Far Cry's answer to 64 MiB of video memory was a smaller resident form, not a cleverer cache
- [ ] The generated-texture cache under the same discipline as every other pool: a declared budget, an eviction unit of one mip of one texture, and a re-generation cost measured rather than assumed. Principle 2 makes a bake normal, which makes the cache load-bearing, and today it appears in no line and no column
- [ ] Shadow map memory declared and counted — four cascades exist with their bytes in no budget (`render/`), and they are device memory, which has no budget at all
- [x] A byte cache with a declared budget, exact LRU and counted evictions (`world/TilePool.cpp:230-254`; 64 MiB, `world/TerrainLoader.cpp:40`)
- [ ] The LRU victim found in O(1) rather than by linear scan under a held lock (`world/TilePool.cpp:236-240`, n ≈ 600 at 64 MiB of z14 tiles) — an intrusive list or a clock hand, per Gregory §6.2.2 on pool allocators, and the choice named on the line
- [x] An evicted tile node releases the collector's device slot, and the slot is recycled rather than freed (`world/World.cpp:534-547`, `clients/Outshine.cpp:309`, `render/stages/TerrainDraw.cpp:810`)
- [ ] Eviction for building prints and verts, water surfaces, courses and levels — the fields grow monotonically and their unit of removal does not exist (`world/BuildingField.h`, `world/WaterField.h`, `world/StreetField.h`); at 70.9 MiB it is the largest single consumer in the tree
- [ ] Every streamed pool a slab of uniform blocks or a ring, which is the argument that no defragmentation pass is needed — and the argument is owed, because both references defragment instead: CryEngine at 64 moves a frame with pin and unpin (`IDefragAllocator::DefragmentTick`, `r_buffer_pool_defrag_max_moves`), Guerrilla at the start of every frame with a 16 MiB copy cap, a three-phase move and a one-frame linger because a block may still be in use, concluding "expensive and complex, but almost no waste". Both defragment because their block sizes come from artists; ours come from us. Under wasm the break cannot be returned, so a slab is the only form in which fragmentation cannot accumulate at all
- [ ] A residency handle minted only by a pool that accepted its byte charge, so "allocated but in no budget" and "resident but not evictable" are unspellable rather than forbidden
