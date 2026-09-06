# A carriageway carries LANES, and a 10 m primary is never two of them

**State:** open · **Waits on:** nothing · **Found:** 2026-09-06, by looking at the T-junction plan

## What is wrong

The lab's road bed solves one AXIS with a crown and the street pass paints one Leitlinie down it
and a Fahrbahnbegrenzung either side. On the synthetic `R4-T90` that gives a 10 m `primary` with
edge lines at 4.75 m and one dashed line at the middle: **two lanes of 4.75 m**, which no road on
this planet has. The picture reads as an airstrip with a stripe.

The same defect is invisible on a 6 m residential street, which is why it went unseen: there the
one-lane-each-way reading happens to be right.

## What the two references do

| | |
|---|---|
| **RAGE** | a lane is authored. GTA's map carries lane counts, widths and directions per link, placed by a designer, and the paint is a decal on the surface |
| **Unreal** | nothing of its own; a road is a spline mesh and the lanes are in the mesh the artist built |
| **CARLA / SUMO** (cited, readable) | `netconvert` reads `osmNetconvert.typ.xml`: `numLanes`, `speed`, `priority` and `allow`/`disallow` PER `highway=*`, defaulting a `primary` to 2 lanes of 3.25 m each way where OSM is silent, and OSM's own `lanes`, `lanes:forward`, `lanes:backward`, `turn:lanes`, `parking:lane:*` override it. `MeshFactory` then samples EACH lane, not the carriageway |

**The choice is SUMO's, and it is not close.** It is the only one of the three that derives a lane
from data rather than from a person, which is the whole premise here; it is readable; and its type
table is exactly the "median of its epoch" rule CLAUDE.md already states for generators. RAGE's
answer needs an author this engine does not have.

## What will be true

- a way carries a LANE LIST, derived once: from OSM's `lanes*` tags where the surveyor gave them,
  otherwise from a type table keyed on `highway=*` that states lanes, width, direction and use
  (driving, parking, cycle, bus) with an origin per number
- the carriageway width is the SUM of its lanes plus the marking allowance, and where OSM states a
  `width` that disagrees, the width is honoured and the lanes are scaled -- never the reverse
- the paint follows the lane boundaries: a Leitlinie between two lanes of the same direction, a
  Fahrstreifenbegrenzung where crossing is forbidden, a Fahrbahnbegrenzung at the outside, and
  nothing at all inside a junction
- a parking lane is a lane: it takes its width out of the carriageway and it is where a parked car
  stands, which is R7's own input

## The measurement that shows I was wrong

**A LANE IS NEVER WIDER THAN ITS CLASS ALLOWS.** RAL/RASt bound a driving lane at 2.75 to 3.75 m
in a town; a case that reads a lane wider than 4.00 m or narrower than 2.25 m goes RED, and the
control is today's tree, which reads 4.75 m on every synthetic major. The second number is the
COUNT: the sum of the lane widths against the way's own width, worst case over the ladder, which
must stand within the marking allowance.
