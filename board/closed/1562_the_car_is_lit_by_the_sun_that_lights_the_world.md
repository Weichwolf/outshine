Type: bug
Area: render
Tags: bug

**The car is lit by the sun that lights the world**

`km0017.3-third.png` (2026-08-22): the F31 stands on the carriageway, correctly placed and sized --
and it is a black silhouette. The ground and the carriageway around it take the key light; the car
does not.

The caveat first: the asset carries its own materials (dark paint among them), so the harmless
explanation is a dark car in a picture whose exposure is set for 40 000 lux -- ruled IN or OUT by
one measurement: the GLASS is black too, and glass at any albedo specular-reflects a 42-degree sun.
A car whose windows return nothing is unlit, not dark.

Suspects, in order: the key light reaches only declared-surface parts (the file parts carry their
own materials through a different table); the emitted-radiance path zeroes file-part shading; the
light direction is in the wrong frame for file parts (the same conjugation family board:1551 just
closed).

- [x] measured: the roof and the rear window carry a bright specular highlight, the aerial stands
      shaded on the roof, the mirrors read against the body -- `km0017.3-third.png`, 2026-08-22
- [x] the glass shows a specular sun
- [x] the paint reads as a DARK paint's shadow side under a 42-degree sun from ahead-left, which is
      what a chase camera behind the car looks at; the remaining darkness is the missing ambient
      term, and that is `Stage::Irradiance` -- the sky chain's next link, on `board:0120`

## Comments

Found the moment board:1551 closed -- the car became visible and visibly unlit in the same still.


## CLOSED -- the light was never wrong; the NORMALS never turned

The vertex shader passed `o.n = v.n` and `o.t = v.t` through untouched: normals and tangents never
rotated with the placement, and the shading position was `v.p + anchor` -- the placement never moved
it. For identity placements every one of those is exact, which is why nothing before board:1551's
conjugation showed it; the moment the car's placement became real, its normals pointed into the
wrong frame and every dot with the light was noise.

The fix gives `S` the placement matrix as a fourth uniform: positions keep their fast path through
`mvp`, while normal, tangent and shading position go through `s.model` -- folded with the camera
anchor in double on the CPU, normalised in the shader against the placement's uniform scale.

Proof by eye and by suite: the specular highlights above, and render/outshine/shader 42/42 --
every twin still agrees, because identity placements are the fixed point of the new path too.
