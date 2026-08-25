Type: task
State: open
Parent: 1498
Area: generators, actor/path
Tags: scope, geometry
Supersedes: 1531, 1534

# A corridor is a reference line, and a crack is unspellable

Not only roads: a road, a railway, a canal, a runway, a pipeline and a wall are one shape — a
reference line with a cross-section swept along it. They differ in the profile and in the
limits, and a railway's limits are the tighter case (gradient under 4 %, radius in the
hundreds, the transition curve compulsory, cant engineered, no lateral freedom at all).

The mechanism is ASAM OpenDRIVE's: the reference line's plan view is a sequence of line, arc and
spiral; elevation along `s` and the roll angle are sequences of cubic polynomials; a spiral
carries the transition *without causing leaps in the curvature*. Take the mechanism — the mesh
is what the curve is EVALUATED into, never the primary thing. The assumption that comes with it:
a road has a design speed, and curvature and its rate are bounded by it, which is what OSM's
`highway=*` classes imply.

Stands: `src/actor/path/ReferenceLine` with elevation and cross-slope; `Ribbon` sweeps the
cross-section into a SOLID (carriageway, shoulders, thickness along the surface normal, closed
end caps) and every top-surface vertex is placed by the same `StandAt` the physics stands on —
0 vertices differ beyond float over 401 stations.

## What will be true

- [ ] The mesh is generated per tile and STREAMS: 774 km of road at a useful resolution is not
      one buffer, and the terrain that carries it says the same (board:1505).
- [ ] Lane markings are on the surface and derived from the same lane count the pilot drives by,
      so what the eye reads and what the car holds are one declaration.
- [ ] **The cross-section changes no faster than a vehicle in it can follow.** Measured: a 12 m
      carriageway narrowing to a 2-lane one moves the lane centre 2.1 m, which at the route's
      tracking budget of 11.13 mm/m needs 189 m of road — the narrowing arrives sooner and puts
      two wheels off the carriageway while the car sits 0.023 m from its own lane centre. The
      taper LEADS the narrowing, and a narrowing no vehicle could follow is a named refusal of
      the ROUTE, not a crash.
- [ ] The width comes from the way, not from the station grid — today it is sampled onto the
      DEM's 96.53 m posts, so a width change lands up to 96 m from where the way changes.
