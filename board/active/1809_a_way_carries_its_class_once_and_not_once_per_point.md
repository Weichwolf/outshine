Type: bug
Area: actor/path
Tags: optimisation, layout, measured

# A way carries its class once, and not once per point

`Network` keeps four parallel arrays of WAY-CONSTANT attributes, one entry per POINT, and this
round added the fifth (`src/actor/path/Wayfinding.h:102-106`):

```cpp
std::vector<double> Points_;      // 16 B / point -- the only per-point datum
std::vector<double> Widths_;      //  8 B / point -- constant over the way
std::vector<double> Gradients_;   //  8 B / point -- constant over the way
std::vector<double> Radii_;       //  8 B / point -- constant over the way, NEW (board:1784)
std::vector<int>    Lanes_;       //  4 B / point -- constant over the way
```

Every one of them is filled from the same four scalars, in one loop, from the way's own
declaration (`src/actor/path/Wayfinding.cpp:61-67`):

```cpp
for (size_t which = 0; which < points; ++which) {
  Points_.push_back(latLonPairs[2 * which]);
  Points_.push_back(latLonPairs[2 * which + 1]);
  Widths_.push_back(halfWidthM);        // the same value, points times
  Gradients_.push_back(maxGradient);    // the same value, points times
  Radii_.push_back(minRadiusM);         // the same value, points times
  Lanes_.push_back(lanes);              // the same value, points times
}
```

and the values ALREADY sit on the `Way` that owns the range (`Wayfinding.h:64-72`:
`HalfWidthM`, `MaxGradient`, `MinRadiusM`, `Lanes`, beside `First` and `Count`).

## The arithmetic

| | bytes per point |
|---|---|
| the point itself | 16 |
| the way's four constants, repeated | **28** |
| a `std::vector<uint32_t> WayOf_` instead | 4 |

**44 B/point today, 20 B/point with an index — 55 % of the point stream is duplication.** For
`board:1503`'s continental graph that is the difference between what streams and what does not,
and it is paid three more times over in `Weave`, which reserves, refills and moves all five
arrays (`Wayfinding.cpp:143-168`).

Only `Weave`'s node merge reads them per point (`:206-222`), and every read is
`X_[point]` where `point` belongs to a known way -- an index lookup, not a scan.

## Why this is filed rather than left as taste

The house rule is *contiguous, one-width, pointer-free layouts; batch over per-item*. Five
arrays in lockstep are the right SHAPE; four of them holding a broadcast constant are the
wrong CONTENT. And it grows: `board:1784` added the fifth this round because adding one was the
path of least resistance, which is exactly how a layout rots.

## What will be true

- [x] `Widths_`, `Gradients_`, `Radii_` and `Lanes_` are gone; a way's constants are read
      through the `Way` the point belongs to, reached by one `std::vector<uint32_t> WayOf_`
      (or by binary search over `Way::First`, if the memory is worth the log).
- [x] The node merge (`Wayfinding.cpp:206-222`) reads the same values through that indirection
      and its three merge rules -- width widest, gradient strictest, radius loosest -- are
      unchanged, proven by `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity`.
- [x] `Network` publishes its heap bytes per point so the saving is a number and not a claim.
- [x] Negative control: the merge rules asserted before and after -- identical routes, identical
      `Leg::MinRadiusM`, `HalfWidthM`, `MaxGradient` and `Lanes` on the Munich--Hamburg plan.

## Repaid (2026-08-24)

Four arrays gone, one index in. `Widths_`, `Gradients_`, `Radii_` and `Lanes_` no longer
exist; `WayOf_` is a `std::vector<uint32_t>` and every reader takes the value off the `Way` it
points at -- which is where the values always were.

```
NOTE points laid                       = 5000 points
NOTE bytes the point stream costs      = 100000 bytes
NOTE bytes it holds                    = 100000 bytes
NOTE bytes a point costs               = 20 bytes
NOTE what the growth overshoots by     = 1 x
```

**44 B/point to 20 B/point**, and the second line is a repair the item did not ask for: the
stream held **163 840** bytes for 100 000 bytes of points, because `Lay` pushed into five
vectors that doubled as they grew. A way declares how many points it lays before it lays them,
so the stream is reserved now and holds exactly what it costs.

`Weave`'s reorder shrank with it -- it moved five arrays and now moves two.

- **Behaviour-neutral, measured**: the shipped Munich--Hamburg drive returns
  `how far the route runs = 742.636082 km`, the same digits as before the change, over
  317 895 snapped nodes.
- **Proving test**: `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity` -- the
  per-point cost and the held-equals-cost claim beside the class-carrying arms this session
  already landed there.
- **Negative control**, run: `Widths_` put back as a per-point array ->

  ```
  NOTE bytes a point costs = 28 bytes
  FAIL **A WAY CARRIES ITS CLASS ONCE, AND NOT ONCE PER POINT**
  ```
- Gate 259/259. The fifth array was added by `board:1784` this same session, which is why this
  finding is filed against work three hours old rather than against the tree's history.
