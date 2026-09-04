Type: feature
State: open
Area: render, generators, engine
Tags: architecture, look, measured, owner
Depends: 2137

# Clouds stand IN the atmosphere and are lit by it, from the weather the provider reports

**Benchmark** -- Unreal: the Volumetric Cloud component (UE 4.26+, source readable): a cloud
LAYER between a bottom and a top altitude, density = weather map (coverage, type) x 3D
Perlin-Worley shape noise x Worley detail noise, ray-marched per pixel at reduced resolution
with temporal reprojection, lit by the sun through Beer-Lambert with a multiple-scattering
approximation (attenuated octaves), a dual-lobe Henyey-Greenstein phase, ambient from the
SAME sky-view LUT the SkyAtmosphere computes, a cloud shadow map onto the ground, and aerial
perspective applied through the same transmittance -- Hillaire, "A Scalable and Production
Ready Sky and Atmosphere Rendering Technique" (EGSR 2020) plus the Frostbite clouds (Hillaire
2016). RAGE: GTA V's cloud DOME -- layered cloud textures on a hat over the horizon, sun-lit
with a cheap scattering term, cloud shadows as a projected texture -- fast and never volumetric.
**Where they differ**: Unreal ray-marches a volume, RAGE paints a dome. **Unreal's is taken**
because it is readable, because it reads the atmosphere this tree already has (`medium.msl`,
`mediumRadiance.msl`, `aerialPerspective.msl` are Hillaire's LUTs), and because a dome is a
texture -- the one thing the budget line refuses. RAGE's dome survives as the FAR FIELD: past
the march's reach a cloud is a lit slab in the sky-view LUT's frame, which is what Unreal does
too with its distant-cloud approximation. **Cited beside the two**: Schneider & Voss, "The
Real-Time Volumetric Cloudscapes of Horizon Zero Dawn" (SIGGRAPH 2015) for the shape recipe,
Wrenninge (2013) for the octave multiple scattering.

## Where it stands, measured 2026-09-04

```
  the medium        Hillaire's LUTs: transmittance, multi-scatter, sky-view, aerial perspective
  the weather       Scenario::Weather carries CloudCover, CloudLow/Mid/High, CloudBaseAglM --
                    the provider's word, obeyed by NOTHING in the picture
  clouds            none; the sky is a clear atmosphere at every place
  textures          none, by policy -- so the noise is GENERATED on the device, never loaded
```

## Why it is essential here

The budget line names *a realistic atmosphere* and refuses textures: an engine that lights
geometry has to light the sky's own geometry, and clouds are most of it on most days. A
photograph of any place on a cloudy afternoon disagrees with this engine in every pixel above
the horizon and in every shadow below it.

## The strategy -- one medium, two roles, five experiments before a line of product code

**Roles (board:2137's split, held)**: the PROVIDER answers what the sky holds (cover per layer,
base height, type -- METAR/model data, or the declared weather); a GENERATOR in `cloud/` turns
that into FORM (a coverage/type map over the area plus the seeds of the shape noise); the
RENDERER owns the LOOK: a volume pass inside the medium, lit by the medium's own LUTs. A cloud
never carries a light.

**The pass**:
1. **Noise, generated** -- a compute kernel writes a 128^3 R8 Perlin-Worley shape volume and a
   32^3 Worley detail volume once per device (Schneider's recipe), tileable, from a seed
2. **Density** -- a layer [base, top] from the provider; density = remap(shape, 1 - coverage)
   x height profile by type x detail erosion at the edges; wind drifts the sample point by the
   scenario clock, so two frames at one time are one picture (determinism)
3. **March** -- per pixel at quarter resolution (320 x 180 at 720p), 48..96 steps adaptive to
   the layer's thickness along the ray, exponential step growth with distance, early out at
   transmittance < 0.01; 4 x 4 temporal reprojection into full resolution over 16 frames
   (Unreal's), with a stationary fallback until the velocity buffer carries the sky
4. **Light** -- sun transmittance by Beer-Lambert over 6 light steps toward the sun with the
   octave approximation (a = 0.5 extinction, b = 0.5 scattering, c = 0.5 phase per octave,
   4 octaves), dual-lobe Henyey-Greenstein (g = 0.8 forward, -0.2 back, mixed 0.7), ambient =
   the sky-view LUT sampled at the cloud's altitude (top: sky, bottom: ground bounce), the
   "powder" darkening at the light-facing edge; sun colour = the transmittance LUT at the
   cloud's altitude -- the same numbers the ground is lit with
5. **Cloud shadows** -- a 512^2 shadow map of the layer's optical depth projected from the sun,
   read by the ground's and the subjects' shading as a transmittance factor (the light
   visibility pass already carries the frame)
6. **Composition** -- clouds are drawn before aerial perspective and fade through the same
   transmittance as a mountain does; a cloud at 30 km is as blue as the ridge behind it

**Budget**: <= 3.0 ms of the frame at 720p on the target, p99, measured over a moving camera
(quarter-res march ~1.5 ms, temporal 0.3, shadow map 0.4, noise 0 per frame).

## The experiments, each a measurement and none a product

- [ ] **E1 noise**: the two volumes generated on the device; cost once (ms), bytes (2.1 MB +
      32 KB), a slice looked at against Schneider's figure
- [ ] **E2 march without light**: a white slab at quarter resolution over Jura's sky, coverage
      from a declared `CloudCover`; cost on the target at 48/64/96 steps, p50/p99, the number
      the budget has to hold
- [ ] **E3 light**: Beer + octaves + dual HG + LUT ambient; the picture set beside a photograph
      of a cumulus day (the Earth is the yardstick) at three sun elevations -- the shape of the
      silver lining, the dark base, the blue ambient in the shadow side
- [ ] **E4 cloud shadow**: the ground under a cloud darker by the layer's transmittance;
      negative control: cover 0 leaves the ground's digest unmoved
- [ ] **E5 temporal**: reprojection over 16 frames on a moving camera; the ghosting looked at,
      the p99 measured with the walk of board:2092
- [ ] Only then the item that implements: `cloud/` with a `reaches`, the pass in the plan
      (`Stage::Clouds` between sky and aerial perspective), the provider's cover obeyed

## What will show I was wrong

A cloud that looks right at one sun elevation and wrong at another: then the pass has its own
lighting instead of the medium's, the split the fifth invariant forbids. Or E2 reads over
3 ms at 64 steps: then the march is not the answer on this GPU and the dome is.
