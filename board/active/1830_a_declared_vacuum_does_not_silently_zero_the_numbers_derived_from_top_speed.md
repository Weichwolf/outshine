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
