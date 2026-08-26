Type: bug
State: open
Parent: 1547
Area: engine, sim, render
Tags: measured, precision, drive

# A drive stands in the WORLD, and the ground is a compositor's draw item

CLAUDE.md says *one world space*. A drive stands on the CORRIDOR's ENU origin and the ground ring
is laid about `Patchwork::OriginEcef`. Everything below is measured at a73c6ca5 by running the
acceptance command in a fresh worktree and reading the ten stills.

## What arrives now

    MEASURES tiles the ring laid = 9 tiles   waiting 0   absent 0   refused 0
    MEASURES batches the picture draws = 259 batches      (258 the vehicle, 1 the ground piece)
    MEASURES the body, up = 513.528 m        the mesh it carries, up = 513.931 m

Nine tiles of nine, awaited rather than polled (board:1914). The vertex stride is named once,
`kTileVertexFloats` in `TileMeshes.h:11`, and the compositor no longer reads a normal as a
position -- 1948 vertices per tile against 5194, 489.8..532.1 m of relief over 815 m of ground.

## What the PICTURE says, and the count does not

**Below the horizon, in all ten stills, the frame is the painted plane it was last round.**
Sampled at x=700 and over a 100 x 700 px block in stills 01, 03, 05 and 10:

    (34..35, 42..43, 32..34)   four counts of variation, identical across 2.9 km

which is `ParticipatingMedium.h:29` `GroundAlbedo = {0.10f,0.13f,0.07f}` seen through the
atmosphere. The ring is on screen only as a thin pale sheet ABOVE the horizon line -- (11,24,50)
then (33,60,107) at x=700 in still 03, a jagged silhouette cutting the sky and vanishing again
by still 05. Nine tiles of terrain arrive, draw, and stand where the sky is.

**The height is the whole of it.** The ring's vertices carry `where.AltM`, geodetic altitude
above the WGS84 ELLIPSOID, straight into the corridor frame's `up`:

    src/engine/Engine.cpp:340   inFrame[at + 1] = (float)where.AltM;

while the body stands at 513.528 m in the same axis. Those two agree only if the corridor frame's
origin is the ellipsoid. At Munich the geoid undulation is about +47.6 m (EGM96), which is the
order of the gap and the first thing to measure -- *confidence: likely, not established.* The
frame's own datum has to be stated and both sides converted to it, once.

**And the surface has no identity.** `ground.Material = 0` takes the subject's first surface,
which is the car's. No albedo, no slope response, nothing that tells road from field.

## The mechanism, corrected -- the headline was wrong twice

- `Live::Carry` already gives every part before `Joined_` the vehicle's placement times
  `MetresPerUnit` and every part at or after it the identity (`src/engine/Live.cpp:583`), so
  "the ground inherits the vehicle's model scale" was false at the placement level.
- `Restand` calls `Stand()`, which resets `Stood_.PartPlacement`, so a `Carry` before it is
  thrown away. That, not scale, was the shrapnel.
- What refused was the ASSET's own camera: `Document.cpp:840` reads a `znear` of 637.888958 in
  asset units -- 9.9 m -- and `Aim`'s `standsInside` was default at both call sites.
- A count is not a picture. 517 batches was met once by nonsense, and 259 is met today by a
  ring in the sky.

## What will be true

- [ ] The ground is a COMPOSITOR's draw item with its own scale and placement, never a part
      appended to the vehicle's glTF. This is the box everything else waits on.
- [ ] The corridor frame and the tile mesh state ONE datum and both are converted to it, with
      the conversion proven at a coordinate whose undulation is published.
- [ ] `if (false)` leaves `Engine.cpp:286` (board:1917) and the ring carries a material of its own.
- [ ] The still shows terrain UNDER the car. Proving case: the declared drive through `include/`
      alone, asserting that the lower half of the frame carries more than the four counts of the
      painted plane AND that its brightness follows the sun's elevation -- a picture and a count,
      because a count alone has now been met by nonsense twice.
