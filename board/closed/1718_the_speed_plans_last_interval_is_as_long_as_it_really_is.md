Type: bug
Area: actor
Tags: correctness, regression-of-repair

**The speed plan's last interval is as long as it really is**

1715 made `Over` sample the clamped final station, but every consumer of the interval still
prices it at a full `stepM`:

- src/actor/path/SpeedProfile.cpp:110-118 — the forward and backward passes use
  `2.0 * accelMs2 * stepM` and `2.0 * brakeMs2 * stepM` for EVERY gap, including the final
  partial one whose true length is `LengthM_ - whole*stepM < stepM`. The backward pass
  therefore allows an entry speed at the second-to-last station from which the declared
  brake CANNOT reach the final bound within the real remaining metres: with step 5 m,
  brake 7 m/s², end bound 5 m/s and a 0.5 m tail, the pass permits
  `sqrt(25 + 70) = 9.7 m/s` where the true bound is `sqrt(25 + 7) = 5.7 m/s`. The plan is
  optimistic exactly where 1715 said a bend in the last metres must bound it.
- src/actor/path/SpeedProfile.cpp:122-131 — `At()` maps `alongM / StepM_` linearly, so the
  final partial interval is interpolated as if it were `stepM` long: at `alongM → LengthM_`
  the value approaches `Held_[whole] + (partial/step) * diff`, then line 125 jumps to
  `Held_.back()`. A discontinuity of `(1 - partial/step) * diff` sits at the plan's end, and
  DriveTick reads `profile.At` live (src/sim/DriveTick.cpp:95,104,178).

Demanded: the passes use the interval's true length (all interior gaps `stepM`, the last one
`LengthM_ - whole*stepM`), `At()` interpolates the tail over that same length, and the unit
twin gets an arm where a sharp final bend behind a partial step proves the entry bound with
the old arithmetic red.

---

Closed -- both passes price every gap at its true length (interior stepM, the last one
LengthM - whole*stepM via one gapM lambda) and At() interpolates the tail over that same
length, so the discontinuity at the plan's end is gone where DriveTick reads live. Proven in
ASpeedPlanScalesWithTheDeclaredGravity: a 20.5 m line at step 5 with the bend behind the
partial step bounds the 20 m entry at sqrt(v_bend^2 + 2 b 0.5) -- the full-step price
allowed sqrt(v^2 + 2 b 5) -- and At() is continuous at the end. Negative control: the
full-step arithmetic reverted fails exactly this arm.
