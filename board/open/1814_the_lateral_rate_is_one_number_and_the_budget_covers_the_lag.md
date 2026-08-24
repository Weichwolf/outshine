Type: bug
Parent: 1767
Area: sim
Tags: two-truths, magic-number, measured, drive

# The lateral rate is one number, and the budget covers the lag rather than the move

Found by attributing `board:1767`'s wheel-off-the-carriageway at km 113.990 all the way down.

## What happens there is not special: it is the largest lane shift, taken at the maximum rate

```
NOTE where it was last calm before that worst  = 113.927091 km
NOTE how far the excursion ran                 =  62.930 m
NOTE the aim where it was calm                 = -2.847 m
NOTE the aim at the worst                      = -2.125 m
NOTE how far the aim moved between them        =  0.722 m
```

**0.722 m over 62.93 m is 11.479 mm per metre**, against the corridor's own declared maximum of
**11.130 mm per metre**. The lane centre moves at its limit for 63 metres and the car, at
176 km/h, arrives 0.888 m behind it. The aim has finished moving by the time the wheel crosses,
which is why an earlier reading of this item saw a settled aim and concluded the excursion had
no cause.

## The rate's premise is wrong by a quarter

```cpp
const double reachM = 1.0 * 232.722657 / 3.6;      // CorridorLay.cpp:320
const double mostPerM = budgetM / reachM;          //  = 0.7195 / 64.645 = 11.130 mm/m
```

The premise is that a car absorbs a **full-budget** lateral move within **one reach length**.
Measured, it does not: the move was 0.722 m and the lag was **0.888 m**, which is **1.23x the
budget**. The budget is spent on the move itself and there is nothing left for the lag it causes.

## And the rate is spelled three times, one of them a literal

| where | spelling |
|---|---|
| `src/sim/CorridorLay.cpp:320` | `budgetM / (1.0 * 232.722657 / 3.6)` -- **a top speed as a magic literal**, beside `stood.Envelope.TopMs()` two hundred lines below at `:456` |
| `src/sim/DriveAssembly.cpp:289` | `(0.5 * narrowestLaneM - 0.5 * carWidthM) / (1.0 * stood.Envelope.TopMs())` |
| `src/sim/DriveTick.cpp` | `mayMoveM = drive.AsideRatePerM * speedMs * dtS` |

Three spellings of one truth, and the numerator of the first two is the same quantity written
two different ways -- `budgetM` against `0.5 * narrowestLane - 0.5 * carWidth`, which is what
`budgetM` IS (`CorridorLay.cpp:306`).

## What will be true

- [ ] One lateral rate, derived once from the declared vehicle, used by the corridor's
      smoothing and by the tick's limiter. No literal top speed anywhere.
- [ ] The budget covers the LAG the move causes, not the move itself -- and the relation
      between rate, speed and lag is derived rather than assumed, because assuming it is what
      put a wheel off the road.
- [ ] Proving test: the drive's worst deviation stays inside the reserve. Negative control: the
      rate raised -> the deviation grows past it and the claim names the station.
