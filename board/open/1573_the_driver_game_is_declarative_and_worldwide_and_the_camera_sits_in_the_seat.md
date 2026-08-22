Type: feature
Area: scenario
Tags: scope

**tools/driver is a Test Drive game: worldwide, declarative, and the camera sits in the seat**

**The owner's direction, 2026-08-22, and it outranks everything on this list:**

- **worldwide, any route**: two coordinates from anywhere on earth to anywhere, driven by the
  engine's own driver or by the player -- Munich-Hamburg is route 1 of board:1524's hundred, not
  a special case
- **FPV is the focus**: the camera sits in the driver's position; the first-person picture is the
  game, chase and framing shots are tooling
- **the game is DECLARATIVE**: it ships assets, scenario setup and an HTML/CSS UI with
  JavaScript-like elements -- and nothing else. Everything that behaves lives in the engine;
  a driver binary that computes grading, rings, relays or cameras in its own C++ is engine work
  wearing a tool's clothes, and each such piece migrates behind the engine's interfaces
- **the engine delivers everything except the car assets**: in this development phase the F31
  model stands in for every car; NPC traffic wears the same model recoloured -- so missing
  vehicle variety is declared, and missing TRAFFIC is a gap

## What must become true

- [ ] a second route, declared only by two coordinates, drives and pictures without any code
      change (the hundred of board:1524 are the destination)
- [ ] the stills/window drivers shrink toward declarations: ground grading, horizon ring, relay
      pacing and camera composition move behind engine interfaces (the compositor's own job by
      CLAUDE.md's decomposition)
- [ ] the in-game UI (speed, route, prompts) is declared in the engine's own markup/style tree
      (src/ui), not painted by the tool
- [ ] NPC cars: the same declared vehicle, a recolour per instance, placed by a compositor

## Comments

The two review crons are aligned with this: the magazine tester (even hours) judges the
first-person picture of a worldwide test-drive game; the technical reviewer (odd hours) judges
the tree against RAGE and Unreal -- including whether the driver is still doing engine work.
