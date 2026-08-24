Type: bug
Parent: 1830
Area: sim
Tags: refusal, vacuum, naming

# An unbounded reach refuses instead of answering zero

`board:1830` closed with four boxes and two of them unmet. Its second box reads:

> *Where a number genuinely needs an unbounded top speed, the refusal is a returned reason
> naming the vacuum, not a zero or an infinity handed on.*

The function the item was filed against is unchanged in that respect:

```cpp
src/sim/CorridorLay.h:71   inline constexpr double kLagMargin = 4.0;
src/sim/CorridorLay.h:73   [[nodiscard]] constexpr double AsideRatePerM(double budgetM, double topMs) noexcept {
src/sim/CorridorLay.h:74     const double reachM = Pilot::kSettleS * topMs;
src/sim/CorridorLay.h:75     return reachM > 0.0 ? budgetM / (kLagMargin * reachM) : 0.0;
```

`topMs = inf` gives `reachM = inf`, `inf > 0.0` is true, and the function answers **0.0** --
the same silent zero the item measured, still reachable, still indistinguishable from a legal
rate. What changed is that the two call sites now pass `Fastest().Ms`
(`src/sim/CorridorLay.cpp:299-301`, `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:120-121`).
The trapdoor was walked away from, not closed.

**And the parameter still invites the old call.** It is named `topMs`, and `Envelope::TopMs()`
is a public method one include away; the one thing the repair established is that `TopMs()` is
the wrong argument. A name that says the wrong thing is the whole of the documentation this
tree allows itself.

The fourth box is also unmet:

> *Proving test: `ADriveTickHoldsTheCarToTheDeclaredWorld` drives its SHIFTING corridor on the
> moon -- the one whose lane centre steps -- and asserts the car follows the step.*

```cpp
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:164   const Rigged onMoon = Stand(car, kMoonMs2, 0.0);
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:166   CHECK(Straight(moonWay, onMoon, kEdgeM, error) && ...
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:230   const double stepM = 0.75;   // the SHIFTING corridor, on onEarth
```

The moon leg -- air density `0.0`, i.e. the vacuum -- still runs the STRAIGHT corridor, which
is the exact sentence the item filed against it: *"a frozen lane centre is the right answer by
accident."* The shifting corridor runs on Earth only.

## What will be true

- [ ] `AsideRatePerM` answers a refusal for a reach that is not finite, or the finite reach is
      established by its type before it arrives. A returned `0.0` for `inf` is removed.
- [ ] The parameter is named for what it must receive -- the speed the PLAN holds -- so
      `Envelope::TopMs()` reads wrong at the call site.
- [ ] `ADriveTickHoldsTheCarToTheDeclaredWorld` drives the SHIFTING corridor on the moon and
      asserts the car claims the step there too. Negative control: `AsideRatePerM` fed
      `Envelope::TopMs()` again -> red on the moon leg, the car never leaving its first offset.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1830`'s two numeric boxes are genuinely
  paid, with a measured admissible band for `kLagMargin` (2.9 to under 6, set to 4.0). These
  two are not, and the item closed without saying so.
