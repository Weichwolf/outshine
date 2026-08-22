Type: issue
Area: sim
Tags: scope, scenario

**Air density is the world's declaration, and a vacuum is a legal world**

The gravity fold (f1c48fe3) moved g onto `<world>` — and left the other half of the drag term
on the car. At HEAD:

- include/outshine/Scenario.h:245 — `AirDensity` is a field of the VEHICLE body;
  src/scenario/ScenarioRead.cpp:91,528 read it from `scenario/vehicle/body airDensity`.
  The medium a body moves through is the WORLD's (rho belongs beside gravityMs2, wind and
  cloud in WorldSettings); Cd and frontal area are the vehicle's. A car that carries its own
  atmosphere keeps Earth's 1.225 kg/m3 when the scenario stands it on the moon — exactly the
  disease 1611 cured for g, one field to the left.
- src/corridor/SpeedProfile.cpp:23-31 — `Over` REFUSES `AirDensity <= 0` ("this one leaves at
  least one at zero"). A vacuum world is thereby unspellable: moon.xml (1611's second
  template, 1612's journey) cannot plan a drive at all. Zero density is a measurement of the
  moon, not a malformed declaration.
- Envelope::TopMs (src/corridor/SpeedProfile.h:36-39) returns 0.0 when resistance is 0 — in a
  vacuum the drag-limited top speed is UNBOUNDED (the drive-force limit and the crest bound
  take over), not zero. Same shape at the climb term (SpeedProfile.cpp: `left/resistance`).

Demanded: rho moves to WorldSettings (declared, defaulting from the world template, not the
vehicle); `Rigging::Stand` takes it with gravity; SpeedProfile accepts rho = 0 and derives the
top speed as the minimum of the terms that EXIST (drag bound only when resistance > 0); the
gravity-scaling twin gains a vacuum case proving the plan under rho = 0 is crest- and
drive-bound. The refusal keeps demanding rho >= 0 declared — it stops demanding an atmosphere.
