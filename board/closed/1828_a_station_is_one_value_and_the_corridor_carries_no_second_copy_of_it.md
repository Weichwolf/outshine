Type: bug
Area: sim
Tags: layering, dead-code, regression, values
Regresses: 1820

# A station is one value, and the corridor carries no second copy of it

`board:1820` closed on this sentence, and `CLAUDE.md:292` repeats it:

> *three bands that had to agree by convention become one Station array whose extent is the type*

**The bands are still there.** One line ABOVE the line the map cites:

```cpp
src/sim/CorridorLay.h:42   std::vector<double> RoadM, HalfWidthM, LaneHalfM, AsideM;
src/sim/CorridorLay.h:43   std::vector<Station> Fine;
```

Four coarse bands and one fine array, both shipped in `Corridor`, both filled by `LayCorridor`,
and the fine array is DERIVED from the coarse ones by an index division that is exactly the
convention the item said it removed:

```cpp
src/sim/CorridorLay.cpp:295   const size_t post = (size_t)((double)at * fineM / spanM);
src/sim/CorridorLay.cpp:296   const size_t band = post < asideM.size() ? post : asideM.size() - 1;
src/sim/CorridorLay.cpp:297   stations[at].AsideM = asideM[band];
```

## The two copies have already diverged, and a car is seated at the wrong one

After line 297 the taper runs, and it writes `stations[].AsideM` only
(`CorridorLay.cpp:348, 357, 379`). `asideM[]` keeps the untapered value. Then:

```cpp
src/sim/DriveAssembly.cpp:261   const double startAsideM = asideM.empty() ? 0.0 : asideM.front();
src/sim/DriveAssembly.cpp:265   body.PositionM[0] = start.EastM - std::sin(start.HeadingRad) * startAsideM;
```

The car is placed on the UNTAPERED lane centre. The first tick then reads the TAPERED one:

```cpp
src/sim/DriveTick.cpp:87   const Station &here = way.At(at.AlongM);
src/sim/DriveTick.cpp:88   const double wantAsideM = here.AsideM;
```

Wherever the lead pass or the edge clamp moved station 0 -- which is exactly the narrow
start the taper exists for -- the drive begins with a lateral error nobody declared. On the
Munich--Hamburg drive station 0 happens to be unclamped, so the defect is silent today; it is
a divergence between two spellings of one number, not an off-by-one.

## `RoadM` is written and read by nothing

```cpp
src/sim/CorridorLay.cpp:225   roadM = heightM;
```

`grep -rn '\bRoadM\b' src test apps tools` outside `CorridorLay.cpp` returns exactly one line:
its own declaration at `CorridorLay.h:42`. A `std::vector<double>` the length of the corridor
(371 000 posts on the driver's route), filled every lay, read never.

## The twin has to populate both, and does

```cpp
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:79   way.LaneHalfM.assign(posts, kLaneHalfM);
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:83   station.LaneHalfM = kLaneHalfM;
```

Two assignments of one fact in the fixture written by the same hour as the repair. Line 79 is
read by nothing the tick touches. A fixture that must fill a dead band is the shape of the
defect, stated by the test itself.

## And `apps/driver` reads the coarse band beside the fine one

```cpp
apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:473   Note("the half lane there", drive.Way.Fine[fine].LaneHalfM, "m");
apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:478   Note("LaneHalfM at the coarse post", drive.Way.LaneHalfM[post], "m");
```

## What will be true

- [ ] `Corridor` holds `Fine` and nothing parallel to it. `RoadM`, `HalfWidthM`, `LaneHalfM` and
      `AsideM` are gone from the header; the per-post work in `LayCorridor` runs over locals
      that never leave the function, or over `Fine` directly.
- [ ] `AssembleDrive` seats the car from `way.At(0.0).AsideM` -- the same value the first tick
      reads -- so the seat and the aim cannot disagree.
- [ ] `Station` carries a `static_assert` on its size and on `std::is_trivially_copyable`,
      beside the struct, the way `Ridden` does at `src/sim/DriveTick.cpp:18`.
- [ ] Proving test: `test/unit/sim/ACorridorIsLaidOverASyntheticRoute` lays a route whose
      lane centre the taper MOVES at station 0 -- a narrowing inside the first look-ahead --
      and asserts the seat position and the first tick's `wantAsideM` agree to 1e-9 m.
      Negative control: `startAsideM` restored to `asideM.front()` -> red, naming the metres.
- [ ] Proving test: `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` fills `Fine` and
      nothing else, and still drives to arrival on earth and on the moon.
- [ ] `CLAUDE.md:292` stops claiming what is not true; the row says what the header holds.

## Comments

- 2026-08-24 -- filed by the hourly review against the hour's own work. `board:1820` is
  reopened by this item rather than sharpened, because its closing sentence is a statement
  about the header that the header contradicts one line above the line the closure cites.

**Closed.** `Corridor` carries `Fine` and nothing parallel to it -- `RoadM`, `HalfWidthM`,
`LaneHalfM` and `AsideM` are locals of `LayCorridor` now and leave no second copy behind. The
compiler enforces what board:1820 only asserted, and no runtime check has to.

```cpp
src/sim/CorridorLay.h:31   static_assert(sizeof(Station) == 24, ...);
src/sim/CorridorLay.h:32   static_assert(std::is_trivially_copyable_v<Station>, ...);
src/sim/DriveAssembly.cpp:260   const double startAsideM = out.Way.Laid() ? out.Way.At(0.0).AsideM : 0.0;
```

The seat reads the same call the first tick reads (`DriveTick.cpp:87`). `RoadM` -- written every
lay over 371 000 posts on the driver's route, read by nothing -- is gone. The twin no longer
fills a dead band, and `apps/driver` no longer notes the coarse post beside the fine one.

Measured on a 600 m corridor whose first six legs are 2.6 m half-width and the rest 6.5 m:

```
NOTE the lane centre the taper holds at station 0 = -1.3 m
NOTE the furthest it moves anywhere               = 2.664 m
```

**1.3 m** is the lateral error the old seat carried on this corridor, because the taper wrote
the fine station and the seat read the coarse band.

Proving test: `unit/sim/ACorridorIsLaidOverASyntheticRoute`, the narrowing block. Negative
control, run: every leg given the same 6.5 m half-width -> the taper moves nothing, station 0
sits at a constant -3.25 m, and the case goes RED at :402 saying so -- it refuses to stand on a
corridor where the two spellings could not have disagreed.

The structural half needs no control: a second band cannot be written because there is no second
band to write.
