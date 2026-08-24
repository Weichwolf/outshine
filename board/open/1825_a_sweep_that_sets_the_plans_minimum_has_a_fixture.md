Type: task
Parent: 1785
Area: actor/path
Tags: proof, telemetry

# A sweep that sets the plan's minimum has a fixture, or the tree says none can

`board:1785` was reopened twice because `Slowest()` and `BoundBy()` were computed inside the
sampling loop while `Over()` lowered `Held_[]` three more times afterwards. They are derived
after the last sweep now, and box 4's proof -- a plan whose minimum is not station 0 -- is
landed at `test/unit/actor/path/AStraightRoadIsPlannedAtItsOwnSpeed`.

**That proof is not additionally a control over the sweeps**, and the closure says so. Two
controls were run against it:

| control | what the case did |
|---|---|
| the curvature term stops naming itself | three older arms red, the top-speed arm **green** -- `slip` takes over and the minimum stays in the bend |
| the seam clamp silenced (`ClampAround` skipped) | **green**, 109.882 km/h at 1500.0 m unchanged |

So on that road the minimum comes from the sampling loop whatever the sweeps do, and a
regression that moved the telemetry back inside the loop would pass.

## Whether such a fixture exists at all is a real question

Three of the four passes PROPAGATE a bound rather than create one:

```
src/actor/path/SpeedProfile.cpp:172   if (entryMs < Held_[0])            -- station 0 only
src/actor/path/SpeedProfile.cpp:177   sqrt(v^2 + 2 a dx) forward         -- never under the station it comes from
src/actor/path/SpeedProfile.cpp:180   sqrt(v^2 + 2 a dx) backward        -- never under the station it brakes for
```

A propagated bound cannot be the global minimum unless the bound it propagates from already is.
**The seam clamp is different**: it evaluates `HeldAt` on an EXTRAPOLATED station

```cpp
src/actor/path/SpeedProfile.cpp:153   tail.CurvaturePerM = 2.0 * middle.CurvaturePerM - head.CurvaturePerM;
```

so it can hold a station tighter than any sampled curvature on the line -- which is a second
question worth its own answer: an extrapolated curvature is a value the reference line does not
carry, and the plan is bound by it.

## What will be true

- [ ] Either a fixture exists in `test/unit/actor/path/` whose plan minimum is set by the seam
      clamp -- so moving the telemetry back before the sweeps turns it red -- or this item
      closes with the PROOF that no sweep can set the minimum, in which case `Slowest()` is
      structurally either `entry` or a geometric term and the header says so.
- [ ] If the seam clamp can bound a plan by a curvature the line does not carry, that is a
      finding about the clamp and not about the telemetry: extrapolating one station past the
      end of a seam interval is a guess, and `CLAUDE.md` requires a guess to be plausible four
      ways or be a finding.

## Comments

- 2026-08-24 -- filed from board:1785's own closure rather than left implicit. The closure
  states what its case does not prove; this item is that statement with a box on it.
