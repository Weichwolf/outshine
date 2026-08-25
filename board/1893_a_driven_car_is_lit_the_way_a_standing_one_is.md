Type: bug
State: open
Parent: 1890
Area: render, clients
Tags: measured, picture, driver

# A driven car is lit the way a standing one is

The SAME asset, the SAME `<lighting>` declaration, two frames from the same binary at d4c8784c:

| frame | what it draws |
|---|---|
| `refused.png` — the studio stand-up, no drive | the F31 in silver: specular along the shoulder line, the glasshouse dark against a lit roof, the wheel arches modelled, the tail lamp reading red. Correct |
| `along01..08.png` — the 136 m Munich drive, `view="chase"` | a BLACK SILHOUETTE. The roof, the boot lid and the rear screen are all `#000`. Two faint grey strokes on the tailgate and the wing mirrors are the only pixels above zero |

A horizontal roof under `<key lux="40000" elevationDeg="42">` cannot be black, and
`<environment r="0.06" g="0.07" b="0.09">` cannot be black either. The first-person view is the
same picture from closer in: a near-black roof with grey edges.

**What it is NOT.** The normal is normalised in the vertex stage —
`o.n = normalize(s.model[0].xyz * v.n.x + ...)` (src/render/shaders/subjectLit.msl:14,
src/render/shaders/subjectMapped.msl:14) — so the uniform `MetresPerUnit` scale
`Live::Carry` builds into the placement (src/clients/Live.cpp:539-547) does not shorten it. That
hypothesis was tested and rejected.

**What it most likely IS, and it is a space seam again.** The only executed shadow path is a
per-pixel software BVH ray per light (`src/render/stages/ShadowRay.h`, board:1575). A BVH built
in one space and shaded from a position in another returns "occluded" for every ray, and every
ray occluded IS a black subject with its ambient term intact only where the ray misses. The
drive is the only path that moves the placement; the studio stand-up is the only one that does
not. That is the difference between the two frames.

board:1890's closing commit d99dcc4c states the chase drive is *"unchanged in what it draws"*.
It is not: what it draws is a silhouette, and no still was looked at when that was written.

**THREE HYPOTHESES TESTED AND KILLED, same session.** Each one by running the drive and
measuring the mean red over the car's own pixels (window x 540..740, y 380..640):

| tried | result |
|---|---|
| the shadow centre, which `Live::Carry` computed through `EcefFromGltf` plus `kStudioAnchorEcefM` -- a studio anchor at the equator, for a body standing on a corridor hundreds of metres away | removed. Mean red UNCHANGED at 3.6. The term was wrong and is gone; it was not the cause |
| the shadow pass itself | it does not run. `Declaration::ShadowRadiusM` is 0 for every scenario the tree stands, so `LightVisibility` is never in the plan and `SetShadowFrame` is never called |
| the sun's direction | raising `elevationDeg` from 42 to 90 lifts the mean from **3.6 to 9.5**. Light reaches the car and is attenuated by a factor of roughly fifteen; the studio stand-up of the same asset reads above 150 in the same window |

The vehicle's rotation is NOT it either: `Engine::Rides` builds the matrix column-major from
`OrientationQ` in (w, x, y, z) order, which is what `Physics::Body` declares, and each of the
nine terms matches the standard form.

**A fourth space seam is beside it and may be the same one.** `src/clients/Live.cpp:218-220`
pins the camera basis at the origin -- `eye = {0,0,0}`, `forward = {0,0,-1}` -- for every
frame, while `GltfStudio.cpp:322` passes the real position. CLAUDE.md: *precision has ONE
boundary and it is the camera*, and a renderer that is camera-relative in 32-bit needs the
camera it is relative TO. A body 116 m along a corridor is shaded from an eye that says it is
at the origin.

**A FOURTH AND FIFTH CANDIDATE, ruled out by reading rather than by running:**

- The axis permutation. `PlacedInEcef` (GltfStudio.cpp:19-27) maps glTF (x right, y up,
  z back) onto the engine's frame with `kAxis = {1,0,2,3}` and `kSign = {1,1,-1,1}`, and the
  sun goes through the SAME `EcefFromGltf` at Live.cpp:200. Light and geometry are permuted
  alike, so a mismatch there would tilt the whole studio too.
- Replacing the node matrix. `Carry` overwrites every part placement rather than composing
  with what `Place` wrote, which would be a defect for a hierarchical asset -- but the F31's
  vertices carry their own positions (the picture is a correct car from every angle the chase
  view takes), so the node matrices it discards are identities.

- The anchor. `Aim` places the camera through `Anchored()` (+kStudioAnchorEcefM) while
  `Placements` permutes the part matrices without it -- but `SubjectMesh::Anchor` carries the
  same anchor (GltfStudio.cpp:396) and the shader offsets by it, so camera and geometry are
  anchored alike.
- The lighting setup. Both paths reach it identically: `Live::Build` calls
  `Surface(*Renderer_, Stood_, ...)` at Live.cpp:226, which is the SAME function
  `GltfStudio::Show` calls, and `Stand()` fills `Stood_.Lights` and `Stood_.Environment` before
  it. There is no order difference between the standing and the driven path.

What is left untested and cheap to test next: the shading path's own inputs. The subject is
lit correctly when `GltfStudio::Show` stands it and wrongly when `Engine::Rides` drives it,
and the two differ in exactly one thing that is still unmeasured: the studio never calls `Carry`
at all. Its part placements are what `Place` wrote from the glTF; the drive's are one matrix
per part, written by `Carry`, carrying a rotation AND the 0.01555 model scale. The next test
is to hand `Carry` a pure rotation with no scale and a translation of zero -- if the car lights
up, the shading reads something out of the placement that the normalise in the vertex stage
does not cover.

## What will be true

- [ ] The driven F31 is lit exactly as the standing one is: the same specular on the shoulder,
      the same roof, the same tail lamp.
- [ ] The occluder set and the shading position are stated to be in ONE space, and a
      `static_assert` or an assembly-time refusal is what says so — not a comment.
- [ ] Proving case: the 136 m drive, chase view, mean luminance of the subject's pixels within
      10 % of the same subject in the studio frame under the same `<lighting>`. Negative
      control: move the occluder set into a second space and the mean falls to the ambient
      floor, which is what it is today.
