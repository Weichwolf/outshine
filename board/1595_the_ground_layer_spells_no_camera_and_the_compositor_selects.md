Type: issue
State: open
Area: world
Tags: scope

**The ground layer spells no camera, and the compositor selects**

The architecture adjudication (2026-08-22) named the two ground-layer classes that spell what
the layer table forbids: `TilePool::Camera(lat,lon)` prioritised fetches by a camera the pool
has no business knowing, and `World::Refine(Eye)` makes quadtree LOD decisions per eye -- the
SOLL gives selection to the compositor with one currency (projected error in px, rung ladder),
and Frustum with it.

Slices:
- [x] the pool's noun dies: `Focus(latDeg, lonDeg)` -- a consumer-declared service point, no
      camera spelling, no behaviour change (2026-08-22, unit/world + render/outshine/world green)
- [ ] `World::Refine`'s eye-driven refinement moves behind a declared budget/error interface --
      the compositor decides WHAT, the world only answers
- [ ] Frustum finds its consumer in the same move (board:1538's cull hole)
