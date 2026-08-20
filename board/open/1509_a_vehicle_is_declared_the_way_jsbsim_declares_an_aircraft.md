Type: task
Parent: 1498
Area: world
Tags: scope

**A vehicle is declared the way JSBSim declares an aircraft**

**JSBSim is named as a QUALITY BAR and not as a thing to copy** -- the owner's words. What is taken is
the shape of the split; the equations are ours (`board:1510`).

 JSBSim is not a general physics engine: it is
a **data-driven flight dynamics model** where the aircraft is declared in XML -- mass and balance, an
inertia tensor, ground reactions as spring/damper contact points, propulsion, and aerodynamics as
coefficient tables -- and the simulator assembles it. **No aircraft is hard-coded, and that is why one
program flies a Cessna and a 737.**

**A car is the same shape with different tables.** So the vehicle is DECLARED and never written --
**and the declaration is a row of OUR scenario and not a second file format.** `<vehicle>` sits beside
`<kinds>`, `<views>` and `<player>`, read by the same grammar table and refused by the same sentence when
it is misspelled. *One format, one reader, one place a typo is caught.*

```xml
<vehicle name="f31" asset="bmw-f31" massKg="1610">
  <inertia ixx="540" iyy="2400" izz="2600"/>
  <contact node="wheel_front_left"  springNPerM="32000" damperNsPerM="3400"
           travelM="0.18" bumpStopNPerM="450000" linkLimitN="24000"/>
  <contact node="wheel_front_right" .../>
  <contact node="wheel_rear_left"   .../>
  <contact node="wheel_rear_right"  .../>
  <tyre grip="0.95" corneringNPerRad="55000" relaxationM="0.4"/>
  <drive peakTorqueNm="400" finalDrive="3.08"/>
  <brake peakTorqueNm="2200"/>
  <body dragArea="0.66" frontalM2="2.19"/>
  <seat node="driver_head" view="eyes"/>
</vehicle>
```

## THE GEOMETRY CARRIES THE ATTACHMENT POINTS AND THE DECLARATION NAMES THEM

**No coordinate above is invented, and that is the owner's point about how other engines do it.** A
socket is *a transform declared at design time, relative to something in the model's own hierarchy* --
Unreal puts sockets on a bone, Blender uses an Empty, Roblox an `Attachment` inside a Part, Maya and Max
a locator or a dummy. **Every one of them attaches physics and behaviour to NAMED PLACES IN THE MODEL.**

**In glTF that mechanism already exists and needs nothing new: a node with a name.** This reader builds
the node hierarchy with its names already, so `<contact node="wheel_front_left">` takes the wheel's
position, its radius and its axis FROM THE MODEL -- and a wheel that moves in the model moves in the
physics without a number changing anywhere.

**And it is robust in a way a convention is not.** [MEASURED, from the field] a `SOCKET_` prefix does
not survive a glTF export in common tooling, so **the scenario names whatever node is actually there**
rather than requiring the artist to have used a magic prefix. *A declaration that adapts to the asset
beats a convention the asset must satisfy.*

- [ ] **A contact, a seat and a camera are NODES of the asset named by the declaration**, never
      coordinates, and a name the asset does not carry is a refusal listing what it does carry

**EVERY NUMBER IS A PHYSICAL QUANTITY WITH A UNIT AND NOT A LIMIT SOMEBODY TUNED.** The lateral
acceleration a corner allows is `grip * g`; the top speed is where drive force equals drag; the bump
that tears a wheel off is where the link's limit is exceeded. *`SpeedProfile::Envelope` already works
this way -- it takes a mass, a force and a drag area and DERIVES every acceleration, because a declared
`LateralMs2` would be a magic number wearing a unit.*

## What must be true

- [x] **A vehicle is a declaration and the engine READS it** -- `<vehicle>` is a row of the scenario
      grammar with its own eight elements, and `tools/driver/f31.scenario` is read by the engine that
      will drive it. *Assembling it into a body is the physics and is not built*, so a car, a lorry, a motorcycle and a
      locomotive are one program and several files
- [x] **Every quantity is physical and carries its unit**, and nothing in the model is a limit chosen
      to make a case pass -- proved by DERIVING one: the ride frequency is nowhere in the file and
      comes out at **1.419 Hz** from the mass and the spring rate, which is a road car's
- [x] **The contact points are a suspension and not a constraint**: spring, damper, travel, bump stop
      and a LINK LIMIT, so `board:1501`'s tear-off threshold is derived. Declared and read; the forces
      are not computed yet
- [ ] **The tyre is a force from slip**, so grip is exceeded rather than clamped -- a car that cannot
      lose grip cannot find a corner built too tight
- [x] **The declaration is a scenario's** (`board:1480`) -- `<vehicle>` sits beside `<views>` and
      `<player>`, read by the same grammar and refused by the same sentence when it is misspelled
- [ ] **It is deterministic to the bit at a declared tick rate**, or a crash cannot be re-driven

## THE SAME INSTRUMENT LANDS AN AIRCRAFT, and that is not a future nicety

**A wheel's link limit and a landing gear's design sink rate are the same number in the same place.** A
car meeting a step takes a vertical velocity its suspension must absorb; an aircraft touching down takes
one its gear must absorb. **Both are a contact with a travel, a damping and a strength, and both fail
the same way.**

| | |
|---|---|
| a car on a 4 cm step at 30 m/s | a vertical velocity the strut converts to force |
| an airliner at 1 m/s sink | a landing |
| the same at 3 m/s | the gear's design limit, and an inspection |
| beyond it | *a controlled descent into the ground*, and the forces say which it was |

**So `board:1501`'s question -- at what bump does the wheel tear off -- is the same question as *was that
a landing or an arrival***, and neither needs its own machinery. `CLAUDE.md` already promised it: *one
physics system carries walking, driving, flying and swimming*. **The contact model is where that promise
is either kept or quietly broken**, so it is written once for wheel-on-road, wheel-on-runway,
hull-on-water and foot-on-ground, and the medium is a declaration.

## Comments

**JSBSim's own lesson is the split, not the equations.** What makes it reusable is that the *integrator
and the force model* are code while the *aircraft* is data -- and the aircraft data is written by people
who are not programmers. **A scenario declaring a vehicle is that same line drawn in this tree**, and it
is the same line `CLAUDE.md` draws when it says an engine is a mechanism and content is data.

## READING IS NOT STANDING UP, and the test found that

`Engine::Load` read a scenario and stood it up in one call, so **checking a declaration needed its 30 MB
of geometry.** That is wrong for a validator, wrong for a test, and wrong for a tool that lists what a
scenario declares. `Engine::Read` is the declaration alone.

## What proves it

**`test/render/outshine/client/TheDriversScenarioLoadsAndItsNumbersArePhysical.cpp`** reads
`tools/driver/f31.scenario` -- **the file that will be driven, not a fixture resembling it** -- and
holds 27 checks over it:

| | |
|---|---|
| wheelbase the four contacts span | **2.81 m** against 2.810 published |
| the track they span | **1.548 m** against 1.543 -- *and the track was never used to set the scale* |
| every contact's height above ground | exactly one tyre radius, which is what a wheel standing still means |
| static sag at a corner | 0.123 m of 0.18 m travel |
| **the ride frequency that implies** | **1.419 Hz** |

**The ride frequency is the check that matters** because it appears nowhere in the file: it follows from
the mass and the spring rate, and its landing in a road car's band is what says the rate is physical
rather than chosen. *A declared `LateralMs2` could never have been checked that way.*

