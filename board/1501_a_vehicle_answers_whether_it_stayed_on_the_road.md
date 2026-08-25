Type: task
State: active
Parent: 1498
Area: world
Tags: instrument

**A vehicle answers whether it stayed on the road**

**The vehicle is the sensor and the road is the subject.** It needs to be good enough that a defect
shows and simple enough that its own behaviour is never the finding.

## The model, and why it is this one

**Four wheels with suspension travel, a load per wheel, and longitudinal and lateral tyre forces.** Not
a full tyre model: **the suspension and the contact are the instrument**.

**AND THE CRASH IS GEOMETRICALLY FREE.** `CLAUDE.md` already declares *one physics system carries
walking, driving, flying and swimming* -- so a body moves only by forces, and every failure is a reading
of forces that were computed anyway. A wheel in free fall has a normal force of zero; a wall is an
impulse; an impossible gradient is a longitudinal force the tyres cannot deliver. **Nothing about
failure needs its own machinery**, which is why the same instrument serves a car, a train, a walker and
a boat. A 5 cm step at 30 m/s is a
vertical acceleration nothing else in the scene produces, and a wheel that leaves the surface is a
number rather than an impression.

**THE SUSPENSION MAKES THE THRESHOLD DERIVED RATHER THAN CHOSEN.** A wheel meeting a step of height h
at speed v takes a vertical velocity it must absorb; the spring compresses, and when the travel runs out
the load goes through the bump stop into the link -- **and a link has a strength.** So *at what bump does
the wheel tear off* is a calculation and not a threshold somebody picked:

| | |
|---|---|
| **what the tick rate bounds** | the smallest defect the instrument can SEE |
| **what the link's limit bounds** | the smallest defect that COUNTS |

**Both are published beside every finding**, and the second is the one that makes a report arguable: *a
4 cm step at 30 m/s exceeds this link by 1.8x* is a sentence an engineer can check, where *a 4 cm step
exceeds the threshold* is one only this repository can.

**THE TICK RATE BOUNDS THE SMALLEST DEFECT THIS CAN SEE, and that is the instrument's domain.** At
30 m/s a 1 kHz tick samples every 3 cm; a defect narrower than two samples is invisible to it. **So the
rate is declared, the smallest detectable defect is derived from it, and no finding smaller than that
is reported.**

## What must be true

- [ ] **Per wheel, per tick: contact or no contact, the load, the suspension travel and the surface
      normal** -- the four numbers every classification below is derived from
- [ ] **The suspension has a travel, a bump stop and a LINK STRENGTH**, so the bump that tears a wheel
      off is derived from the vehicle rather than declared as a limit -- and the derivation is published
      as a curve: *at this speed, this step*
- [ ] **A CRASH IS NOT DETECTED, IT IS READ.** Nothing here moves except by force, so free fall is a
      normal force of zero and a wall is an impulse -- **the quantities are already there and a crash
      is a threshold on them.** There is no separate crash check to write, to forget or to get wrong.
      *That is the same rule as preferring a shape that makes a mistake unspellable, applied to an
      instrument*
- [ ] **The telemetry is continuous and not only the crash**: vertical jerk, cross-slope, curvature and
      its rate, and the deviation from the lane centre -- **so a road that is nearly bad is visible
      before one that is bad**
- [ ] **The vehicle's parameters are declared and PINNED**, and changing them is a measurement that says
      the instrument was wrong -- never a way to make a route pass
- [ ] **It takes nothing from the allocator per tick**, because at 1 kHz over 800 km it would take
      everything
- [ ] **Two runs of one seed produce the same telemetry to the last bit**, or nothing here is a test
