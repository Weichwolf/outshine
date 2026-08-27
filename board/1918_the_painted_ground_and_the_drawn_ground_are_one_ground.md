Type: bug
State: open
Parent: 1890
Area: render, world
Tags: measured, picture, medium

# The ground the sky paints and the ground the compositor draws wear one atmosphere

**Benchmark** — Unreal: ONE `FSkyAtmosphereSceneProxy` feeds both the sky dome and the aerial perspective applied to geometry, so a surface and the air above it cannot disagree. RAGE: one timecycle drives both. **Both agree** — one atmosphere, two consumers.

Measured through the windscreen at 873f8f65, first person, one frame:

| | |
|---|---|
| the composed ring, 2–5 km out | **(78, 94, 109)** |
| the sky stage's ground below it | **(34, 42, 32)** |
| the sky itself above | (45, 73, 108) |

The ring is CORRECT. Its albedo is the medium's own `GroundAlbedo` and its distance is 2 to 5 km,
so the atmosphere veils it toward the sky's colour — that is aerial perspective and a photograph
of Munich does the same thing.

**The sky stage's ground does not do it.** It is one flat olive tone at every distance, and the
two grounds meet at a seam: real terrain fades into the haze, the painted plane behind it does
not, and the horizon shows the join.

## What the stated cause is NOT, read at HEAD

The item said the painted ground is "seen through a transmittance that does not vary with the
ground point's range". That is wrong and reading the kernel says so:

    src/render/shaders/medium.msl:153-165   if (toGround >= 0.0) { ... summed += throughput *
                                            toSunGround * cosSunAt * GroundAlbedo / PI; }

`throughput` is the accumulated view-ray transmittance at the point the march reaches the ground,
and the march runs to `min(toTop, toGround)` (`medium.msl:118`). So the painted ground IS veiled
by its own range, and whatever makes the seam is somewhere else.

Three candidates remain and each needs a MEASUREMENT at the device, not a reading:

1. **the two grounds are lit differently.** The painted one is `transmittanceToSun * cos * albedo
   / PI` -- a Lambertian sun term. The composed ring goes through the subject shading path with
   the key light. Two lighting paths for one surface is enough to make 44 counts on its own.
2. **the painted ground is at the wrong altitude.** `hitsGround` resolves against
   `bottomRadiusKm`, sea level, while the terrain the ring draws stands at ~500 m. The range to
   sea level is longer, so the painted plane should be MORE veiled -- and it is measured LESS
   veiled, which makes this the wrong sign and probably not the cause.
3. **the ring gets aerial perspective the painted ground does not, applied twice or by another
   term.** Ruling this in or out means reading what the ring's own shading does with the medium.

Whichever it is, it is a difference between two SHADING PATHS for one surface, not a missing
veil, and the repair is likely to be that there is only one path.

## What will be true

- [ ] One ground, one atmosphere: whatever paints below the horizon is veiled by the same
      transmittance the composed ring is, so the two are indistinguishable where they meet.
- [ ] Proving case: the pixel where the drawn ring ends and the painted ground begins differs by
      less than the frame's own quantisation. Negative control: the painted ground unveiled, and
      the seam is the 44 counts measured above.
