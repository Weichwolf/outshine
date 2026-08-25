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

**THREE HYPOTHESES TESTED AND KILLED, same session.** Each one by running the drive and
measuring the mean red over the car's own pixels (window x 540..740, y 380..640):

| tried | result |
|---|---|
| the shadow centre, which `Live::Carry` computed through `EcefFromGltf` plus `kStudioAnchorEcefM` -- a studio anchor at the equator, for a body standing on a corridor hundreds of metres away | removed. Mean red UNCHANGED at 3.6. The term was wrong and is gone; it was not the cause |
| the shadow pass itself | it does not run. `Declaration::ShadowRadiusM` is 0 for every scenario the tree stands, so `LightVisibility` is never in the plan and `SetShadowFrame` is never called |
| the sun's direction | raising `elevationDeg` from 42 to 90 lifts the mean from **3.6 to 9.5**. Light reaches the car and is attenuated by a factor of roughly fifteen; the studio stand-up of the same asset reads above 150 in the same window |

The vehicle's rotation is NOT it either: `Engine::Rides` builds the matrix column-major from
`OrientationQ` in (w, x, y, z) order, which is what `Physics::Body` declares, and each of the
nine terms matches the standard form.

**MEASURED AT 817ea333, AND IT IS NOT A DRIVEN-VERSUS-STANDING DEFECT.** The shipped route
still refuses, so the drive was overridden -- `--from 48.13720,11.57560 --to 48.13600,11.58200`,
`DROVE 1534 frames over 0.282 of 0.302 km, kept 9 still(s)`. Mean luminance over the subject's
own opaque pixels, with its screen bounding box beside it:

| still | opaque px | bbox | mean L |
|---|---|---|---|
| along01 | 38473 | x[515,1147] y[636,719] | 15.1 |
| along05 | 38568 | x[515,1148] y[636,719] | 12.6 |
| along07 | 37238 | x[510,1133] y[637,719] | 12.8 |
| along08 | 38024 | x[513,1142] y[636,719] | **45.3** |
| along09 | 37486 | x[511,1136] y[637,719] | **43.8** |
| refused.png (studio, no drive) | 87874 | x[366,886] y[266,515] | 46.6 |

**The camera is bolted to the body, so the same triangles cover the same pixels in all nine --
the bounding box moves by three pixels over the drive and the opaque count by 3 %. The only
thing that changed between along07 and along08 is the body's heading, and the shading changed
by a factor of 3.5.** A yaw about the surface's own up cannot change a horizontal roof's
diffuse term at all: `N.L` is `sin(elevation)` for every heading. That it changes by 3.5x is a
proof, not a hypothesis -- **the key's up axis and the body's up axis are not the same axis in
the space the shading happens in**.

The two candidate ends of that seam are both citable:

- `src/clients/Live.cpp:195-205` builds the key in the glTF frame and maps it with
  `EcefFromGltf`, whose up is `out[0] = gltf[1]` -- ECEF +X, the local up at the equator on the
  prime meridian, where `kStudioAnchorEcefM` puts the studio. The drive stands at 48.14 N.
- `src/clients/Live.cpp:218-220` pins the camera basis at `eye = {0,0,0}`,
  `forward = {0,0,-1}` for every frame while `GltfStudio.cpp:322` passes the real position. A
  renderer that is camera-relative in 32-bit needs the camera it is relative TO.

**The studio frame has the same defect and it was read as correct.** `refused.png` at 817ea333
is silver on the flanks and BLACK on the roof, the tailgate and the rear quarter -- a horizontal
roof reading `#000` under a 42-degree 40000 lux key, in the frame this item cited as the one
that is lit properly. The comparison this item was filed on does not hold; what it measures is
one asset lit from a direction that is not the declared one, in both frames.

## What will be true

- [ ] The F31 is lit from the direction the scenario declares, standing and driven alike: a
      horizontal roof under `elevationDeg="42"` reads `sin(42) = 0.669` of the key, and no
      heading changes it.
- [ ] The occluder set and the shading position are stated to be in ONE space, and a
      `static_assert` or an assembly-time refusal is what says so — not a comment.
- [ ] Proving case: the 136 m drive, chase view, mean luminance of the subject's pixels within
      10 % of the same subject in the studio frame under the same `<lighting>`. Negative
      control: move the occluder set into a second space and the mean falls to the ambient
      floor, which is what it is today.

## 2026-08-25: two causes eliminated from the door side

`Renderer_->SetSky(toSun, up, KeyLux, 0)` -- the one call carrying the key's direction to the
device -- stood inside `if (Declared_.DrawsSky)`, so a scenario declaring `<lighting>` and no sky
never set the sun. Fixed; only `SetMedium` is under that guard now. This is a candidate for the
driven/standing difference and has not been re-measured against a drive.

A door-side case now proves the key's direction DOES reach a subject: the same triangle at +42
deg and at -80 deg elevation renders differently, fourfold apart in the cosine
(`harness/outshine/door/ScoreWhatTheKeyLuxDoes`). So whatever is wrong with a driven car is not
that the subject shader ignores its key.
