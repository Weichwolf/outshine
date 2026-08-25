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

**MEASURED FURTHER, same session.** The vehicle half of the chain is CORRECT and the ground
half is not, and the two were separated by running the drive with the ground composition
suppressed:

| run | still |
|---|---|
| drive, chase view, NO ground | the F31 from 7 m behind and slightly above, exactly what `<view id="chase" distanceM="7.0">` declares |
| the same, ground composed | black, or an edge of untextured geometry filling the frame |

So `Rides` -> `Carry` -> `Eye` place the car and the camera in one another's space correctly
once the model scale is applied (`Rigged::MetresPerAssetUnit`, 0.01555 m per unit, which
`Rigging.cpp:112` has always computed and nothing applied). What misses is the GROUND: it is
laid about `Patchwork::OriginEcef` in metres, a fourth origin, and dividing its positions by
the model scale moves it without placing it.

Composing a ground for a scenario that declares only a DRIVE is withdrawn until this is
settled -- a ground that swallows the camera is worse than no ground, and shipping it would
have been a half answer. `Engine::Compose` keeps its caller in `Assemble` and still lays the
ground a `<ground>` declares.

**NARROWED, same session.** Two of the four spaces are gone. `Live::Declaration` carries
`MetresPerUnit` -- glTF is metres BY SPECIFICATION and the F31 asset is not, which is exactly
what `assetWheelbase` corrects -- and `Live::Carry` takes a placement in WORLD METRES, building
the model scale into the placement it writes. `Engine::Rides` and the camera work in metres
throughout; nothing divides by a scale any more.

What remains is ONE seam, and it is the ground's: `LayPatchwork` anchors its ring on
`Patchwork::OriginEcef` while the drive stands on the corridor's own origin. Composing a ground
for a drive still swallows the camera, so it stays withdrawn with this reason.

**SEEN BY THE REVIEW at d4c8784c, and it is a THIRD reading of the same seam.** The 136 m
Munich drive with the SHIPPED scenario's own `<player view="eyes">` — a FIRST-PERSON eye
declared at `(-0.494, 1.220, 0.003)`, inside the cabin — keeps eight stills that all look DOWN
at the car's ROOF from outside and above-left, the body cropped by the bottom edge. Not one of
them is a view from a seat.

`Engine::Rides` builds the eye correctly in metres (src/clients/Engine.cpp:836-841: the offset
is carried on the body's own unit basis, `ahead` is one metre along `-Z`), and then hands it to
`Live::Eye(from)` — while `Live::Carry` has just scaled the body placement's basis by
`MetresPerUnit` into the studio's model units (src/clients/Live.cpp:539-547). The placement is
converted and the camera is not, so the two stand in different spaces by a factor of 64.3. The
same defect as the ground ring, one seam over.

**THE SEAM IS LOCATED, same session, and the ground still does not land.** Four measurements,
each one narrowing it:

| step | what it showed |
|---|---|
| pose in metres against an asset in its own units | every still black; fixed by `MetresPerUnit` at the stand |
| ground in ECEF-relative metres, vehicle in corridor ENU | ground and car in ONE picture for the first time, but the ground reads as wedges |
| ground converted to the corridor's frame (`Corridor::FrameLat/FrameLon/PerLatM/PerLonM`, which the corridor has always published, plus `FrameAltM` added here) | unchanged -- because `Carry` was handing the GROUND the vehicle's own pose as its `built` matrix, so the ground drove off with the car |
| ground given the identity placement instead | the ground vanishes entirely |

So the ground's vertices are now in the corridor's frame, in metres, and the placement that
should leave them there makes them invisible. The remaining unknown is what space the subject's
part placements are expressed in once `MetresPerUnit` scales the vehicle's -- a ground in metres
with an identity placement is not in the same space as a model in units with a scaled one, and
sharing one Subject is what forces the question.

TARGET answers it: the ground is a COMPOSITOR'S draw item, not a part appended to the vehicle's
glTF. `Gltf::Subject world = Shown(); world.Append(ground)` is the shortcut this seam is made
of.

## What will be true

- [ ] A driven body's placement is expressed in the ONE world space, in metres, as a 64-bit
      position. The corridor states where its origin is in that space; the drive does not carry
      a private one.
- [ ] The subject's model-to-metre scale is READ from the asset the scenario declares
      (`assetWheelbase` over the declared `wheelbaseM` is that number and the scenario already
      states both) and applied once, where the part placement is built -- never assumed to be 1.
- [ ] `Live::Carry` takes a placement in world metres. The studio anchor is what a scenario
      WITHOUT a sphere stands on, not a term every placement passes through.
- [ ] No branch in the engine is nailed shut by a constant. `const bool overADrive = false;`
      (src/clients/Engine.cpp:271) is a measurement scaffold whose reason lives only in commit
      657b5903 -- the source may carry no comment, so a switch like this is unreadable from the
      tree and must not outlive the question it was cut for.
- [ ] The CAMERA is in the same space as the placement. A first-person view declared inside
      the cabin renders the cabin; a chase view at `distanceM=7.0` renders the car from seven
      metres. Today both render the roof from above.
- [ ] Proving case: the 136 m Munich drive with `--stills 6` keeps six stills in which the
      ROAD is visible and the horizon is where the camera height implies. Negative control:
      feed the pose in corridor-relative metres as model units again and every still is black.
