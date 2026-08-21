Type: task
Parent: 0120
Area: render
Tags: instrument

**The multiple scattering table is computed on the device, from the device's own transmittance**

The second link of the sky chain (`board:1555` was the first). Hillaire 2020's isotropic transfer:
each texel of a 32x32 table integrates second-order luminance and the transfer term f_ms over 64
directions x 20 steps, and 1/(1-f_ms) sums every order as a geometric series.

| | |
|---|---|
| `src/render/stages/MediumMultiScatterStage.{h,cpp}` | the compute stage: samples `TransmittanceLut`, writes `MultiScatterLut`, medium as uniform |
| `src/render/stages/ParticipatingMedium.h` | `MediumMultiScatterTexel` (C++, templated on the transmittance source) and `mediumMultiScatterTexel` (MSL) side by side |
| `src/render/plan/RenderPlan.cpp` | **a compute stage that reads what the open pass writes closes the pass** -- without this the plan merged both stages into one compute pass and the second would sample a texture bound for write |
| `src/render/Renderer.cpp` | creates `MultiScatterLut` 32x32 and the `LutSampler`; `SetMedium` reaches both stages |

## The physics the table is checked against

- **f_ms < 1 over the whole domain** -- the series' own admission condition, largest measured 0.253
- **a sun below the planet lights nothing**: at the left edge the second order collapses below 1e-2
  of noon's
- **from horizon to zenith the second order only grows**, texel by texel over the daylight half
- 1024 texels, all finite, all non-negative

`test/unit/render/stages/TheSecondOrderSkyIsBoundedByTheFirst.cpp` (twin) and
`test/render/outshine/shader/TheSecondOrderTableAgreesAcrossTheChain.cpp` (device): the twin samples
the DEVICE'S half-precision transmittance table, so the comparison isolates this kernel from the
first stage's storage. All 3072 values within 1 % (floored 2e-3 absolute); worst 0.096 %.

## Comments

The reference lifts the bottom row by adding 0.01 to a UV -- a km constant in uv units, landing ~1 km
up. Here the lift is 10 m in km (`kMediumGroundLiftKm`), which is what PLANET_RADIUS_OFFSET meant.

The luminance march keeps the reference's 0.3 sample point, where the transmittance test deviated to
0.5 -- the 0.3 belongs to the analytic per-segment transmittance weighting used HERE and not in a pure
optical depth. One constant each, named for its own integral.
