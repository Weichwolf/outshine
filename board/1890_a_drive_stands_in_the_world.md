Type: bug
State: open
Parent: 1547
Area: clients, sim, render
Tags: measured, precision, drive

# A drive stands in the WORLD, and the studio is not a place

The chain from the drive to the picture is now joined -- `Engine::Rides` builds a pose from
`DriveState::Body` and hands it to `Live::Carry`, and the declared view aims the camera
(board:1862). The stills DIFFER, which is what that item asked for. **And they are black.**

Three spaces meet at `Live::Carry` and no two of them agree:

| what | space | unit |
|---|---|---|
| `DriveState::Body::PositionM` | ENU about the corridor's own origin (`DriveTick.cpp:53-58` reads `[0]` as east, `-[2]` as north, `[1]` as up) | metres |
| the glTF subject | the studio, anchored at `kStudioAnchorEcefM = {kWgs84A, 0, 0}` | MODEL units: `assetWheelbase="180.71"` for a 2.810 m wheelbase, so 64.3 per metre |
| `Live::Carry`'s own arithmetic | treats `body[12..14]` as glTF, converts with `EcefFromGltf` and adds the studio anchor | model units |

So a pose in corridor-relative metres is read as studio model units: the camera lands inside
the bodywork for the first-person view and inside the ground for the chase view, and both
answer black. The ground composes correctly at the same time (9 tiles meshed), which is how we
know the world exists and the placement is what misses it.

CLAUDE.md: *one world space*, and *precision has ONE boundary and it is the camera* -- the
scene keeps 64-bit positions and the renderer is camera-relative in 32-bit. A studio anchor at
the equator is a THIRD origin beside the world and the corridor, and the model-unit scale is a
fourth quantity nobody converts.

## What will be true

- [ ] A driven body's placement is expressed in the ONE world space, in metres, as a 64-bit
      position. The corridor states where its origin is in that space; the drive does not carry
      a private one.
- [ ] The subject's model-to-metre scale is READ from the asset the scenario declares
      (`assetWheelbase` over the declared `wheelbaseM` is that number and the scenario already
      states both) and applied once, where the part placement is built -- never assumed to be 1.
- [ ] `Live::Carry` takes a placement in world metres. The studio anchor is what a scenario
      WITHOUT a sphere stands on, not a term every placement passes through.
- [ ] Proving case: the 136 m Munich drive with `--stills 6` keeps six stills in which the
      ROAD is visible and the horizon is where the camera height implies. Negative control:
      feed the pose in corridor-relative metres as model units again and every still is black.
