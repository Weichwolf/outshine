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

**`board:1465` IS CLOSED AND TWO OF THE THREE DID NOT MOVE**, which is what narrows this item to its
real subject. `Animation_Node_03`'s refusal went from 3.138705 m to 3.061730 m when posing began to
follow the declaration -- so it IS posed now, and it still misses.

## What is left is a disagreement about the POSE, and the numbers say so

[MEASURED] both sides sample the same instants -- the preparer publishes them and they match the grid
the runner poses at:

| case | instants | the union Blender saw | the vertex this engine renders |
|---|---|---|---|
| `Animation_Node_03` | 0 s, 2 s | `x in [-0.4, 0.2]`, a 0.6 box, half-diagonal **0.5196 m** | **0.707 m** from the centre |
| `Animation_NodeMisc_02` | 0 s, 3 s | `y in [-0.4, 0.2]` | 0.0098 m past the plane |

**A vertex 0.707 m from the centre of a box whose half-diagonal is 0.5196 m is not in that box.** The
camera is conservative by construction -- the sphere of the half-diagonal contains the AABB, which
contains every transformed vertex -- so a vertex outside it means the two sides put the subject in
different places at the same instant.

`Animation_Node_03` is **STEP** interpolation and `Animation_NodeMisc_02` has channels that start after
zero. Both are cases where the value AT a keyframe boundary is a convention, and the two importers may
not share it: glTF's STEP holds the previous key's value up to and including the next, and Blender's
f-curve CONSTANT interpolation is what its importer maps that onto.

## THE MEASUREMENT WAS TAKEN AND IT IS THE PREPARER'S SIDE

`camera.derivedFrom.atEachInstant` now publishes what every instant of the walk contributed, because a
UNION says nothing about whether the walk moved anything -- a camera derived from one pose twice looks
exactly like a camera derived from a subject that does not move.

[MEASURED] `Animation_Node_03` keys a **STEP** translation at 0, 1, 2, 3 and 4 seconds with values
`-0.1, 0.0, +0.1, 0.0, -0.1`. glTF's STEP holds a key's value from that key until the next, so at
**t = 2 s the value is +0.1** and this engine renders it there. The preparer's walk reports:

| instant | what Blender's bounds gave |
|---|---|
| 0.0 s | `x in [-0.4, 0.2]` -- the cube at translation **-0.1**, which is right |
| 2.0 s | `x in [-0.4, 0.2]` -- **the same pose again** |

**So the engine is right and the ORACLE'S CAMERA WALK is stuck at the first instant.** The rest pose
would be `x in [-0.3, 0.3]`, so an action IS assigned and evaluated -- it simply never advances.

**`bpy.context.view_layer.update()` before re-taking the depsgraph does not fix it**, which is worth
recording: the obvious answer to *a `frame_set` that does not take* is not the answer here.

## What is left to try, in order

- [ ] **Print the f-curve keyframe positions from inside Blender.** The importer converts seconds to
  frames with the scene rate at import time, and this corpus is the first to use a FRACTIONAL rate --
  `fps = 1`, `fps_base = 2.0`. If the importer reads `scene.render.fps` alone and ignores the base,
  every key lands at twice its frame and the walk samples inside the first interval forever
- [ ] **Derive the camera AFTER the preparer's own animation setup**, which runs later in `main` and
  bakes channels itself -- the camera may simply be reading a scene that is not yet posed
- [ ] **Compare against a case with a WHOLE rate**, which is every case of the older corpus: if those
  walk correctly, the fractional rate is the discriminator and the first line above is the answer

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
