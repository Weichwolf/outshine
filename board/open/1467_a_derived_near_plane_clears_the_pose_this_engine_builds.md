Type: bug
Area: corpus
Tags: oracle, khronos

**A derived near plane clears the pose this engine builds**

A camera derived from a subject's bounds has a near plane no vertex of that subject can sit inside, at
every frame of the grid it was derived over -- on the side that renders it as well as on the side that
derived it.

## What it is

`camera.source: derived` unions the subject's world bounds inside Blender and sets
`clipStartM = distance - radius`, which is exact for a bounding SPHERE and conservative for the box the
radius is the half-diagonal of. **No vertex can be closer than that -- from the bounds Blender saw.**

[MEASURED] three cases of the generator corpus refuse anyway:

| case | what it keys | the refusal |
|---|---|---|
| `Animation_Node_03` | translation, STEP interpolation | vertex inside the near plane |
| `Animation_NodeMisc_02` | two channels, 2 s to 6 s and 1 s to 5 s | the same |
| `Animation_NodeMisc_03` | one keyframe at `[-0.1, 0, 0]` | 3.138705 m against a plane of 3.191292 m |

**`Animation_NodeMisc_03` is `board:1465` wearing this face**: its grid is one frame, so `Animated()`
is false and this engine builds the REST pose while Blender applies the animation -- 0.0526 m apart,
which is half the translation. The other two declare two frames and are posed on both sides, so their
disagreement is about the bounds themselves.

## What must be true

- [ ] **The instants the bounds are unioned over are the instants both sides pose at**, stated once and
  read by both -- the preparer publishes them in `camera.derivedFrom.instantsS` already, so the runner
  can refuse a grid that does not match rather than discovering it as a clipped vertex
- [ ] **A refusal names the frame it happened at**, because *a vertex is inside the near plane* says
  nothing about which pose put it there
- [ ] **`board:1465` is closed first**, since one of the three is that defect and fixing the bounds
  would paper over it

## What this bug may NOT be fixed by

**It may not be fixed by widening the near plane.** A clip range is a depth window and a margin nobody
derived is a number that hides the next disagreement instead of reporting it -- and `board:1433` records
this tree already paying for a clip range that was not the grid's.
