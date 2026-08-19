Type: bug
Area: corpus
Tags: khronos, instrument, scope

**Every animated case samples where its curves differ**

**Ten of the twenty-four animated cases in this corpus sample nothing but keyframes.** Their grids land
exactly on the file's own key times, where every interpolation mode returns the key's own value -- so
those cases test a sequence of POSES and say nothing about the curves between them.

[MEASURED] grid samples falling strictly between keyframes, per case:

| none of them | some of them |
|---|---|
| `AnimatedColorsCube` 0 of 4 | `BoxAnimated` 28 of 31 |
| `AnimatedCube` 0 of 3 | `AnimatedMorphCube` 9 of 18 |
| `AnimatedTriangle` 0 of 2 | `AnimationPointerUVs` 4 of 5 |
| `ChronographWatch` 0 of 5 | `CommercialRefrigerator` 4 of 5 |
| `DiffuseTransmissionPlant` 0 of 5 | `IridescentDishWithOlives` 4 of 5 |
| `LightVisibility` 0 of 5 | `BrainStem` 2 of 5 |
| `MeshoptCubeTest` 0 of 3 | `InterpolationTest` 2 of 5 |
| **`RecursiveSkeletons` 0 of 3** | `PotOfCoalsAnimationPointer` 2 of 5 |
| `SimpleMorph` 0 of 5 | `RiggedFigure` 2 of 3, `RiggedSimple` 2 of 18 |
| `VirtualCity` 0 of 5 | `CesiumMan`, `CesiumMilkTruck`, `Fox`, `MorphStressTest` 1 each |

**`RecursiveSkeletons` is the row that matters most**: 84 skins, **840 joints**, 840 rotation channels --
the largest skinning subject in the tree -- and not one of its three samples asks what a joint does
between two keys. **`SimpleMorph` is the same for morph weights.**

## This is the shape `board:1439` found in one case and it is general

There, the only CUBICSPLINE sampler in the corpus sat on its keys at every sample and the case named for
interpolation decided nothing about any of the three curves. One number -- the declared frame rate -- put
three of five samples between keys, and the case came back **0 codes at all five frames**, which is a
capability proven rather than a green inherited.

## What must be true

- [ ] every animated case's grid places at least one sample strictly between two keyframes, or declares
      why its file cannot offer one
- [ ] the choice is **derived and not hand-set per case** -- `board:1421` already owns the grid
      derivation and its endpoint problem, and this is the second reason to revisit it in the same round

## Two of the ten are taken by hand, and they are the two that carry mechanisms

**Not because ten hand edits are the answer** -- the box above says the choice must be derived -- but
because two capabilities were worth knowing before the derivation is priced, and neither was known.

| case | what it now asks | verdict at every frame |
|---|---|---|
| `RecursiveSkeletons` | 84 skins, **840 joints**, slerped between two keys at 0.5 s and 1.5 s | `disagreement_p99_px` **0**, `picture_p99_delta_code` **1 code** |
| `SimpleMorph` | a morph weight **blended** between two keys at 0.5 s and 1.5 s | `picture_p99_delta_code` **0 codes** |

**So the skinning path's quaternion interpolation over 840 joints is right against an independent
renderer, and so is the morph blend.** Neither was tested by anything before this.

**Eight remain**, and two of them are a different shape: `VirtualCity` and
`DiffuseTransmissionPlant` carry keys every 1/30 s, so *every whole second is a key* and the grid lands on
one by arithmetic rather than by coarseness. A rate for those has to avoid a multiple of 30 rather than
simply rise.

## What it costs, priced before it is taken

Each case's rate change moves its oracle -- a Cycles render per frame -- and changes what the grid
sweeps, so the framing rule's window may move with it. [MEASURED] on `InterpolationTest`, which needed
exactly that: a vertex landed at 40.997298 m against a near plane of 41.273223 and the window had to be
re-derived about the declared eye. **Ten cases, and the camera check is what catches the ones that move.**

## Comments

Found by asking a coverage question and then a sharper one. *Which interpolation modes does this corpus
exercise?* -- LINEAR 1310 samplers, STEP 4, CUBICSPLINE 3. Then: *does any grid sample fall where those
three differ?* **A count of samplers is not a count of what was tested**, and the second question is the
one that produced work.
