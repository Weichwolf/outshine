Type: task
State: open
Parent: 1813
Area: generators
Tags: scope
Supersedes: 1529

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
