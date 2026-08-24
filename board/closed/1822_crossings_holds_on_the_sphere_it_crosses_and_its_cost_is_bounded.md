Type: bug
Area: actor/path
Tags: geometry, antimeridian, cost, drive

# Crossings holds on the sphere it crosses, and its cost is bounded by a grid

`Network::Crossings` (`src/actor/path/Wayfinding.cpp:268-309`) is the whole input to
`board:1813`'s reconstruction of the third dimension: 17 472 grade separations found where the
provider carries no bridge, tunnel or layer tag. It does its geometry in **degrees, on a
plane**.

## The antimeridian, which this file already knows about

Every other query in the same class works on the sphere -- `Nearest` and `Within` use `ApartM`
with `RadiusM_` (`:319-343`) -- and the tree carries
`test/unit/actor/path/AWayAcrossTheAntimeridianWeavesAndRoutes` because the seam is a real case
here. `Crossings` uses raw `LatDeg`/`LonDeg` as `y`/`x`:

```cpp
src/actor/path/Wayfinding.cpp:289   if (theirs.MaxLon < mine.MinLon || theirs.MinLon > mine.MaxLon) { continue; }
src/actor/path/Wayfinding.cpp:301   if (!SegmentsMeet(ax, ay, bx, by, cx, cy, dx, dy, &atX, &atY)) { continue; }
```

A way whose points step from 179.99 to -179.99 gets a `Way` bounding box spanning
`[-179.99, 179.99]` (`Way::MinLon/MaxLon`, `src/actor/path/Wayfinding.h:86`) -- **the whole
planet** -- so it is a crossing candidate against every way on Earth, and the segment that
carries the seam is a 360-degree-long chord that intersects most of them. Two ways that genuinely
cross AT the seam are missed for the same reason.

`test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity.cpp:190-268` proves the
overpass, the at-grade junction and the parallel pair -- all at 51 N, 10 E. There is no seam
case and no polar case, and the seam case exists for `Weave` right beside it.

## The cost

The sweep prunes by `Way` bounding box, then brute-forces `|a| x |b|` segment pairs for every
surviving pair. A motorway way carries hundreds of points and a bounding box the length of the
country it crosses, so the pruning does almost nothing for exactly the ways the reconstruction
is about. `board:1524` asks for a hundred routes; this is the term that will not scale to them,
and it is not measured today.

## What will be true

- [x] The candidate search is over SEGMENTS in a cell grid on the sphere (the class already
      owns `SnapM_` and `RadiusM_` and already grids nodes in `Within`), not over way boxes,
      and it publishes the number of segment pairs tested per crossing found.
- [x] A way crossing the antimeridian neither becomes a global candidate nor loses a real
      crossing at the seam.
- [x] Proving test: `ANetworkIsWovenFromWaysThatShareNoIdentity` gains a seam case -- two ways
      crossing at 180 deg -- and a cost case that asserts pairs-tested against ways-laid.
      Negative control: planar degrees restored -> the seam case finds the wrong count and the
      cost case names the blow-up.

**Closed**, and the item's own diagnosis needs one correction first, because it matters for
what was repaired.

"It does its geometry in degrees, on a plane" is two claims and only one of them bites. A
segment intersection is invariant under affine maps, and over the 10-100 m a tile's polyline
segment spans, the difference between a degree-space chord and a great-circle arc is far below
the tile's own quantisation. Latitude scaling does not change WHETHER two short segments cross.
What breaks is the SEAM -- a discontinuity, not a distortion -- and the cost. Both are repaired;
the sphere-versus-plane framing is withdrawn.

**The seam.** Longitudes are unrolled once, about the first point, before any comparison:

```cpp
src/actor/path/Wayfinding.cpp:277   double away = Points_[2 * at + 1] - aboutLon;
                                    while (away > 180.0) { away -= 360.0; }
                                    while (away < -180.0) { away += 360.0; }
```

A way stepping 179.99 -> -179.99 is 0.02 degrees long after that, not 359.98, so it is no longer
a candidate against every way on the planet and a crossing AT the seam is found. The crossing is
wrapped back into [-180, 180] when it is reported.

**The cost.** Segments -- not way boxes -- go into a counting-sorted cell grid whose cell is
sized from the segments' own count and extent, so a cell holds O(1) of them regardless of how
long the ways are. A pair is tested only where it shares a cell, and a crossing is kept only by
the cell that contains it, so no pair is double-counted and no dedup set is allocated.
`Network::PairsTested()` publishes the term.

Measured, `unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity`, 120 ways of 200
segments crossing off-node:

| | |
|---|---|
| crossings found | 3600 of 3600 |
| segment pairs tested | 142 171 |
| pairs per crossing found | 39.5 |
| what every-pair would cost | 288 000 000 |

Proving test: the same case, two new blocks -- a seam case at 180 degrees and a cost case.
Negative controls, both run: unrolling removed -> the seam case finds **0 places** where it
should find 1; the cell collapsed to the whole extent -> **285 600 000 pairs tested**, 79 333
per crossing, and the cost check names the blow-up. Note that the collapsed grid still finds
3600 -- the grid is cost alone, and the correctness is the unrolling and the strict-interior
intersection test.
