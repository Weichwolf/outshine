Type: bug
State: withdrawn
Area: world, generators
Tags: measured

# A run of places is stored as places, not as doubles two at a time

**Benchmark** — Unreal stores a polygon as `TArray<FVector2D>` and a path as `TArray<FVector>`;
RAGE's spline and boundary types hold their own point type. **Neither stores a run of coordinates as
a flat float array the caller indexes by stride.** This tree does, in the one place where the
coordinates are geodetic and the stride is the only thing saying which of the two is latitude.

## Measured 2026-09-02, while giving Geodesy a type

`OsmField::Points()` hands back `std::span<const double>` and every caller reads it as
`pts[i * 2]` and `pts[i * 2 + 1]`. Giving the geodesy functions a
`LongitudeLatitudeHeight` parameter -- which is right, and which board:2093 did -- turned every one
of those call sites into this:

```cpp
EnuOffsetM({.LongitudeDeg = ring[k * 2 + 1], .LatitudeDeg = ring[k * 2]}, ...)
```

That is UGLIER than what it replaced, and the ugliness is information: the type at the boundary is
correct and the storage behind it is not. A `std::span<const LongitudeLatitudeHeight>` would make
the same call read `EnuOffsetM(from, ring[k])`.

It is also where the swap can actually happen. `ring[k * 2]` is latitude and `ring[k * 2 + 1]` is
longitude by convention only, and the convention is written nowhere the compiler can read.

## What it costs and why board:2093 did not do it

The rings feed the OSM vector path -- building footprints, water bodies, street centrelines -- which
is what both control places draw. A change to how those points are stored is a change the digests
will judge, and it deserves a commit whose subject is that change and whose digests are checked
before and after, not a line inside a naming pass.

## Done when

`Points()` returns a span of places, nothing indexes a coordinate run by stride 2, and
`bugprone-easily-swappable-parameters` has no finding left that comes from a lat/lon pair.
