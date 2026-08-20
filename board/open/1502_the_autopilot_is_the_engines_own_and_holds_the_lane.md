Type: task
Parent: 1498
Area: world
Tags: scope

**The autopilot is the engine's own and holds the lane**

**The owner's ruling: autopilot is standard outshine functionality.** **And it obeys PHYSICS and not
traffic law** -- no speed limits, no signs, no right of way yet; the speed a corner allows is the speed
grip allows, and that is a number the geometry produces rather than a tag anybody wrote. It is a REFLEX -- `board:1495`'s
fastest tier -- and it is the vehicle's counterpart to `steer`: `uses="drive"`.

## What must be true

- [x] **The speed comes from the CURVATURE and never from a tag.** `v = sqrt(a_lat / k)` at every
      station, capped by the vehicle's top speed, then a forward pass for what the drivetrain can reach
      and a backward pass for what the brakes can shed -- so the car is slow in a tight curve and fast
      on a straight **because the geometry says so**. `src/core/SpeedProfile.{h,cpp}`
- [ ] **`drive` steers as well as it paces**: a steering angle from the lane centre, with a look-ahead
      derived from speed
- [ ] **It looks AHEAD by a distance derived from speed**, which is what a driver does and what makes a
      clothoid the right transition -- a controller that steers at the point it is standing on
      oscillates
- [ ] **On a road that is CORRECT it holds the lane centre within a declared tolerance**, at every speed
      the suite drives -- *that is the autopilot's own test and it uses the synthetic road, not OSM*
- [ ] **It answers what it achieved**: the deviation it held, the speed it managed against the speed the
      road allows, and whether it had to brake -- **both directions**, and every one of those is a
      finding when it exceeds the floor
- [ ] **HAVING TO SWERVE IS ITSELF THE FINDING.** A controller that steers around a step has not
      survived a defect, it has MEASURED one -- so the deviation from the ideal line is the primary
      number and a crash is only its extreme. *That makes the instrument continuous rather than binary
      and far more sensitive: a road that forces 40 cm of correction is found long before one that
      throws the car off.*
- [ ] **The ideal line is the reference line's**, so "where the car should be" is a geometric fact and
      not the controller's opinion
- [ ] **It costs microseconds and allocates nothing**, because it is a reflex
