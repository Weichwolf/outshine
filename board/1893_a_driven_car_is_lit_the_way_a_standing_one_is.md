Type: bug
State: open
Parent: 1890
Area: render, clients
Tags: measured, picture, driver

# A driven car is lit the way a standing one is

The SAME asset, the SAME `<lighting>` declaration, two frames from the same binary at d4c8784c:

| frame | what it draws |
|---|---|
| `refused.png` — the studio stand-up, no drive | the F31 in silver: specular along the shoulder line, the glasshouse dark against a lit roof, the wheel arches modelled, the tail lamp reading red. Correct |
| `along01..08.png` — the 136 m Munich drive, `view="chase"` | a BLACK SILHOUETTE. The roof, the boot lid and the rear screen are all `#000`. Two faint grey strokes on the tailgate and the wing mirrors are the only pixels above zero |

A horizontal roof under `<key lux="40000" elevationDeg="42">` cannot be black, and
`<environment r="0.06" g="0.07" b="0.09">` cannot be black either. The first-person view is the
same picture from closer in: a near-black roof with grey edges.

**What it is NOT.** The normal is normalised in the vertex stage —
`o.n = normalize(s.model[0].xyz * v.n.x + ...)` (src/render/shaders/subjectLit.msl:14,
src/render/shaders/subjectMapped.msl:14) — so the uniform `MetresPerUnit` scale
`Live::Carry` builds into the placement (src/clients/Live.cpp:539-547) does not shorten it. That
hypothesis was tested and rejected.

**What it most likely IS, and it is a space seam again.** The only executed shadow path is a
per-pixel software BVH ray per light (`src/render/stages/ShadowRay.h`, board:1575). A BVH built
in one space and shaded from a position in another returns "occluded" for every ray, and every
ray occluded IS a black subject with its ambient term intact only where the ray misses. The
drive is the only path that moves the placement; the studio stand-up is the only one that does
not. That is the difference between the two frames.

board:1890's closing commit d99dcc4c states the chase drive is *"unchanged in what it draws"*.
It is not: what it draws is a silhouette, and no still was looked at when that was written.

## What will be true

- [ ] The driven F31 is lit exactly as the standing one is: the same specular on the shoulder,
      the same roof, the same tail lamp.
- [ ] The occluder set and the shading position are stated to be in ONE space, and a
      `static_assert` or an assembly-time refusal is what says so — not a comment.
- [ ] Proving case: the 136 m drive, chase view, mean luminance of the subject's pixels within
      10 % of the same subject in the studio frame under the same `<lighting>`. Negative
      control: move the occluder set into a second space and the mean falls to the ambient
      floor, which is what it is today.
