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

**THAT IS THE THING TO MEASURE NEXT, and it is measurable without a picture**: pose the subject at the
declared instants on both sides and compare the world positions. A disagreement there is a reader
defect and belongs to `src/gltf`; agreement there would move this item to the camera after all.

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
