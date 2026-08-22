Type: bug
Area: sim

**Every field of Ridden is written by the tick or does not exist**

`Ridden` (src/sim/DriveTick.h:22-60) declares fields that NO code writes -- DriveTick.cpp is
the only producer:

- `OffTheRoad` -- never assigned, and JUDGED by the drive suite:
  tools/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp:83 and
  tools/driver/ASecondRouteIsOnlyTwoCoordinates.cpp:66,79 break/CHECK on `rode.OffTheRoad`.
  The check can never fire; the real off-road signal travels as `LeftTheRoadAtM`/`BrokeAtM`
  with an early return, and the case only fails later via `Arrived` staying false. A verdict
  field the suite reads and nothing writes is a lie in the specification.
- `AlongM`, `PlannedMs`, `InLaneM`, `AsideM`, `EdgeM`, `RatioOfHold`, `CurvaturePerM`,
  `CurvatureRatePerM`, `Airborne` -- all default-initialised, never assigned (the Left*/Worst*
  twins are the written ones).

Demanded: `OffTheRoad` is set where the tick decides it (the `read.OffTheSurface > 0` return,
DriveTick.cpp:178-182) and the suites' checks become live, or the field dies and the suites
judge the signal that exists. The nine dead fields are deleted or written -- a struct that is
the tick's public product may not carry silent zeros a consumer can mistake for measurements.

---

**Closed (review 2026-08-22, evening).** Proof at HEAD (e5a1122): `OffTheRoad` is written where
the tick decides it (src/sim/DriveTick.cpp:179) and the suites' checks are live; the nine
silent-zero fields (`AlongM`, `PlannedMs`, `InLaneM`, `AsideM`, `EdgeM`, `RatioOfHold`,
`CurvaturePerM`, `CurvatureRatePerM`, `Airborne`) no longer exist in `Ridden`
(src/sim/DriveTick.h:23-52 carries only written fields). Task 1617 closed with gate 119/119,
Munich 40/0, Kyoto 35/0.
