Type: task
Parent: 1498
Area: world
Tags: scope

**A vehicle is declared the way JSBSim declares an aircraft**

**The owner named the mechanism and it is the right one.** JSBSim is not a general physics engine: it is
a **data-driven flight dynamics model** where the aircraft is declared in XML -- mass and balance, an
inertia tensor, ground reactions as spring/damper contact points, propulsion, and aerodynamics as
coefficient tables -- and the simulator assembles it. **No aircraft is hard-coded, and that is why one
program flies a Cessna and a 737.**

**A car is the same shape with different tables.** So the vehicle is DECLARED and never written:

```xml
<vehicle name="hatchback" massKg="1320">
  <inertia ixx="420" iyy="1900" izz="2000"/>
  <contact at="fl" x="1.25" y="0.76" z="-0.35" springNPerM="32000" damperNsPerM="3400"
           travelM="0.18" bumpStopNPerM="450000" linkLimitN="24000"/>
  <contact at="fr" .../><contact at="rl" .../><contact at="rr" .../>
  <tyre grip="0.95" radiusM="0.31" corneringNPerRad="55000" relaxationM="0.4"/>
  <drive peakTorqueNm="240" wheelRadiusM="0.31" finalDrive="3.9"/>
  <brake peakTorqueNm="1400"/>
  <body dragArea="0.68" liftArea="0.0" frontalM2="2.2"/>
</vehicle>
```

**EVERY NUMBER IS A PHYSICAL QUANTITY WITH A UNIT AND NOT A LIMIT SOMEBODY TUNED.** The lateral
acceleration a corner allows is `grip * g`; the top speed is where drive force equals drag; the bump
that tears a wheel off is where the link's limit is exceeded. *`SpeedProfile::Envelope` already works
this way -- it takes a mass, a force and a drag area and DERIVES every acceleration, because a declared
`LateralMs2` would be a magic number wearing a unit.*

## What must be true

- [ ] **A vehicle is a declaration and the engine assembles it**, so a car, a lorry, a motorcycle and a
      locomotive are one program and several files
- [ ] **Every quantity is physical and carries its unit**, and nothing in the model is a limit chosen
      to make a case pass
- [ ] **The contact points are a suspension and not a constraint**: spring, damper, travel, bump stop
      and a LINK LIMIT, so `board:1501`'s tear-off threshold is derived
- [ ] **The tyre is a force from slip**, so grip is exceeded rather than clamped -- a car that cannot
      lose grip cannot find a corner built too tight
- [ ] **The declaration is a scenario's** (`board:1480`), so the drive suite pins its vehicle and a
      scenario may carry another
- [ ] **It is deterministic to the bit at a declared tick rate**, or a crash cannot be re-driven

## Comments

**JSBSim's own lesson is the split, not the equations.** What makes it reusable is that the *integrator
and the force model* are code while the *aircraft* is data -- and the aircraft data is written by people
who are not programmers. **A scenario declaring a vehicle is that same line drawn in this tree**, and it
is the same line `CLAUDE.md` draws when it says an engine is a mechanism and content is data.
