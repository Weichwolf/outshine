Type: task
State: open
Parent: 1813
Area: generators
Tags: scope
Supersedes: 1529


**Benchmark** — Unreal: landscape sculpting and road splines are authored together in the editor. RAGE: the map ships cut and filled. **Neither derives it** — the cut and fill is ours because the road is derived, and the item it answers to is geometric plausibility.
# The terrain is cut and filled to meet the road, and the road is drawn geometry

A road is not laid on the ground; the ground was moved to carry it. Once every node has an
elevation the road and the DEM disagree almost everywhere — five metres on an embankment, ten
in a cutting — and that disagreement is what makes a world look wrong: a motorway floating over
a valley, a ramp buried in a hill, an abutment sticking out of a river.

The direction inverts what the corridor does today: the reference line samples the elevation
source and takes whatever gradient it finds, which on the shipped route gives -17.1 % at
km 313.9 — a gradient no motorway has. A real road declares its own vertical alignment (grades
joined by vertical curves, fitted within a declared tolerance and bounded by what the class and
the vehicle allow) and the TERRAIN is deformed to meet it.

## The picture has no road on it at all -- measured at bb9472db, 32 stills, two routes

Before cut and fill there is a prior fact the driver's stills carry. Chase view, Ludwigstrasse
`48.1420,11.5800 -> 48.1518,11.5820` and the Isar `48.1310,11.5820 -> 48.1290,11.5930`:

    the plane under the car, 170 x 25 px      R in 61..62, G in 75..77, B in 55..57
    the same plane 200 px to either side      identical to within 1 count

One painted colour from the wheels to the horizon. There is no carriageway distinguishable from
the verge, no centreline, no edge line, no kerb, no shoulder, no oncoming carriageway and no
change of material anywhere on the surface the car stands on. The corridor exists -- the run
prints its stations, its widths and its class -- and NOTHING of it is drawn.

That is this item's first box (*the car drives on the DRAWN surface*) failing at its weakest
reading: the physics has a road and nobody can see it. Until a carriageway is in the frame, the
embankment/cutting half of this item cannot be judged from a still, and it is the head of the
stakeholder's work order (board:1865).

## What will be true

- [ ] The car drives on the DRAWN surface: one geometry, two readers — the renderer draws it and
      the contacts stand on it. A road the physics agrees with but nobody can see, or one that is
      drawn and cannot be driven, is two roads.
- [ ] The geometry is a SOLID: thickness, shoulder, verge and side slope; a deck has a soffit
      something passes under.
- [ ] A fill slopes away at the angle of repose and a cut rises at it, declared per material; a
      retaining wall stands where the slope would not fit, against a declared budget.
- [ ] The corridor blends back into the DEM with continuity — the verge mesh's boundary row sits
      ON the ribbon's edge polyline, not on a grid's nearest approximation of it, or the seam is
      a cliff and the car finds it.
- [ ] Two overlapping corridors produce ONE surface by a declared rule; a bridge and a tunnel do
      NOT cut, and a tunnel cuts only its portal.
- [ ] Cut and fill are published per tile and per metre of road — a road that fills 30 m is a
      viaduct nobody marked, one that fills 0.3 m is a road.
