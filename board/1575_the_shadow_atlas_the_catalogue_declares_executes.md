Type: task
State: open
Parent: 0120
Area: render
Tags: perf

**The shadow atlas the catalogue declares executes, and the ray demotes to the oracle suites**

The only executed shadow path is a per-pixel software BVH ray per light
(`ShadowRay.h:25-41`, called at `SubjectDraw.cpp:485`) -- the correct instrument against the
Cycles oracle, the wrong mechanism at 60 Hz: cost scales with lights x pixels x depth complexity
and caches nothing across frames. `LightVisibility` -> `ShadowAtlas` sit in the catalogue
(`RenderCatalogue.h:234-235`) and `Renderer::Executable` refuses them.

What ships: cascaded/atlas maps rendered only when a light or the world under it moves, O(1)
per pixel (id Tech 7 caches essentially all static shadows). The catalogue already names it.

- [x] `LightVisibility` renders the atlas: a depth-only pass over the subject residency (empty
      fragment -- Metal refuses a null one with a segfault, found the loud way), reverse-Z like
      the scene, the orthographic sun texel-snapped in WORLD space so the camera-relative rebase
      cannot shimmer it. Proven by readback: 4 194 304 texels written, the depth field structured
      (nearest 0.774 against median 0.495) -- the atlas holds the scene from the sun's seat
- [ ] the geometry shading samples the atlas (plan-selected against the BVH ray, which stays for
      the oracle suites)
- [ ] the sun's cast shadow lands in the driver's picture (the reviewer's standing finding)

---

Parked: slices 1-2 closed with the atlas proving itself in the chase test; slice 3 (shading
samples the atlas plan-selected, shadow lands in the driver's picture) waits behind the
architecture queue -- the review is the work list until it is clean.

- 2026-08-25, SHARPENED by the hourly review, slice 3 measured against the picture --
  `$TMPDIR/outshine-stills/km0721.0-third.png` (drive of 07:07, Munich--Hamburg) shows the F31
  standing on the deck under an unmistakably lit sky: the gradient warms toward the horizon, the
  roof carries a specular sheet, the glass reads. **The road under the car is byte-identical grey
  to the road ten metres away.** No contact shadow, no cast shadow, no ambient occlusion at the
  four wheel wells. The car reads as pasted onto the surface rather than standing on it, and that
  single missing term is the largest gap to the bar in the whole frame -- larger than markings,
  larger than the verge -- because a shadow is what tells the eye a body is IN the scene.
  `km0016.8-framed.png` says the same from the third-person seat at a different sun angle.
  This is the standing finding of checkbox 3, now with its still named.
