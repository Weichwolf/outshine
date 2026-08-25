Type: bug
Parent: 1840
Area: sim
Tags: static_assert, refusal, proof

# A constexpr refusal is proven at compile time

`board:1840` turned `AsideRatePerM` into a function that refuses:

```cpp
src/sim/CorridorLay.h:76
[[nodiscard]] constexpr std::expected<double, std::string_view> AsideRatePerM(
    double budgetM, double heldMs) noexcept {
  if (!(heldMs > 0.0)) { return std::unexpected("... holds none"); }
  if (!(heldMs < std::numeric_limits<double>::infinity())) { return std::unexpected("... "); }
  return budgetM / (kLagMargin * Pilot::kSettleS * heldMs);
}
```

Neither refusal is proven by anything. `grep -rn 'AsideRatePerM' src test tools apps` finds four
call sites and all four pass a finite, positive speed:

| site | what it hands in |
|---|---|
| `src/sim/CorridorLay.cpp:301` | `inPlan.Fastest().Ms` -- the plan's own held speed, finite by construction |
| `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:121` | `way.Profile.Fastest().Ms`, and the result is folded to `0.0` on refusal at `:122` |
| `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:339, :431` | reads the laid `Corridor`, not the function |

The closure's proof -- the moon leg driving a SHIFTING corridor and claiming 0.358 m of a 0.75 m
step -- proves the SUCCESS path is scaled to the plan and not to drag, which is `board:1830`'s
subject and is a good measurement. It says nothing about either refusal. `grep -rn 'holds none'
test` returns nothing outside the source itself.

The function is `constexpr` and `noexcept`. Both refusals and the value are therefore decidable
at compile time, at zero runtime cost, beside the declaration -- which is what CLAUDE.md asks
for by name: *`static_assert` where sensible: layout/size/trait obligations proven at compile
time, beside the struct they guard*. A refusal nothing exercises is a refusal that can be
deleted by a future edit with the gate staying green.

The test helper at `:122` is its own small defect: `drive.AsideRatePerM = rate ? *rate : 0.0;`
writes exactly the zero the item was filed against, in the file that proves the item.

## What will be true

- [x] Two `static_assert`s beside `AsideRatePerM` in `src/sim/CorridorLay.h`: one that a
      zero/negative held speed does not have a value, one that infinity does not, and one that
      a finite speed gives the derived number.
- [x] The unit twin asserts the refusal TEXT of both, since the sentences are the door's answer
      and CLAUDE.md asks a refusal to carry its reason.
- [x] `test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:122` stops folding a refusal to
      zero -- it refuses the seat, so a rate that cannot be computed cannot look like a rate of
      nothing.
- [x] Negative control: either `if` deleted -> the `static_assert` fails to compile.

## Comments

- 2026-08-25 -- filed by the hourly review. The repair itself is right and cheap: the error type
  is `std::string_view`, so nothing allocates, and `constexpr`/`noexcept` survived the change.

**Closed.** Five `static_assert`s beside the function, which is what a `constexpr` refusal costs:

```cpp
src/sim/CorridorLay.h:85   static_assert(!AsideRatePerM(1.0, 0.0).has_value(), ...);
src/sim/CorridorLay.h:87   static_assert(!AsideRatePerM(1.0, -3.0).has_value(), ...);
src/sim/CorridorLay.h:89   static_assert(!AsideRatePerM(1.0, std::numeric_limits<double>::infinity()).has_value(), ...);
src/sim/CorridorLay.h:91   static_assert(AsideRatePerM(0.7195, 30.0).has_value(), ...);
src/sim/CorridorLay.h:93   static_assert(AsideRatePerM(0.7195, 30.0).value() > 0.0, ...);
```

All four call sites hand in a finite positive speed, so no runtime path could ever reach those
branches -- a refusal nothing reaches is a refusal nothing proves, and the tree's rule is
`static_assert` and the type system over checkers.

Negative control: the infinity refusal deleted ->

```
src/sim/CorridorLay.h:89:15: error: static assertion failed due to requirement
  '!AsideRatePerM(1., std::numeric_limits<double>::infinity()).has_value()':
  a rate scaled to an unbounded speed -- a declared vacuum -- is refused
```

The build fails. No test had to run.
