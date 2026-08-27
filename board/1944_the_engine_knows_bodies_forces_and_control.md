Type: feature
State: open
Progress: actors
Area: core
Tags: benchmark, target

# The engine knows bodies, forces, actuators and control -- and no vehicle

**Benchmark** — Unreal: `FBodyInstance` in the engine, `CVehicle`-equivalents in a plugin. RAGE: `phInst` in physics, `CVehicle` in the game layer. **Both agree** — no vehicle noun inside the engine.

RAGE keeps `CVehicle` in the GAME layer; Unreal keeps wheeled movement in a plugin over a
component model where a body carries components and a controller possesses a pawn. outshine's
engine core names `Drive` 29 times and `Vehicle` 6 times in `Engine.cpp` alone.

- [x] what a wheel stands on comes from the ground, not from a corridor: height, normal and
      friction from the surface under it
      proof: outshine/physics/ScoreWhatAWheelFindsOffTheMadeSurface
- [x] a road class carries its surface and that number survives to the contact unchanged
      proof: outshine/physics/ScoreWhatASurfaceCarriesToAWheel
- [ ] a body declares ACTUATORS -- steer, drive, brake, lamps, walk, open -- and a control
      command activates a force rather than setting a state (board:1897)
- [ ] a drive is ONE assembler among several chosen from the catalogue, and the engine holds the
      list rather than the call (board:1897)
- [ ] `Column<Vehicle>` and `Column<Drive>` leave `Engine::State`; the engine places every body
      that moved and reads a POSE, not `Drive.State.Body` (board:1897)
- [ ] a vehicle is declared the way JSBSim declares an aircraft: every number named, sourced and
      refusable (board:1509)
- [ ] a component knows what it CAN, what it MAY and what it DOES, refused at assembly
      (board:1583)
