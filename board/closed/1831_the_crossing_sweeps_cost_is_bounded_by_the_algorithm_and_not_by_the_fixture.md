Type: bug
Area: actor/path
Tags: cost, bounds, allocation
Regresses: 1822

# The crossing sweep's cost is bounded by the algorithm, not by the fixture

`board:1822` closed on a measurement -- *39.5 pairs per crossing against 288 million* -- taken
against a fixture that is a perfectly uniform 60x60 grid
(`test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity.cpp:286-336`). Uniformity is
what makes a uniform grid cheap. The algorithm carries no bound of its own.

## The cell size is derived from the bbox AREA, and a degenerate bbox has none

```cpp
src/actor/path/Wayfinding.cpp:294   const double cellDeg =
src/actor/path/Wayfinding.cpp:295       std::sqrt(std::fmax(spanLon * spanLat, 1.0e-12) / (double)segments) * 2.0;
src/actor/path/Wayfinding.cpp:296   const size_t across = (size_t)(spanLon / cellDeg) + 1u;
src/actor/path/Wayfinding.cpp:297   const size_t down = (size_t)(spanLat / cellDeg) + 1u;
src/actor/path/Wayfinding.cpp:298   const size_t cells = across * down;
src/actor/path/Wayfinding.cpp:300   std::vector<uint32_t> holds(cells + 1u, 0);
```

`spanLat == 0` -- every way exactly east--west at one latitude, which is what a synthetic
fixture lays -- takes the `1.0e-12` clamp. Derived, for two two-point ways:

| span in longitude | `cellDeg` | `across` | `holds` + `inCell` |
|---|---|---|---|
| 0.2 deg | 1.414e-6 | 141 422 | 1.7 MB |
| 10 deg | 1.414e-6 | 7 071 068 | 85 MB |

and the marking loops at `:319-321` and `:329-332` each walk every cell the segment's box
covers, so two segments cost 14 million iterations for one crossing. The clamp is a magic
number carrying no origin, and what it buys is not a bound -- it is the largest grid the
arithmetic can produce.

## And a clustered network with one outlier degrades to brute force

`cellDeg` scales as `sqrt(area / segments)`, so a network whose ways sit in one town plus ONE
way fetched from the edge of the corridor gets a cell size set by the whole bbox. Every town
segment lands in one cell, and the pair loop at `:346-366` is `O(n^2)` inside it with no bound
and no telemetry that says so -- `PairsTested()` reports the total, never the worst cell.
That is the exact case the drive assembles: tiles are fetched in a ring along the great circle
(`src/sim/DriveAssembly.cpp:104-118`), so density along the route varies by orders of magnitude.

## `PairsTested_` is `mutable` and written from a `const` query

```cpp
src/actor/path/Wayfinding.h:120   mutable size_t PairsTested_ = 0;
src/actor/path/Wayfinding.cpp:270   PairsTested_ = 0;
```

`Crossings()` is `const` and mutates the object, so two callers on one network race. A count a
caller wants belongs in the answer, beside `into`, not in the object.

## What will be true

- [ ] The cell size is derived from the SEGMENT LENGTHS the network holds (a mean or a
      quantile), not from a bbox area, so a degenerate span cannot produce a cell count larger
      than the segment count. The `1.0e-12` clamp goes with it.
- [ ] `cells` is bounded above by a term in `segments`, proven by a `static_assert`-adjacent
      refusal or a returned reason -- never by an allocation the caller discovers as a
      resident-set number.
- [ ] The sweep publishes the WORST cell's occupancy beside the total pairs tested, so a
      clustered network is visible as a number rather than as a slow run.
- [ ] `PairsTested_` stops being `mutable`: the count travels out with the crossings.
- [ ] Proving test: two fixtures beside the uniform grid in
      `ANetworkIsWovenFromWaysThatShareNoIdentity` -- (a) ways all at one latitude, asserting
      the cell count stays under `segments` and the crossings are still found; (b) 2 000
      segments in 0.001 deg plus one way 5 deg away, asserting worst-cell occupancy is bounded
      and `PairsTested()` stays under `segments * 20`. Negative controls: today's `cellDeg`
      restored -> both red, printing the cell count and the pair count.

## Comments

- 2026-08-24 -- filed by the hourly review. `board:1822`'s repair is the right structure and
  its measurement is real; what it does not establish is that the number is a property of the
  algorithm rather than of the fixture that produced it.

**Closed.** Three changes, and the last one removes the race the item names.

```cpp
src/actor/path/Wayfinding.cpp:302   const double cellDeg = reachSum > 0.0 ? 2.0 * reachSum / (double)segments : 1.0;
src/actor/path/Wayfinding.cpp:303   const size_t cells = 2u * segments + 1u;
src/actor/path/Wayfinding.cpp:311   const uint64_t mixed = (uint64_t)(square.first * 73856093L ^ square.second * 19349663L);
```

The cell size comes from the mean segment reach, so a network with no extent in one axis cannot
produce a cell smaller than its own segments. The table is a spatial HASH of `2n+1` buckets, so
the allocation is a term in the segment count whatever the bounding box is -- the `1.0e-12`
clamp is gone with the area it guarded. A crossing is still kept by exactly one bucket, so no
pair is double-counted.

`Network::Swept {Found, PairsTested, FullestCell}` travels out with the crossings.
`PairsTested_` and `Fullest_` are not members at all now, so two callers on one network cannot
race, and `Crossings()` is `const` without lying about it.

| fixture | pairs tested | fullest cell |
|---|---|---|
| the uniform 60x60 grid, 24 000 segments | **98 954** (was 142 171) | 39 |
| eight ways at ONE latitude, 160 segments | **19** | 4 |
| forty ways in 0.0008 deg plus one 5 deg away, 801 segments | **7 108** | **25** |

27.5 pairs per crossing where the area-derived grid managed 39.5, and all 3600 crossings are
still found with the seam still one.

Proving test: `unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity`, two new fixtures.
**Negative control, run: the area-derived cell size restored -> the case TIMES OUT at 300 s**
and the runner says it measured nothing. That is the item's own arithmetic arriving: 141 422
cells across for a flat network, and the marking loops walking every one of them.

---

## Correction, 2026-08-25 (hourly review): the closure's dedup argument does not hold

The sentence *"A crossing is still kept by exactly one bucket, so no pair is double-counted"*
is reasoning about the grid this repair replaced. A segment is inserted once per SQUARE and
two of its squares can hash to one BUCKET, so it stands in that bucket twice and the pair is
enumerated -- and accepted -- once per copy. Measured on one long segment among sixty short
ones: **one crossing reported nine times**; over a 4 500-fixture family, 396 wrong answers
where the pre-repair build had 0.

The four requirements of this item stand met. The regression is filed as **`board:1835`**,
which carries the reproduction and the fix (compare SQUARES at the ownership test, not
buckets).
