Type: task
State: open
Parent: 1868
Area: render
Tags: perf, picture
Supersedes: 1567

# The shadow atlas the catalogue declares executes, and the sun's direction shows on the ground

**Benchmark** — Unreal: a shadow atlas allocated per view from a pool, cascades inside it, the shader picking one. RAGE: cascaded shadow maps over the map. **Both agree** — one atlas, cascades within it.

The atlas renders: a depth-only pass over the subject residency, reverse-Z, the orthographic
sun texel-snapped in world space so the camera-relative rebase cannot shimmer it — proven by
readback (4 194 304 texels written, nearest 0.774 against median 0.495). **Nothing samples it.**
The only executed shadow path is still a per-pixel software BVH ray per light
(src/render/stages/ShadowRay.h) — the right instrument against the Cycles oracle, the wrong
mechanism at 60 Hz: cost scales with lights x pixels x depth complexity and caches nothing.

The picture says the same thing from the other end: a visible sun disc with no consequence
anywhere in the image — no sun-side/lee-side relief on the terrain, and the bonnet's highlight
not aligned with the disc.

## What the driver's still says at bb9472db -- the contact shadow is 0.7 counts

Chase view, city route, `shots-chase/along05.png`, sun declared at 42 deg elevation:

| the plane, 170 x 25 px | sRGB |
|---|---|
| directly beneath the car's sills | (61.4, 76.0, 55.3) |
| 200 px to the left, same rows | (61.2, 75.9, 55.2) |
| 200 px to the right, same rows | (62.0, 76.0, 56.1) |

**0.2 to 0.8 counts.** The car is not on the ground in any sense the picture can see: no cast
shadow, no contact darkening, not even an ambient occlusion term at the sills. The same run
prints `batches the shadow casts = 259` and `the shadow atlas, least depth = 1 / its most = 1` --
259 casters rendered into an atlas that holds one constant, which is the whole story in two
numbers.

The other end of the same term is board:1935: a straight terminator crosses the ground at 15:1
and darkens the car with it. Something IS producing visibility, and it is not producing it from
the geometry in the frame.

## What will be true

- [ ] The geometry shading samples the atlas, plan-selected; the BVH ray stays for the oracle
      suites and for nothing else.
- [ ] Cascade selection is DECLARED rather than one atlas for every light
      (`class LightVisibilityStage {`, src/render/stages/LightVisibilityStage.h:16).
- [ ] The lit/lee luminance ratio is measured on one hill at low sun, so "the picture reads
      flat" is a number and not an impression.
- [ ] The sun's cast shadow lands in the driver's still, and the owner sees it.
