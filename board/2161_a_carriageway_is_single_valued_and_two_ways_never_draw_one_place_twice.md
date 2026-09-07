# A carriageway is SINGLE-VALUED, and two ways never draw one place twice

**State:** open · **Lab rank:** 4 · **Waits on:** board:2162 · **Found:** 2026-09-07, standing in a street in Rothenburg

## What is wrong

**22.5 % of the drawn carriageway of a real extract has TWO SURFACES in one place**, and where it
does they stand up to **4.15 m** apart. Measured over 4 000 points the carriageway covers, at
OldTown, a 90 m reach:

| | |
|---|---|
| median difference between the drawn surfaces at a point | 0.0 mm |
| 99th percentile | 2 238 mm |
| worst | 4 145 mm |
| points carrying two surfaces over 50 mm apart | 900 of 4 000 |

This is why a street-level look at a real place shows a shattered road: folded plates, black
vertical wedges rising out of the asphalt, a zebra crossing ramping up like a jump. From 26 m up
it is invisible, because every plate is near-horizontal in plan and only its HEIGHT is wrong.

## Why NO existing check could see it

Every invariant the road bed holds is stated on the AXIS or at a NODE:

| | |
|---|---|
| I1 | every way through a node reads one height |
| I2, I16 | the profile is C1 at and across a node |
| I13 | the Fahrbahn is one continuous LINE with no gap and no jump |
| I6 | a leg's surface against the junction's, along the leg's cut |
| I7, I9 | the drawn surface against the ANALYTIC one, sampled ALONG EACH WAY |

Not one of them asks what happens where two ways' carriageways OVERLAP IN PLAN without sharing a
node. A footway drawn along a street, a service road inside a car park, a `highway=pedestrian`
area over a carriageway, a `living_street` and the `footway` beside it whose widths overlap:
each solves its own profile correctly, each draws its own ribbon correctly, and the two ribbons
land on top of one another at different heights. I9 samples along each way and finds each one
right. **The defect is between them, and every measure looks within.**

The consequence measured downstream, before the cause was found: the carriageway EDGE's own
height steps 782 mm at the 99th percentile and 1 854 mm at worst between points 0.5 m apart, and
the kerb ring -- which correctly reads that edge -- inherits it (median 3.5 mm, p99 191 mm).

## What the references do

| | |
|---|---|
| **RAGE** | a road is authored; two overlapping surfaces are a bug a level designer sees and fixes |
| **Unreal** | the same |
| **SUMO / CARLA** (cited, readable) | `netconvert` builds a LANE-based network: a lane belongs to exactly one edge, junction areas are computed as polygons and the internal lanes are the only thing inside them, and `MeshFactory` meshes lanes. Two edges cannot claim one piece of ground because the ground is partitioned by construction |

**The choice is SUMO's**, and it is the same argument this tree already accepted for junctions
(`Structure._junction_polygons`, `Mesh.region_of`): a place on the ground belongs to ONE surface,
and the question is only which. It is also the same rule `surfaces.py` already applies to OSM's
area features, where a PRIORITY decides and the rest is subtracted.

## What will be true

- the drawn carriageway is a PARTITION: every point of the drivable area is drawn by exactly one
  way's ribbon or one junction's fan
- where two ways' carriageways overlap in plan without sharing a node, the higher-priority class
  keeps the ground and the lower one is clipped to the difference -- which is what `ROAD_RANK`
  already states and what `_ribbons_cross` already computes and does not yet act on
- a way clipped to nothing is not drawn at all, and says so

## The measurement that shows I was wrong

**I26**, the number above, over a real extract: the greatest difference between the drawn road
surfaces at a point the carriageway covers, and the share of points carrying more than one.
It must read 0 mm and 0 %; the control is the tree as it stands, which reads 4 145 mm and 22.5 %.
The synthetic ladder cannot produce it -- 81 of 81 are green and always were, because a synthetic
network is drawn with ways that do not overlap.
