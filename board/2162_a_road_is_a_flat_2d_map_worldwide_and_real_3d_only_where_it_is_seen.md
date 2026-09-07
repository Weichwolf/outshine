# A road is a FLAT 2D MAP worldwide and REAL 3D only where it is seen

**State:** open · **Lab rank:** 2 · **Waits on:** board:2164 · **Found:** 2026-09-07, waiting minutes for one picture

## What is wrong

**The twin solves the whole road network's vertical profile for every picture.** `roads_of` fetches
a 700 m extract around the camera, builds the network, runs ONE convex programme over all of it and
meshes all of it -- and then the camera, standing at 1.7 m in an old town, sees about a twentieth
of what was computed. Move the camera one metre and it all runs again, because the extract is
shaped by the camera and nothing about it is reusable.

Measured at OldTown: minutes inside cvxpy before a triangle exists.

The buildings already have the answer -- a quadtree and a horizon cull them to 5 of 1054 bodies,
137 s to 0.26 s. A road resisted the same treatment because the profile is a GLOBAL solve: one
height per OSM node, shared by every way through it, which is what makes C0 hold by construction.
Cut the network where the camera stops seeing and the heights CHANGE, exactly where the eye is.

**The error was treating "the road network" as ONE object.** It is two, they answer different
questions, and only one of them is three-dimensional.

## The decision

| | the LOGICAL map | the PHYSICAL road |
|---|---|---|
| answers | is there a way, where does it lead, may I turn here | what does the eye see, what may I stand on |
| **dimension** | **FLAT. 2D. A map** | **real 3D, correct geometry** |
| extent | the world, tile by tile, COMPLETE | the frustum, minus what is occluded |
| built from | the OSM tile, as it arrives | the logical map plus the DEM plus a solve |
| costs | a read. **No solve** | the convex programme and the meshing |
| when | per tile, once, cached | per frame |

**Out of sight there are no cars to watch.** A vehicle nobody can see needs a position ALONG an
edge and a direction of travel -- not a carriageway to stand on. That is RAGE's dummy vehicle,
which carries no physics and no drawable until it enters the radius where it is promoted, and it
is why a flat map is not a compromise out there but the CORRECT answer: a height nobody can see
and nothing can touch is a number with no consumer.

### Tier 1 is SOLVED, and not by us

Every navigation stack has already built the tiled, world-wide, flat road graph, and the two
readable ones agree on its shape:

- **Valhalla** (MIT, readable) names every node and edge by a `GraphId` = (hierarchy level, tile,
  index in tile). An edge lives in the tile of its start node; one that leaves names a node in the
  neighbour. Its hierarchy LEVELS -- motorway, arterial, local -- are an LOD rung applied to a graph
- **OSRM** (BSD-2, readable) partitions into cells and keeps an overlay over each cell's BORDER
  nodes. The border node is the unit of the seam, which is the object this item needs

**Taken: Valhalla's addressing.** A node belongs to the tile that CONTAINS it and is named (tile,
index); an edge crossing a border names a node in the neighbour. `test/lab/tile.py` already holds
those addresses and this is what it was written for. **Nothing about tier 1 is invented here.**

### Tier 2 is ours, and it is small

Navigation software carries no vertical alignment, and the readable body that does -- CARLA through
`netconvert` -- does it with a Laplacian relaxation of 100 rounds, a smoother with neither a band
nor a design grade. So the solve stays ours. What changes is its EXTENT and its ADDRESS.

**A camera-shaped domain is a defect on its own**, before any cost: walk one metre and the extract
moves, the solve changes and the road under your feet moves with it. Determinism is compulsory, so
the domain must be FIXED to the ground. It is therefore tile-aligned: the tiles the visible set
touches, solved with a HALO, keeping only what each tile OWNS, cached under a hash of its own
inputs. A tile that is never looked at is never solved.

**The halo has a derivation.** The objective is `int (z-d)^2 + l^2 (z'-d')^2 + l^4 (z'')^2` with
`l = smooth_m`, whose Euler-Lagrange equation is `l^4 z'''' - l^2 z'' + z = 0`. With `x = l*zeta`
the characteristic equation is `r^4 - r^2 + 1 = 0`, so `r = exp(+-i pi/6)` and `exp(+-i 5pi/6)`:
the decaying solutions fall as **exp(-0.866 x / l)** and oscillate with period `4 pi l`. A
disturbance at the halo's edge reaches the tile's own nodes multiplied by `exp(-0.866 H / l)`, and
the largest disturbance available is the band itself, 4 m. For that to land under **5 mm** -- a
tenth of the 50 mm the carriageway mesh resolves --

    H > l * ln(4000 / 5) / 0.866 = 7.7 l          [derived]

At `l = 2 * posting` and a 25 m posting that is **386 m**. The CONSTRAINED problem decays faster,
because an active band or grade constraint truncates the influence outright, so 7.7 l is an UPPER
BOUND and the halo actually used is the MEASURED one. Overlap costs `((a + 2H) / a)^2` per tile, so
a bigger tile is cheaper per square metre and a smaller one is lazier; at the vector source's own
z14, `a = 1604 m` at this latitude and the overlap is **2.2x**.

### Tier 3's halo is derived too, and shares no number with tier 2's

The kerb ring, the markings and the wear are LOCAL operators with a hard support radius: a kerb at
a point is decided by ways within `max half width + corner radius + footway` = 3.75 + 8.0 + 2.5 =
**14.25 m** by RASt 06, so a 25 m halo around the visible set is conservative and provable.

## The proofs

| | claim | negative control |
|---|---|---|
| **L1** | every node has exactly one owning tile, and a seam node reads one height | let two tiles write a border node -> red |
| **L2** | tiled-with-halo against ONE global solve: max abs dz over owned nodes < 5 mm | halo = 0 -> red, and by how much |
| **L3** | the flat map is CONNECTED across borders: every edge leaving a tile arrives in a neighbour | drop the join -> red |
| **P1** | **the culled picture equals the uncut picture, pixel for pixel** | hide one visible way -> red |
| **P2** | kerb, markings and wear over the visible set equal their restriction from the full set | halo 0 m -> red |

**P1 is the one that matters.** Every other number can be right while the picture has quietly lost
something, and only a pixel comparison against the uncut render can say it has not.

## What it changes

- `test/lab/roads/logical.py` is new: the flat per-tile map on `tile.py`'s addresses, Valhalla's
  ownership rule, the cross-tile joins -- and no z at all
- `test/lab/roads/aligned.py` is new: the tile-addressed vertical solve with its halo and its cache
- `roads_of` stops solving. It reads the tiles the frustum touches, culls the ways with the same
  quadtree and horizon the buildings use, and meshes what is left
- The reach stops being a quality knob. A picture costs what is IN it
