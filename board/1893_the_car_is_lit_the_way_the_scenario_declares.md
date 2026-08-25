Type: bug
State: open
Parent: 1890
Area: render, engine
Tags: measured, picture, driver

# The car is lit the way the scenario declares, standing and driven alike

Under `<key lux="40000" elevationDeg="42" bearingDeg="150"/>` and
`<environment r="0.06" g="0.07" b="0.09"/>`, the F31 renders BLACK.

## Measured at a32c4919, decoded channel by channel

The 302 m overridden drive, first-person `<view id="eyes">`, nine stills:

| still | mean `max(R,G,B)` over 921 600 px | px above 32 | share |
|---|---|---|---|
| along01 | 0.45 | 7 131 | 0.77 % |
| along05 | 0.38 | 6 172 | 0.67 % |
| along08 | 1.72 | 9 465 | 1.03 % |
| along09 | 1.69 | 9 772 | 1.06 % |

**99 % of every frame is below RGB 32 and the frame's mean channel is half a level out of 255.**
The eye now sits in the cabin (board:1890), so 62 % of the frame is the car's own interior at
`alpha = 255`, and essentially none of it carries light.

**The earlier evidence in this item measured a composite, not the picture.** The table that read
"mean luminance 15.1 ... 45.3" over "38 473 opaque px" was an image viewer's alpha compositing
over white; the colour channel was near zero then too. What survives that correction is the
original observation and it is now the whole item: a horizontal surface under a 42-degree,
40 000 lux key cannot be `#000`, and neither can an interior under an ambient of 0.06.

What was ruled out, each by running the drive: the vertex normal is normalised so the placement
scale cannot shorten it; the shadow centre term is gone and changed nothing; the shadow pass is
not in the plan for this scenario; the body's orientation matrix matches the standard form; and
`ScoreWhatTheKeyLuxDoes` proves through the door that the key's DIRECTION does reach a subject.

Still standing as the two candidate ends of the seam:

- `src/engine/Live.cpp:195-205` builds the key in the glTF frame and maps it with `EcefFromGltf`,
  whose up is ECEF +X -- the local up at the equator on the prime meridian, where
  `kStudioAnchorEcefM` puts the studio. The drive stands at 48.14 N.
- `src/engine/Live.cpp:218-220` pins the camera basis at `eye = {0,0,0}`, `forward = {0,0,-1}`
  for every frame while `GltfStudio.cpp:322` passes the real position. A renderer that is
  camera-relative in 32-bit needs the camera it is relative TO.

## What will be true

- [ ] A horizontal surface under `elevationDeg="42"` reads `sin(42) = 0.669` of the key, standing
      and driven, and no heading changes it.
- [ ] The occluder set and the shading position are stated to be in ONE space, by a
      `static_assert` or a refusal at assembly -- not by a comment.
- [ ] Proving case: the drive's stills, decoded per channel, carry a subject whose mean
      `max(R,G,B)` is within 10 % of the same subject in the studio frame under the same
      `<lighting>`. Negative control: the key elevation lowered to -80 degrees and the same case
      falls to the ambient floor.
- [ ] No case and no ledger in this tree scores a still by compositing its alpha. A luminance is
      read from R, G and B.
