Type: bug
Area: corpus
Tags: khronos, instrument

**A two-frame grid over a looping animation compares a pose with itself**

The frame grid is derived as `min(5, ceil(durationS) + 1)` at the case's declared rate, which puts the
last sample at **exactly** the animation's duration. **For an animation that returns to where it
started, that is the same pose twice.**

[MEASURED] `AnimatedTriangle` turns a full 360 degrees in one second -- its first rotation keyframe is
`(0, 0, 0, 1)` and its last is `(0, 0, 0, 1)`, the same quaternion. At 1 fps the grid ran 0 s and 1 s
and the runner reported *subject motion from frame 0, furthest vertex = 0 m*. **The sequence check is
right to refuse it**: a sequence that does not move agrees with the oracle by construction, which is
what that check exists to say.

**Repaired for this case by raising its declared rate to 2 fps**, so the grid is 0 s and 0.5 s -- the
half turn, which is the furthest the subject ever gets from where it started. The rate is a
declaration, the threshold is untouched, and the case's criterion now passes.

## What is NOT repaired, and it is the general shape

- [ ] **The derivation still lands on the period's endpoint** for any case whose animation loops. Two
  frames is the degenerate count; at three or more the interior samples differ and the check passes, so
  the defect is confined to `ceil(durationS) + 1 == 2` -- which is every animation of a second or less
- [ ] **The general repair is a grid that EXCLUDES the endpoint** -- `k * duration / frames` for
  `k = 0..frames-1` -- and it moves every animated case's oracle, so it is priced before it is taken
- [ ] **How many cases are affected was not measured.** One was found; the population was not swept

## What it uncovered, which is a different question

**At t = 0 the two sides now agree EXACTLY** -- `worst_disagreement_px` 0, `iou` 1, p99 0 codes. **At
t = 0.5 s they disagree by 39.843127 px.** That time is an exact keyframe, `(0, 0, 1, 0)`, so no
interpolation is involved and neither side is guessing a pose. *The case moved from "cannot decide
anything" to "decides, and one of its two frames disagrees", which is the whole point of repairing it.*

## The frame-1 disagreement, narrowed by three measurements and one refutation

| | frame 0, t = 0 s | frame 1, t = 0.5 s |
|---|---|---|
| `coverage_fraction_outshine` | 0.023282335 | **0.015630425** |
| `coverage_fraction_oracle` | **0.023282335** | **0.0015527344** |
| `worst_disagreement_px` | 0 | 39.843127 |
| `iou` | 1 | -- |

**Frame 0 is bit-identical and frame 1 is a factor of TEN apart in coverage.** So the geometry, the
camera and the projection agree -- they are the same at both frames -- and what differs is what the two
sides did with the rotation.

**Both baked at the same rate and the same instant**, read from `provenance.json`: `fps 2.0`,
`frame 1`, `interpolation LINEAR`, `keyframes 5`. And t = 0.5 s is an EXACT keyframe, `(0, 0, 1, 0)`,
so neither side is interpolating.

**THE AXIS-CONVENTION HYPOTHESIS IS DEAD.** glTF is +Y-up and Blender +Z-up, and a 180-degree turn
about the wrong axis would agree at the identity and diverge exactly here -- which fits the shape of the
evidence perfectly. It is wrong: [MEASURED] every other case whose rotation is carried by an OBJECT is
green -- `BoxAnimated`, `AnimatedCube`, `CesiumMilkTruck`, `AnimatedColorsCube`, `InterpolationTest`,
`VirtualCity` -- and so is every bone-carried one. **Only `AnimatedTriangle` and `ChronographWatch`
fail, and the conversion cannot be wrong for two subjects and right for six.**

- [ ] **What is left is what makes this subject unlike those six**: it is a single flat triangle with
  ZERO extent in one axis, and after a half turn about the normal it occupies the quadrant opposite the
  one the camera was framed on. Whether a degenerate bound, a cull or a clip explains a tenfold
  coverage difference is the next question, and it is asked of a subject that has no thickness
