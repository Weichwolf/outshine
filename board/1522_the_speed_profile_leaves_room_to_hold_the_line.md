Type: bug
State: open
Parent: 1498
Area: base/curve, generators/path
Tags: instrument, measured
Supersedes: 1825

# The speed profile leaves room for the mind to react

**Benchmark** — Unreal: `UPathFollowingComponent` keeps a look-ahead and slows for it. RAGE: the vehicle AI drives a node list with a target speed. **Both agree** — a speed plan that leaves no room to react is a plan the mind cannot follow.

**Reframed under board:1919.** A plan is not a line the car HOLDS -- a car reacts to what is
under it and around it, and other traffic will be in the way. What the plan owes is not
adherence but ROOM: a speed at which a mind still has grip left to answer something the plan
never knew about.

`SpeedProfile` plans to `v = sqrt(a_lat / kappa)` — the speed at which the tyres are exactly at
their limit — and leaves NOTHING for the tracking error, so the car cannot hold the line it was
planned for. Measured on the 840 m synthetic road, F31, 1 s look-ahead:

| | at the profile's speed | held to 25 m/s |
|---|---|---|
| worst deviation | 1.409 m at 601.8 m | 0.099 m |
| the clothoid lag `c d^3/6` accounts for | 0.477 m | 0.054 m |
| **left unnamed** | **0.93 m** | 0.045 m |
| share of a contact's grip in use | 0.884 | — |
| suspension travel used | 0.168 m of 0.18 | — |

## What will be true

- [ ] The profile leaves a DERIVED margin for tracking, or the pilot shortens its look-ahead as
      grip usage rises: holding the line within `e` costs the pursuit law an extra curvature of
      `2e/d^2`, so the grip that tracking needs is answerable before the speed is planned.
- [ ] The 0.93 m is attributed. At 0.88 of grip there is nothing left to correct with, and the
      deviation that results is the planner's and not the road's.
- [ ] 93 % of suspension travel on a SMOOTH road at the planned speed is its own finding: the
      declared ride frequency and the load say what the travel should be.
- [ ] A fixture exists whose plan minimum is set by a SWEEP rather than by the sampling loop, or
      the tree says none can — three of the four passes propagate a bound rather than create one,
      so the telemetry that reports `Slowest()` has no negative control today.
