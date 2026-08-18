Type: bug
Area: corpus
Tags: oracle, khronos

**A still is rendered at the instant it declares**

A case that declares no `sequence` is a still at **t = 0**, and the oracle rendered it at **t = 1/fps**.
Blender opens at frame 1; the preparer set the frame only on the animated path, so a still whose file
happened to carry an animation was posed one frame into it and nobody said so.

## The measurement came before the repair and predicted its size

`SimpleSkin`'s joint turns 45 degrees over half a second, so at 24 fps one frame is **3.75 degrees**.
Rotating the far corner `(-0.5, 2, 0)` about the joint at `(0, 1, 0)` by that angle moves it
`(-0.064, -0.035)` in the subject's own metres; the strip is one metre wide and spans 108 px, so the
prediction is **6.9 px left and 3.8 px down**.

| | predicted | observed |
|---|---|---|
| far corner, horizontal | 6.9 px left | **6 px left** |
| far corner, vertical | 3.8 px down | **4 px down** |

| `SimpleSkin` | before | after |
|---|---|---|
| `worst_disagreement_px` | 6.5815859 against a floor of 0.005 | **0** |
| `iou` | 0.9591842 | **0.99997393** |
| `pixels_disagreeing` | 1595 | **1** |

## Which side was wrong was decided by a third instrument, not by preference

At `t = 0` both joint matrices are the identity, so the strip is its own rest mesh -- ten vertices of a
flat 1x2 rectangle. **Rasterised on the CPU from the file's own POSITION accessor and the manifest's own
camera**, that mask differs from ours by **1 pixel** and from the oracle's by **1594**. *A disagreement
between two renderers is settled by a third thing that is neither of them.*

## The population, quoted with the number

[MEASURED] **2 of the 147 manifests** that declare no animation have a subject whose file carries one:
`SimpleSkin` and `CubeVisibility`. Every other still is untouched by construction, because a frame index
decides nothing in a file with no sampler in it. `CubeVisibility` cannot be measured either way today --
Blender's importer refuses `KHR_node_visibility`, which is one of the three cases already standing
unprepared.

## Comments

`board:1419` refuted five hypotheses on this case and every one of them stands: the slerp is a slerp, the
weights sum to one, the skinning is identity at t = 0, the camera agrees to every digit, and the missing
`skeleton` is conforming. **It was none of those and it was not in the engine at all** -- the value of
those five is that they are what made the sixth cheap, because they left only the instant.
