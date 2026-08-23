Type: bug
Area: actor
Tags: correctness

**The speed profile samples its final partial step**

`SpeedProfile::Over` (src/actor/path/SpeedProfile.cpp:40-48) takes
`samples = (size_t)(L / stepM) + 1`, stations `at * stepM`. For L not divisible by stepM the
last station is `floor(L/stepM) * stepM` and the tail `(floor(L/stepM)*stepM, L]` — up to one
full step — is NEVER sampled: a bend or crest in the final metres bounds nothing, and
`At()` (line 112) serves the tail from `Held_.back()`, the speed of a station up to stepM
away. The clamp at line 48 (`station > LengthM_ ? LengthM_`) is dead — `floor(L/s)*s > L` is
impossible — which is the fossil of the intended `+2` sampling.

Fix: `samples = (size_t)(L / stepM) + 2` when L is not on the grid (then the clamp goes
live and the last station is exactly L), or ceil-based counting. Proof: a line of length
10.5 at step 1 with all its curvature in the last half metre plans a bounded speed there;
today it plans `topMs`.
