Type: bug
Area: generators
Tags: instrument

**A sharp corner is fitted by its own measurement and not by a first-order formula**

`CornerRadiusM` closes the spiral-arc-spiral corner in the FIRST-ORDER clothoid shift `L^2 / 24R`. That
is accurate to 0.28 % at a right angle and loses accuracy as the turn sharpens: on the Munich to
Hamburg corridor the worst vertex offset is **23.56 m against a 9.55 m budget, at 641.4 km**, where the
sharpest turn on the route is 137.3 degrees.

**The construction is derived and the residual is a named term** -- the clothoid's higher-order series
-- but a bound that is exceeded by 2.5x is not a bound.

## What must be true

- [ ] **Each corner measures its own offset from the vertex after laying**, and reduces its radius
      until the measurement is inside the budget. The correction is MEASURED per corner rather than
      fitted once, so it stays derived
- [ ] **The number of corners that needed correcting is published**, because it is the size of the
      first-order approximation's domain
- [ ] **The measurement is bounded**: a corner that cannot be brought inside the budget at any radius
      the vehicle can drive is a REFUSAL, which is what `board:1524` calls a pass on broken data

## Comments

At a right angle the closed form is 0.28 % out and the whole route's median corner is far gentler than
that. What breaks it is the tail: 25 turns past a right angle and 1 past 135 degrees in 2480 vertices.
**Fixing the median would have bought nothing; the tail is the whole finding.**
