Type: bug
State: open
Parent: 1547
Area: engine, sim, render
Tags: measured, precision, drive


**Benchmark** — Unreal: the landscape is a primitive in the same scene as everything else, drawn through the same proxy path. RAGE: the map is streamed geometry like any other. **Both agree** — the ground is a draw item, not a special case beside the drawing.
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

**The height is NOT it, and the measurement says so.** At the vertex nearest the frame origin
the ring stands at **522.23 m** against a body at **524.444 m** -- 2.2 m, which is a ride height
and a DEM's resolution, not a datum. The geoid hypothesis is dead: an undulation of 47 m would
show as 47 m. What stands above the horizon is the ring's FAR edge, 532 m against 522 m near,
rising 10 m over 800 m: +0.7 degrees above a 1.2 m eye, which is what terrain does.

**The surface is what is left.** `Live::Restand` now hands the composed ground the medium's own
`GroundAlbedo`, so the drawn ground and the sky's painted ground name ONE colour instead of a
mid-grey `Material{}` sheet. What it still lacks is a surface with identity: no slope response,
nothing that tells road from field, and no use of the ground colour table the world already
carries (`VegetationTemplates::Row::Ground`, reached at `src/engine/Sim.cpp:98`).

Measured through the windscreen after: the ring reads (78, 94, 109) at 2 to 5 km, the painted
ground below it (34, 42, 32), the sky above (45, 73, 108). The ring is bluish because it IS
bluish -- the atmosphere veils it, which is aerial perspective and correct. The painted ground is
not veiled at any distance, and the seam between them is board:1918.

## The picture, re-read, and the headline was wrong a third time

Six stills over 300 frames of the reference route, read at HEAD:

    the elevation where the route starts   522.308 m   (the DEM)
    the ring's nearest vertex               522.672 m   0.36 m above it -- DEM resolution
    the body                                522.252 m

**The ring is not in the sky and it is not at the wrong height.** It stands under the car, within
a third of a metre of the terrain the corridor was laid on. What the still shows below the
horizon IS the ring, and it reads as a painted plane because it wears the same `GroundAlbedo` the
painted plane wears and carries nothing else -- no slope response, no class, no texture, no
markings. The only part of it a reader can SEE is its far edge, where terrain rising 10 m over
800 m stands against the sky and cuts a dark jagged band.

So "nine tiles arrive, draw, and stand where the sky is" was a misreading of a picture in which
the ring is almost entirely indistinguishable from what it is drawn over. The count and the
placement are both right; **the surface is the whole of what is left**, and the box below that
names it is the one everything now waits on.

Measured in the lower middle of three stills: R 1..65, G 1..80, B 2..90, a spread of 88 counts --
against the four counts this item recorded for the painted plane last round. The spread is the
ATMOSPHERE over a smooth surface plus the car, not relief: the gradient runs top to bottom with
no structure in it anywhere.

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

- [ ] The ground carries a SURFACE: the class under each point drives its colour and its
      roughness from `VegetationTemplates::Row::Ground`, slope answers the light, and a made
      carriageway is told from a field. This is the box everything else waits on, and the drive
      path can now ask the question -- `ClassField::ClassAt` reaches it through `GroundStack`
      (board:1924).
- [ ] The ground is a COMPOSITOR's draw item with its own scale and placement, never a part
      appended to the vehicle's glTF.
- [ ] The corridor frame and the tile mesh state ONE datum and both are converted to it, with
      the conversion proven at a coordinate whose undulation is published.
- [ ] `if (false)` leaves `Engine.cpp:286` (board:1917) and the ring carries a material of its own.
- [ ] The still shows terrain UNDER the car. Proving case: the declared drive through `include/`
      alone, asserting that the lower half of the frame carries more than the four counts of the
      painted plane AND that its brightness follows the sun's elevation -- a picture and a count,
      because a count alone has now been met by nonsense twice.
