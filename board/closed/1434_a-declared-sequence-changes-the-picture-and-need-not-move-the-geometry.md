Type: bug
Area: harness
Tags: instrument, khronos

**A declared sequence changes the picture, and need not move the geometry**

Two of the animated suite's claims asked for geometric motion where the format asks only for a changing
picture. `KHR_animation_pointer` lets a channel drive a material, and
`PotOfCoalsAnimationPointer` uses it for exactly that: [MEASURED] its two channels are
`/materials/2/normalTexture/extensions/KHR_texture_transform/rotation` and the same rotation on
`KHR_materials_volume`'s thickness texture. **Not one vertex moves at any frame of its grid**, so both
claims were unsatisfiable by any correct implementation.

| claim | asked | asks now |
|---|---|---|
| the sequence is not a still | the DRAWN SUBJECT moves over the grid | the PICTURE changes -- the subject moves, **or the oracle's own frames differ** |
| `velocity_pixels_moving` | at least one moving pixel at every frame after the first | at least one where **this frame's pose moved**, and **none** where it did not |

## The vacuity question is asked of the reference, and that is the point

What the clause is for survives whole: a grid that renders one thing N times agrees with the oracle by
construction rather than by being right. **But whether the grid is empty is a property of the
DECLARATION, and the oracle is what renders the declaration without our gaps in it** -- so a grid the
reference varies is a real comparison even where the variation is a material's, and a grid the reference
renders identically is empty whatever we do with it. Measured by an FNV-1a over the oracle's f32 samples,
because the answer wanted is a boolean and a picture-sized subtraction to reach it would be a second
comparison nobody reads.

## The velocity claim got STRICTER in the same edit, and that is not a widening

Where the pose did not move, the velocity target must now carry **no** motion -- `at most 0` -- which is
a claim the old rule never made at all. A frame that moves still owes a moving pixel. *A rule about a
comparison names which side it constrains, and this one constrains both, differently, by what the pose
did.*

## What this does NOT do

**It does not make `PotOfCoalsAnimationPointer` correct.** `board:1392` records that nothing drives a
material per frame yet -- the channel is read, resolved and carried, and `Pose` answers the material arm
by doing nothing. This edit only stops the case failing for a reason that was never true of it.

## What must be true

- [x] a case whose oracle renders one picture at every frame of its grid is still refused -- `PotOfCoalsAnimationPointer` is, and it closes on a declared reduction naming the oracle's limit rather than on a widened bound
- [x] a case whose subject moves still owes a moving velocity pixel at every frame that moved -- [MEASURED] `Fox` carries 21 410, 21 522, 20 225 and 20 489 of them across its four moving frames

## The first gate used the wrong distance, and the full run said so

The velocity claim was gated on `Motion::MovedPx`, which is measured **from frame 0** -- deliberately, so
that an animation ENDING where it began is not refused for it (`board:1169`). A looping grid's last frame
therefore reads zero there while its geometry moved a great deal since the frame before, and the gate
asked a moving subject to hold still.

[MEASURED] six animated cases went red on it in one run -- `AnimatedColorsCube`, `Fox`, `MeshoptCubeTest`,
`RecursiveSkeletons`, `RiggedSimple`, `SimpleMorph` -- with `Fox` frame 4 carrying **20 489 moving pixels
against a bound of zero**. `Motion` now answers both distances, the velocity claim takes the one from the
PREVIOUS frame, and all six are green again alongside the three the change was for.

**The full suite is what caught it**, one run after the change and before it was committed. *A number
that only the case in front of you agrees with is not a measurement of the tree.*
