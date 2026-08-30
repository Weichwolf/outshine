Type: bug
State: open
Area: render
Tags: measured, benchmark, picture

# The air carries the aerosol a clear day actually has, and the shadows stop being twice too deep

**Benchmark** — Unreal: `SkyAtmosphere` ships Bruneton/Hillaire's coefficients AND exposes
`MieScatteringScale`, `MieAbsorptionScale` and `RayleighScatteringScale` per scene, because the
shipped numbers are a PRISTINE atmosphere and a scene has weather. RAGE: aerosol is authored in
the timecycle per hour and per weather state, which is the same admission made by hand.
**Both agree the shipped constant is a starting point, not the answer**; taking Unreal's, because
a scale on a physical coefficient keeps the physics and a hand-authored colour does not.

## The implementation is CORRECT and that is the point

`ParticipatingMedium.h:18-26` carries Bruneton's exact numbers, ozone included. Derived vertical
optical depth at 550 nm:

| term | per km | scale | tau |
|---|---|---|---|
| Rayleigh | 0.013558 | 8.0 km | 0.10846 |
| Mie extinction | 0.004440 | 1.2 km | **0.00533** |
| ozone | 0.001881 | 15 km (tent area) | 0.02822 |
| | | **total** | **0.14201** |

At Zurich's solar noon, elevation 66.066 deg, air mass 1.0946: tau = 0.15544, T = e^-0.15544 =
**0.856**. Measured through the engine at that place and hour: **0.866**. The 1.2 % is spherical
against plane-parallel air mass. Nothing here is broken.

## What it costs the picture, measured at ZurichPlan

| quantity | this tree | a clear day |
|---|---|---|
| direct normal illuminance | 115 232 lx | ~100 000 lx |
| sky irradiance, horizontal | 7 246 lx | ~12 500 lx |
| **sun : sky** | **14.5 : 1** | **~6.8 : 1** |
| aerosol optical depth, 550 nm | **0.0053** | 0.05 - 0.15 |

Aerosol optical depth of 0.0053 is a mountain-top on the clearest day of a decade. A real clear
lowland day is ten to twenty times that, and the term appears TWICE with opposite signs: it dims
the sun and it brightens the sky. Both errors push the same way, which is why the ratio is off by
2.1x while each side is only off by ~1.5x. The picture's shadows are twice as deep as a
photograph's, and that is the single loudest thing between `ZurichPlan` and an aerial photograph.

## What will be true

- [ ] Aerosol is DECLARED, not baked -- a scenario states the day's turbidity (or its
      Angstrom beta) and the medium scales its Mie terms from it, with the shipped pristine
      value as the engine's own default when nothing is declared.
- [ ] Measurement that shows the change is wrong: sun : sky at ZurichPlan's place and hour. It
      reads 14.5 : 1 today and must approach 6.8 : 1; if it overshoots below ~5 : 1 the aerosol
      is too thick and the picture will read as haze.
- [ ] Negative control: turbidity declared at the pristine value reproduces today's 14.5 : 1
      exactly, so the knob is proven to be the thing that moved it.
- [ ] The vendor sky corpus (test/clearsky) gains a DIFFUSE row -- it scores global horizontal
      today, which a too-thin atmosphere passes because the direct excess and the diffuse
      deficit cancel in the sum. That cancellation is why this was invisible until the ratio
      was asked for.
