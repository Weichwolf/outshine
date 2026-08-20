Type: task
Parent: 1498
Area: world
Tags: instrument

**A train cannot steer, and that is what makes it the purer instrument**

`board:1501`'s car has two degrees of freedom and an autopilot between the world and the verdict. **A
train has one.** It is constrained to the rails, so **there is no controller whose quality could explain
a finding** -- the three-way confound of `board:1498` loses a leg.

## What the train measures that the car cannot

| | |
|---|---|
| **gauge held** | the two rails stay a gauge apart, or the corridor's profile is deforming |
| **wheel on rail** | flange contact, and lift -- a derailment is unambiguous where leaving a lane is a judgement |
| **lateral force against cant** | the cant should balance it at design speed, so a residual is a curve built for the wrong speed |
| **coupler forces along a consist** | several bodies on one line, so a gradient change shows as a force between wagons that one body would not feel |

**And the last one is a genuinely different sensor**: a 400 m train spans a vertical curve a 4 m car
never notices. *A consist is a distributed probe of the profile, which is exactly what a long viaduct's
approach needs.*

## What must be true

- [ ] **A train is 1..N bodies on one reference line**, coupled, and the consist length is declared
- [ ] **Position is arc length along the corridor**, so the physics solves one dimension plus the
      vertical rather than six -- **cheaper than the car and more sensitive**
- [ ] **The crash conditions are declared and each names a class**: wheel lift · flange climb · gauge
      out of tolerance · lateral force past the cant's balance · coupler force past its limit
- [ ] **It runs headless faster than real time** and reports the same telemetry shape the car does, so
      one suite reads both
- [ ] **A run's verdict is kilometres per fault**, per corridor class, because a tram line and a
      mainline are different populations and averaging them would name neither

## Comments

**The two instruments are complementary and the pair is worth more than either.** A car finds what a
driver would hit: a step, a hole, a turn too tight for its speed. A train finds what a driver would
absorb without noticing: a gradient that changes too fast, a curve whose cant is wrong, a viaduct
approach that is a metre out over 300 metres. **Neither alone would find both**, which is the same
sentence `board:1506` makes about the eye and the physics.
