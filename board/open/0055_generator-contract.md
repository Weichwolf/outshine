Type: feature
Area: generators
Tags: perf

**I.9 Generator contract**

- [x] A generator is a pure `const` function `(Region, Ground) -> Yield` (`generators/Generator.h`)
- [x] `Ground` carries height, slope, class, edge distance, runner-up, source feature and ring, water level, declared tables
- [x] `Ground` carries no camera, frustum, frame index, clock, LOD level, renderer, device, sun or weather — unspellable, not forbidden
- [x] Three products: occupancy, draw, point query
- [x] Occupancy carries bounds, substitute contact body, mass, contact material — never triangles, material or kind
- [x] Draw carries clusters with model-space error, instances, material row — never bounds or mass
- [x] Region pool and schedule, N concurrent regions without a lock
- [x] The engine knows only physics: a trunk is a cylinder; no content taxonomy exists in it
- [x] A generator runs continuously per region, not once at load
- [ ] Actor spawner sharing the region key and handing seed to an entity store — actors are not generators
- [ ] The draw product declares the generator's **capability**: how many rungs it can deliver, the model-space error of each, whether an impostor exists and from which rung it takes over — the renderer optimises against that declaration, and the header that declares it carries this rule as its own comment rather than a document elsewhere
- [ ] Selection on screen-space error alone, one criterion for terrain, trunk, façade and crown — a generator never chooses its own rung and never carries a distance
- [ ] Only what contributes to the image is drawn, and "contributes" is that same error against the same threshold, so a thing too small to change a pixel is never selected rather than culled by a special case
- [ ] Every kind of content on the one cluster DAG the terrain already uses — a second selection path for a second kind of content is the defect this line exists to prevent
- [ ] `DrawSink` truncation reported rather than silent (`ForestDraw.cpp:18`)
- [ ] `RegionPool::Extent::Reached` read by the thing whose budget it claims to bound
