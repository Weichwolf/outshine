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

## The population was swept, and the last question above is answered

[MEASURED] **24 manifests declare an animation.** Twenty-three derive `ceil(duration * fps) + 1` and land
on three frames or more, where the interior samples differ and the endpoint costs nothing. **Exactly one
derives two**, and it is this case, and it is repaired. *So the general defect's population is zero
today* -- and the sequence check is what keeps it that way, because a grid that does not move is refused
rather than reported.

**The tenfold coverage was a CLIP, and `board:1433` carries it**: the declared far plane was derived from
the rest bounds and cut two of the triangle's three vertices at the half turn, so the oracle rendered a
1431 px sliver of a 14405 px triangle. Neither a degenerate bound nor a cull -- the third candidate this
item named. `AnimatedTriangle` is green on both frames at `iou` 1 and 0 pixels disagreeing.

**The remaining scope is unchanged and still unticked**: a grid that excludes the endpoint by
construction, priced across all 24.

## A second reason to revisit the derivation, measured

`board:1440` measures that **ten of the twenty-four animated cases sample nothing but keyframes**, so the
grid derivation has two defects rather than one: it can land on the period's endpoint, and it can land on
every key. Both are the same shape -- a grid whose samples are where the answer is already known -- and
both are answered by the same change, which is why they are priced together rather than twice.

## A derivation that answers both defects at once, and its cost

**Offset the grid by half a step**: sample at `(k + 1/2) / fps` instead of `k / fps`. A grid that landed
on every key lands strictly between every pair of them, for every case, with no per-case rate hunt -- and
the period's endpoint stops being a sample by construction, which is this item's first defect.

**What it costs and why it is not taken here.** Frame 0 would no longer be `t = 0`, and `t = 0` is where
`board:1432` puts a still, where `board:1437` takes the framing bounds, and where the sweep is seeded.
Blender renders a subframe -- `scene.frame_set(frame, subframe)` -- so the oracle can follow, but **every
animated case's oracle re-renders and its camera may move with the swept bounds**.

**The alternative is a per-case rate that is not a divisor of the key spacing**, which is what
`board:1439` and `board:1440` did by hand three times. [MEASURED] it does not generalise: `VirtualCity`
and `DiffuseTransmissionPlant` carry keys every 1/30 s, so a rate has to avoid a multiple of 30 rather
than simply rise, and `AnimatedTriangle`'s keys at 0.25 s make every rate that is a multiple of four land
on keys again.

*Both are the owner's call on scope, and the measurement for it is `board:1440`'s table.*
