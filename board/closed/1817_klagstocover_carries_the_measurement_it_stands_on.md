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

- [x] `kLagsToCover` is either (a) derived from a measured ratio with its population stated in
      this item -- one drive is one sample, so the population is the routes it was measured
      over -- or (b) named as a `[SET]` margin with the measured ratio it stands above.
- [x] Whichever it is, the drive's lag-to-budget ratio is PUBLISHED per route, so the factor's
      headroom is a number and not an argument.
- [x] Proving test: the ratio asserted as a shape (measured lag < kLagsToCover x pursuit lag)
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

**Closed.** The factor is named `kLagMargin` and it is `[SET]`, because the structure its old
name asserted was refuted: "two lags in series" justifies 1.235, and 1.235 is itself outside
what the drive tolerates. It is not free either -- the fast gate now pins it between two
driving facts, each with its own failure mode.

The lateral rate is ONE function, `Sim::AsideRatePerM(budgetM, topMs)` in CorridorLay.h, read
by the corridor and by its twin. Before this, `ADriveTickHoldsTheCarToTheDeclaredWorld:112`
recomputed a one-lag formula by hand, so the factor could be set to anything and no gate case
moved (board:1819).

Measured, `unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld`, a 300 m synthetic corridor whose
lane centre steps 0.75 m sideways at 50 m, side budget 0.2 m:

| kLagMargin | step still unclaimed at the end | worst offset while claiming it | verdict |
|---|---|---|---|
| 1.0 | 0 m | 0.294 m | red -- outside the budget |
| 1.2 | 0 m | 0.247 m | red |
| 1.5 | 0 m | 0.205 m | red, barely |
| 1.6 | 0 m | 0.195 m | green, the lower edge |
| **2.0** | 0 m | **0.168 m** | green, the value set |
| 3.0 | 0 m | 0.127 m | green |
| 4.0 | 0.182 m | 0.096 m | red -- the step is never claimed |
| 5.0 | 0.295 m | 0.076 m | red |
| 8.0 | 0.466 m | 0.048 m | red |

So the admissible band on this corridor is **1.6 to somewhere between 3 and 4**, and 2.0 sits
inside it nearer the lower edge. The reviewer's 1.235 is not a floor 2.0 stands 1.62x above --
it is a value the drive refuses: at 1.2 the car leaves its side budget by 47 mm.

The two bounds are not one assertion twice. Too small yanks the aim and the car lags out of the
budget it was laid for; too large stretches the catch-up past the route and leaves the car
pinned to the old lane centre. Reduce the margin and the LOWER check goes red; raise it and the
UPPER one does. Both were run.

The upper edge is corridor-dependent -- 300 m of road and a 0.75 m step -- and it would move
out on a longer one. The lower edge is not: it is the budget the corridor declares against the
lag the rate produces, and that ratio does not depend on how much road follows.

On the shipped Munich-Hamburg drive the same relation is published per route:

| | share of the pursuit lag (0.35975 m at top speed) |
|---|---|
| deviation at p99 | 0.869 x |
| worst excursion | 2.473 x |

The item asked for the shape `measured lag < kLagMargin x pursuit lag`. That assertion is not
written, and the reason is written instead: `kLagMargin x pursuitLag` IS `budgetM` by
construction, so against p99 it restates the budget check the drive already makes, and against
the worst excursion (2.473) it is red. The factor cannot be proven by a statement about the
drive it shapes. What survives it is the clearance, and board:1818 measures that.

Proving test: `unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld` -- the shifting-lane block,
23 checks, 331 ms. Negative controls, both run: `kLagMargin = 1.0` -> the lower check red at
0.294 m; `kLagMargin = 8.0` -> the upper check red with 0.466 m of the step unclaimed.

A second defect fell out of writing it, recorded because it was mine: the block first read
`Ridden::LeftAimStillMovingM`, which belongs to the off-the-road attribution family and is
therefore zero on every drive that arrives -- the upper check was a tautology and passed at
8.0. It reads `DriveState::HeldAsideM` now. And before that, the block sat after a second
`return Report();` and never ran at all; the case reported 18 checks and its old COVERS line
while I read the numbers as if they were new.
