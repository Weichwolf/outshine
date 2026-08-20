Type: task
Parent: 1498
Area: world
Tags: instrument

**The speed profile leaves room to hold the line**

`SpeedProfile` plans to `v = sqrt(a_lat / kappa)` -- the speed at which the tyres are exactly at their
limit -- and leaves **nothing** for the tracking error. Driving that plan, the car cannot hold the
line it was planned for.

Measured on the negative control, the 840 m synthetic road, F31, 1 s look-ahead:

| | at the profile's speed | held to 25 m/s |
|---|---|---|
| top speed reached | 51.6114563 m/s | 25 m/s |
| worst deviation | 1.4085528 m at 601.8 m | 0.0990219923 m |
| what the clothoid lag `c d^3 / 6` accounts for | 0.47735981 m | 0.0542534722 m |
| **left unnamed** | **0.93 m** | 0.045 m |
| share of a contact's grip in use | 0.883717207 | -- |
| suspension travel used | 0.168050475 m of 0.18 | -- |

**Two findings, and neither is about the road.**

- [ ] **The profile leaves a declared margin for tracking**, or the pilot shortens its look-ahead as
      grip usage rises -- at 0.88 of grip there is nothing left to correct with, and the deviation
      that results is the planner's and not the road's
- [ ] **The margin is DERIVED and not chosen**: the lateral acceleration a corner needs is
      `v^2 kappa`, and holding the line within `e` costs the pursuit law an extra curvature of
      `2 e / d^2`, so the grip that tracking needs is answerable before the speed is planned
- [ ] **On a smooth road at the planned speed the car already uses 93 % of its suspension travel.**
      The lateral transfer alone accounts for 0.158 m of the 0.168 -- `m a h / track` at 3.89 m/s2 --
      so the FIRST real bump bottoms out. Either the profile is too brave or the F31's declared
      travel is too small, and which of the two is the question this item answers
- [ ] **Whatever is decided, the floor is re-measured**, because both numbers above are the floor

## Comments

**This is what measuring the floor at TWO speeds bought.** At 25 m/s the deviation is 0.099 m, of
which 0.054 is the named clothoid lag and 0.042 is the measured cost of being a vehicle rather than a
bicycle -- so almost all of it is accounted for. At the profile's speed 0.93 m of 1.41 m is unnamed.
One speed would have produced one number and no way to tell those two situations apart.
