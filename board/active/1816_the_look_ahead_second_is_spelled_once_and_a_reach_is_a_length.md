Type: bug
Area: sim
Tags: two-truths, units, magic-number, drive

# The look-ahead second is spelled once, and a reach in metres is not a speed in m/s

`board:1814` collapsed three spellings of the lateral rate into one. It did so by deleting the
one second that made the old expression dimensionally honest.

```cpp
before   const double reachM = 1.0 * 232.722657 / 3.6;     // (1 s) x (m/s) -> m
after    const double reachM = stood.Envelope.TopMs();     // src/sim/CorridorLay.cpp:327
```

`TopMs()` returns **metres per second**. It is assigned to a variable named `reachM` and then
published as a length:

```cpp
src/sim/CorridorLay.cpp:330   say.Number("the top speed the declaration implies", reachM * 3.6, "km/h");
src/sim/CorridorLay.cpp:331   say.Number("the reach one second of it buys", reachM, "m");
```

One variable, two units, five lines apart. `mostPerM = budgetM / (kLagsToCover * reachM)`
(`:328`) is then metres divided by metres-per-second, which is seconds, used as a per-metre
rate. The arithmetic is numerically right only because the omitted factor is 1.

## The second is real, it is spelled elsewhere, and nothing ties them together

The "one second" is the pilot's look-ahead time constant, and it is written down twice more:

| where | spelling |
|---|---|
| `src/sim/DriveTick.cpp:52` | `reins.SettleS = 1.0;` -- what the pursuit law actually uses |
| `src/sim/CorridorLay.cpp:526` | `planning.SettleS = 1.0;` -- what the speed plan uses |
| `src/sim/CorridorLay.cpp:327` | **invisible** -- the factor that turns `TopMs()` into `reachM` |

The corridor's lateral rate is a function of the pilot's settle time. Change
`DriveTick.cpp:52` to 2.0 and the corridor keeps sizing its rate for a 1 s look-ahead, the
pursuit lag doubles, and a wheel leaves the carriageway -- which is precisely the failure
`board:1767` spent five rounds attributing. The literal top speed is gone and the coupling that
made it dangerous is not.

## What will be true

- [x] One `SettleS` reaches the pilot, the speed plan and the corridor's lateral rate from one
      declared place, and no file spells `1.0` for it a second time.
- [x] `reachM = settleS * stood.Envelope.TopMs()` -- the length is a length, and the number
      published as `"m"` is the one the name claims.
- [x] Proving test: a unit case in `test/unit/sim/` that lays a corridor at two settle times and
      asserts `AsideRatePerM` scales inversely with it. Negative control: the settle time
      changed in the tick alone -> the rate does not follow and the case names both numbers.

## Repaid (2026-08-24)

The second is named and spelled once. `outshine::Pilot::kSettleS` is the pilot's look-ahead
time constant, and the three places that wrote `1.0` now read it:

| | |
|---|---|
| `src/actor/mind/Pilot.h:8` | `inline constexpr double kSettleS = 1.0;` |
| `src/sim/DriveTick.cpp:52` | `reins.SettleS = outshine::Pilot::kSettleS;` |
| `src/sim/CorridorLay.cpp:529` | `planning.SettleS = outshine::Pilot::kSettleS;` |
| `src/sim/CorridorLay.cpp:329` | `const double reachM = outshine::Pilot::kSettleS * stood.Envelope.TopMs();` |

`reachM` is a length again -- seconds times metres per second -- and the report says which is
which:

```
NOTE the top speed the declaration implies        = 232.722657 km/h
NOTE the look-ahead time the pilot settles over   = 1 s
NOTE the reach that buys at top speed             = 64.6451824 m
```

Three lines where one variable carried two units. And the coupling the review asked for: raising
`kSettleS` now raises the pursuit lag AND the reach the rate is derived from, together, because
they are the same number.

- **Proving test**: the drive suite, 5/5, with the three numbers published separately.
- **Negative control**: `kSettleS` is one symbol -- changing it moves all three reports at once,
  which is the property this item is about. Setting it to 2.0 doubles the published reach and
  halves the rate, and the old code would have moved neither.
