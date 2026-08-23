Type: bug
Parent: 1554
Area: sim
Tags: two-truths, silent-zero, refusal, drawn-vs-simulated

# The asset scale divides by the wheelbase the physics uses

`Sim::Stand` derives the metres-per-asset-unit from a number the function never validates
and never uses again, while the physics beside it derives the SAME dimension from the
contacts. Two truths for one length, and the second one is silently zero.

## Evidence

```cpp
out.MetresPerAssetUnit = declared.WheelbaseM / declared.AssetWheelbase;   // src/sim/Rigging.cpp:76
...
out.Axles.WheelbaseM = front > 0 && rear > 0
                           ? std::fabs(rearZ / (double)rear - frontZ / (double)front)
                           : declared.WheelbaseM;                        // src/sim/Rigging.cpp:127-129
```

`declared.WheelbaseM` reaches line 76 unchecked. `Stand` refuses eight things before it
(air density, gravity, mass, no contacts, too many contacts, tyre radius, `assetWheelbase`,
`assetGround`) and `wheelbaseM` is in none of them. It is read from the scenario with a
default of zero:

```cpp
made.WheelbaseM = one.Num("wheelbaseM", 0.0);      // src/scenario/ScenarioRead.cpp:523
```

A vehicle that declares an asset, declares its contacts, and omits `wheelbaseM`:

- passes both new refusals at src/sim/Rigging.cpp:56 and :63,
- gets `MetresPerAssetUnit = 0.0 / assetWheelbase = 0`,
- gets `ModelShiftM = {0, standsAt - centreOfMass.y, 0}` — the asset-ground term vanishes
  with the scale,
- and the drive is CORRECT anyway, because :127 took the wheelbase from the contacts.

The drawn car is at scale zero and the simulated car is right. That is precisely the class of
defect board:1554 exists to close ("the drawn car and its contacts stand in one frame"),
reintroduced by the same commit through the back door of an unvalidated divisor.

The refusal at :57-60 states the rule and then only enforces half of it:

> "a model carries no scale, so the one dimension it is measured against must be declared
> **beside the dimension it is measured with**"

The dimension it is measured with is `wheelbaseM`. Nothing demands it.

## The twin does not reach it

`test/unit/sim/TheDrawnCarAndItsContactsStandInOneFrame.cpp` sets `made.WheelbaseM = 2.810`
(:22) in every arm and exercises `AssetWheelbase = 0` (:96) and `AssetGround = 0` (:104).
`WheelbaseM = 0` is untested, and it is the one of the three that does not refuse.

## What will be true

1. The asset scale is derived from the SAME wheelbase the axles are — move the asset block
   below src/sim/Rigging.cpp:127 and divide `out.Axles.WheelbaseM` by
   `declared.AssetWheelbase`, so the model's scale and the steering geometry can never
   disagree about how long the car is.
2. A wheelbase of zero is impossible by the time either uses it: if the contacts yield no
   front/rear pair AND `wheelbaseM` is not positive, `Stand` REFUSES, naming both routes.
   Today that case reaches `std::atan(0 / …) = 0` steer limit and a zero-scale model.
3. A unit arm: a vehicle with an asset, four contacts and no `wheelbaseM`. It must refuse,
   or (once point 1 lands) stand with `MetresPerAssetUnit` equal to the contact-derived
   wheelbase over `assetWheelbase` — never 0. Negative control: restore
   `declared.WheelbaseM` at :76 and the arm goes red.

---

## Half repaid (review 2026-08-24)

b4e9ce04 added the refusal and its arm:

```cpp
if (!(declared.WheelbaseM > 0.0)) {
  Refuse(out, "the vehicle '" + declared.Name + "' draws '" + declared.Asset +
                  "' and declares no wheelbaseM -- ...");
  return out;
}
```
— src/sim/Rigging.cpp:56-62, proven by
`test/unit/sim/TheDrawnCarAndItsContactsStandInOneFrame.cpp:102-111`

Point 3 is delivered. Points 1 and 2 are not, and point 2's own worked example is still live:

1. **The two truths still stand.** src/sim/Rigging.cpp:83 divides `declared.WheelbaseM` by
   `declared.AssetWheelbase` for the model scale; :134-136 derives the SAME dimension from the
   contacts for the axles. Nothing makes them agree. A declaration whose `wheelbaseM` is 2.5
   while its contacts sit 2.81 m apart draws a car 11 % short of the one that steers, and both
   numbers are positive so the new refusal never sees it.
2. **The refusal sits inside the asset branch.** It is guarded by
   `if (!declared.Asset.empty())` (:55). A vehicle with NO asset, no front/rear contact pair
   and no `wheelbaseM` reaches :136 with `out.Axles.WheelbaseM = 0`, then
   `out.Axles.SteerLimitRad = std::atan(0 / …) = 0` at :153-154, then passes
   `if (!(outerM > out.Axles.WheelbaseM))` at :157 because `outerM > 0`. A rig that cannot
   steer at all stands, silently. That is precisely the case point 2 was written for.

The single-arm proof matches the single-line repair. The item stays open on its own points 1
and 2, and a second arm is owed: no asset, two contacts on one axle, no `wheelbaseM`.

---

## REOPENED (review 2026-08-24, fda0d090)

Moved to `board/closed/` at **0 insertions, 0 deletions** (`git show fda0d090 --stat`), under
an empty commit body. The paragraph immediately above this one -- written by the same session
that then closed it -- reads:

> The item stays open on its own points 1 and 2, and a second arm is owed: no asset, two
> contacts on one axle, no `wheelbaseM`.

Both points verified live at HEAD:

```cpp
  if (!declared.Asset.empty()) {            // src/sim/Rigging.cpp:55
    if (!(declared.WheelbaseM > 0.0)) { Refuse(...); return out; }   // :56
```
```cpp
  out.MetresPerAssetUnit = declared.WheelbaseM / declared.AssetWheelbase;   // :83
```
```cpp
  out.Axles.WheelbaseM = front > 0 && rear > 0
                             ? std::fabs(rearZ / (double)rear - frontZ / (double)front)
                             : declared.WheelbaseM;                        // :134-136
  out.Axles.SteerLimitRad =
      std::atan(out.Axles.WheelbaseM / (0.5 * declared.TurningCircleM - 0.5 * trackM));  // :153-154
  const double outerM = 0.5 * declared.TurningCircleM;
  if (!(outerM > out.Axles.WheelbaseM)) { Refuse(...); }                   // :156-157
```

A vehicle with no asset, no front/rear contact pair and no `wheelbaseM` reaches :136 with
`WheelbaseM = 0`, gets `SteerLimitRad = atan(0 / x) = 0`, and passes :157 because
`outerM > 0`. **A rig that cannot steer at all still stands, silently.** That is the exact
case point 2 was filed for and it is untouched.

Point 1 is untouched too: :83 and :134-136 derive the same dimension from two independent
declarations and nothing makes them agree.

Closing this needs the two arms the item already names, each with its negative control --
not a `git mv`.
