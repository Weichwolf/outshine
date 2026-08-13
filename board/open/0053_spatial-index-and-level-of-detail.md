Type: feature
Area: render
Tags: perf, instrument

**I.7 Spatial index and level of detail**

- [x] One quadtree over the sphere with a vertical extent per node; it answers *where* and owns nothing
- [ ] Vertical split only where content demands it
- [ ] Tile centre at the node's real ECEF origin everywhere — `World::Center` still puts it at `alt = 0`, so a pedestrian at altitude gets a coarser tile under his feet
- [ ] Split metric whose focal length is the projection's, and which is distance-free under an orthographic camera
- [x] One screen-space-error ladder for every instanced model (`core/ClusterDag.h`, `core/ModelLadder.h`)
- [x] Cluster DAG with model-space error per level
- [x] Impostor rung above the mesh levels, its error anchored on the atlas cell texel
- [ ] Measured screen-space error: render the chosen cut against the finest and difference the silhouette — TOOL, two renders and a difference
- [ ] A stand appears in exactly one rank per frame, counted exactly rather than statistically
- [ ] More than one prototype and more than one impostor atlas resident at a time — `render/ModelDraw` holds a single `SetPrototype` slot and a single atlas (32 784 KB), so a shrub cannot be drawn beside a tree at all; this is the line that blocks every second model kind
- [ ] The impostor atlas under a declared byte budget with an eviction unit of one cell, and a bake that is scheduled as non-frame work rather than run on arrival
- [ ] A prototype's rungs evictable independently of the prototype, so a species seen once at distance costs its impostor and not its four meshes
- [ ] Hysteresis on a rank switch, a minimum observer movement before anything updates, and a per-frame update budget
