Type: bug
State: open
Area: render, scenario
Tags: measured, lighting, units

# A studio's ambient carries a unit, and it is the key's

**The sphere half of this item is done and measured** -- a declared sphere with air supplies the
irradiance every surface under it receives (`MediumSkyIrradiance`, reached the day
`Declared_.DrawsSky` was first written), proven by
`test/harness/outshine/door/ScoreWhatASkyLightsInShadow.cpp`. What is left is the studio.

A scenario that declares NO sphere has exactly one ambient and it is the typed triple:

    include/Scenario.h:96   double Environment[3] = {0.0, 0.0, 0.0};

`<key lux="40000">` is an ILLUMINANCE. `<environment r g b>` is a bare triple with no unit, and
the two are summed after one exposure divides both:

    Exposure = 2.5 / (1.2 * KeyLux)  = 5.208e-05
    key      = KeyLux * Exposure     = 2.0833      -- independent of KeyLux
    ambient  = 0.06   * Exposure     = 3.125e-06   = 0.80 of 255 in sRGB

So a studio scenario's ambient is off by five orders of magnitude unless its author happens to
write the number in lux, and nothing in the declaration says to. `apps/driver/src/f31.scenario`
carried `r="0.06" g="0.07" b="0.09"` and removing it changed the picture by nothing measurable:
mean max(RGB) 47.19 with, 47.19 without.

## Measured on the DRIVER at a73c6ca5, which is where it is paid for

`f31.scenario` declares `<key lux="40000" elevationDeg="42">` -- a bright afternoon -- and no
ambient at all. Across the ten stills of the acceptance drive:

    mean max(RGB)   43.3 .. 49.0 of 255
    peak            119 .. 162 of 255      (250 one round earlier)

Nothing in a 40 klux scene reaches half the range. `Exposure_` is 1.0 (`RenderPlan.cpp:218`),
the curve is ACES filmic, and `2.5 / (1.2 * KeyLux)` divides the key back out before it -- so the
declared illuminance sets the fill RATIO and nothing about the level, and the level that stands
in its place is not derived from anything. A number with a unit that decides no absolute is the
same defect as an ambient with no unit; they are one item.

## What will be true

- [ ] The declared ambient is an ILLUMINANCE, in the same unit as the key, so a studio and a
      sphere are lit by numbers that can be compared.
- [ ] Proving case: a studio scenario declaring an ambient equal to its key lights a shaded
      surface to the same order as a lit one, through `include/` alone. Negative control: the
      same declaration read as a bare triple falls below one level of 255, which is what it does
      today.
