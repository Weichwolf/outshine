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
