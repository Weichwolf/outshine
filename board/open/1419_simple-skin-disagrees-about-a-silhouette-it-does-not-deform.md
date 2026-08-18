Type: bug
Area: gltf
Tags: khronos, oracle

**SimpleSkin disagrees about a silhouette it does not deform**

[MEASURED] `worst_disagreement_px` **6.5815859** over **1595** samples, IoU **0.9591842** — and
`picture_p99_delta_code` **0**, so the colour is exact wherever both sides agree a pixel is covered.
**The disagreement is purely which pixels the strip covers.**

## Four hypotheses, all four refuted before anything was changed

| hypothesis | how it died |
|---|---|
| **the rotation is lerped where glTF says slerp** | `Track.cpp` slerps: `Spherical_ && Curve_.How() == Interpolation::Linear` |
| **the weights are not normalised and Blender normalises** | [MEASURED] all ten vertices sum to **exactly 1.0** |
| **the two sides sample the pose at different times** | the manifest declares no `sequence`, so the case is a still at t = 0, and the preparer sets `scene.frame_start = 0` and calls `frame_set` explicitly |
| **the skinning is wrong** | **at t = 0 BOTH joint matrices are the identity.** Keyframe 0's rotation is `[0, 0, 0, 1]`; joint 0 is the origin with an identity inverse bind, and joint 1 is `T(0, 1, 0)` against an inverse bind of `T(0, -1, 0)`. `world * inverseBind` is `I` for both, so the mesh is **undeformed** and no blend of identities can move a vertex |

**So the case named for skinning is not failing at skinning**, and that is the whole value of these four:
the next round starts with the mesh, the camera or the rasterisation of a thin quad, and not with the
skin.

## What is left, and it is narrowed rather than named

- [ ] **The two pictures were LOOKED AT.** Same green strip, same place, same lean; the visible
  difference is the **left edge of the upper half**, where the reference carries a small step ours does
  not. That is consistent with 6.58 px at the worst sample and 4 % of the union
- [ ] **The camera is not it**: the declared frame fraction and the projected one agree to every digit
  printed, `0.0416262738`
- [ ] **A ten-vertex strip has long thin triangles**, and a silhouette over one is the most sensitive
  thing this corpus rasterises. Whether Blender's armature conversion moves a rest vertex is the next
  question, and it is asked of the ORACLE rather than of us
