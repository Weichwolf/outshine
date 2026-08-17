Type: bug
Area: corpus
Tags: instrument, khronos

**The framing rule frames a rest pose, and an animated subject leaves the frame**

`src/gltf/Framing.h` derives a camera from `Subject::RadiusM()` and `CentreM()` -- the bounding box of
the subject **as the file poses it**. For a still that is the whole story. For an animated subject it is
the pose at t = 0, and the camera it produces frames a shape that does not stay there.

[MEASURED] on `AnimatedTriangle`, whose case was being authored when this surfaced and was **not
committed** because of it:

| | |
|---|---|
| the animation | one channel, `rotation` on node 0, LINEAR over keyframes at 0, 0.25, 0.5, 0.75, 1.0 s — quaternions `(0,0,0,1)`, `(0,0,.707,.707)`, `(0,0,1,0)`, `(0,0,.707,-.707)`, `(0,0,0,1)`. **A full 360-degree spin about Z in one second** |
| rest bounds | `[0,0,0]` to `[1,1,0]`, so centre `(0.5, 0.5, 0)` and radius **0.7071** |
| the node's rotation is about the ORIGIN | so the swept extent is a disc of radius `max\|vertex\| = sqrt(2)` = **1.4142** about `(0,0,0)` |
| what the rule frames | a sphere of radius 0.7071 at fill 0.6, which covers **1.178** about the rest centre |
| what the subject reaches | up to `1.4142 + 0.7071` = **2.12** from that centre |

**So the triangle leaves the frame during its own animation, by nearly a factor of two.** A case built on
that camera would compare two renders of a clipped silhouette and the clip would be the frame's, not the
model's -- which is the *population moved underneath the number* failure with the population being the
pixels a subject occupies.

## Why nothing has caught it

**Every animated case in the tree today moves a little.** `BoxAnimated`, `RiggedSimple` and
`AnimatedMorphCube` all keep their motion well inside the rest-pose sphere, so the rule has never been
asked a question it cannot answer. **`AnimatedTriangle` is the first subject whose motion is larger than
itself**, and it is one of the smallest files in the index.

## What the repair has to decide, and it is not obvious

- [ ] **Frame the SWEPT bounds** — the union of the subject's bounds over the declared frame grid. It is
  the obvious answer and it costs the still cases nothing, because for a still the union is the pose.
  **What it changes is that the camera becomes a function of the GRID as well as of the subject**, so a
  case that shortened its frame count would silently zoom in
- [ ] **Frame the rest pose and declare the overflow** — keep the rule, and let a case state that its
  subject leaves the frame, with the comparison restricted to pixels both sides agree are inside. *That
  is a reduction, and it would be the first one in this tree that is about the CAMERA rather than about
  the oracle.*
- [ ] **Whichever is taken, `ADerivedCameraIsTheFramingRuleAndNotAQuotation` has to compute the same
  thing.** It recomputes the rule per case and scores the declaration; a swept camera quoted against a
  rest-pose recomputation is red by construction.

**`AnimatedTriangle` is the case that is waiting on this**, and behind it every animated core model with
no case: `AnimatedCube`, `SimpleMorph`, `SimpleSkin`, `MorphPrimitivesTest`, `MorphStressTest`,
`RecursiveSkeletons`, `BrainStem`, `CesiumMan`, `Fox`, `RiggedFigure`.

## The engine half is in, and the runner half turned out to be the wrong place

**`FramingFor(minM, maxM, out)` is committed** in `src/gltf/Subject.h`/`.cpp`: the rule is decoupled from
one pose and `Subject::Frame` is that function called with the subject's own box. That part stands.

**The sweep was then written into `Parity.cpp` and removed the same round, because it could not fire.**
`ResolveCamera` handles `camera.source` of `manifest` and of `gltf`, and **the schema's discriminator has
exactly those two variants** -- so the framing-rule branch under them is unreachable for any manifest the
schema admits. [MEASURED] the change cost every animated case a pose per frame at load and moved
**nothing**: `criteria 31 met of 37, picture bound 18 within` before and after, and all three animated
cases green on every frame either way.

**So the camera is AUTHORED, not derived at run time**, and that relocates the whole defect:

| where the sweep belongs | why |
|---|---|
| **the authoring step** | the number quoted in a new animated case's manifest must come from the union over its declared grid, not from the stored pose |
| **`ADerivedCameraIsTheFramingRuleAndNotAQuotation`** | it recomputes the rule **at rest** and scores the declaration against it, so a correctly swept camera would be scored **red by the test that exists to keep cameras honest**. Until it sweeps too, an animated case cannot quote a camera that frames its own motion |

- [ ] **That test is the change, and it is small**: for a case declaring an animation, pose over the grid
  and union before calling `FramingFor`. *For a still the union is the pose, so nothing about the 34
  existing still cases moves.*

## Closed: the rule sweeps, and the blast radius was one camera and 4.8e-07 m

`ADerivedCameraIsTheFramingRuleAndNotAQuotation` now poses the subject over the case's own declared
frame grid and unions the bounds before calling `FramingFor`. **For a still the union IS the pose**,
which is why not one of the 34 still cases moved.

**One case moved and it is the one that should have.** `AnimatedMorphCube` MORPHS, so its swept box is
`[-1.0000003501772881, 1.0000004433095455]` on X against the rest pose's `[-1, 1]`, and its quoted eye
was stale by **4.81649513e-07 m** -- five orders above the test's 1e-12 relative tolerance and invisible
in any picture. **The camera was re-derived, its oracle re-rendered, and the corpus is unchanged**:
`criteria 31 met of 37, picture bound 18 within` before and after, `AnimatedMorphCube` green on every
frame either way.

**The number being tiny is not the point and the item says so plainly.** `AnimatedTriangle` spins 360
degrees about the origin: its swept radius is **twice** what the rest-pose rule frames, so it leaves the
picture entirely. The rule now covers both, and the ten animated core models behind it can be authored
with a camera that frames what they do rather than where they start.

**What is NOT in this repair, and it was tried and removed**: a sweep inside `Parity.cpp`'s framing
branch. That branch is unreachable for any manifest the schema admits -- the camera discriminator has
exactly two variants and both are handled above it -- so the sweep cost every animated case a pose per
frame at load and could not fire. **The camera is authored, so the sweep belongs where the number is
written down.**
