Type: bug
State: open
Parent: 1547
Area: engine, sim, render
Tags: measured, precision, drive

# A drive stands in the WORLD, and the ground is a compositor's draw item

CLAUDE.md says *one world space*. A drive stands on the CORRIDOR's ENU origin and the ground ring
is laid about `Patchwork::OriginEcef`. The car now has a sky and a horizon (board:1870) and no
TERRAIN, and this is the item that ends that.

## What is TRUE at 3187c4c3, measured by running the binary

The overridden 302 m drive, first person, 1280x720:

    MEASURES the body, up = 524.444 m          -- the terrain altitude at this coordinate
    MEASURES the eye,  up = 525.115 m          -- eye - body = 0.671 m, the declared seat
    MEASURES batches the picture draws = 258   -- the car alone
    mean max(RGB) 47.19, peak 250, 921 600 px opaque

The placement chain is right and each part has a proving case: the 0.55 m centre-of-mass
subtraction, the material shift on `Append`, the framing over the framed parts only, `Joined_`
before `Build`, and the tile round trip.

## THE THREE LINES WERE TRIED AND THE PICTURE REFUTED THEM

A previous round of this item held that three named lines land the ground in the draw list
WITHOUT waiting for the compositor cut, and that a change moving 258 batches to 517 is provable
through the door as it stands. All three were written and the count came back exactly as
predicted:

    MEASURES batches the picture draws = 517 batches      (258 the car, 259 the ground)

**And the picture is shrapnel.** The ring draws as a fan of jagged triangles across the right
half of the windscreen, reaching from the bonnet to the frame edge. It is not terrain seen
wrongly -- it is the vehicle's ASSET UNITS applied to a ring measured in METRES, which is what
`Gltf::Subject world = Shown(); world.Append(ground)` does by construction. The near-plane walk
refuses one step further in, at a vertex 214 m along a view axis whose derived near plane is
637.9 m.

A COUNT IS NOT A PICTURE. 517 batches is the number the earlier round asked for and it means
nothing on its own: the engine drew confidently and drew nonsense, which is worse than the
refusal it replaced, because a failure here is meant to be loud. The three lines were reverted
and the refusal now carries this measurement instead of an assertion:

    src/engine/Engine.cpp:284   if (Drove) { ... "it draws as shrapnel across the windscreen -- a
                                count is not a picture" ... }

So the scale mixing is NOT the fourth box behind three easier ones. It is the FIRST box, and the
three lines are its consequence.

## FOUR CORRECTIONS, measured at 3f50607c, and each one moves the item

**1. The placement chain ALREADY separates the two scales, so the headline mechanism is not the
mechanism.** `Live::Carry` gives every part before `Joined_` the vehicle's placement multiplied
by `MetresPerUnit`, and every part at or after it a second matrix:

    src/engine/Live.cpp:583   const double *const from = part < Joined_ ? body : built;
    src/engine/Engine.cpp:932 const double stillM[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                              Standing->Carry(bodyFromWorld, stillM, Error)

`built` is the IDENTITY. A ground appended after the driven parts is placed in metres, unscaled,
which is exactly what it needs. "The ground inherits the vehicle's model scale" is false at the
placement level.

**2. `Rides()` runs AFTER `Composes()`, and the item had the order backwards.** `Restand` calls
`Stand()`, which resets `Stood_.PartPlacement` to identity for every part. A `Carry` before the
restand is thrown away, so the vehicle stands unplaced beside a ground that is placed -- which is
what the shrapnel was.

**3. What refuses is the ASSET'S OWN CAMERA, and its near plane is a constant.**

    CARRIES the ground did not compose: vertex 1228279 sits 401.258304 m along the view axis,
    inside the near plane of 637.888958 m this placement declares

637.888958 is IDENTICAL in every configuration run -- `overADrive` true and false, different
vertices, different routes. A derived near plane does not do that. It is the subject's own
declared camera:

    src/content/gltf/Document.cpp:840   camera.ZNearM = lens["znear"].Num(0.0)

in ASSET UNITS: 637.9 x 0.0155 m/unit = 9.9 m. A studio camera's near plane, applied to an eye
sitting in the cabin. `Aim`'s `standsInside` parameter exists for exactly this and `Place` passes
its default -- but `Stood_.EyeStandsInside` is set from `HaveEye_`, and at ASSEMBLY time no view
has been taken yet, so it is false when it matters.

**4. The draw list does reach 517 batches** -- 258 the car, 259 the ground -- and the refusal now
lands AFTER the geometry rather than before it. The count was never the hard part.

## THE GROUND IS IN THE PICTURE, and two numbers stand between it and terrain

At b0b59b3a the ring is nine tiles of nine (board:1914) and reaches the horizon through the
windscreen. What it is not yet:

**1. It sits too high.** The ring's vertices carry `where.AltM` -- geodetic altitude above the
ellipsoid -- straight into the corridor frame's `up`:

    src/engine/Engine.cpp   inFrame[at + 1] = (float)where.AltM;

while the body stands at `body.PositionM[1] = 524.444 m` in the same axis. Those two agree only
if the corridor frame's origin is the ellipsoid, and the picture says it is not: the terrain
band crosses the windscreen ABOVE the horizon rather than running under the car.

**2. It is pale and flat.** The composed patch carries one material -- `ground.Material = 0` --
so it takes the subject's first surface, which is the car's. There is no ground albedo, no slope
shading beyond the normals, and nothing that distinguishes road from field.

Both are placement and surface questions over geometry that is now correct: 1948 vertices per
tile at the stride the layout declares, 489.8..532.1 m of relief over 815 m of ground.

## What will be true

- [ ] The ground is a COMPOSITOR's draw item with its own scale and its own placement, never a
      part appended to the vehicle's glTF. This is the box everything else waits on.
- [ ] No branch in the engine is nailed shut by a constant. `overADrive` (Engine.cpp:283) goes
      the day the compositor carries the ring.
- [ ] The still shows terrain under the car. Proving case: the 302 m drive through `include/`
      alone, asserting the composed batch count exceeds the vehicle's own AND that the still's
      lower half carries a surface whose brightness follows the sun's elevation -- a count and a
      picture, because this round proved a count alone can be met by nonsense. Negative control:
      the ring appended to the vehicle's glTF as it is today, and the case is red on the picture
      while green on the count.
