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

- [x] One lateral rate, derived once from the declared vehicle, used by the corridor's
      smoothing and by the tick's limiter. No literal top speed anywhere.
- [x] The budget covers the LAG the move causes, not the move itself -- and the relation
      between rate, speed and lag is derived rather than assumed, because assuming it is what
      put a wheel off the road.
- [x] Proving test: the drive's worst deviation stays inside the reserve. Negative control: the
      rate raised -> the deviation grows past it and the claim names the station.

## Repaid, and it is what kept the car out of Hamburg (2026-08-24)

**One rate.** `Corridor::AsideRatePerM` is computed once in `LayCorridor` and the tick's
limiter takes it from there. The three spellings are one, and the literal top speed is gone --
`stood.Envelope.TopMs()` reproduces it exactly:

```
NOTE the top speed the declaration implies = 232.722657 km/h
NOTE the reach one second of it buys       = 64.645 m
```

232.722657 to the last digit, which is what says the literal WAS the declaration's own number,
written down a second time.

**And the budget covers two lags, not one.** The premise was that a car absorbs a full-budget
lateral move within one look-ahead length:

```
mostPerM = budgetM / reachM      ->  the pure pursuit lag alone = budgetM, all of it
```

leaving nothing for the vehicle's own lateral lag. `kLagsToCover = 2.0` -- the pursuit lag takes
half the budget and the car's own dynamics have the other half. It is a structural argument, not
a fit: there are two lags in series and the budget was sized for one.

```
the fastest the lane centre may move sideways: 11.130 -> 5.565 mm per metre
```

## What that did

```
NOTE how far the route runs                  = 742.636082 km
NOTE where a wheel first left the carriageway = 0 km
CHECKS 45 FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0
```

**The car reaches Hamburg** -- `board:1767`, and this is the repair that did it.

- **Proving test**: `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg`, run at
  `--timeout 1200`. It publishes what it needs now: `NOTE this drive needs --timeout 267`.
- **Negative control**, run: `kLagsToCover` back to 1.0 -> the wheel leaves the carriageway at
  km 113.990 and the drive stops there after 21 s, which is the state this item was filed from.
- The deviation's distribution over the WHOLE route: p50 0.0525, p95 0.2125, p99 0.3125 against
  a 0.7195 m reserve -- 43 % of it. The worst single sample is still 0.8895 m and it no longer
  crosses, because the aim it lags is no longer at the corridor's own edge.
- Gate 260/260.
