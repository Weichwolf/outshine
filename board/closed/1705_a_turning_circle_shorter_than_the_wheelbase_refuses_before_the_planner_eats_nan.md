Type: bug
Area: sim

**A turning circle the Pythagoras cannot take refuses by name before the planner eats NaN**

src/sim/DriveAssembly.cpp:206-208:

```cpp
const double outerM = turning.TurningCircleM * 0.5;
const double tightestM = std::sqrt(outerM * outerM - turning.WheelbaseM * turning.WheelbaseM);
```

For a declaration with `TurningCircleM < 2 * WheelbaseM` (e.g. circle 4 m, wheelbase 2.5 m,
track 1.8 m), `tightestM` is NaN. `Stand`'s refusal (src/sim/Rigging.cpp:111,
`TurningCircleM > trackM`) does not cover this case — and Stand runs at line 232, AFTER the
NaN has flowed into `roads.Plan(..., tightestM, ...)` at line 210. Every comparison against a
NaN turn bound is false, so the planner silently refuses no turn at all: the sharpest-turn
admission the number exists for is disabled, without a word.

Demanded: refusal at assembly, naming circle and wheelbase, ordered before the plan — or the
derivation moves into `Stand` beside its sibling geometry checks so one refusal owns the
vehicle's turn geometry. The proof arm lands in
test/unit/sim/ARigRefusesADeclarationItCannotDrive.cpp (which today only exercises
TurningCircleM = 11).

---

Closed -- the derivation moved into Stand beside its sibling geometry checks: Rigged carries
TightestM, a half-circle no longer than the wheelbase refuses naming both, and DriveAssembly
stands the rig BEFORE the planner so no NaN can reach roads.Plan. Proven in
ARigRefusesADeclarationItCannotDrive (circle 4 m vs wheelbase 2.5 m refuses naming the
wheelbase; the standing rig's TightestM is positive and under the half-circle). Negative
control: the pre-fix tree goes red on this test.
