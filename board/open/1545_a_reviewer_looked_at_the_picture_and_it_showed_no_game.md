Type: bug
Area: render
Tags: instrument, bug

**A reviewer looked at the picture, and every number until then had hidden what it showed**

**The owner asked for random stills to be given to a games-magazine tester.** The verdict was *no AAA,
no recommendation* -- *"there is no game depicted in these pictures"* -- and it was **measured**, not
impressionistic. It found four defects that a full suite of green tests had not.

| What the reviewer measured | Value |
|---|---|
| empty frame area | **98.3 %** |
| car pixels at exactly (255,255,255) | **57.9 %** |
| luminance band 96-223 | **0.35 %** -- effectively unoccupied |
| distinct alpha values in the whole frame | **five** -- the silhouette is binary, there is no antialiasing |
| stills from km 117.4 onward | **twelve byte-identical empty files** |

## What each one really was

| Reading | What it actually is |
|---|---|
| *"a dashed debug polyline"* | **the road.** The swept corridor, drawn so thin that a professional read it as an overlay |
| *"a free-floating wheel ovoid beside the car"* | a real geometry defect, not yet diagnosed |
| *"no lighting on the bodywork"* | `RenderPlan::Exposure` was **1.0** while the key declared 40 000 lx. Every surface clipped |
| *"nothing renders after km 117"* | `ClearsNearPlane` refuses when the subject is not wholly in front of the camera -- a studio rule on a world path, and a driver always sits **inside** the scene |

## What was fixed in the same round, with the measurement

**Exposure is derived from the declared illuminance.** EV100 = log2(E/2.5) = 13.966 at 40 000 lx, and
the saturation-based exposure is 1/(1.2 * 2^EV100) = **5.208e-5** (Lagarde/de Rousiers, *Moving
Frostbite to PBR*, already in this file's references). A scenario may declare one; absent that, the
client derives it from the key it already declares.

**Against the reviewer's own metric, on the same image:** pure white **57.9 % → 0.0 %**, midtone band
**0.35 % → 10.7 %**, over an unchanged 15 773 opaque pixels. The picture went from a paper cut-out to a
shaded body with a shoulder line, a dark roof and window frames -- and the road became a solid ribbon
running to the horizon instead of a hairline.

## What must still be true

- [ ] **The world is in the picture** -- terrain, sky, horizon. 98.3 % of the frame is nothing, and the
      whole claim of this engine is a world loaded from upstream data
- [ ] **A camera may stand INSIDE the scene.** `ClearsNearPlane` and `kStudioAnchorEcefM` are studio
      assumptions on a world path; while they hold there is no first-person frame at all
- [ ] **The car meets the ground** -- a contact shadow, and geometry that is not floating
- [ ] **The silhouette is antialiased.** Five alpha values over 921 600 pixels is a hard edge, and at
      720p a moving hard edge crawls
- [ ] **The chase camera holds its subject.** Measured 15 773 -> 11 412 -> 5 696 opaque pixels over
      61 km, clipped at the top edge by then
- [ ] **A detached wheel is diagnosed** -- the reviewer saw it in all three images and I had not

## THE DRIVER'S FRAME, LOOKED AT

**A carriageway running to a vanishing point, green ground either side, both road edges legible.**
The first frame in this work that reads as a road one could drive down.

| on the reviewer's measures | round 1 | round 2 | now |
|---|---|---|---|
| opaque fraction, driver's frame | 1.09 % | -- | **49.28 %** |
| pure white on the subject | 57.9 % | 0.00 % | **0.00 %** |
| saturation p50, whole frame | -- | 0.038 | **0.05** driver / **0.31** framed |

**Saturation is the honest caveat**: 0.05 across the driver's frame, because a road fills half of it
and asphalt has no colour. The green is real and measured where the ground is; the number is low
because the picture is mostly road, which is what a driver sees.

## Comments

**THE INSTRUMENT WAS THE FINDING.** Before this, the windowed drive reported p50 0.655 ms, p95 0.985 ms
and p99 1.355 ms over 1 469 407 frames and every claim in it passed. **The frame was (0,0,0,0) --
fully transparent.** The cost of drawing nothing was being published as the cost of drawing a world.

**And one claim I wrote was the exact defect `CLAUDE.md` names.** *"Both persons were used"* tested
`View::Person` **strings**. First person, third person and engine-framed came back **byte-identical**,
because `Live::Look` only ran when a turntable orbit was declared -- a camera set after stand-up never
reached the frame. *A grep proves a string absent, never a capability*, and I proved a string.
