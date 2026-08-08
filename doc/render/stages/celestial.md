# Celestial — sun, moon and stars

**Passes:** `SunStage` · `MoonStage` · `StarsStage` (`sim/src/render/stages/`). **One document because
they share one ephemeris**: the run's declared instant decides all three, and a disagreement between them
is a defect none of them could detect alone.

Neighbours: [`atmosphere.md`](atmosphere.md) (the dome they are drawn over, and the transmittance LUT
that gives the sun its colour), [`tile-lights.md`](tile-lights.md) (the other night-time light source),
[`../renderer.md`](../renderer.md) §4 (which today carries the ephemeris detail and the SVS/EVS table).

## Spec

| Contract | Why |
|---|---|
| all three read **one ephemeris**, `core/Ephemeris.h` | it lives in `core/` and not in `render/` because visual acquisition needs the sun too, and `core/`/`sensors/` may not include `render/` |
| each is an **additive draw** (`One`/`One`) right after the sky dome | they add light to a dome that is already there; they never own the pixel |
| the sun contributes **the disc and nothing else**. There is no glow, no halo and no aureole term here | the aureole is Mie forward scattering and it is already in the sky-view LUT (phase g = 0.8). A hand-written lobe on top is a second sky: measured at nebelhorn 2026-07-28 11:00Z, the one that stood here added (0.417, 0.333, 0.228) at 22.8° elevation to a LUT radiance of (0.129, 0.275, 0.608) and turned a B/R that falls 4.73 → 3.81 toward the horizon into one that rises 1.53 → 2.14 |
| the disc's radiance is **derived from the drawn solid angle**, `L = E_TOA / (2π(1 − cos θ))` with θ the same angular radius the disc is cut at | widening the disc then cannot change the flux it carries. At θ = 0.5° that is 1/2.392e-4 = **4180** per unit TOA irradiance; the `30` that stood there was a `[SET]` with no origin |
| each **self-gates** and costs nothing when it cannot contribute | the sun returns `vec4f(0)` outside EVS; the stars need SVS off, night, and a catalogue |
| the moon is an **illuminated sphere**, not a painted disc | phase and terminator then emerge physically instead of being drawn, and the moonlight the ground receives is the same quantity |
| the moon **owns** its albedo texture | it is the sole consumer of the NASA LROC map |
| the stars are placed at **true alt/az** from a real catalogue (HYG), Polaris-pinned | a star field that is decoration cannot be checked; one that is an ephemeris can |
| the day factor that fades all three is **ONE number** shared with sky and ground | `t = clamp((sunElDeg + 9)/12, 0, 1)`, smoothstepped — full day above ≈ +3°, dark from ≈ −9° (nautical twilight). Derivation in [`../renderer.md`](../renderer.md) §4 |

## State

**All three are built** and landed with the stage split (`c9206eb`…`2099cb0`, see
[`../renderer.md`](../renderer.md) `## State`).

| Stage | What is built |
|---|---|
| `SunStage` | disc only, additive, sun colour from the transmittance LUT; angular radius `cos(0.5°)` and the derived radiance `1/(2π(1 − cos 0.5°))` in `AtmoBuf` |
| `MoonStage` | lit sphere with the LROC albedo, phase from the ephemeris, angular radius 0.0045 rad × `FB_MOON_SCALE` |
| `StarsStage` | HYG field, instanced additive quads at true alt/az, self-gated on SVS, daylight and a missing catalogue |

**Accuracy of the ephemeris, as its own source states it:** `SunPos` is a verbatim port of the NOAA
approximation formulas (**< ~0.5° error**); `MoonPos`/`MoonPhase` port Paul Schlyter's approximation
**without** its long perturbation-term table — good to about a degree, which is enough for a disc plus a
phase and not enough for navigation. The phase is `(1 − cos(elongation))/2`.

## Gaps

- **The sun has no glare and the frame has no bloom.** With the authored halo gone the disc is a hard
  three-pixel dot at 320×180 (frame: `sim/web/cams/` — render a pose with the sun in view). Physically
  that is what a 0.5° source is; what a photograph adds is lens flare and what an eye adds is scatter in
  its own optics, and neither is in this renderer. A bloom pass would be the honest answer and it is a
  pass, so it is not free. Until then the sun in frame is under-sold.
- **`FB_MOON_SCALE` exists**, which means the moon can be drawn at a size that is not its own. A scale
  factor with no declared value in this document is a knob without a number; what it is set to, and why,
  belongs in a Spec row and is missing.
- **No star magnitude policy is recorded.** Which catalogue cut is drawn, at what magnitude limit and
  with what point-spread at 720p, is unstated here and therefore unchecked — and at 720p a star is a
  sub-pixel source, which is the case [`../lod.md`](../lod.md)'s τ argument and
  [`../gpu-determinism.md`](../gpu-determinism.md)'s coverage warning both bite on.
- **Nothing verifies the three against each other.** The obvious gate is cheap and absent: render a
  known instant and place, and check sun and moon altitude/azimuth against an external ephemeris. The
  error bars (0.5° and ~1°) are published by the ports themselves and have never been confirmed in this
  tree.
- **Moonlight reaches the ground through the atmosphere chain, and no measurement covers it.** Whether a
  full moon produces a plausible ground level is judged by eye. The night-vision pass that most depended
  on the answer was deleted with the avionics group on 2026-08-07, so nothing consumes it today.

## Knowledge

The ephemeris ports, their error bars and the day-factor derivation are stated **once**, in
[`../renderer.md`](../renderer.md) §4; the accuracy figures are repeated in `## State` above only because
they are this pass's acceptance limit. Migrating that section here is the same outstanding work
[`atmosphere.md`](atmosphere.md) `## Gaps` names.
