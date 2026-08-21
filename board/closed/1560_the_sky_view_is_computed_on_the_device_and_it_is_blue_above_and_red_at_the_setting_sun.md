Type: task
Parent: 0120
Area: render
Tags: instrument

**The sky view is computed on the device, and it is blue above and red at the setting sun**

Link three of the chain `board:1549` named -- Hillaire's Sky View LUT: 192x108 in the reference's
non-linear parameterisation (half the table is sky and half ground at ANY eye height, with the
horizon always on the middle row, where the gradient is steepest), 30 steps per texel, Rayleigh and
Cornette-Shanks Mie phases, transmittance and second order read from the chain's own tables.

| | |
|---|---|
| `src/render/stages/MediumRadianceStage.{h,cpp}` | the compute stage; re-dispatches when medium, sun zenith or eye height change, `Settled()` otherwise |
| `src/render/stages/ParticipatingMedium.h` | `SkyViewParams`/`SkyViewUv` (bijection), `RayleighPhase`, `MiePhase`, `MediumSkyRay` -- C++ and MSL side by side |
| `src/render/Renderer.{h,cpp}` | `SetSun(cosSunZenith, eyeHeightM)`; creates `SkyViewLut` 192x108 |

## The sky everyone knows, derived and not named

`test/unit/render/stages/TheSkyIsBlueAboveAndRedAtTheSettingSun.cpp`:

- **noon, 30 deg off zenith: blue > green > red** -- nothing in the code names a colour; the
  Rayleigh coefficients order the channels
- **the horizon is brighter than the zenith** (0.074 vs 0.034 blue), because a grazing path holds
  more air to scatter and extinction has not won yet
- **toward the setting sun the sky is red** (red 0.173 vs blue 0.013) and away from it the red is
  six times dimmer -- the anisotropy a fixed sky dome cannot have
- **the sun 30 deg under: below a thousandth of noon** -- night falls out of the geometry
- the parameterisation round-trips over all 20 736 texels, worst drift 4.2e-5

`test/render/outshine/shader/TheSkyViewAgreesAcrossTheWholeChain.cpp`: all three shipping stages in
one command buffer, three passes; twin samples the device's own two half tables; all 62 208 values
agree, worst 0.094 %.

## Comments

**One sign defect, found by physics and not by diffing**: the sunset was red AWAY from the sun. The
reference negates cosTheta at the phase call site ("negate because WorldDir is an in direction") and
negates it AGAIN inside Cornette-Shanks; copying the body while calling with the un-negated angle
pointed Mie's forward lobe backwards. The test that caught it asks the sky which side the sunset is
on -- a question a bitwise twin comparison would never ask, since both sides would have agreed on the
wrong answer.

**The horizon row measures the mapping, not the kernel, if compared pointwise.** 576 of 62 208
values sat past two percent, every one within two rows of v = 0.5 -- where the parameterisation is
steep BY DESIGN. The bound is a quarter-texel bracket in v; bracketed, zero are past. The steepness
is what buys the horizon its resolution, and an instrument that punishes it is measuring float
rounding in acos/cos round trips.
