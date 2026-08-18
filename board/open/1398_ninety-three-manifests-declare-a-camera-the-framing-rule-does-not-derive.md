Type: bug
Area: corpus
Tags: khronos, instrument

**Ninety-three manifests declare a camera the framing rule does not derive**

`ADerivedCameraIsTheFramingRuleAndNotAQuotation` exists so that no case can be made to pass by moving
its viewpoint. [MEASURED] over the corpus as it now stands, it reports:

| the check | cases failing |
|---|---|
| the derived eye IS the declared eye | **93** |
| the declared aim IS the bounds' centre | 33 |
| the declared clip contains the rule's | 33 |
| the declared frame fraction is the rule's | 16 |
| the declared camera is the rule's answer or a metre away, never fitted near it | 12 |

## CORRECTION -- the cause I filed is one of at least three, and it is the smallest

**What this item first said was that the authoring helper bounds each primitive by the eight
transformed corners of its local AABB, which is always at least as loose as the engine's bound on the
transformed vertices, so the eye sits behind the rule's by a hair. That is true, it is measurable, and
it explains almost none of this.** Replacing all 100 derivable cameras with the rule's own output and
measuring what moved:

| | |
|---|---|
| cases whose camera moved at all | **37 of 147** |
| relative move, p50 over those that moved | **0.00 %** |
| relative move, p95 | **228.6 %** |
| largest absolute move | **653.4 m**, `RecursiveSkeletons` |

**A hair does not move a camera by twice its own distance from the subject.** The named causes,
each measured as engine radius against helper radius:

| cause | evidence | size |
|---|---|---|
| **the pose is not the rest pose** | `RecursiveSkeletons` 45.552 -> 75.581 m (+66 %), `SimpleMorph` 0.559 -> 0.901 m (+61 %) | dominant |
| **the eight-corner bound** | `NodePerformanceTest` helper 207.610 against engine 206.628 m (+0.47 %, and the helper is the LARGER one, as predicted) | small |
| **something that is not the radius at all** | `BrainStem` -- radii agree to six decimals and the camera still moved 1.103 m (9.7 %) | not yet named |
| **manifests the helper never wrote** | `AlphaBlendModeTest` (252 %), `NormalTangentTest` (187 %) have no facts file, so they were hand-authored in an earlier round | not yet named |

**The first is the one that matters and it is a real defect, not an approximation.** The helper reads a
POSITION accessor's declared `min`/`max`; the engine bounds the subject it actually DRAWS, after
skinning and after morphing. A skinned figure's vertices ride its joints far outside the accessor's
extent, so the two are not a loose bound and a tight one -- **they are bounds on two different sets of
points**, and no amount of tightening the helper would have closed it.

*The correction is the round's result. The repair -- quoting the engine's own answer instead of
computing a second one -- is right for all four causes at once, which is why it was not undone when the
cause turned out to be wrong.*

## Why it cannot be repaired by editing the manifests alone

**The camera is in the render key.** Every corrected camera is a new oracle render, so this is one full
preparation pass -- which is also why it belongs with `board:1360`'s `kFramingFill` change rather than
before it: two viewpoint changes measured separately would cost two passes to learn one thing.

## What must be true

- [x] **Every non-`exact` camera is HARVESTED from the rule's own output** -- 100 replaced, 4 `exact` exempt, 47 with no harvest yet because their case does not prepare, the way the frame fraction
  now is, and never recomputed by a second implementation
- [x] **The authoring helper stops computing bounds at all** -- what it writes is now a starting camera the harvest replaces, and the manifest note says which -- a helper that can disagree with the
  engine about a subject's extent is a second implementation of the thing under test
- [ ] **`kFramingFill` 0.6 -> 0.9 lands in the same pass** (`board:1360`), because both move every camera
- [ ] **`exact` cases keep their cameras**, read from `acceptanceClass` rather than from a list
- [ ] **The before-and-after quotes the same population**, and says how many cases changed verdict

## Comments

**This is what the test is FOR and it took a corpus of 151 cases to make it audible.** With twenty
cases the same defect was five reds that read as noise; at scale it is a ranked list with one cause.

## A second defect of mine, in the repair rather than in the finding

**The harvest wrote `yfovRad` into an ORTHOGRAPHIC camera.** `NormalTangentTest` declares
`projection: orthographic` with a `yMagM`, and the bulk edit -- which assumed every derivable camera
is the framing rule's perspective one -- left it carrying both. The preparer refused it at
`manifest.scene.camera.yfovRad`, so the case that had been scoring stopped preparing at all.

[MEASURED] **4 orthographic cameras in the corpus, 1 of them reached by the harvest and broken.** The
other three were not in the harvest's population, which is luck rather than design.

**An orthographic camera is not a case the framing rule can answer**, so it belongs with the `exact`
exemption and not with the derivable set. It was restored from before the edit; the rule the harvest
must carry is that it writes only where the declared projection is the one it derives.

**A camera and its frame fraction move together, and correcting one without the other is a repair that
reads as a regression.** It has now happened three times in this run of work: the fraction is
recomputed from the camera on every run and refused on mismatch, so a corrected camera leaves the
declared fraction stale and the case goes red for the repair. **Harvest both in the same round or
neither** -- and the tell is unmistakable once seen, because the case fails on `frame_fraction_error`
alone while its picture is inside the bound.

*[MEASURED] `Fox` and `MorphStressTest` came back at 133 checks against the 32 they ran before: with a
fraction that matches, the frame loop runs the whole declared grid instead of stopping at the first.
So the correction bought four times the comparison, and for one run it looked like a loss.*

## Seventy of the eighty-two internal failures were a rounded quotation, and they are gone

**The cameras were re-derived once and WRITTEN BACK WITH FEWER DIGITS THAN THE RULE PRODUCES.**
[MEASURED] fourteen cases declared an eye within a nanometre of the framing rule's and outside the
rule's own tolerance -- `AnimatedColorsCube` at 4.35253998e-07 m against 2.65069674e-11 m,
`USDShaderBallForGltf` at 8.28691782e-09 m. **That is not a different camera; it is the same camera
quoted short**, which is precisely what this test's name says it exists to catch.

Re-stated from the rule's own output, at its own precision, over all fourteen: **82 internal failures
to 12.**

## What the twelve are, and eight of them are THIS item's own finding

| | count | what |
|---|---|---|
| the frame fraction under the rule | **8** | `AnimationPointerUVs`, `BrainStem`, `CesiumMan`, `DiffuseTransmissionPlant`, `Fox`, `LightVisibility`, `RiggedFigure`, `SimpleMorph` |
| the camera is fitted NEAR the rule | 3 | not investigated this round |
| the case's subject reads | 1 | `AnimationPointerUVs`, which does not prepare |

**THE EIGHT WERE NOT OVERWRITTEN AND THAT IS DELIBERATE.** Six of them are animated or skinned, and
this item already measured why the two numbers differ: *the pose is not the rest pose* -- the runner
harvests the fraction from the DRAWN pose at frame 0 and this test computes it from the rule over
REST-pose bounds. **Two numbers, two questions**, and copying one into the other would have made the
unit test green by breaking the runner's own refusal. *The rounded quotations were a defect; these are
the finding this item was filed for.*
