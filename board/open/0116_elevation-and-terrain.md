Type: feature
Area: world
Tags: oracle

**II.1 Elevation and terrain**

- [x] DEM tile fetch, decode and stitch
- [x] Terrain mesh per quadtree node, LOD by screen-space error
- [x] Height at a point on the CPU with no device present
- [x] The height oracle answers on the *drawn* surface, so physics and picture cannot disagree
- [x] `GroundSample` as a tri-state return type — Resolved, Pending, Hole — so a caller cannot place on a sentinel
- [ ] Slope and aspect published as first-class ground quantities everywhere they are used
- [ ] Curvature, for a convex ridge to read differently from a hollow
- [ ] Vertical accuracy of the source stated per place — the chain is faithful; Badwater is 10.9 m off on flat ground and that is the DEM's error
- [ ] Hydro-flattening: a lake polygon carved to a constant elevation at or just below the surrounding terrain
- [ ] A river polygon carrying a monotone downstream gradient, as the engine already enforces for water lines
- [ ] Terrain carved under a road so the carriageway does not ride a raw DEM ripple
- [ ] Terrain carved for a building pad, so a house does not float or bury
- [ ] Cliff and overhang — a heightfield cannot carry one; a declared vertical face is the substitute
- [ ] Cave and tunnel volume — REFUSED as terrain, owed to a declared mesh volume instead
- [ ] Erosion as a function over the DEM — Ebert/Musgrave et al. ch. on terrain; the reference paints this by hand and we cannot
