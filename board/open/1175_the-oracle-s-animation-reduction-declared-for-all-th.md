Type: task
Parent: 1128
Area: corpus
Tags: oracle, khronos, instrument

**The oracle's animation reduction, declared for all three interpolations**

**A declared instrument stands in the path of every animated case and only one of the three interpolations
has been settled.** This is the same class as `board:1135`'s recipe question — a reduction applied to the
reference, which every case downstream inherits whether or not it knows — and it wants the same
visibility: written where the next animated case will read it, not left in one case's provenance.

## `LINEAR` on a quaternion: settled, and the engine was right

**Blender's importer evaluates a quaternion `LINEAR` sampler component-wise**, and glTF specifies
**spherical** interpolation. [MEASURED] from `BoxAnimated`'s own pose record: the two disagree by up to
**7.9275°**, **exactly 0 at the keyframes and at each span's midpoint**, worst at frames 12 and 18 — which
is the signature of component-wise against slerp and not of a sampling error.

**With the plain oracle the case was red at frame 11**: `worst_disagreement_px = 0.97892503` over **43 of
1889** covered pixels, **every colour channel agreeing to the last bit**. Geometry displaced, shading
identical — a pose disagreement and nothing else.

**glTF states slerp, so the reduction is the oracle's and not ours.** The imported keyframes are resampled
onto the declared grid with **Blender's own `mathutils.Quaternion.slerp`** in
`test/harness/shared/corpus/prep/in_blender_render.py`. **The independence is the load-bearing property**: an engine with
a wrong slerp still disagrees with it, because the reduction uses Blender's implementation and not a copy
of ours. Residual **1.55e-05°**, which is Blender's f32 f-curve storage and is the instrument's floor.

**Rung 2 of the ladder — `reduce the oracle` — taken with its measurement**, as `board:0087` requires and
`board:0085` frames.

## `CUBICSPLINE` and `STEP`: open, and the next case inherits them

- [ ] **`CUBICSPLINE` imports as BEZIER and was deliberately left alone**, counted in the case's
  provenance under `rotationCurves.leftAlone`. **A Bézier f-curve and glTF's cubic Hermite with in- and
  out-tangents are not the same function**, and `InterpolationTest` — the next animated case — is exactly
  where that difference is the subject. **Its residual must be measured before the case is scored**, or
  the case reports our sampler and the importer's together
- [ ] **`STEP` is unmeasured and is the one most likely to be free**: a constant between keyframes has no
  interpolation to disagree about, so the expectation is an exact match. **State it as a measurement
  rather than as an expectation** — a residual of zero that nobody took is not evidence
- [ ] **Scale channels and morph weights are unexercised by `Pose`.** Morph weights are a named refusal
  elsewhere; **scale is not, and it is a `core` channel with no case**
- [ ] **Every reduction is published per case, with which interpolations it touched**, so a green animated
  case says what stood between it and the reference. A reduction visible only in one case's provenance is
  the shape `board:1135` was filed about

**Done when** each of `LINEAR`, `STEP` and `CUBICSPLINE` carries either a measured reduction with its
residual and its independence argument, or a measurement showing none is needed; and an animated case
publishes which of the three stood in its path.

## DECIDED by the owner: the oracle renders poses, it does not interpolate

*"We must match the oracle"* and *"use what the oracle can do"*, taken together with `CLAUDE.md`'s ladder —
**fix the engine · reduce the oracle · patch the asset · disqualify**, in that order.

**The rule this settles, and it generalises the reduction that already exists:** Blender is asked only for
what it does well — path-tracing a stated pose — and is never asked to reproduce a glTF sampler, because it
demonstrably converts them on import: component-wise on a `LINEAR` quaternion, Bézier on a `CUBICSPLINE`.
**Every declared frame is baked to an exact key and Blender interpolates nothing.**

**The independence argument survives that, and it is the reason the shape is admissible.** The pose handed
to Blender is computed **in the preparer, from the file's own accessor bytes, against the specification** —
never from this engine's sampler. Two implementations of one published formula, in different languages, on
different code paths. **An engine whose sampler is wrong still disagrees with the oracle**, which is the
whole property a reduction can destroy and this one does not.

*The slerp reduction already had this shape and was read as a special case; it is the rule.*

## What `CUBICSPLINE` needs, and where it can go wrong quietly

**glTF's cubic Hermite over the in-tangent · value · out-tangent triples, with the tangents scaled by the
segment duration.** That scaling is what makes it a different function from a Bézier carrying the same
handles, and it is the likeliest place for an implementation that looks right and is not.

**Our side is already built** — `src/gltf/Track.h` states *glTF's Hermite basis with its tangents scaled*
and `src/core/Keyframes.h` reads the triples. **The open half is the oracle's**, and it is open because
nobody has yet measured whether Blender's Bézier conversion reproduces the Hermite or departs from it.
**That measurement comes before the rung is chosen**, per the ladder: if it reproduces it, there is nothing
to reduce.

**`InterpolationTest` is the case**, and it is not fetched. It is the only asset carrying `LINEAR`, `STEP`
and `CUBICSPLINE` side by side on one subject, so it decides all three in one case rather than three — and
**the three modes must be shown to differ at the sampled frames by more than the picture bound before it is
scored**, or the case cannot distinguish a sampler that ignores the declared mode entirely. **`STEP` is the
one that would pass by accident.**
