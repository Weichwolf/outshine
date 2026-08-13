Type: feature
Area: world
Tags: scope

**Water, roads and rail — the three the owner did not name, and two of them split**

- [ ] **Water splits and the split is not optional.** A *water body* — lake, pond, reservoir, a closed OSM polygon — is **a part**, generated from its ring, composed by placement, exactly a building. A *water surface over terrain* — river, coast, floodplain — is **not a part**: it is a continuous field with the same adjacency and skirt problem terrain has, so it is a **layer of the terrain compositor** and not a compositor of its own. `world/WaterField.h` and `generators/Water.h` both exist today and the split between them is not stated anywhere
- [ ] **A road is a part whose generation depends on another part, and it is the one exception to statement 2's clean shape.** A road is a ribbon along a polyline of arbitrary length, it is *cut into* the terrain (`World::CutKerbs`), and it is the substrate traffic composes on. **The dependency is already expressible without a peer call**: `Ground` carries height, slope, class, edge distance, source feature and ring (`generators/Ground.h`), so a road generator takes the terrain's *field* as an input and never the terrain's *mesh*. **`Ground` is the mechanism that keeps statement 2 true for roads, and naming it here is what stops a later round from handing the road generator a mesh pointer**
- [ ] **A road part is bounded by the tile segment it lies in**, so "arbitrary length" never becomes an unbounded part; the join across the segment boundary is the terrain compositor's, by the same rule that gives it the water surface
- [ ] **Rail, bridges and tunnels take the road's answer** (§ IV.10, § IV.11) and are named here so they are not rediscovered as a fourth case
- [ ] **Traffic composes on roads and its parts are vehicles**, which are the cleanest case in the enumeration: bounded, reusable, high instance count, low distinct-part count — a forest whose placement changes every frame
