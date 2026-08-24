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

- [ ] The candidate search is over SEGMENTS in a cell grid on the sphere (the class already
      owns `SnapM_` and `RadiusM_` and already grids nodes in `Within`), not over way boxes,
      and it publishes the number of segment pairs tested per crossing found.
- [ ] A way crossing the antimeridian neither becomes a global candidate nor loses a real
      crossing at the seam.
- [ ] Proving test: `ANetworkIsWovenFromWaysThatShareNoIdentity` gains a seam case -- two ways
      crossing at 180 deg -- and a cost case that asserts pairs-tested against ways-laid.
      Negative control: planar degrees restored -> the seam case finds the wrong count and the
      cost case names the blow-up.
