Type: feature
Area: render
Tags: instrument, oracle

**A deciding pixel can be interrogated, and the answer says what each side had there**

A case fails on its worst channels, and the tail is a max — so a handful of pixels decide a verdict over
2.6 million. Today nothing in the render suite can be asked what was at one of them. Every question a
failing case raises is one query against a named pixel, and there is no way to ask it.

**THE QUESTION IS NOT "HOW WAS IT SHADED" AND THAT WAS ESTABLISHED THE HARD WAY.** `board:1136` read its
four deciding-pixel sets as a missing specular lobe and asked for a shading-point probe. **All four of
its cases carry zero shaded fragments** — [MEASURED] from `outshine.normal.raw`, which the suite already
writes: `materials/a-beautiful-game`, `materials/scifi-helmet`, `coverage/negative-scale` and
`texture/texture-coordinate-test` each report **0 non-zero shading normals of 921 600**, against 294 876
for `materials/normal-tangent`. Their manifests declare `light: none` and lower the oracle's material to
`kind: emission`, so **both sides compute `declaredRadiance x baseColour(u, v)` and nothing else**. A
probe of the BRDF's inputs would have published an empty frame on every one of them.

So the interrogation has three parts, and the order is what a pixel can disagree about at all:

- [ ] **WHAT COVERS IT.** Which material and which object each side says is at the pixel. Both sides can
  be *covered* and still disagree about *what* — [MEASURED] at `coverage/negative-scale` (717, 274),
  where the oracle holds `0.5, 0.5, 0.5` (the manifest's `LabelMat`, verbatim) and ours holds
  `0.0891927, 0.1792562, 0.64` (its `BackgroundMaterial`, verbatim, to eight digits). Two exact declared
  colours is the fingerprint of a surface swap, and the whole 3-channel failure of that case is this one
  pixel. `board:1144` is the defect this half exposes
- [ ] **WHICH TEXEL IT SAMPLED.** The uv each side took, the difference in **texels** rather than in uv,
  and the tap each uv produces from the decoded image. [MEASURED] at `materials/a-beautiful-game`
  (626, 347), (541, 368), (583, 391): the bright warm value the oracle has is present **one pixel away in
  our own render too** — 0.4621 at (627, 346) on both sides — and `0.44520125` is the oracle's value at
  two of the three pixels, so it is **one texel, reached by one side and missed by the other**. The uv
  field's central difference there is 0.043 to 0.277 **per pixel**, which on a 1024- or 2048-texel map is
  tens to hundreds of texels: the discontinuity population `board:1130` already measured
- [ ] **WHICH TERM ATTRIBUTED IT**, and only where the case shades. Eight of the thirty-five cases
  declare a light and a metal-rough oracle — `lighting/directional-light`, `lighting/point-light-intensity`,
  `materials/{boom-box, corset, lantern, normal-tangent, normal-tangent-mirror, water-bottle}` — and they
  are the subjects of `board:1126`, `board:1130`, `board:1131` and `board:1132`. **The probe's domain is
  those eight and its report says so**, because an instrument that reports on twenty-seven cases it cannot
  see is the *population too small* face of a number about something else
- [ ] **THE POPULATION IS DERIVED, NEVER A LIST OF COORDINATES.** The pixels interrogated are the ones the
  picture bound's own worst-channel table selects, so the record follows the camera and a case that passes
  prints nothing. A hard-coded pixel list goes stale at the first reframing and then reads as a finding
- [ ] **NO THRESHOLD, NO NEW ACCEPTANCE.** Every number this produces is `Direction::Reported` or a printed
  line. A bound on it would be a number nobody derived, and this tree has already paid for a metric whose
  name misstated its instrument

**Done when** a failing case prints, for each pixel that decides its tail, what both sides say covers it,
what both sides sampled and — where the case shades — what both sides attributed the radiance to; and
when the three readings of `board:1136` are each either a finding with a mechanism or a refusal that says
what could not be decided.
