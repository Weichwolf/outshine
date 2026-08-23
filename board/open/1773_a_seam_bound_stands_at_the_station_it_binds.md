Type: bug
Parent: 1767
Area: actor/path
Tags: speed-profile, over-restriction, measured, no-negative-control

# A seam bound stands at the station it binds, and not over the whole interval

The crest repair (b4e9ce04) is right about the DEFECT and wrong about the REMEDY. It bounds
each seam interval by the tightest of three probes and then applies that one number to
**every station the interval touches** -- so a constraint that binds at one end of a spiral
flattens the kilometre of straight road in front of it, and the backward brake pass that
exists to propagate exactly such a constraint is bypassed.

```cpp
const double bound =
    std::fmin(HeldAt(head), std::fmin(HeldAt(middle), HeldAt(tail)));
const size_t first = (size_t)(from / stepM);
const double reach = to / stepM;
const size_t last = (size_t)reach == reach ? (size_t)reach : (size_t)reach + 1;
for (size_t at = first; at <= last && at < samples; ++at) {
  if (bound < Held_[at]) { Held_[at] = bound; }
}
```
— src/actor/path/SpeedProfile.cpp:105-111

## Measured

A line of three segments: straight 1000 m, spiral 500 m from curvature 0 to 0.01 per m,
arc 300 m at 0.01 per m. No rise, no bank, so the ONLY binding term is cornering. F31
envelope (grip 0.95, 1610 kg, 20 kN drive, 6.607 kN brake, g = 9.80665), step 20 m,
entry 0 m/s. Seams = {0, 1000, 1500, 1800}.

| station | 891a40f8 (before) | b4e9ce04 (after) | curvature there |
|---|---|---|---|
| 300 m | 310.799 km/h | 294.156 km/h | 0 |
| 500 m | 322.276 km/h | 255.451 km/h | 0 |
| 800 m | 268.241 km/h | 182.610 km/h | 0 |
| **1000 m** | **225.124 km/h** | **109.882 km/h** | **0** |
| 1500 m | 109.882 km/h | 109.882 km/h | 0.01 |

At station 1000 m the road is dead straight, level, and unbanked. The plan is **51.2 %
slower** than the road allows, because the interval [1000, 1500] evaluated its tail at
curvature 0.01 and stamped 109.882 km/h onto its head. The deficit reaches back to 300 m
(5.4 %). Reproduce: build `src/actor/path/{ReferenceLine,SpeedProfile}.cpp` at both revisions
against the geometry above and print `SpeedProfile::At` every 100 m.

## Why the whole-interval stamp is never needed

The item's own analysis proves it. Over a seam interval `CurvaturePerM` is linear,
`CurvatureRatePerM` is constant, and `SlopeRatePerM` is linear (the second derivative of a
cubic Hermite; ReferenceLine.cpp:118-122). Every term of `HeldAt` is monotone in one of those
three, so the tightest point of the interval is at an ENDPOINT. Clamping the sample nearest
that endpoint and letting the backward pass (SpeedProfile.cpp:141-145) brake back from it
gives the same guarantee at a fraction of the cost -- that pass is 100 % of the reason a
point constraint is enough.

## Two more defects in the same block

1. **`first` truncates and lands outside its own interval.** For a rise knot at 205 m with a
   20 m step, `(size_t)(205 / 20) = 10` -- station 200 m, which the interval [205, 210] does
   not touch. `last` rounds UP (:107-108). The two ends use opposite roundings and the bound
   leaks a full step backwards into the previous interval.
2. **`Slope` keeps exactly the defect this item repaired.** `Placed tail = middle;`
   (SpeedProfile.cpp:100) overrides `CurvaturePerM`, `CurvatureRatePerM` and `SlopeRatePerM`
   and leaves `Slope` at the middle's value. board/open/1767 justifies that with "the station
   walk already sees it" -- the same argument the item's opening paragraph refutes for the
   crest ("a term sampled at stations is a statement about the stations, never about the road
   between them"). `Slope` is the FIRST derivative of a cubic, quadratic in t, and its
   maximum can lie strictly between two stations, so the tractive-force bound
   (SpeedProfile.cpp:82-88) is still a point statement on a pitch whose steepest place falls
   between stations. A quadratic's extremum is closed-form; there is no excuse for a probe.

## What will be true

1. A seam bound is written at the station where its term is extremal, never across the
   interval. On the geometry above, station 1000 m reads within one step of 225.124 km/h
   again while station 1500 m still reads 109.882 km/h.
2. The station range a bound touches is derived once and consistently -- ceil at both ends,
   or the two straddling samples of the extremum, named either way.
3. The tractive-force term reads its slope from the interval's true extremum, not from a
   chimera `Placed` that stands nowhere on the line.
4. **Proof**: `test/unit/actor/path/ACrestBetweenTwoStationsIsStillACrest` gains the negative
   control it lacks -- an arm asserting the plan is NOT slower than the road allows where the
   road is straight and level. Today no test in `test/unit/actor/path/` fails against a 51 %
   over-restriction: `ASpeedPlanScalesWithTheDeclaredGravity` passed through this change
   unmoved. A bound with no upper proof is a bound that will keep tightening.
