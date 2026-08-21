Type: task
Parent: 1498
Area: clients
Tags: scope

**Both modes are one code path, and the split is at the SINK**

The goal says *game ready means both modes are the same code*: headless with no renderer linked at
all, and the identical scenario in a window at 720p60. They must be two BINARIES, because a headless
run that links a renderer is not a headless run -- so what they share has to be a translation unit and
not a `main`.

`tools/driver/Journey.h` declares that unit. The split that makes it work is:

> **The journey REPORTS and the caller JUDGES.** Every `Note` in the drive becomes `Sink::Number`,
> every `CHECK` becomes `Sink::Claim`. The test implements the sink with the harness; the windowed
> program implements it with a log or with nothing. Not one line of the driving code knows which it
> is talking to.

## What must be true

- [ ] **`Journey::Lay` holds everything from the fetch to the speed profile**, and `Journey::Ride`
      is one physics step -- so a headless loop and a frame loop call the same two methods
- [ ] **The state that crosses from Lay into Ride is named**, not implicit: the corridor, the profile,
      the rig, the per-station lane and edge arrays, and the lane centre the car is holding
- [ ] **The windowed program stands the same scenario up** and draws the F31 at `Carried()`'s pose,
      first and third person, with the player able to take the wheel
- [ ] **The headless binary links no renderer**, and a claim proves it by reading its link line

## Comments

**The move is mechanical and it is not small**: 693 lines of preparation and 236 of driving, with 136
measurements and 33 claims inside them. The transformation is `Note -> say.Number`,
`CHECK -> say.Claim`, and every local that crosses from Lay into Ride becomes a member.

**It is deliberately not half-done.** That code has driven 774.847 km without leaving the carriageway,
and a refactor that lands partly is worse than the file it replaces. `board:1526` is next to it in the
same area: the headless binary today links SDL because `TerrainLoader` reaches for `IMG_Load`, so
"links no renderer" is not yet true either.

The F31 asset is in place and verified: `scene.gltf` hashes to
`c60068fcd0f8c25e73225cd3725a422fca46c00a2a68ca481988a6680cc5fb1d`, exactly what
`tools/driver/f31.scenario` declares, with `scene.bin` and its textures beside it -- 30 MB, and not
committed, which is why the digest is in the declaration.
