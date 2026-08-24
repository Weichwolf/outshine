Type: bug
Area: test, sim
Tags: mirror, regression-gate, drive

# The lateral rate has a unit twin, and the twin does not spell a formula of its own

`board:1814` changed the lateral rate that decides whether the drive arrives. The proof it
names is a live-network case that needs `--timeout 1200`:

> *Proving test: `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg`, run at
> `--timeout 1200`. Negative control, run: `kLagsToCover` back to 1.0 -> the wheel leaves at
> km 113.990 after 21 s.*

CLAUDE.md states what the mirror is for: *"the unit mirror is the REGRESSION GATE and it is
fast; the long device and corpus suites are the sporadic full proof, run when named"*. A number
whose only guard is a twenty-minute fetch-bound case is a number the gate cannot defend.

## What the mirror holds today

```
$ grep -rn 'AsideRate' test/unit/
test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:112:
    drive.AsideRatePerM = (kLaneHalfM - 0.5 * car.WidthM) / stood.Envelope.TopMs();
```

That is the **old, one-lag formula**, hand-written in the test. `board:1814`'s first acceptance
box reads *"One lateral rate, derived once from the declared vehicle, used by the corridor's
smoothing and by the tick's limiter"*; the three spellings in `src/` became one and a **fourth
spelling survives in the twin that would have to catch a change to it**. Set `kLagsToCover` to
any value and this test does not move.

`test/unit/sim/ACorridorIsLaidOverASyntheticRoute.cpp` -- the twin of the file the constant
lives in -- names `AsideRatePerM` nowhere at all.

## What will be true

- [ ] `test/unit/sim/ACorridorIsLaidOverASyntheticRoute` asserts `Corridor::AsideRatePerM`
      against the declared vehicle and the declared settle time, with the derivation stated in
      the case's own prose.
- [ ] `ADriveTickHoldsTheCarToTheDeclaredWorld` takes the rate from the corridor it drives
      instead of computing one, so no test carries a formula `src/` does not.
- [ ] The negative control runs in the fast gate: `kLagsToCover` at 1.0 -> a synthetic corridor
      whose lane centre steps, driven at the declared top speed, puts the deviation past the
      reserve and the unit case names it -- seconds, not twenty minutes, and no network.

**Closed.** `Sim::AsideRatePerM(budgetM, topMs)` is the one spelling; CorridorLay.cpp:330 and
`ADriveTickHoldsTheCarToTheDeclaredWorld:112` both call it, and the hand-rolled
`(kLaneHalfM - 0.5 * car.WidthM) / TopMs` is gone -- it omitted both `Pilot::kSettleS` and the
margin, so it was not a stale copy of the formula, it was a different formula.

Reading the same function was necessary and not sufficient: `Straight()` lays a corridor whose
`FineAside` is all zero and whose `FineLaneHalfM` is empty, so the lane centre never moves and
the rate multiplies nothing. `Shifting()` steps the centre 0.75 m at 50 m and fills both bands,
which is also the first gate case to fill `FineLaneHalfM` at all (board:1820 stands on that).

Proving test: `unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld`, the shifting-lane block.
Negative controls, both run: `kLagMargin = 8.0` -> FAIL, 0.466 m of the step unclaimed;
`kLagMargin = 1.0` -> FAIL, 0.294 m against a 0.2 m budget. Before this commit both values
were green, which is the defect the item names.
