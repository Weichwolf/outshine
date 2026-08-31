Type: task
State: active
Parent: 1498
Area: generators, base/curve
Tags: scope, geometry
Supersedes: 1531, 1534

# A corridor is a reference line, and a crack is unspellable

**Benchmark** — Unreal: splines with tangent continuity for roads. RAGE: node lists with widths. **Taking Unreal** — a reference line whose curvature is continuous is the only thing that makes a crack unspellable, and a node list cannot state continuity.

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

Stands: `src/base/curve/ReferenceLine` with elevation and cross-slope; `Ribbon` sweeps the
cross-section into a SOLID (carriageway, shoulders, thickness along the surface normal, closed
end caps) and every top-surface vertex is placed by the same `StandAt` the physics stands on —
0 vertices differ beyond float over 401 stations.

## THE DERIVATION IS BUILT AND THE WORLD PATH BUILT A SECOND ONE

Measured 2026-08-31, and it is the reason this item goes active rather than staying a plan.

**The chain this item describes is complete:**

    src/generators/path/Fit.h        Simplify(polyline, withinM)
                                Fit(eastNorthM, withinM, tightestM, classTightestM, into)
                                -> Fitted{ WorstOffsetM, TightestRadiusM, Undrivable,
                                           UnderClass, SharpestTurnRad, Runs, ... }
    src/base/curve/ReferenceLine.h   Curve{Straight, Arc, Spiral}; Placed carries curvature,
                                     slope and bank WITH THEIR RATES; Knot; Segment
    src/generators/path/Ribbon.h          Sweep(line, Section{HalfWidth, Shoulder, Thickness}, ...)
    src/base/curve/Carriageway.h     Stand / StandAt
    src/sim/CorridorLay.h            Corridor{ ReferenceLine, Fitted, SpeedProfile, fine
                                     Stations with lane half width, edge and friction },
                                     Laying{ Holes, LanelessKinds, GradelessKinds, WorstGradeM,
                                     ClimbLimit }

`Fit` even takes `classTightestM` -- the per-class minimum radius, which is the number board:2076
went looking for and reported as read by nothing.

**And the world path uses NONE of it.** `src/engine/Picturing.cpp`, 2 401 lines, derives its own:
`RoadStation` is a raw polyline, `RoadProfile` a fixed cross-section, the crossfall a constant, and
there is no curvature, no spiral and no bank anywhere in it. So the SAME fact -- where a road runs,
how wide, how steep, how banked -- is derived twice from the same OSM ways, once for driving and
once for drawing, and the two cannot agree except by accident.

That is the twin board:1580 names in the shaders, in a place nobody was looking: **the car and the
picture disagree about the road they share.** Both references refuse it -- Unreal's spline mesh and
its collision come off one spline; RAGE's map geometry and its nodes come off one resource.

**The repair is not a new derivation.** It is the world path asking the one that exists, through
the generators' door, and `RoadStation`/`RoadProfile`/`RaiseRoad`/`RaiseJunction` going.

## WHAT THE FIT SAYS ABOUT OUR WAYS, and it is not what the deletion needed

Before deleting the second derivation, the first has to be able to carry the ways. Measured at
Kaiserberg, and the parameter that decides it was mine and had no origin:

    kFitTightestM   refused        offset p95
        8.0 m       6 391  24.1%     0.867 m     <- guessed
        3.0 m       4 189  15.6%     0.952
        1.0 m       1 121   4.2%     1.001       <- and 1 m is not a road, it is "fit anything"
        5.5 m       5 328  20.1%     0.903       <- DECLARED: a passenger car's kerb-to-kerb
                                                    turning circle is about 11 m across

The floor is the binding constraint and accuracy is what a lower one costs. **5.5 m is the value
with an origin**, and at it the fit lays 21 197 ways with a median offset of 4 mm and a p95 under a
metre, and refuses 5 328.

**A hypothesis of mine was refuted on the way and is worth keeping.** The sweep DENSIFIES a way to
one station every 16 m so the terrain can be sampled, and the refusals mention vertices four metres
apart, so the densification looked like the cause. Fitting the way's OWN nodes instead:

    original nodes, 5.5 m   5 171 refused  19.5%   p95 2.239 m   worst 146.807 m
    densified,      5.5 m   5 328 refused  20.1%   p95 0.903 m   worst  18.317 m

Barely fewer refusals and two and a half times the error, with a worst case eight times worse. **The
densification HELPS the fit** -- more constraints leave the arc less room to wander -- and the
hypothesis was wrong.

## SO THE DELETION IS BLOCKED BY A FINDING ABOUT THE INPUT, NOT ABOUT THE FITTER

One OSM way in five in this city carries a corner tighter than a car can turn. `Fit` says so in its
own words -- "a corner tighter than the lock is a route that doubles back on itself, and that is a
finding about the graph" -- and it is right: a driveway, a parking aisle, a switchback and a mapping
artefact all look like this.

**The answer is not a smaller floor.** It is that a vertex a vehicle cannot drive round is not a
CORNER, it is a NODE: the way is split there and the junction body board:2076 already builds fills
the gap. That unifies two things this tree treats separately and it is the shape the deletion needs.

Until then the second derivation cannot go, because deleting it would drop one way in five.

## What will be true

- [ ] `Picturing.cpp` derives NO road geometry of its own: the world path lays its ways through
      `Fit` into a `ReferenceLine` and sweeps them with `Ribbon`, so the drawn road and the driven
      road are one line. `RoadStation`, `RoadProfile`, `RaiseRoad` and `RaiseJunction` are deleted
      rather than left standing beside it
- [ ] Negative control: two derivations disagreeing is what this fixes, so the proof is that
      `Carriageway::StandAt` on the laid line and the drawn surface return the same height at the
      same station -- to float, at every station, or the twin is back
- [ ] The mesh is generated per tile and STREAMS: 774 km of road at a useful resolution is not
      one buffer, and the terrain that carries it says the same (board:1505).
- [ ] Lane markings are on the surface and derived from the same lane count the way declares, so
      what the eye reads and what a mind reads are one declaration.
- [ ] **A carriageway TAPERS where its width changes**, because a road does. A 12 m carriageway
      meeting a 2-lane one moves the surface edge 2.1 m, and a taper is the geometry that joins
      them; the length of the taper is the road's, from its class, not from any vehicle's
      tracking. What a car does about it is the car's business and is decided by what its wheels
      stand on (board:1919) -- this item owes the SHAPE, and a shape with a step in it is a road
      nobody built.
- [ ] The width comes from the way, not from the station grid — today it is sampled onto the
      DEM's 96.53 m posts, so a width change lands up to 96 m from where the way changes.
