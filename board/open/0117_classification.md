Type: feature
Area: world
Tags: scope

**II.2 Classification**

- [x] Class grid from OSM vectors, arbitrated in a declared order (`world/ClassBuilder`)
- [x] Edge distance to the nearest boundary of the winning class
- [x] Runner-up class at a point, so a boundary knows what it blends towards
- [x] Class as a state, not a default: `no row` where OSM has no datum (`generators/Cover`)
- [x] Unmapped substrate that is drawn and grows nothing — the retired global `meadow` default is now unspellable
- [x] Twelve declared land templates plus the unmapped substrate row
- [x] Way width per street kind, 1.5 m path to 45 m
- [x] One predicate, two evaluators: the edge test a fragment runs is the edge test a CPU query runs
- [x] Three tiers over the vectors: AABB on the CPU, source polygon on the CPU, refinement on the GPU one-way
- [ ] Runner-up and edge distance consumed for a height-driven layer blend — available, nothing reads them for this
- [ ] Per-place default where OSM is silent, which needs a climate model this engine does not have
- [ ] OSM layer names spelled once rather than in three files
