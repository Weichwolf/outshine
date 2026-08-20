Type: task
Parent: 1498
Area: world
Tags: instrument

**A vehicle answers whether it stayed on the road**

**The vehicle is the sensor and the road is the subject.** It needs to be good enough that a defect
shows and simple enough that its own behaviour is never the finding.

## The model, and why it is this one

**Four wheels with suspension travel, a load per wheel, and longitudinal and lateral tyre forces.** Not
a full tyre model: **the suspension and the contact are the instrument**. A 5 cm step at 30 m/s is a
vertical acceleration nothing else in the scene produces, and a wheel that leaves the surface is a
number rather than an impression.

**THE TICK RATE BOUNDS THE SMALLEST DEFECT THIS CAN SEE, and that is the instrument's domain.** At
30 m/s a 1 kHz tick samples every 3 cm; a defect narrower than two samples is invisible to it. **So the
rate is declared, the smallest detectable defect is derived from it, and no finding smaller than that
is reported.**

## What must be true

- [ ] **Per wheel, per tick: contact or no contact, the load, the suspension travel and the surface
      normal** -- the four numbers every classification below is derived from
- [ ] **The crash conditions are DECLARED and each names a class**: a wheel off the surface longer than
      a declared time · suspension bottoming out · lateral acceleration past the declared grip · roll
      past a declared angle · the body touching anything
- [ ] **The telemetry is continuous and not only the crash**: vertical jerk, cross-slope, curvature and
      its rate, and the deviation from the lane centre -- **so a road that is nearly bad is visible
      before one that is bad**
- [ ] **The vehicle's parameters are declared and PINNED**, and changing them is a measurement that says
      the instrument was wrong -- never a way to make a route pass
- [ ] **It takes nothing from the allocator per tick**, because at 1 kHz over 800 km it would take
      everything
- [ ] **Two runs of one seed produce the same telemetry to the last bit**, or nothing here is a test
