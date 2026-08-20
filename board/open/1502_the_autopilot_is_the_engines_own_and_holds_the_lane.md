Type: task
Parent: 1498
Area: world
Tags: scope

**The autopilot is the engine's own and holds the lane**

**The owner's ruling: autopilot is standard outshine functionality.** It is a REFLEX -- `board:1495`'s
fastest tier -- and it is the vehicle's counterpart to `steer`: `uses="drive"`.

## What must be true

- [ ] **`drive` follows a route as a vehicle**: a speed profile from the curvature ahead, a steering
      angle from the lane centre, and braking that respects the declared grip
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
