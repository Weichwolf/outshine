Type: bug
Parent: 1624
Area: sim
Tags: corridor, first-unit-reach, fitted-to-one-route

# A straight synthetic route lays without refusing its own claims

The first unit case ever to reach `LayCorridor` (board:1624) laid a straight 2 km route over
a flat synthetic ground and the lay refused **four of its own published claims**, then
produced a speed plan with **zero stations** over a corridor 1997.756 m long.

```
NOTE the lay refused: **AND WHAT IS LEFT IS DRIFT, WHICH NO CORNER CAN CORRECT.** ...
NOTE the lay refused: **AND WHERE THE DATA CANNOT SUPPORT A ROAD AT ANY RADIUS THE CAR CAN TURN ...
NOTE the lay refused: the plan view alone gives a speed at every station, before the ground is consulted
NOTE the lay refused: and a speed profile is solved over the whole corridor from its geometry alone
NOTE ground queries the lay made = 67 queries
NOTE stations in the plan = 0 stations
NOTE the corridor's length = 1997.75566 m
```

## What each one looks like

| claim | why it plausibly refuses here |
|---|---|
| the drift term | `fitted.DriftM < 0.05 * quantumM * fitted.Corners` -- a straight route has **no corners**, so the bound is 0 and any residual at all fails it. The claim's own prose says *"over 2300 corners"*: it was written against the 753 km route and reads as a constant of that one drive |
| the corner-support claim | reports data that cannot support a road at any drivable radius, on ground that is flat by construction |
| the plan-view speed | *"a speed at every station, before the ground is consulted"* -- with zero stations, vacuously unmet |
| the profile | `profile.Over(...)` at `CorridorLay.cpp:524` fails, and everything downstream reads an empty plan |

The profile step is derived at `:520-521`: `0.5 * 1.5 * tightestM * 0.1` = 0.0750 x tightestM
= **0.3676 m** for this vehicle, so a 1997.756 m corridor should carry roughly 5 400 stations.
It carries none.

## What will be true

- [ ] A straight route over flat ground lays with **zero** refused claims. Whatever each of
      the four is really asserting, a road with no corner and no slope is the simplest case
      there is, and a lay that cannot do it cannot be trusted on one that has both.
- [ ] The drift bound is stated in something a corner-free route has -- per metre, or per
      corner **when there are corners** -- rather than a product that is zero on a straight.
- [ ] `profile.Over` succeeds or the refusal names why, and `SampleCount() == 0` on a
      1997 m corridor is itself a refusal rather than a number the next reader averages.
- [ ] The unit twin's pin (`Refused() <= 4`) drops to `== 0`.

## Comments

- 2026-08-24 -- found by the twin the same hour it became possible to write. board:1624
  narrowed `LayCorridor`'s door from `GroundStream &` (a thread pool) to the two queries it
  actually makes, and the first case through that door failed four claims immediately.
- The twin does NOT assert these away: it publishes each refusal and pins the count at four,
  so a fifth is a regression while the four stand as this item's subject. Asserting
  `Refused() == 0` today would have meant asserting the defect away.
