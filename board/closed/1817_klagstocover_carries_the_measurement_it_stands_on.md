Type: bug
Area: sim
Tags: magic-number, measured, drive

# kLagsToCover carries the measurement it stands on, and it is not 2

`src/sim/CorridorLay.cpp:17`

```cpp
constexpr double kLagsToCover = 2.0;
```

`board:1814`'s closing note argues it structurally:

> *`kLagsToCover = 2.0` -- the pursuit lag takes half the budget and the car's own dynamics
> have the other half. It is a structural argument, not a fit: there are two lags in series and
> the budget was sized for one.*

**Two lags in series is an argument for a factor of `1 + secondLag/firstLag`, not for 2.** The
factor 2 asserts the two lags are EQUAL, and the same item measured them and they are not:

```
the pursuit lag at the old rate  = budgetM         = 0.7195 m   (rate x reach, by construction)
the total lag measured           =                   0.8884 m   (board:1812, km 113.990)
so the vehicle's own lateral lag =                   0.1689 m   = 0.235 x the pursuit lag
```

The measurement supports **1.235**. `2.0` is a safety factor of 1.62x on top of it, chosen and
not derived, and CLAUDE.md's bar is that every number carries its origin (derived, measured or
`[SET]`) with unit and population. A `constexpr` in an anonymous namespace carries none, and
the board entry that is supposed to carry it states a derivation the tree's own numbers
contradict.

A conservative factor is a defensible engineering choice. Presenting it as structural is not,
because the next reader who needs to move it will look for the structure and find an assertion.

## What will be true

- [ ] `kLagsToCover` is either (a) derived from a measured ratio with its population stated in
      this item -- one drive is one sample, so the population is the routes it was measured
      over -- or (b) named as a `[SET]` margin with the measured ratio it stands above.
- [ ] Whichever it is, the drive's lag-to-budget ratio is PUBLISHED per route, so the factor's
      headroom is a number and not an argument.
- [ ] Proving test: the ratio asserted as a shape (measured lag < kLagsToCover x pursuit lag)
      rather than the factor asserted as a value. Negative control: the factor reduced to the
      measured 1.235 -> the assertion is at its edge and the case says so.

## Comments

- The residual `board:1814` did not close: its own acceptance box reads *"the drive's worst
  deviation stays inside the reserve"* and is ticked `[x]`, while the closing measurement in
  the same file reads *"The worst single sample is still 0.8895 m"* against a 0.7195 m reserve.
  What the case actually asserts is `quantile(0.99) < BudgetM`
  (`apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg.cpp:172`). The p99 bar is the
  right instrument and the argument for it is sound; the box that claims the worst is bounded
  is not. `board:1818` carries the invariant that IS missing.
