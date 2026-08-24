Type: bug
Area: actor/path, sim
Tags: refusal, numbers, vacuum, telemetry
Depends: 1627

# A declared vacuum does not silently zero the numbers derived from top speed

`board:1627` made a vacuum LEGAL and said so in its closure:

> *SpeedProfile's gate accepts a vacuum, TopMs is unbounded there (infinity, guarded at
> CorridorLay's drag-at-top)*

```cpp
src/actor/path/SpeedProfile.h:39   [[nodiscard]] double TopMs() const {
src/actor/path/SpeedProfile.h:40     const double resistance = 0.5 * AirDensity * DragArea;
src/actor/path/SpeedProfile.h:41     return resistance > 0.0 ? std::sqrt(DriveN / resistance)
src/actor/path/SpeedProfile.h:42                             : std::numeric_limits<double>::infinity();
```

Two numbers landed this hour that divide by that infinity, and neither is guarded.

## The speed histogram collapses to one bin

```cpp
src/actor/path/SpeedProfile.cpp:189   BinMs_ = within.TopMs() / (double)kSpeedBins;
src/actor/path/SpeedProfile.cpp:191     if (BinMs_ > 0.0) {
src/actor/path/SpeedProfile.cpp:192       const size_t bin = (size_t)(Held_[at] / BinMs_);
```

`inf > 0.0` is true, so the guard passes. `Held_[at] / inf` is 0 for every station, so all
512 bins but the first are empty and

| query | what it answers in vacuum |
|---|---|
| `Quantile(0.5)` | `(0 + 0.5) * inf` = **inf** |
| `Quantile(0.95)` | inf |
| `StationsUnder(ms)` | `upTo = (size_t)(ms / inf) = 0`, loop never runs, **0** for every ms |
| `BinMs()` | inf |

`board:1785` closed on *"the plan publishes its own distribution and the case judges the
instrument against the plan's samples"*. In a vacuum the instrument publishes infinity and
zero, and nothing refuses.

## The lane-centre rate collapses to zero, on the same input

```cpp
src/sim/CorridorLay.h:66   [[nodiscard]] constexpr double AsideRatePerM(double budgetM, double topMs) {
src/sim/CorridorLay.h:67     const double reachM = Pilot::kSettleS * topMs;
src/sim/CorridorLay.h:68     return reachM > 0.0 ? budgetM / (kLagMargin * reachM) : 0.0;
```

`reachM = inf`, so the rate is `budgetM / inf` = **0 m per m**. Every consumer then freezes:

```cpp
src/sim/CorridorLay.cpp:310     const double most = mostPerM * fineM;          // 0
src/sim/DriveTick.cpp:93        const double mayMoveM = drive.AsideRatePerM * speedMs * dtS;   // 0
```

`most == 0` makes the taper sweeps at `CorridorLay.cpp:353-376` force every station's `AsideM`
to station 0's value, and `mayMoveM == 0` makes `HeldAsideM` constant for the whole drive. A
corridor planned in vacuum has no lane-centre plan at all, and the refusal for that is a
returned `0.0` that reads like a legal rate.

## The tree's own twin drives this path and proves nothing about it

```cpp
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:122   drive.AsideRatePerM =
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:123       outshine::Sim::AsideRatePerM(kLaneHalfM - 0.5 * car.WidthM, stood.Envelope.TopMs());
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:167   const Rigged onMoon = Stand(car, kMoonMs2, 0.0);
```

The moon leg runs with `AsideRatePerM == 0` and arrives, because its synthetic corridor is
straight and one lane wide, so a frozen lane centre is the right answer by accident. The case
that exists to prove the vacuum is legal is the case that hides what the vacuum does.

## What will be true

- [ ] `TopMs()` stops being the source of a divisor. A histogram's span comes from the plan's
      own samples (`max(Held_)`), which is finite whatever the air is; the pilot's reach comes
      from the speed the plan actually holds, not from the speed drag would eventually allow.
- [ ] Where a number genuinely needs an unbounded top speed, the refusal is a returned reason
      naming the vacuum, not a zero or an infinity handed on.
- [ ] Proving test: `test/unit/actor/path/ASpeedPlanScalesWithTheDeclaredGravity` or its
      neighbour plans a curved road in `airDensityKgM3 = 0` and asserts `Quantile(0.5)` lies
      between the plan's own slowest and fastest samples, and that `StationsUnder(top)` equals
      the sample count. Negative control: `BinMs_` restored to `TopMs()/512` -> red, printing
      `inf`.
- [ ] Proving test: `ADriveTickHoldsTheCarToTheDeclaredWorld` drives its SHIFTING corridor on
      the moon -- the one whose lane centre steps -- and asserts the car follows the step.
      Negative control: today's tree -> red, the car never leaves its first offset.

## Comments

- 2026-08-24 -- filed by the hourly review. The vacuum is not an edge case somebody might
  declare; it is a case this tree ships a proof for and drives in its own fast gate.

**Closed.** Both numbers come from the plan's own samples now.

```cpp
src/actor/path/SpeedProfile.cpp:195   if (at == 0 || Held_[at] > Fastest_.Ms) { ... }   // after every sweep
src/actor/path/SpeedProfile.cpp:202   BinMs_ = Fastest_.Ms / (double)kSpeedBins;
src/sim/CorridorLay.cpp:299           const double fastestMs = inPlan.Fastest().Ms;
src/sim/CorridorLay.cpp:301           const double mostPerM = AsideRatePerM(budgetM, fastestMs);
```

`Fastest()` is finite whatever the air is, because the acceleration sweep bounds every station
from a finite entry. Measured in a declared vacuum on the hump fixture:

| | before | after |
|---|---|---|
| the histogram's resolution | **inf** | 0.496 km/h |
| `Quantile(0.5)` | **inf** | 12.655 km/h |
| `StationsUnder(fastest)` | **0** | 801 of 801 |
| the fastest the plan holds | -- | 254.084 km/h |

**And the repair moves a number that was calibrated against the old divisor.** The lane-centre
rate is larger when it is scaled to the speed the plan HOLDS rather than the speed drag would
eventually allow, so the margin that covers it is larger. Re-measured on the twin:

| kLagMargin | worst offset while claiming a 0.75 m step | verdict |
|---|---|---|
| 2.0 | 0.271 m | red, over the 0.2 m budget |
| 2.5 | 0.221 m | red |
| 2.8 | 0.202 m | red, barely |
| 3.0 | 0.192 m | green, the lower edge |
| **4.0** | **0.159 m** | green, set here |
| 5.0 | 0.137 m | green |
| 6.0 | 0.117 m | red -- 0.054 m of the step never claimed |

The admissible band is **2.9 to under 6**, where board:1817 measured 1.6 to under 4 against the
old divisor. Set to 4.0, the middle.

On the shipped Munich--Hamburg drive the rate does not change at all, and the reason is worth
recording: `Fastest()` there is **232.723 km/h**, which IS `TopMs()` -- a motorway route reaches
the speed drag allows, so the old divisor was accidentally right on the one route that was
measured. The drive holds: 5/5, least clearance 0.160301873 m, worst deviation 0.878 m (was
0.890), and the room-at-p01 against deviation-at-p99 relation improves from 1.752 to 1.810.

Proving test: `unit/actor/path/ASpeedPlanScalesWithTheDeclaredGravity`, the vacuum block.
Negative control, run: `BinMs_` taken from `within.TopMs()` again -> resolution `inf`, p50
`inf`, `StationsUnder` = 0, three claims red.
