Type: task
State: open
Parent: 1868
Area: render
Tags: perf, picture
Supersedes: 1567

# The shadow atlas the catalogue declares executes, and the sun's direction shows on the ground

The atlas renders: a depth-only pass over the subject residency, reverse-Z, the orthographic
sun texel-snapped in world space so the camera-relative rebase cannot shimmer it — proven by
readback (4 194 304 texels written, nearest 0.774 against median 0.495). **Nothing samples it.**
The only executed shadow path is still a per-pixel software BVH ray per light
(src/render/stages/ShadowRay.h) — the right instrument against the Cycles oracle, the wrong
mechanism at 60 Hz: cost scales with lights x pixels x depth complexity and caches nothing.

The picture says the same thing from the other end: a visible sun disc with no consequence
anywhere in the image — no sun-side/lee-side relief on the terrain, and the bonnet's highlight
not aligned with the disc.

## What will be true

- [ ] The geometry shading samples the atlas, plan-selected; the BVH ray stays for the oracle
      suites and for nothing else.
- [ ] Cascade selection is DECLARED rather than one atlas for every light
      (`class LightVisibilityStage {`, src/render/stages/LightVisibilityStage.h:16).
- [ ] The lit/lee luminance ratio is measured on one hill at low sun, so "the picture reads
      flat" is a number and not an impression.
- [ ] The sun's cast shadow lands in the driver's still, and the architect sees it.
