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

- [x] A straight route over flat ground lays with **zero** refused claims. Whatever each of
      the four is really asserting, a road with no corner and no slope is the simplest case
      there is, and a lay that cannot do it cannot be trusted on one that has both.
- [x] The drift bound is stated in something a corner-free route has -- per metre, or per
      corner **when there are corners** -- rather than a product that is zero on a straight.
- [x] `profile.Over` succeeds or the refusal names why, and `SampleCount() == 0` on a
      1997 m corridor is itself a refusal rather than a number the next reader averages.
- [x] The unit twin's pin (`Refused() <= 4`) drops to `== 0`.

## Comments

- 2026-08-24 -- found by the twin the same hour it became possible to write. board:1624
  narrowed `LayCorridor`'s door from `GroundStream &` (a thread pool) to the two queries it
  actually makes, and the first case through that door failed four claims immediately.
- The twin does NOT assert these away: it publishes each refusal and pins the count at four,
  so a fifth is a regression while the four stand as this item's subject. Asserting
  `Refused() == 0` today would have meant asserting the defect away.

---

## The root was one missing refusal, four layers up (2026-08-24)

`profile.Over` was failing with:

> an envelope is a vehicle standing in a world and not a set of limits ... this one leaves a
> vehicle term at zero or the air below nothing

The zero term was `DragArea`. `Rigging.cpp:188` computed it as
`DragCoefficient * FrontalM2` and **`Stand` accepted a vehicle that declared neither**. The
envelope then had no top speed to bound, `SpeedProfile::Over` refused, and every reader
downstream averaged an empty plan.

A term the plan divides by belongs to the stand-up that assembles it, not to the fourth layer
that discovers it missing. `Stand` now refuses, naming both numbers.

| | |
|---|---|
| stations over the 1997.756 m corridor | **0 -> 5 436** (the derived step of 0.3676 m implies about 5 400) |
| claims the lay refuses on a straight route | **4 -> 2** |

- **Proving test**: `test/unit/sim/ARigRefusesADeclarationItCannotDrive` -- a vehicle with no
  drag coefficient and one with no frontal area, each refused at the stand-up.
- **Negative control**: the refusal disabled -> both claims red, and `ACorridorIsLaidOverASyntheticRoute`
  goes back to zero stations. Reverted.
- The unit twin's pin drops from `<= 4` to `<= 2`.

**Still open**: the two remaining refusals -- the drift bound, which is
`0.05 * quantumM * Corners` and therefore ZERO on a road with no corner, and the
corner-support claim on ground that is flat by construction. Both are claims written against
the 753 km route reading as constants of that one drive.

## And it found a second hole of the same kind

Two unit fixtures (`TheDrawnCarAndItsContactsStandInOneFrame`, and this item's own twin) built
vehicles with no declared body and stood them up green. They could not have driven: the same
missing `DragArea` would have refused their speed plans. The new refusal caught both the hour
it landed.

---

## All four refusals are gone, and two were claims fitted to one drive (2026-08-24)

| claim | was | is |
|---|---|---|
| drift | `DriftM < 0.05 * quantumM * Corners` -- **zero bound on a road with no corner** | `DriftPerCornerM < 0.05 * quantumM`, and `Fitted` already carried the rate |
| corner support | `Strained * 200 < Corners` -- `0 * 200 < 0` is false | `Strained * 200 <= Corners`, true at 0 of 0 |
| plan-view speed | vacuous on an empty plan | holds, the plan has 5 436 stations |
| the profile | refused for a zero envelope term | holds, and `Stand` refuses that vehicle now |

Both of the first two were written against the 753 km route -- the drift claim's own prose
still said *"over 2300 corners"* -- and read as constants of that one drive. A claim stated as
a TOTAL over corners says something false about a road that has none; the same claim stated as
a RATE says nothing, which is correct.

```
NOTE stations in the plan = 5436 stations
NOTE claims this lay refuses on a straight route = 0 claims
```

## And the twin corrected me twice more

Its climb arm asserted that ground steeper than the drivetrain must be refused. **Measured, it
is not, and should not be**: a corridor is cut and filled into the terrain, so ground rising
at 36 % under legs declaring `maxGradient = 0.06` produces a **6 %** road and the climb gate
has nothing to refuse. I was wrong about the engine, not the engine about itself.

The arm now asserts what the machine actually does, and the refusal it does owe:

```
NOTE the steepest gradient the lay built over a 36 % wall = 6 %
NOTE the steepest gradient a 40 % route builds = -40 %
NOTE it refused: **AND NOTHING ON IT IS STEEPER THAN THE CAR CAN CLIMB.** ...
```

A route whose own legs declare 40 % against a rig that pulls 23.97 % IS refused. The wall is
earthworks; the declared gradient is the road.

- **Proving test**: `test/unit/sim/ACorridorIsLaidOverASyntheticRoute`, now 0 refusals on the
  straight, 6 % built over a 36 % hillside, and a 40 % route refused.
- Gate 238/238.
