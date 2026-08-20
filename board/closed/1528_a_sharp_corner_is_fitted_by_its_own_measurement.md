Type: bug
Area: generators
Tags: instrument

**A sharp corner is fitted by its own measurement and not by a first-order formula**

`CornerRadiusM` closes the spiral-arc-spiral corner in the FIRST-ORDER clothoid shift `L^2 / 24R`. That
is accurate to 0.28 % at a right angle and loses accuracy as the turn sharpens: on the Munich to
Hamburg corridor the worst vertex offset is **23.56 m against a 9.55 m budget, at 641.4 km**, where the
sharpest turn on the route is 137.3 degrees.

**The construction is derived and the residual is a named term** -- the clothoid's higher-order series
-- but a bound that is exceeded by 2.5x is not a bound.

## What must be true

- [ ] **Each corner measures its own offset from the vertex after laying**, and reduces its radius
      until the measurement is inside the budget. The correction is MEASURED per corner rather than
      fitted once, so it stays derived
- [ ] **The number of corners that needed correcting is published**, because it is the size of the
      first-order approximation's domain
- [ ] **The measurement is bounded**: a corner that cannot be brought inside the budget at any radius
      the vehicle can drive is a REFUSAL, which is what `board:1524` calls a pass on broken data

## Comments

At a right angle the closed form is 0.28 % out and the whole route's median corner is far gentler than
that. What breaks it is the tail: 25 turns past a right angle and 1 past 135 degrees in 2480 vertices.
**Fixing the median would have bought nothing; the tail is the whole finding.**

## Comments -- what the measured correction actually showed

Correcting each corner by its own measured offset does NOT converge on the Munich to Hamburg
corridor. Over 24 passes: 6517 corner corrections, the worst offset falling 23.56 m -> 15.64 m and
then stalling, with **1284 corners of 2300 pinned at the vehicle's tightest radius of 4.876 m**.

**1284 hairpins on a motorway route is not a road, so the measurement is wrong somewhere and not the
data.** Simplifying the polyline first with Douglas-Peucker inside the same 9.55 m accuracy removed
258 of 2560 vertices and moved the worst offset by 0.2 m -- so vertex density is not the cause
either.

The next thing to check is the offset measurement itself: it resects each vertex against the laid
line in a window around that vertex's own fitted station, and a station that is wrong by more than
the window gives a huge offset for a corner that is fine. `atVertexM` is assigned for every interior
vertex but never for the LAST one, which is one vertex and not 1284 -- so the window width, four
times the incoming leg, is the candidate: on a leg of a few metres that window is a few metres wide
and the true nearest station can lie outside it.

## Closed -- it was drift and not the corner

**The corner correction was chasing something no corner caused.** Instrumenting the worst vertex named
it: vertex 2294 of 2302, second to last on the route, incoming leg 81.64 m, outgoing 810.70 m, turn
87.35 degrees, radius pinned at the vehicle's 4.876 m floor -- and a corner of that radius and that
turn leaves its vertex by 1.96 m, not by the 15.64 m measured. The resection was right: the station it
found, 770794.49 m, matches the station the fit expected, 770791.82 m.

**So the line was 15.6 m from the polyline for reasons that had nothing to do with that corner.** The
line is walked corner by corner and each spiral is integrated by 8-node Gauss-Legendre; the residual
accumulates LATERALLY over 2300 corners.

Each corner now compares its measured offset against its OWN predicted one,
`R (shiftShare / cos(theta/2) - 1)`, and only corrects when the corner itself is at fault. What is
left is published as drift:

| | |
|---|---|
| passes needed | 24 and not converging -> **1** |
| corners pinned at the vehicle's floor | 1284 of 2300 -> **0** |
| drift over 774 km | **17.16 m** |
| per corner | **7.46 mm** |

**A correction that chases a term it cannot move will consume every degree of freedom it has**, and
here it consumed the radius of 1284 corners that were never wrong.
