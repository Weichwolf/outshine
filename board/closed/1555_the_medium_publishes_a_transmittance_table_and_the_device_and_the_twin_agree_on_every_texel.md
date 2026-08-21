Type: task
Parent: 0120
Area: render
Tags: instrument, perf

**The engine carries a participating medium, and its transmittance table is computed on the device**

**This is the first link of the sky chain and the engine's FIRST COMPUTE PASS.** `board:1549` measured
that declaring `Stage::Sky` refuses at `mediumTransmittance` -- the compiler pulls the plan backwards,
so the sky is not one missing stage but a chain, and this is its head.

## What the engine can now do

| | |
|---|---|
| `src/render/stages/ParticipatingMedium.h` | the medium as **sixteen floats in four aligned rows**, and the physics over it: extinction at a height, the ray-sphere reaches, Bruneton's transmittance parameterisation both ways, and the march. **One source, two languages** -- the C++ and the MSL are side by side in one header and a device test ties them |
| `src/render/stages/MediumTransmittanceStage.{h,cpp}` | the compute stage: one kernel, 8x8 groups over a 256x64 table, the medium pushed as a uniform |
| `src/render/Renderer.cpp` | `EncodePass` opens a **compute** pass when the plan declares one, binding the storage textures the pass writes; `Create` makes the table at its own resolution rather than the frame's |
| `src/render/plan/RenderPlan.cpp` | a compute pass now carries the textures its stages **write**, in `Pass::Targets` -- renamed from `Pass::Colours`, because the same set is colour attachments for a raster pass and read-write storage for a compute one |
| `src/render/GpuOwned.h` | `OwnedComputePipeline` |

## The numbers, and where each came from

**The parameters are Bruneton's, fetched from the reference implementation that accompanies Hillaire
2020 rather than recalled** -- `sebh/UnrealEngineSkyAtmosphere`, `Application/SkyAtmosphereCommon.cpp`,
`SetupEarthAtmosphere`. Rayleigh 0.005802 : 0.013558 : 0.033100 per km over an 8 km scale height, Mie
0.003996 scattering against 0.004440 extinction over 1.2 km, ozone 0.000650 : 0.001881 : 0.000085 per
km on a tent, planet 6360 km inside a 6460 km medium.

| number | origin |
|---|---|
| 6360 / 6460 km | `[SET]` from the source, **and taken WITH its assumption**: the coefficients were integrated over a wavelength power spectrum against exactly this pair. The horizon dip a 2 m eye sees differs from WGS84's by 0.00006 deg, which is 1/750 of a pixel at 720p |
| ozone as a **tent**, centre 25 km, half width 15 km | **derived** -- algebraically identical to the reference's two linear layers with their hand-solved constants (1/15, -2/3 and -1/15, 8/3), and written as the shape it is because a hand-solved constant is where a sign goes missing |
| 40 march steps | **measured**, not copied. Against the settled 4096-step march the blue optical depth is off by 2.098e-2 at 10 steps and 1.928e-3 at 40 -- so 40 removes 91 % of what 10 leaves. The source only said 10 "starts to be visible" |
| **sample at 0.5 of the step, not 0.3** | **measured, and a deliberate deviation.** Hillaire samples at 0.3 because the same function also integrates scattered luminance, where the transmittance weight leans the average toward the near end. A pure optical depth carries no such weight: blue error is 1.93e-3 at 0.5 against 1.74e-2 at 0.3, a factor of nine, and copying 0.3 here would import a bias belonging to a different integral |
| 256 x 64 | `[SET]`, Bruneton's `TRANSMITTANCE_TEXTURE_WIDTH/HEIGHT` |

## The oracle is a closed form and not another implementation

**A radial ray has an analytic optical depth** -- the sphere contributes nothing to it, so it is
`sigma * H * (1 - exp(-100/H))` per exponential layer plus the tent's own area. That is a population of
one where the answer is known without the march, and the march lands on it to **0.10 %, 0.13 % and
0.19 %** in red, green and blue -- inside half a step of an 8-bit quantisation.

`test/unit/render/stages/TheZenithRayAgreesWithTheAnalyticAtmosphere.cpp` -- 21 checks. Zenith
transmittance 0.940 / 0.868 / 0.762. The round trip of the parameterisation over **all 16 384 texels**
drifts at worst 1.35e-4, a twenty-ninth of a texel. The grazing ray from the lowest texel row travels
1139 km and leaves 4.05e-5 of the blue against 0.099 of the red -- **which is sunset, out of the
coefficients alone**.

`test/render/outshine/shader/TheMediumsTableIsTheSameOnBothSides.cpp` -- the shipping stage runs on the
device and **every one of the 49 152 values agrees with the C++ twin**, worst 2.33 half-steps.

## What it costs a frame, and the answer is nothing

**The table is a function of the medium alone, so the stage dispatches when the declaration changes and
never again.** `Settled()` publishes it. A stage that redispatched every frame would spend 655 360
ray-march steps producing a texture identical to the one already bound -- and *the frame path is made
of bounded terms*.

## Comments

**Two instruments were wrong before the engine was, and both looked exactly like a physics defect.**

**One.** The half-float decoder in the device test computed the subnormal exponent as `127 - 15 - shift`
where normalising to a leading 1 gives `127 - 14 - shift`. Every subnormal read back **halved**. It
surfaced as a texel where the twin said 6.042e-5 and the device "said" 3.019e-5 -- a ratio of 2.0014,
which is not clean enough to look like a bit shift and is exactly what an off-by-one exponent plus one
step of rounding produces. Checked afterwards against all 1023 subnormal codes: 0 wrong.

**Two.** The same test first reported the worst disagreement in ABSOLUTE terms while the bound was in
half-steps, so it printed a widest error of 1.04 half-steps beside a count of 890 values past four --
two numbers that cannot both be true, and the pair is what gave the defect away. **Report a residual in
the unit its bound is written in.**

**Three, and this one is the engine's.** A ray leaving the ground straight up was stopped by the ground
at t = 0: the reference's `raySphereIntersectNearest` returns the far root when the near one is behind,
which is right for an origin INSIDE the sphere and wrong for one standing on it. The whole table came
back as transmittance 1. **The domain is part of the claim** -- this engine's rays never start below the
ground, so only the entry root can stop one, and the function says so in four lines instead of seven.

**The exactly-tangent corner is checked rather than avoided.** At u=1, v=0 the ray is tangent from sea
level, float32 rounds it a billionth below the horizon, and the ground stops it after **6 mm** for a
transmittance of 0.9999998. The reference dodges this with a 10 m planet-radius offset; here it is a
named limit with its number, unreachable through a sampler because a clamped fetch lands on a texel
centre.

**What the 16-bit table loses is below a millionth of the light.** 890 of 49 152 values are subnormal
and 118 underflow to zero, the largest of them 5.74e-8. The reference's note that "32f is required if
you do not want extra visual artefacts" is said of the SCATTERING volume, where a lost bit becomes a
band across the sky; transmittance underflows only where the medium has already taken everything.
