Type: feature
State: open
Area: apps, scenario
Tags: scope, driver, product

# the driver client (deleted) is a Test Drive game: worldwide, declarative, and the camera sits in the seat

**Benchmark** — Unreal: a sample project is content plus a launcher. RAGE: the game IS the client. **Neither ships a declarative test drive**, so the shape is this tree's own — and its line count is the measure of the door it stands on.

The owner's direction, and it outranks the rest of this board:

- **worldwide, any route**: two coordinates anywhere on Earth, driven by the engine's own driver
  or by the player. Munich--Hamburg is route 1, not a special case.
- **FPV is the focus**: the camera sits in the driver's position and the car's interior is
  modelled. Chase and framing shots are tooling.
- **the game is DECLARATIVE**: it ships assets, a scenario and a markup UI and nothing else.
  Every line of C++ in the app that grades terrain, rings a horizon, paces a relay or composes a
  camera is engine work wearing a tool's clothes and migrates behind an engine interface.
- **the engine delivers everything except the car assets**: the F31 stands in for every car,
  NPC traffic wears it recoloured — so missing vehicle variety is declared and missing TRAFFIC
  is a gap.

The draw is not the race, it is the PLACE: the Gotthard, the Stelvio, the Nullarbor — layers
`the driver client (deleted)/routes.xml` already names. What is timeless in Test Drive (1987): the cockpit
as the only view, the public road instead of the circuit, speed as transgression, and the car as
an object one inhabits. Not rebuilt: lives and a score, police as a random spawn, engine damage
on a wrong gear.

## What will be true

- [ ] A second route, declared by two coordinates alone, drives and pictures with no code change.
- [ ] The app's own C++ shrinks to a declaration, a loop and a UI (board:1805, 1862).
- [ ] The in-game UI is declared in the engine's own markup tree (board:1579).
- [ ] NPC cars: the same declared vehicle, recoloured per instance, placed by a compositor.
