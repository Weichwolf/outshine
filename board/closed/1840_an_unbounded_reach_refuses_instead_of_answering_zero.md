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

- [x] `AsideRatePerM` answers a refusal for a reach that is not finite, or the finite reach is
      established by its type before it arrives. A returned `0.0` for `inf` is removed.
- [x] The parameter is named for what it must receive -- the speed the PLAN holds -- so
      `Envelope::TopMs()` reads wrong at the call site.
- [x] `ADriveTickHoldsTheCarToTheDeclaredWorld` drives the SHIFTING corridor on the moon and
      asserts the car claims the step there too. Negative control: `AsideRatePerM` fed
      `Envelope::TopMs()` again -> red on the moon leg, the car never leaving its first offset.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1830`'s two numeric boxes are genuinely
  paid, with a measured admissible band for `kLagMargin` (2.9 to under 6, set to 4.0). These
  two are not, and the item closed without saying so.

**Closed, both halves.**

```cpp
src/sim/CorridorLay.h:78   [[nodiscard]] constexpr std::expected<double, std::string_view> AsideRatePerM(
src/sim/CorridorLay.h:79       double budgetM, double heldMs) noexcept {
src/sim/CorridorLay.h:84     return std::unexpected("... an unbounded one -- a declared vacuum, where drag allows
                              any speed at all -- is not a speed anything holds");
```

The trapdoor is shut rather than walked away from: `inf > 0.0` used to pass the guard and the
answer was a silent zero. And the parameter is `heldMs`, not `topMs` -- the one thing
board:1830 established is that `Envelope::TopMs()` is the wrong argument, and a name that
invites it back is the whole of the documentation this tree allows itself.

**The vacuum leg drives a corridor the rate can move.** It ran `Straight`, where a frozen lane
centre is the right answer by accident. It runs both now: the straight one for the arrival that
proves gravity and grip, and a stepping one that measures the aim.

```
NOTE the lane centre the vacuum drive claimed = 0.357789645 m
NOTE what it was asked to claim               = 0.75 m
NOTE how far that ride reached                = 128.856267 m
```

0.358 m of a 0.75 m step, claimed in 128.9 m of vacuum road. It does NOT claim the whole step,
and the case says why rather than loosening: a sixth of the gravity is a sixth of the lateral
force, and the vacuum plans faster, so the rate scaled to that plan is smaller.

Negative control, run: both call sites handed `Envelope::TopMs()` again ->

```
NOTE the lane centre the vacuum drive claimed = 0 m
FAIL ...:200  **AND IN A VACUUM THE LANE CENTRE STILL MOVES**
```

Frozen at zero, which is board:1830's measurement arriving in the case that could not see it
before.
