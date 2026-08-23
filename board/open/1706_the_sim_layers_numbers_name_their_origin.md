Type: issue
Area: sim

**The sim layer's numbers name their origin**

1693 repaid src/data, 1700 repaid the generators; src/sim is the remaining naked layer.
Every number below carries neither derived/measured/[SET] nor population:

- src/sim/DriveTick.cpp:15-16 — `kResectM = 4.0`, `kFromM = 50.0`
- src/sim/DriveTick.cpp:42 — `reins.SettleS = 1.0`
- src/sim/DriveTick.cpp:51 — the `3.0 * drive.LostM` resect window factor
- src/sim/DriveTick.cpp:87 — the `1.0` fallback divisor when `BrakeMs2()` is not positive
  (a silent brake of 1 m/s² is a picture choice nobody published)
- src/sim/DriveTick.cpp:90 — the 12-step braking lookahead
- src/sim/DriveTick.cpp:192 — the 20.0 m arrival margin
- src/sim/DriveAssembly.cpp:38-39 — `kPatienceS = 900.0`, `kJoinMs = 20.0`
- src/sim/DriveAssembly.cpp:83 — `kCorridorRing = 2`
- src/sim/DriveAssembly.cpp:254 — the 50.0 m stand-at search window

Demanded: each carries origin, unit and population beside its declaration, the way
SourceSet.cpp:10-14 and TreeGrower.cpp:14-19 now do; where a number is a tuned picture
choice, `[SET]` says so and what it was tuned against.
