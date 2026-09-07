Type: bug
State: open
Area: render, engine
Tags: look, measured
Depends: nothing

# The ambient the sky casts reaches the SHADER, and a shaded face reads right

**Benchmark** -- Unreal: a SkyLight captures the real sky into a cubemap and the shaded side of an
object is lit by THAT, with distance-field or screen-space occlusion damping it and Lumen carrying
the bounce from the sunlit wall opposite. RAGE: the timecycle keyframes a sky ambient and an
artificial ambient per hour and per weather, plus SSAO, and a street's shaded side is lifted by an
authored bounce. **Both agree**: the shaded side is lit by the SKY it can see plus what the scene
bounces at it, never by a single scalar.

## Where it stands, measured 2026-09-07 at Rosenheim

```
  the light that reaches the ground                    89 457.9 lux
  the sun stands this high                                 47.6 deg
  the ambient the sky casts        R 2731.1   G 3679.9   B 6116.3
  the ambient the ground bounces   R 3866.9   G 2971.0   B 2010.0
  lighting: the sky's own radiance      0.000     0.000     0.000  cd/m2
  lighting: the ground's bounced        0.000     0.000     0.000  cd/m2
```

**The same quantity is published twice and one of the two reads zero.** `AmbientStood_` carries
2731 and `Picture.Standing->AmbientStanding().RadianceLinear` carries 0.000, and
`src/engine/Laying.cpp:131` publishes the second. One source per rule, and this rule has two.

**A storage buffer of the sky's own irradiance is bound into every subject fragment shader and
never indexed.** `skyIrradiance` is threaded through six call sites in `subjectLit.msl`,
`subjectLitTextured.msl` and `subjectMapped.msl`; `grep 'skyIrradiance\['` over `src/` returns
nothing. What the shader actually uses is `subjectLit.msl:144`:

```
  const float skyShare = clamp(dot(n, lights.up.xyz) * 0.5 + 0.5, 0.0, 1.0);
  const float3 ambient = mix(lights.bounced.rgb, lights.environment.rgb, skyShare);
```

-- a two-colour hemisphere lerp. It has no idea what the sky over THIS place at THIS hour looks
like, although the engine computed exactly that and bound it beside.

**And the hemisphere has no local term.** No occlusion (`stage_without_a_body stage=ambientOcclusion`
prints on every run) and no inter-reflection, so a wall in a street receives nothing from the
sunlit wall three metres opposite -- which in a photograph is most of what lifts it.

Measured on one material, the terracotta roofs at Rosenheim: p10 66.0 and p90 169.7 of 255, a
ratio of 2.57 in sRGB and about 7.9 in linear light.

## The solution

1. The published measure and the shader read ONE source; the dead member goes
2. The fragment stage READS `skyIrradiance` -- the sky's own answer for this place and hour --
   instead of the two-colour lerp, and the buffer stops being decoration
3. The `ambientOcclusion` stage gets a body, so the hemisphere is damped by what the surface can
   actually see of the sky
4. A local bounce term, so a street's shaded side is lifted by the wall opposite

## What will be true

- [ ] `lighting: the sky's own radiance` and `the ambient the sky casts` read the SAME number
- [ ] `skyIrradiance` is indexed, and changing the hour changes the colour of a shaded wall
- [ ] The lit-to-shaded ratio on one material at Rosenheim is looked at beside the photograph, and
      the shaded face carries the sky's colour rather than a grey
- [ ] Negative control: zero the sky irradiance and every shaded face goes black

## What will show I was wrong

The ratio is already what a photograph shows once a tone curve is over it, and what reads as too
harsh is the missing grading of board:2155 rather than the ambient. Then this item is the two
defects above -- the doubled source and the unread buffer -- and nothing about the look.
