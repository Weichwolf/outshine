# A reference line is fitted in under a microsecond

State: open

`streets: of paving, the fit` is 620 ms of a 1.0 s preload on OldTown -- the largest single line in
the rebuild once the ground seams were repaired, and it has nothing to do with the terrain.

## Measured 2026-09-04, by a trace built for the question and removed again

```
  lanes  16000   (four rebuilds, so ~4000 a rebuild)
  calls  19995   -- 1.25 calls a lane, so the retry loop is NOT the cost
  points 350052  -- 17.5 points a call
  inside Fit()   1554.7 ms
```

**78 microseconds to fit a reference line through 17 points.** At 3 GHz that is roughly 14000
cycles PER POINT, for a Douglas-Peucker simplification and a pass of `atan2` over the legs. The
work described is a fraction of a microsecond.

The first suspicion was the loop around it -- `FitAlongLane` refits the whole remainder after every
refusal, which is O(N²) in the number of cuts. The measurement refuses that reading: 1.25 calls per
lane means the loop almost never retries. The cost is inside one call.

## What is visible without profiling

`Fit` allocates on every call: `keep` (a `vector<bool>`), `out`, `legM`, `headingRad`, `turnRad`,
and whatever `ReferenceLine` holds. Six or more allocations for 17 points, twenty thousand times --
which matches `heap taken under road-fit: 167 MB`. But allocation is ~100 ns and cannot be 78 us on
its own, so there is a second cost that has not been found yet and this item does not guess at it.

## What Unreal does, what RAGE does

**NEITHER FITS A REFERENCE LINE AT RUN TIME.** Unreal's Landscape Splines and RAGE's road network
are authored: the curve is a spline the artist placed, and its geometry is baked. This tree fits
one because the world arrives as OSM polylines over the wire, so the choice is MINE -- and being
mine, the bound has to be stated rather than inherited. A polyline of 17 points is 34 doubles, one
cache line's worth of work several times over; the bound is **under one microsecond a call**.

## What will show I was wrong

`streets: of paving, the fit` on OldTown. Today 620 ms; the arithmetic above says a few
milliseconds. If splitting `Fit` into its parts shows the time is genuinely in the mathematics
rather than in allocation and indirection, the bound in this item is wrong and it says so here
first.
