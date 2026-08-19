Type: bug
Area: gltf
Tags: khronos, instrument

**The framing rule frames the pose that renders**

`ADerivedCameraIsTheFramingRuleAndNotAQuotation` projected the subject's **stored** pose -- what its
accessors hold -- where the picture it is about is drawn at **frame 0** of the case's own grid. For an
animated file with a keyframe at t = 0 those are two different shapes, so the rule framed something
nothing renders and the frame fraction it computed was of a subject nobody sees.

[MEASURED] the declared and computed fractions disagreed on **8 cases, and every one of the 8 is
animated**: `AnimationPointerUVs`, `BrainStem`, `CesiumMan`, `DiffuseTransmissionPlant`, `Fox`,
`LightVisibility`, `RiggedFigure`, `SimpleMorph`. Not one still case moved. `SimpleMorph` was out by a
factor of three -- 0.00707695694 against a declared 0.0022899601 -- because a morph target at rest is not
the morph target at t = 0.

*The same discriminator `board:1432` found one suite over, and the same sentence: **a still is at the
instant it declares, and that instant is zero**.*

| | before | after |
|---|---|---|
| failures in this test | **12** | **0** |
| fraction mismatches | 8 | 0 |
| cameras "fitted near" the rule | 3 | 0 |
| a case the reader declines | counted as a failure | **announced as its verdict** |

## The sweep was already there and started one frame late

`board:1366` had already made the rule take the union over the declared grid -- but it seeded that union
from the stored pose and swept frames **1** onward. The seed is now frame 0's pose, which is what every
other instrument in the tree uses.

## Four cameras were re-derived and it moved their pictures

`CompareAlphaCoverage` by 0.2241 m, `CompareAmbientOcclusion` by 0.1634 m, `IridescentDishWithOlives` by
0.4639 m, `RiggedFigure` by 0.0885 m -- each of them a camera *fitted near* the rule's answer, which is
the one placement this test forbids: **a camera is the rule's answer, or it is deliberately elsewhere,
never a fraction of a metre from it**, because that placement is what neither determination can account
for. `AnimatedCube` moved by 1.4e-07 m, which no picture can carry and the rule's own 1e-12 relative
tolerance can. All five re-rendered and all five are still within their bounds.

## A declined case is not a failed case, and this test now knows it

`SpecGlossVsMetalRough` names `KHR_materials_pbrSpecularGlossiness` in `extensionsRequired`; Khronos
archived that extension and this engine declines it by the owner's ruling, so a conforming reader MUST
refuse the file. `board:1424` gave that refusal a spelling -- the `limits-probe` criterion -- and the
render suite has consumed it since. **This test had not**, so the case was red here for behaving exactly
as its own manifest declares it must. It is now announced as `DECLINED` rather than skipped, because a
silence could not be told from a pass that framed something.

## Comments

This closes the last unticked line of `board:1361`'s dispatch -- *every non-`exact` camera is re-derived
from the rule and the test is green again* -- and it took the rule's own answer at the pose that renders
rather than a re-harvest of what was there.

## A harvested number is read by identity, never by proximity

`board:1440` moved `SimpleMorph`'s grid, which moved what the rule sweeps, which put its declared camera
0.844343596 m from the rule's answer -- the *fitted near it* placement this test forbids. Re-deriving it
was routine; **reading the new frame fraction out of the log was not.** The value taken was
0.0416262738, which belongs to **`SimpleSkin`** -- the next case in the log -- and the real one is
0.0025950821239740908, a factor of sixteen away.

**The test caught it on the first run**, which is what a declaration checked against a rule is for. But
the lesson is about the harvest rather than the check: *a log is read by the case's own name and not by
what follows the case's name*, and a number that arrives a factor of sixteen from where it should be is
a number that was read from somewhere else.
