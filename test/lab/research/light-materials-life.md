# Light, materials and life: what is proven, what is readable, what fits the target

Research report, 2026-09-05, revised the same day after the owner's correction on SDL_GPU.
Target: A18 Pro (5 GPU cores, ~2.3 TFLOPS FP32, 60 GB/s LPDDR5X; PS4: 1.84 TFLOPS, 176 GB/s) at
720p60 = 16.7 ms/frame, 921 600 px, SDL_GPU only. **What SDL_GPU exposes (verified in
`include/SDL3/SDL_gpu.h`, 2026-09-05)**: shader stages VERTEX and FRAGMENT only; COMPUTE
pipelines (`SDL_CreateGPUComputePipeline`, `SDL_DispatchGPUCompute`,
`SDL_DispatchGPUComputeIndirect`, `SDL_GPUIndirectDispatchCommand`); storage buffers and
textures readable from vertex, fragment and compute (`SDL_BindGPU{Vertex,Fragment,Compute}
Storage{Buffers,Textures}`), written from compute (`COMPUTE_STORAGE_WRITE`,
`COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE`); indirect draws (`SDL_GPU_BUFFERUSAGE_INDIRECT`,
`SDL_DrawGPUIndexedPrimitivesIndirect`, `SDL_GPUIndexedIndirectDrawCommand {num_indices,
num_instances, first_index, vertex_offset, first_instance}`); render-pass load/store ops incl.
`DONT_CARE` and `RESOLVE`; cube and array textures; mipmap generation. **Not exposed**: mesh,
geometry and tessellation stages, ray tracing, programmable tile shading, and -- a finding for
every budget below -- occlusion and TIMESTAMP queries. So the GPU can cull, bin, reduce, scan
and drive its own draws; it cannot amplify geometry in a shader stage, and it cannot time itself.
The bandwidth gap to the PS4 (about 3x less) is the number that bounds this target more than the
FLOPS, and every budget below is set with it; section 6 says what compute moves and what it
does not. Every number carries its origin: **measured** (by the cited body, on the named
platform), **derived** (shown), **[SET]** (a proposal of this report), **unverified** (repeated
from a secondary source; the primary was not read this session). Board items this feeds: 2128
(many lights), 2129 (reflections), 2127 (a body meets the ground), 2134 (autopilot), 2136 (a
thousand minds), 2142 (a mind is a provider), 2151 (savegame = scenario + snapshot + events).
Buildings, ground cover, water and clouds are `world.md`'s and are not covered here.

Conventions: Lab = `test/lab/<dir>/<exp>.py` per `test/lab/README.md`; inputs are the engine's
own (OSM via the same fetch, terrarium z14, `Scenario::Weather`, `Scenario::Exposure`) and the
providers named in section 3. A PROOF is a check that goes red plus a negative control that
passes.

What the tree holds today (grep 2026-09-05): `src/render/stages/Medium{Transmittance,
MultiScatter,Radiance}Stage`, `AerialPerspectiveStage`, `IrradianceStage`, `SkyStage` (an
atmosphere of Hillaire's shape; `include/Earth.h:60` cites Bruneton's Rayleigh fit);
`MetalRoughBrdf.h`, `MicrofacetEnergy.h`, `SheenLobe.h`, `IridescenceLobe.h`, `NormalFromMap.h`
(a lit model with energy compensation, sheen, iridescence); `LightVisibilityStage` with ONE
2048 px shadow of the sun and no cascade; `subjectBindings.msl: Light items[16]` walked per
pixel (board:2128 measured it); `Scenario::Weather` (`Haze`, cloud layers, wind);
`Scenario::Exposure {ApertureFStops, ShutterS, SensitivityIso}` "as Filament's
`Camera::setExposure` takes it"; no sun ephemeris, no moon, no stars, no auto-exposure, no
wetness, no season, no navmesh, no crowd, no mind, no audio synthesis, no profiler hook.

---

## 1. Realistic materials: parameterised, physically based, weathered

### 1a. Who has done it and proven it

| body | model | proof |
|---|---|---|
| **Disney** (Burley 2012) | 10 parameters: baseColor, subsurface, metallic, specular, specularTint, roughness, anisotropic, sheen, sheenTint, clearcoat, clearcoatGloss; GGX/GTR lobes, Burley diffuse | Wreck-It Ralph and every Disney feature since; fit against MERL's 100 measured BRDFs |
| **Filament** (Google, Apache-2.0) | Lit = baseColor, metallic, roughness, reflectance (F0 = 0.16 r² for dielectrics, 4 % at r = 0.5), clearCoat + clearCoatRoughness + clearCoatNormal, anisotropy + anisotropyDirection, sheenColor + sheenRoughness, subsurfaceColor/thickness, emissive, ambientOcclusion; Cloth and Subsurface models beside it; energy compensation after Fdez-Agüera 2019 | ships on phones (the target's class); `docs/Filament.md.html` derives every term |
| **Unreal Substrate** (UE 5.4+, source readable under UE EULA, not OSI) | a material is a graph of SLABS (principled BSDF with physical units) combined by Vertical Layer / Horizontal Blend; GBuffer budget `r.Substrate.BytesPerPixel` = 80 default, 3-4 layers per pixel; "Use Parameter Blending" collapses to 28 B/px; mobile: single slab | Fortnite, UE5 titles |
| **Call of Duty: Infinite Warfare** (Drobot 2017) | multilayer materials for Forward+: each layer a simple BRDF/BSSRDF plus thickness, scattering, absorption; car paint, lacquer, ice | shipped at 60 fps on PS4 |
| **Call of Duty: WWII** (Chan 2018) | diffuse BRDF with multiscattering Lambertian microfacets; mip-mapping normal+gloss by average normal length; auto cavity maps | shipped on PS4 |
| **The Order: 1886** (Neubelt & Pettineo 2013) | offline layer compositing into parameter maps + runtime layer blending | shipped on PS4 |
| **Gears of War 4** (Penty & Wong 2017) | layered material system at 60 fps | shipped |
| **Forza Horizon 4** (GDC 2019, Liu) | Physically-Based Calibration: spectrophotometer-matched car paint for ~900 real paints | shipped |

### 1b. Readable code

| repo | licence | read this |
|---|---|---|
| `google/filament` | Apache-2.0 | `shaders/src/surface_brdf.fs` (D_GGX, V_SmithGGXCorrelated, F_Schlick, D_Charlie, V_Neubelt, Fd_Burley, Fd_Lambert), `surface_shading_model_standard.fs` (`isotropicLobe`, `anisotropicLobe`, `clearCoatLobe`, `sheenLobe`, stacking order clear coat > sheen > specular > diffuse), `surface_shading_model_cloth.fs`, `surface_shading_model_subsurface.fs`, `surface_light_indirect.fs` (DFG LUT, energy compensation `1 + f0 (1/dfg.y - 1)`), `docs/Materials.md.html` (`specularAntiAliasing`, variance 0.15 / threshold 0.2 defaults; `quality: low|normal|high`; `specularAmbientOcclusion` off on mobile by default) |
| `EpicGames/UnrealEngine` | UE EULA (readable, not redistributable) | `Engine/Shaders/Private/Substrate/*.ush` -- the slab evaluation and the closure packing. Cite, never copy |
| `ebruneton/clear-sky-models` | BSD-3 | not materials; listed under 2 |
| `GameTechDev/XeGTAO` | MIT | GTAO, if a screen-space AO is wanted beside the sky irradiance |

### 1c. Papers

| paper | where |
|---|---|
| Burley, "Physically Based Shading at Disney", SIGGRAPH 2012 course | disneyanimation.com/publications/physically-based-shading-at-disney |
| Lagarde & de Rousiers, "Moving Frostbite to PBR" v3.2, SIGGRAPH 2014 | seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf |
| Fdez-Agüera, "A Multiple-Scattering Microfacet Model for Real-Time IBL", JCGT 8(1) 2019 | jcgt.org/published/0008/01/03 |
| Kulla & Conty, "Revisiting PBS at Imageworks", SIGGRAPH 2017 course | blog.selfshadow.com/publications/s2017-shading-course |
| Belcour, "Efficient Rendering of Layered Materials using an Atomic Decomposition with Statistical Operators", TOG 2018 | doi 10.1145/3197517.3201289 |
| Weidlich & Wilkie, "Arbitrarily Layered Micro-Facet Surfaces", GRAPHITE 2007 | doi 10.1145/1321261.1321292 |
| Drobot, "Practical Multilayered Materials in CoD:IW", SIGGRAPH 2017 | blog.selfshadow.com/publications/s2017-shading-course/drobot/s2017_pbs_multilayered.pdf |
| Chan, "Material Advances in CoD:WWII", SIGGRAPH 2018 | advances.realtimerendering.com/s2018/MaterialAdvancesInWWII.pdf |
| Kaplanyan et al., "Filtering Distributions of Normals for Shading Antialiasing", HPG 2016; Tokuyoshi & Kaplanyan, "Improved Geometric Specular Antialiasing", I3D 2019 | the specular AA Filament implements |
| Dorsey & Hanrahan, "Modeling and Rendering of Metallic Patinas", SIGGRAPH 1996 (layers + coat/erode/polish operators, Kubelka-Munk) | graphics.stanford.edu/papers/patina |
| Chen et al., "Visual Simulation of Weathering by gamma-ton Tracing", TOG 24(3) 2005 | doi 10.1145/1073204.1073321 |
| Dorsey, Rushmeier, Sillion, *Digital Modeling of Material Appearance*, Morgan Kaufmann 2008, ch. "Aging and Weathering" | book |
| Lagarde, "Water drop 3a/3b -- Physically based wet surfaces", 2013 (porosity, darkening, roughness drop) | seblagarde.wordpress.com/2013/03/19 and /2013/04/14 |

### 1d. What the target can carry, CPU vs GPU

| thing | where | why |
|---|---|---|
| lit model: GGX + Smith-correlated V + Schlick F + Burley/Lambert diffuse + energy compensation | GPU, per pixel | ~40 ALU per light; Filament ships it on Adreno/Mali |
| clear coat | GPU, per pixel, VARIANT | "effectively doubles the cost of specular" (Filament docs) -- a shader variant, never a branch in the uber-shader |
| sheen (cloth), anisotropy | GPU, variants | same argument |
| Substrate-style N slabs per pixel | NOT here | 80 B/px GBuffer at 921 600 px = 74 MB written + read per frame against 60 GB/s = 2.5 ms of pure bandwidth before lighting. Unreal itself drops to a single slab on mobile. Decided: ONE slab per pixel, layering resolved at PARAMETER level (Substrate's "parameter blending", Drobot's medium stack collapsed to one BRDF) |
| weathering (dirt, rust, moss, patina) | COMPUTE BAKE per piece at piece time (revised: was "CPU/GPU precompute"), then a MASK sampled per pixel | gamma-ton lite is embarrassingly parallel: one thread per texel of the piece's mask, N rays toward the sky hemisphere against the piece's own height/depth, plus cavity from the AO term -- read the piece once, write the mask once. The numpy version in the lab is the oracle; the compute bake is compared against it. At runtime the mask blends layer PARAMETERS (albedo, roughness, normal); Lagarde's porosity is one such parameter |
| wetness | GPU, per pixel, driven by one scalar per material class; the scalar table lives in a STORAGE BUFFER a compute pass updates when the weather changes, never a per-frame upload | Lagarde 3b: albedo darkening by porosity, roughness -> ~0.05..0.2 wet, F0 -> water's 0.02 over the surface; puddles are a height-field threshold (his "type 4") computed ONCE per weather change by a compute pass over the ground height field |
| triplanar + detail maps | GPU, per pixel | 3 samples instead of 1; use ONLY on ground and procedural pieces with no UVs, and cap the detail octave at 2 |
| material LOD | the PIPELINE variant is a CPU bind (a pipeline is an SDL object); the per-INSTANCE rung index and the visible-instance list come from a compute cull that writes indirect draws, one draw per (pipeline, rung) bucket | Unreal `r.MaterialQualityLevel` / QualitySwitch, Filament `quality:` flag for the variant; Haar & Aaltonen, "GPU-Driven Rendering Pipelines" (SIGGRAPH 2015, Ubisoft Anvil/Dunia -- talk public, code not) for the cull-then-indirect shape |
| GPU-driven piece drawing (board:2122's arena) | COMPUTE: frustum + Hi-Z cull against `DepthPyramidStage`'s pyramid, rung pick by geometricError, PREFIX-SUM compaction into `SDL_GPUIndexedIndirectDrawCommand` buffers per bucket; CPU issues one `SDL_DrawGPUIndexedPrimitivesIndirect` per bucket | the CPU never walks pieces per frame; determinism: compaction by scan keeps instance order, an atomic-append list would be completion-ordered and is forbidden (fourth invariant) |

Budget [SET], derived from the sibling report's frame split and Filament's Pixel measurements: opaque lit pass at 720p including ONE directional + clustered lights + IBL: 3.0 ms p95; clear coat / anisotropy variants may add 0.5 ms on the pixels that carry them (cars, glass), never on the ground; compute cull + compaction 0.15 ms [SET] for 20 000 instances. Bandwidth rule, unchanged by compute: a G-buffer, if one is ever kept, is at most 16 B/px (15 MB, 0.5 ms of write+read). Compute does NOT buy a fat G-buffer -- a compute lighting pass still has to READ it; what compute buys is passes that read a full-res buffer ONCE and write something SMALL (a histogram, a min/max, a light list, an indirect buffer).

### 1e. Lab experiments

| question | inputs | solution | PROOF | to C++ |
|---|---|---|---|---|
| does the tree's lit model conserve energy over roughness x F0 x view angle? | `MetalRoughBrdf.h`/`MicrofacetEnergy.h` rewritten in numpy; a white furnace | integrate BRDF·cos over the hemisphere (2 000 stratified samples) per (roughness, F0, θ) | albedo ≤ 1 everywhere and ≥ 0.98 for F0 = 1 with compensation on; negative control: compensation off -> albedo at roughness 1 falls to ~0.6 (Kulla-Conty's known loss) and the case goes RED | the DFG LUT values and the compensation term become a `static_assert`-able table |
| does the C++ BRDF match the Python to the bit? | `make shots` readback of a sphere at 8 roughness values under one light | compare per-pixel radiance against numpy | max abs error < 1/255 in linear; negative control: swap V for the uncorrelated Smith and the error exceeds it | the case |
| how much does wetness change? | Lagarde's porosity table; Open-Meteo `precipitation` for a place and hour | darkening = f(porosity, albedo); roughness drop | wet albedo < dry for every porosity > 0 and the ratio matches Lagarde's fit within 5 %; negative control: porosity 0 (metal) -> no darkening | the wetness function, one scalar per material class |
| where does dirt settle on a piece? | a piece mesh from `TilePieces` (board:2122); rain from +y; AO | gamma-ton lite: N particles from the sky, deposit where they stop; cavity term from AO | mask ∈ [0,1], sums to the emitted mass; negative control: rain from -y deposits on ceilings, RED | the mask baker on the IO/compute pool at piece build |
| specular AA variance/threshold on the tree's geometry | a brushed cylinder at 720p, camera receding | Tokuyoshi 2019 kernel | shimmer (frame-to-frame pixel variance) falls > 10x with AA on; negative control: threshold 0 -> shimmer returns | the two constants |

### 1g. Requirements

- One material schema with Filament's names (`baseColor`, `metallic`, `roughness`, `reflectance`, `clearCoat`, `sheenColor`, `anisotropy`, `emissive`) plus outshine's own `porosity` (wetness) and `wear` (weathering mask): the door already speaks Filament, so the material block keeps its keys
- Variants compiled per rung, chosen on the CPU; the fragment shader carries no `if (hasClearCoat)`
- Weathering masks are built at piece time from the piece's own geometry and the sun/rain directions the ephemeris (section 2) gives; a mask is deterministic given the seed (board:2098)
- Budget: 3.0 ms opaque lit at 720p p95 [SET]; textures via a 16 MB residency (the sibling report's number); no per-pixel layer loop

---

## 2. Day-dependent lighting: the sun where the clock puts it

### 2a. Who has done it and proven it

| body | what | proof |
|---|---|---|
| **Unreal** `SunPosition` plugin + `SkyAtmosphere` (Hillaire) | `USunPositionFunctionLibrary::GetSunPosition(lat, lon, tz, dst, y, m, d, h, min, s)`; `SunSky` actor binds the directional light to it; atmosphere from Hillaire 2020 | every UE5 outdoor title; the archviz standard |
| **Frostbite** (Lagarde 2014) | physical light units (lux, lumen, candela, EV100), exposure from the camera triangle, sun 100 000 lux, sky ~20 000-30 000 lux, moon 0.1-0.3 lux | Battlefield, Need for Speed |
| **Rockstar RDR2** (Bauer, SIGGRAPH 2019 Advances) | voxelised scattering for sky, clouds and fog; sky irradiance probe grid; fully dynamic day | shipped on PS4 at 30 fps |
| **Cesium** | `Georeference` + a sun/moon ephemeris (`Simon1994PlanetaryPositions`), `SunLight`, `Scene.light` | every Cesium client |
| **Filament** | `LightManager::Builder(Type::SUN).intensity(100000 lux).sunAngularRadius(0.545).sunHaloSize/Falloff`; `IndirectLight` 30 000 lux default; `Exposure.cpp` | phones |

### 2b. Readable code

| repo | licence | read this |
|---|---|---|
| `cosinekitty/astronomy` (Astronomy Engine) | MIT | `source/c/astronomy.c` (one file): `Astronomy_SunPosition`, `Astronomy_Equator`, `Astronomy_Horizon`, `Astronomy_Illumination` (moon phase and magnitude), `Astronomy_MoonPhase`; truncated VSOP87 + NOVAS-derived lunar theory; **±1 arcmin, validated against NOVAS and JPL Horizons, 1700-2200**. This is the ephemeris to take: one C file, no dependency, MIT |
| **NREL SPA** (Reda & Andreas 2004) | NREL licence: internal, non-commercial, NOT redistributable | ±0.0003° from -2000 to 6000; cite as the TRUTH oracle in the lab (download it there), never ship it |
| Grena 2012 "Five new algorithms", `klausbrunner/solarpositioning` (Java, MIT) `Grena3.java` | MIT | 0.01° for 2010-2110 in ~30 lines; the cheapest sun-only option |
| `libnova` | LGPL | Meeus-based; LGPL is heavier than needed |
| `sebh/UnrealEngineSkyAtmosphere` | MIT (Epic) | the paper's demo: `Resources/...` shaders with the four LUTs and the multi-scattering approximation |
| `ebruneton/precomputed_atmospheric_scattering` | BSD-3 | `atmosphere/functions.glsl`: `ComputeTransmittanceToTopAtmosphereBoundary`, `ComputeSingleScattering`, `ComputeMultipleScattering`, `GetSkyRadiance`, `GetSunAndSkyIrradiance`; `model.cc` precompute. Ozone and custom density profiles. The TRUTH the tree's `Medium*` stages are measured against |
| `PetrVevoda/pragueskymodel` | Apache-2.0 | `src/PragueSkyModel.{h,cpp}`; fitted dataset 103 MB (ground) to 2.2 GB (full). A second oracle for sky RADIANCE incl. below-horizon sun |
| ArHosekSkyModel (`ebruneton/clear-sky-models/atmosphere/model/hosek`) | BSD-3 | the analytic sky the whole industry compares against |
| `google/filament` | Apache-2.0 | `filament/src/Exposure.cpp`: `ev100 = log2(N²/t · 100/S)`, `exposure = 1/(1.2 · 2^ev100)`, `luminance = 2^(ev100-3)`, `illuminance = 2.5 · 2^ev100` (K = 12.5, C = 250); `filament/src/Froxelizer.{h,cpp}`: `FROXEL_SLICE_COUNT = 16`, `FROXEL_BUFFER_MAX_ENTRY_COUNT = 8192`, 256 lights max, z slices `zFar · exp2((i-n)·log2(zFar/zNear)/(n-1))`, froxel side rounded to 8 px, first slice 5 m, last 100 m; `ShadowMapManager.cpp`, `LightManager.h::ShadowOptions` (`mapSize 1024`, `shadowCascades 1..4`, `cascadeSplitPositions {0.125, 0.25, 0.5}`, `constantBias 1 mm`, `normalBias 1.0`, `shadowNearHint 1`, `shadowFarHint 100`, VSM/EVSM/PCF/DPCF/PCSS) |
| `astronexus/hyg` (Codeberg) | CC BY-SA 4.0 | HYG 4.2: ~120 000 stars (Hipparcos + Yale BSC5 + Gliese) with V mag, B-V colour, RA/Dec. Yale BSC5 alone (9 110 stars to mag 6.5, Hoffleit 1991) is on `tdc-www.harvard.edu/catalogs/bsc5.html`; its licence is not stated (unverified), HYG's is |

### 2c. Papers

| paper | where |
|---|---|
| Hillaire, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique", CGF 39(4), EGSR 2020 | sebh.github.io/publications/egsr2020.pdf; LUTs transmittance 256x64, multi-scattering 32x32, sky-view ~200x100, aerial perspective 32x32x32 (unverified this session -- the PDF exceeded the fetch limit; the tree's `Medium*` stages already carry these shapes) |
| Bruneton & Neyret, "Precomputed Atmospheric Scattering", EGSR 2008; Bruneton 2017 "a new implementation" | ebruneton.github.io/precomputed_atmospheric_scattering |
| Hosek & Wilkie, "An Analytic Model for Full Spectral Sky-Dome Radiance", SIGGRAPH 2012 | cgg.mff.cuni.cz/projects/SkylightModelling |
| Wilkie et al., "A Fitted Radiance and Attenuation Model for Realistic Atmospheres", TOG 40(4) 2021 | cgg.mff.cuni.cz/publications/skymodel-2021 |
| Reda & Andreas, "Solar position algorithm for solar radiation applications", Solar Energy 76, 2004 | doi 10.1016/j.solener.2003.12.003 |
| Grena, "Five new algorithms for the computation of sun position from 2010 to 2110", Solar Energy 86, 2012 | |
| Meeus, *Astronomical Algorithms* 2nd ed. 1998 | the book everything above derives from |
| Krisciunas & Schaefer, "A Model of the Brightness of Moonlight", PASP 103, 1991 | doi 10.1086/132921; moonlit sky brightness vs phase, zenith distances, extinction; 8-23 % accuracy |
| Jensen et al., "A Physically-Based Night Sky Model", SIGGRAPH 2001 | moon, stars, zodiacal light, airglow |
| Zhang et al., "Parallel-Split Shadow Maps", VRCIA 2006 (practical split, λ = 0.5) | doi 10.1145/1128923.1128975 |
| Lauritzen, Salvi, Lefohn, "Sample Distribution Shadow Maps", I3D 2011 | doi 10.1145/1944745.1944761 |
| Jimenez et al., "Practical Real-Time Strategies for Accurate Indirect Occlusion" (GTAO), 2016: 0.5 ms on PS4 at 1080p (measured) | activision.com/cdn/research |

### 2d. The physical ladder, and the numbers with their origin

| source | illuminance | origin |
|---|---|---|
| sun, clear, high | 32 000-100 000 lux | measured, standard tables |
| overcast day | 1 000 lux | measured |
| sunrise/sunset, clear | 400 lux | measured |
| civil twilight limit | 3.4 lux | measured |
| full moon, clear | 0.05-0.3 lux (0.25 typical) | measured, Krisciunas & Schaefer |
| moonless clear sky with airglow | 0.002 lux; zenith 22.0 mag/arcsec² = 1.7e-4 cd/m² (`L = 10.8e4 · 10^(-0.4 m)`) | measured |
| street lighting, EN 13201 | M1 2.0 cd/m² road luminance ... M6 0.3; P1 15 lux ... P6 2 lux; a 100 W LED cobra head 15 000 lm; poles 8-12 m, spacing 25-50 m | standard |

Exposure: EV100 for sunny 16 is 15; the tree's `Scenario::Exposure` triangle maps through Filament's formulas above. Auto-exposure (Narkowicz 2016, Tardif's histogram) is a 128-bin luminance histogram in one compute dispatch over a 1/4-res buffer with a temporal adaptation constant; on a phone the dispatch is ~0.1 ms. Filament has no auto-exposure; Unreal's is histogram-based -- take Unreal's shape.

### 2e. What the target can carry, CPU vs GPU

| thing | where | cost |
|---|---|---|
| ephemeris (sun, moon, phase, stars' rotation) | CPU, once per simulated minute; a `constexpr`-able double path | µs |
| atmosphere LUTs (Hillaire) | GPU compute, transmittance and multi-scatter only when the sun moves > 0.25° or the haze changes; sky-view and aerial perspective every frame | Hillaire reports the whole pipeline scaling to phones; the paper's per-LUT costs were not re-read (unverified). Budget [SET] 0.6 ms total at 720p |
| sun shadow | GPU, CASCADED: 3 cascades of 1024² [SET] (3 MB depth, not the 4x2048² Killzone spent). Split: SDSM (Lauritzen 2011) -- a compute reduction of last frame's depth to min/max (reads 3.7 MB once, writes 8 bytes) fits the cascades to the depth the frame actually holds; Zhang's practical split λ = 0.5 over {0.05, 0.2, 1.0} of a 300 m far [SET] is the fallback for the first frame and for a reduction that lands late. Caster culling per cascade is a compute pass writing three indirect buffers; PCF 3x3 near, 1 tap far | Revised: SDSM was implicitly dismissed in the first draft because it needs a depth reduction, which is compute; Lauritzen measured it beating hand-tuned PSSM on every scene. Unreal mobile default is 2 cascades (`r.Shadow.CSM.MaxMobileCascades`); budget [SET] 1.5 ms for the three depth passes + 0.3 ms filtering + 0.05 ms reduction; whether SDSM lets the count fall to 2 (-0.5 ms) is a measurement, section 6 |
| punctual lights | COMPUTE froxelises (revised: was "CPU froxelises"): one thread per froxel loops the lights IN DECLARATION ORDER and writes its list -- 8192 froxels x 256 lights = 2 M sphere-frustum tests, < 0.1 ms; the fragment stage reads the list from a storage buffer (`SDL_BindGPUFragmentStorageBuffers`) | Filament does this on the CPU (~0.2 ms/frame, its own timing) because GL ES 2 had no compute; Unreal's `LightGridInjection` does it on the GPU. The per-froxel loop over lights is what keeps the list order declared rather than completion-ordered; an atomic-append variant is refused. Budget [SET] 1.0 ms for 100 lamps + 8 shadowed spots at 256² each, CPU share 0 |
| moon as a second directional at 0.25 lux with the SAME shadow path (one cascade) | GPU | at night the sun cascade is free; reuse it |
| stars | GPU: 9 110 point sprites from BSC5 rotated by sidereal time, magnitude -> luminance `2.5e-? · 10^(-0.4 m)`; hidden by exposure in daylight | ~0.05 ms |
| light pollution / sky glow | sky-view LUT gets an additive term from VIIRS radiance at the place (section 3 provider) | 0 ms extra |
| auto-exposure | GPU-RESIDENT: a compute histogram (128 integer bins, atomics on integers commute so the result is bit-deterministic) over a quarter-res luminance, a second dispatch reduces it to one EV100 in a 16-byte storage buffer the tonemap pass reads; NO CPU readback (revised) | 0.1 ms; the CPU learns the EV only if a client asks (`SDL_DownloadFromGPUBuffer`), for telemetry |
| stars culled by magnitude vs exposure | a compute pass over the 9 110-entry catalogue writes a visible list + an indirect draw; a star below the exposure floor is never rasterised | < 0.02 ms |
| what is NOT here | ray-traced shadows/reflections, tile-shaded deferred lighting, hardware tessellation for terrain or displacement, and GPU TIMESTAMPS (SDL_GPU has no query API: pass costs are measured by fence-bracketed submissions or by the difference method, toggling a pass and reading the frame time) | -- |

The rule that avoids "a hand-set sun disagrees with its shadows": the scenario declares a PLACE and a CLOCK; the ephemeris derives the sun's direction, and the shadow, the sky LUT, the specular highlight and the shadow's colour (sky irradiance) all read that ONE direction from one frame uniform. A `Scenario::Weather::SunDir` does not exist and must not be added; `Exposure` is the only hand-set light number, and it is a camera number.

### 2f. Lab experiments

| question | inputs | solution | PROOF | to C++ |
|---|---|---|---|---|
| is the tree's sun within 1 arcmin of NREL SPA? | place (OldTown, Jura, Kaiserberg) x 365 days x 24 h; SPA (downloaded into `$TMPDIR`, licence honoured) | run `astronomy.c` and SPA in Python (ctypes) | max angular error < 1 arcmin (Astronomy Engine's own bound); negative control: Grena3 in 1900 exceeds its 2010-2110 window and diverges | `astronomy.c` vendored MIT, the case as SPEC |
| do the `Medium*` LUTs match Bruneton's reference? | the tree's atmosphere parameters (Earth.h); Bruneton's `functions.glsl` transcribed to numpy | compute transmittance and single+multi scatter for 64 (h, μ, μs) samples | relative error < 2 % away from the horizon, < 10 % within 2° of it (Bruneton's own tolerance); negative control: drop ozone and the blue at sunset moves > 10 % | tolerance quoted in the case that compares `make shots` readback of the sky |
| does the cascade split hold the sample density? | camera on the OldTown street, shadow far 300 m; the depth buffer from `make shots` readback | compute texel/pixel ratio per depth for λ ∈ {0, 0.5, 1} AND for SDSM bounds (min/max of the read-back depth) | the ratio's max/min over the frustum is smallest at λ ≈ 0.5 among PSSM (Zhang) and > 4 at λ = 0 (negative control); SDSM's min density ≥ 2x PSSM's on the street (Lauritzen's claim); negative control: feed the reduction the far plane -> equals PSSM | λ and the three fallback splits `constexpr`; the min/max reduction as a compute pass |
| is the histogram exposure deterministic and equal to numpy? | the quarter-res luminance of one frame read back | numpy 128-bin histogram vs the compute histogram read back via `SDL_DownloadFromGPUBuffer`, 10 runs | bins EQUAL (integers), 10/10 runs identical bytes; negative control: a float-atomic mean-luminance variant differs across runs | the two dispatches |
| is the night sky luminance right? | date, place, moon phase from the ephemeris; Krisciunas & Schaefer | model moonlit zenith luminance | zenith at full moon within the model's 23 % band; moonless 1.7e-4 cd/m² ± 30 %; negative control: moon below horizon gives the moonless value | the moon light's intensity function |
| how many froxels at 720p, and is the GPU list the CPU list? | 921 600 px, 8 px rounding, 16 slices; 100 declared lamps | Filament's formula in numpy for the grid; numpy bins the lamps per froxel in declaration order; the compute pass's buffer read back | froxel count ≤ 8192 and slices cover 5-100 m; the GPU per-froxel lists EQUAL numpy's, order included, over 10 runs; negative control: 4096 entries at 4 px froxels overflows; second negative control: an atomic-append binning gives the same SETS but a different ORDER between runs, RED | the constants and the per-froxel loop |

### 2g. Requirements

- A CLOCK and a PLACE in the scenario; the sun, moon and stars are DERIVED, never declared
- Providers: none for the ephemeris (pure computation); weather from section 3 feeds `Haze`, cloud cover and precipitation
- Budgets [SET] at 720p, p95: atmosphere 0.6 ms, sun cascades 1.85 ms (3 depth passes 1.5 + filtering 0.3 + SDSM reduction 0.05), punctual clustered 1.0 ms (binning now < 0.1 of it, on the GPU), auto-exposure 0.1 ms; sum 3.55 ms GPU of a 16.7 ms frame; CPU frame-thread share of all of it: the ephemeris, three cascade matrices and one indirect-draw issue per bucket -- under 0.1 ms [SET], where the first draft had ~0.5 ms of CPU light binning and piece/caster walks
- Memory: LUTs < 2 MB; cascades 3 MB; shadow atlas for spots 4 MB
- Determinism: the ephemeris is double, no `std::chrono` at run time -- the simulation clock is the input; the froxeliser's light order is the declaration order

---

## 3. Seasons and geo-dependence

### 3a. Who has done it and proven it

| body | what | proof |
|---|---|---|
| **Forza Horizon 4** (Playground, 2018) | four seasons over one map: puddles and wetness in autumn/spring, frozen water in winter, leaf-off trees; materials via Substance graphs with a season parameter | shipped; GDC 2019 covered only the paint calibration, the seasons pipeline has no public talk (unverified beyond press) |
| **Assassin's Creed III** (AnvilNext, 2012) | seasons with snow deformation (vertex displacement tracking characters), frozen rivers | shipped |
| **Assassin's Creed Shadows** (2025) | dynamic seasons + `Atmos` weather system: rain, wind, snow, GI change per season | shipped; GDC 2025 "Rendering AC Shadows" |
| **RDR2** | region-locked snow, deformation, dynamic weather | shipped |
| **CARLA** | per-country signs via OpenDRIVE `@country` on signals; SUMO `--lefthand` | shipped simulator |
| **OSM2World / blosm** | roof and facade by OSM tags, region only through tags | shipped |

### 3b. Open data: what answers the question, licence, API

| question | provider | resolution / cadence | licence | API |
|---|---|---|---|---|
| leaf-on / leaf-off / autumn colour dates | **MODIS MCD12Q2 v061** (Greenup, MidGreenup, Maturity, Peak, Senescence, MidGreendown, Dormancy as days since 1970; up to 2 cycles) | 500 m, yearly 2001-2020 | NASA, no restrictions | AppEEARS point/area API (Earthdata login) |
| greenness now | MODIS MOD13Q1 NDVI/EVI | 250 m, 16-day | NASA | AppEEARS |
| ground truth for colour curves | **PhenoCam v2/v3** (393 sites, GCC time series) | site, daily | CC BY | phenocam.nau.edu/api |
| climate zone | **Köppen-Geiger** Beck 2018/2023 (1901-2099) | 1 km | CC BY 4.0 | gloh2o.org/koppen GeoTIFF |
| climatology (bioclim) | WorldClim 2.1 | 1 km | CC BY-SA 4.0 (v1 stated; v2.1 unverified) | download |
| hour weather, past and forecast | **Open-Meteo** (ERA5 0.25° from 1940, ERA5-Land 0.1° from 1950, IFS 9 km from 2017; forecast from ICON/GFS/...) hourly: `temperature_2m`, `precipitation`, `rain`, `snowfall`, `cloud_cover_low/mid/high`, `weather_code`, `wind_speed_10m`, `wind_direction_10m`, `shortwave/direct/diffuse_radiation`, `soil_moisture_0_to_7cm`, `snow_depth` (ERA5-Land only, not ERA5), `is_day`; forecast adds `visibility` | hourly | CC BY 4.0 data; AGPL server; free non-commercial ≤ 10 000 calls/day | `archive-api.open-meteo.com/v1/archive`, `api.open-meteo.com/v1/forecast` |
| airport observation (visibility, cloud base in hundreds of ft, FEW/SCT/BKN/OVC) | **AWC METAR/TAF** | per station, minute | US government, public | `aviationweather.gov/api/data/metar?ids=EDDF&format=json`, 100 req/min |
| snow cover today | MODIS MOD10A1 (NDSI 0-100) | 500 m daily | NASA | NSIDC/AppEEARS |
| snow depth | ERA5-Land `sd` via Open-Meteo `snow_depth` | 9 km hourly | Copernicus/CC BY | as above |
| land cover | ESA WorldCover 2021 v200 (11 classes) / Dynamic World (9 classes, near-real-time) | 10 m | CC BY 4.0 | Zenodo 7254221 / GEE |
| canopy height | ETH Global Canopy Height 2020 (10 m), Meta/WRI 1 m | 10 m / 1 m | CC BY 4.0 | AWS |
| biome / ecoregion | RESOLVE Ecoregions 2017 (846 ecoregions, 14 biomes) | polygon | CC BY 4.0 | shapefile |
| leaf form & habit (needle/broad x evergreen/deciduous) | Ma et al. 2023, Nature Plants 9:1795 | 1 km global maps (download location unverified) | CC BY (paper) | figshare |
| tree species per country | GlobalTreeSearch (BGCI) via GBIF; species occurrences GBIF API (CC0/CC BY per dataset) | country / point | CC BY / CC0 | api.gbif.org |
| driving side | OSM `driving_side=left|right` on the COUNTRY RELATION; per-way only for exceptions; 75 LHT / 165 RHT | country | ODbL | Overpass; fallback table (Wikipedia list) |
| default speed limits | OSM wiki "Default speed limits" + `westnordost/osm-legal-default-speeds` (per-country table, `maxspeed:type=DE:urban`) | country/road class | ODbL / MIT (lib) | JSON in that repo |
| sign catalogue per country | OSM `traffic_sign=DE:274-50` (country prefix + national id); SVGs: Wikimedia Commons "SVG road signs by country" (DE: § 5 UrhG public domain; VzKat 2017 images free from BASt) | -- | PD / CC per file | Commons |
| marking colour convention | MUTCD (yellow centre, white lane; US, CA, AU) vs Vienna Convention (white or yellow; EU); hybrids in LATAM and parts of Asia | country | -- | a table, hand-written from the treaty lists |
| building height where OSM lacks it | GHS-BUILT-H R2023A (100 m ANBH), GlobalBuildingAtlas LoD1 (2.75 G buildings, 3 m height maps; polygons ODbL, LoD1/height CC BY-NC 4.0 -- NON-COMMERCIAL, a finding), Microsoft GlobalML (ODbL, 1.4 G, 174 M heights), Google Open Buildings v3 (CC BY 4.0, 1.8 G, Africa/S+SE Asia/LATAM) | -- | see cells | download |
| building attributes (material, floors, age, type) | Overture Buildings (`height`, `num_floors`, `roof_shape`, `roof_material`, `facade_material`, `facade_color`; ODbL because OSM-derived); **OpenFACADES** (Liang & Biljecki 2025, ISPRS 230; VLM over Mapillary + OSM isovists; 86-96 % type accuracy over 10-12 classes; 1.2 M images / 0.5 M buildings) | building | ODbL / paper CC BY | Overture STAC/parquet; github seshing/OpenFACADES |
| night lights (sky glow, "is this street lit") | VIIRS VNL V2.2 annual (EOG, Colorado School of Mines; no restrictions) / NASA Black Marble VNP46A4 (discontinued after Jan 2025; v1 archive stays) | 500 m / 15 arcsec | CC BY 4.0 / NASA | download |
| street lamps | OSM `highway=street_lamp` (235 528 uses at last count; `lamp_type`, `light:colour`, `light:count`, `lamp_mount`), `lit=yes` on ways | node | ODbL | Overpass |

### 3c. Papers

| paper | what it gives |
|---|---|
| Delpierre et al. 2009, Agric. For. Meteorol. 149:938 (doi 10.1016/j.agrformet.2008.11.014) | senescence = cold-degree-day sum gated by photoperiod (the "DM" model); r² 0.74-0.83 for beech/oak over France |
| Jeong & Medvigy 2014, Global Ecol. Biogeogr. 23:1245 (doi 10.1111/geb.12206) | continental autumn coloration prediction from temperature + daylength |
| Beck et al. 2018 Sci. Data 5:180214; 2023 Sci. Data 10:724 | the Köppen maps above |
| Brown et al. 2022 Sci. Data 9:251 (Dynamic World); Zanaga et al. 2022 (WorldCover) | land cover |
| Ma et al. 2023 Nature Plants 9:1795; Liang et al. 2022 Nat. Ecol. Evol. 6:1423 | leaf form/habit; species richness at 0.025° |
| Elvidge et al. 2021 Remote Sensing 13:922; Román et al. 2018 RSE 210:113 | night lights |
| Olson et al. 2001 BioScience 51:933; Dinerstein et al. 2017 BioScience 67:534 | ecoregions |
| Lagarde 2013 "Water drop 2b" | puddles as height-field threshold; four wetness states |
| Poggenhans et al. 2018 ITSC (Lanelet2) | lane-level map with per-country traffic rules |

### 3d. What the target can carry, CPU vs GPU

Everything in this section is a PROVIDER answer (one right value per place and date) consumed on the CPU at PRELOAD and turned into a handful of per-frame scalars; nothing here costs the GPU beyond what sections 1-2 already pay.

| thing | where | form |
|---|---|---|
| season state of foliage | CPU, once per simulated day: MCD12Q2's seven dates for the place -> phase ∈ {dormant, greenup, mature, senescing}; colour from a per-biome curve calibrated on PhenoCam GCC. The BLEND into every flora instance's material row is a compute pass over the instance parameter storage buffer (one thread per instance, reads the phase, writes albedo/roughness/coverage), dispatched on change, never per frame | one scalar + one colour per flora kind on the CPU; the per-instance rows on the GPU |
| snow | CPU: `snow_depth` (ERA5-Land) + MOD10A1 NDSI -> coverage ∈ [0,1] per place; a compute pass over the ground normal/height field writes a coverage mask (`n·up > cos θ`, height-dependent) ONCE per coverage change; the ground shader samples it | one scalar in, one R8 mask per tile out (256 KB at 512² per 500 m tile) |
| wetness / puddles | CPU: hours since last `precipitation` > 0.5 mm -> wetness with a drying time constant per material class; the per-class table is a storage buffer updated by compute; puddles = a compute threshold over the ground height field, once per weather change | one scalar per class; one mask per tile |
| driving side, speed defaults, sign set, marking colour | CPU at preload from the country relation: a `Region` record {side, speedTable, signCatalogue, markingColour} the road generator and the autopilot read | a struct per country, `constexpr` table for the treaty defaults, OSM overrides |
| building style | CPU at preload: Köppen zone x ecoregion x Overture/OSM `building:material`/`roof:shape` statistics per tile -> a style prior the building generator samples (the sibling report's `FacadeStyle`); OpenFACADES is the oracle for the prior, never a runtime dependency | a distribution per tile |
| species | CPU at preload: RESOLVE biome + Ma 2023 leaf form + GBIF top-N species -> the flora catalogue's choice | a weighted list |
| sky glow, lamps | VIIRS radiance -> sky LUT term; OSM lamps -> punctual lights (section 2) with EN 13201 intensities by road class | scalars + a light list |

### 3e. Lab experiments

| question | inputs | solution | PROOF | to C++ |
|---|---|---|---|---|
| does MCD12Q2 predict PhenoCam's colour turn? | MCD12Q2 dates for 20 PhenoCam sites; PhenoCam GCC | compare MidGreendown to the GCC 50 % crossing | median |Δ| < 10 days (the product's own stated accuracy band, unverified); negative control: shuffle sites -> |Δ| > 30 days | the seven dates as the season provider's schema |
| can Delpierre's DM model replace the satellite where 2020 data ends? | Open-Meteo `temperature_2m` hourly + daylength from the ephemeris for the same sites | fit CDD threshold and photoperiod gate per biome | r² > 0.7 against MCD12Q2 Senescence (Delpierre's own range); negative control: photoperiod gate off -> r² drops below 0.5 | a `constexpr` model per Köppen class, used when the provider has no year |
| wetness decay | Open-Meteo precipitation + `soil_moisture_0_to_7cm` for a rainy week | fit a drying time constant per material class against soil moisture as proxy | residual < 15 %; negative control: constant 0 -> instant drying, RED | the time constants |
| is the country record right everywhere? | OSM country relations (Overpass) for all 195 | read `driving_side`, compare to the Wikipedia LHT list | 100 % agreement or a named exception list; negative control: flip one -> RED | the `Region` table |
| does the marking colour follow the treaty? | the MUTCD/Vienna signatory lists; 10 OSM places | per place, predicted centre-line colour vs Mapillary/OSM `lane_markings` where tagged | no contradiction in the sample; negative control: swap the two lists | the table |
| snow blend threshold | MOD10A1 NDSI for Jura in February; a ground normal field | coverage vs slope | coverage falls with slope as the NDSI pixels do on the DEM's aspect map (Spearman > 0.5); negative control: random normals | the cos θ threshold |

### 3g. Requirements

- Providers, all reachable now: Open-Meteo (archive + forecast), AWC METAR, AppEEARS (needs an Earthdata token in config, never in the tree), Köppen GeoTIFF (one-time fetch, cached), WorldCover COGs, VIIRS VNL, Overpass for country relations and lamps
- Every provider answer is CACHED under `$TMPDIR/outshine-lab/` or `build/` with its fetch date; a scenario declares the DATE and the engine fetches the year's phenology once
- Licence findings: GlobalBuildingAtlas LoD1/heights are CC BY-NC (not usable in a product); Overture Buildings and Microsoft footprints are ODbL (share-alike on the DERIVED DATABASE, fine for a renderer); Open-Meteo free tier is non-commercial (a product needs the paid tier or its own ERA5 mirror -- the data itself is CC BY)
- Frame cost 0 ms; the compute passes above run on CHANGE (a season step, a weather step), each reads a tile field once and writes a mask once: [SET] < 0.2 ms GPU per tile per change, amortised to nothing; preload cost per tile [SET] < 50 ms CPU; memory for the region record and the phenology curve < 64 KB, plus one R8 mask per tile per effect

---

## 4. NPC and autonomy

### 4a. Who has done it and proven it

| domain | body | what | proof |
|---|---|---|---|
| driving | **CARLA Traffic Manager** (MIT) | five stages per tick: ALSM (agent lifecycle), `LocalizationStage` (waypoint buffer on the in-memory map), `CollisionStage` (bounding-box path overlap), `TrafficLightStage` (lights, stops, junction priority), `MotionPlanStage` (PID longitudinal + lateral), `VehicleLightStage`; hybrid physics (far vehicles kinematic) | the readable baseline named in CLAUDE.md; used by every CARLA benchmark |
| driving | **CARLA PythonAPI agents** | `basic_agent.py`, `behavior_agent.py` (cautious/normal/aggressive), `local_planner.py` (two PIDs), `global_route_planner.py` (A* over topology) | the same |
| driving | **SUMO** (EPL-2.0) | car-following Krauss (default), IDM, EIDM; lane change LC2013 / MOBIL-derived; `netconvert --tls.guess-signals --tls.join --junctions.join`, right-of-way from edge priority, `--lefthand` | 20 years of traffic research; the netconvert that CARLA's OSM path uses |
| driving | **Autoware universe** (Apache-2.0), **Apollo** (Apache-2.0) | `behavior_path_planner` (behaviour tree of scene modules), `behavior_velocity_planner` (plugins: crosswalk, intersection, stop line), Lanelet2 (BSD) maps; Apollo EM planner (DP path, QP speed), lattice planner | real cars; far heavier than a game needs, cite for the STRUCTURE |
| driving | **Unreal MassTraffic** (City Sample; UE EULA) | ECS vehicles on `ZoneGraph` lanes with `MassTrafficLaneChangingProcessor`, intersections, parked cars; 10 000s at LOD | The Matrix Awakens |
| walking | **Recast/Detour** (zlib) | `Recast` voxelises to a navmesh; `Detour` A* (`dtNavMeshQuery::findPath`), `DetourCrowd` (`dtCrowd::update`, `dtObstacleAvoidanceQuery::sampleVelocityAdaptive` -- a sampled-velocity ORCA-like avoidance), `DetourTileCache` streaming | Unreal's own navmesh (`ARecastNavMesh`, `PImplRecastNavMesh.cpp`), Unity, Godot, O3DE |
| walking | **RVO2** (Apache-2.0, `snape/RVO2`) | ORCA (van den Berg, Guy, Lin, Manocha, ISRR 2011): linear-programming velocity choice per agent, O(neighbours) | the crowd standard; Godot's `NavigationServer` |
| walking | **Assassin's Creed Unity** (GDC 2015, Cournoyer) | 10 000 on screen with 40 real AIs and 120 high-res models; AI LOD with a pool that swaps rungs unnoticed | shipped on PS4 |
| walking | **Hitman: Absolution** (GDC 2012, Fauerby) | 1 200-character interactive crowds at 30 fps on PS3-class | shipped |
| walking | **Unreal MassCrowd** | ZoneGraph lanes, density, waiting slots, StateTree brains, SmartObjects | City Sample |
| animation | **Motion Matching** (Büttner & Clavet, nucl.ai 2015; Clavet GDC 2016) | nearest-frame search over a mocap database on (pose, future trajectory); no state machine | For Honor, The Last of Us Part II |
| animation | **Learned Motion Matching** (Holden et al., TOG 39(4) 2020) | replaces the database with networks: ~600 MB -> ~9 MB | Ubisoft |
| flying | **JSBSim** (LGPL-2.1) | 6-DoF FDM, aircraft as XML (`aircraft/c172x/c172x.xml`: aerodynamics as tabulated functions, `flight_control`, propulsion) | FlightGear, ArduPilot SITL, DARPA ACE |
| flying | Stevens, Lewis, Johnson, *Aircraft Control and Simulation* 3rd ed. 2015 | the 6-DoF equations and the NASA F-16 model tables (NASA TM-2003-212145 has them in MATLAB) | textbook |
| schedules | Skyrim Radiant AI (packages: place, time, duration), Kingdom Come: Deliverance ("Long Distance Move" in the behaviour tree, ~2 400 NPCs in KCD2 with AI LOD -- GDC), Watch Dogs: Legion `Census` (GDC 2021, Dragert: generated demographics -> daily schedule), The Sims (smart objects advertise utility) | daily routine as data on the world, not code in the agent | shipped |
| brains | Isla, "Handling Complexity in the Halo 2 AI", GDC 2005 (behaviour trees); Unreal `AIModule` BehaviorTree (`BTCompositeNode`) and `StateTree` | a tree over a blackboard, ticked at a rate | every AAA since |
| LLM minds | **Generative Agents** (Park et al., UIST 2023; `joonspk-research/generative_agents`, Apache-2.0) | memory stream of natural-language records; retrieval score = recency (exp decay 0.995/hour) + importance (LLM-rated 1-10) + relevance (embedding cosine), all weights 1; reflection; planning. 25 agents x 2 game days cost "thousands of dollars" (2023, GPT-3.5/4) | the paper everyone cites |
| LLM minds | **Voyager** (Wang et al. 2023, TMLR 2024; `MineDojo/Voyager`, MIT) | automatic curriculum, a SKILL LIBRARY of executable code, iterative prompting with environment feedback and self-verification; GPT-4 as a black box | Minecraft tech-tree milestones |
| LLM minds | Inworld, Convai (commercial, not readable) | speech-to-speech under 1 s; Convai claims sub-200 ms pipeline but users report multi-second stalls (2025-26) | products |

### 4b. Readable code, by file

| repo | licence | read this |
|---|---|---|
| `carla-simulator/carla` | MIT | `LibCarla/source/carla/trafficmanager/{ALSM,LocalizationStage,CollisionStage,TrafficLightStage,MotionPlanStage,VehicleLightStage,TrafficManagerLocal}.cpp`, `PIDController.h`, `InMemoryMap.cpp`; `PythonAPI/carla/agents/navigation/{local_planner,global_route_planner,behavior_agent}.py` |
| `eclipse-sumo/sumo` | EPL-2.0 | `src/microsim/cfmodels/MSCFModel_{Krauss,IDM,EIDM}.cpp`, `src/microsim/lcmodels/MSLCM_LC2013.cpp`, `src/netbuild/NBNodeShapeComputer.cpp`, `NBOwnTLDef.cpp` (guessed signal plans), `NBNode::computeLogic` (right of way) |
| `recastnavigation/recastnavigation` | zlib | `Recast/Source/Recast{Rasterization,Region,Contour,Mesh}.cpp`, `Detour/Source/DetourNavMeshQuery.cpp`, `DetourCrowd/Source/{DetourCrowd,DetourObstacleAvoidance,DetourPathCorridor}.cpp` |
| `snape/RVO2` | Apache-2.0 | `src/Agent.cpp` (`computeNewVelocity`: the ORCA half-planes and the 2-D LP), `KdTree.cpp` |
| `orangeduck/Motion-Matching` | MIT | `database.h` (the search), `controller.cpp` (spring damper, inertialisation), the LMM branch |
| `JSBSim-Team/jsbsim` | LGPL-2.1 | `src/FGFDMExec.cpp`, `src/models/FG{Aerodynamics,Propagate,Atmosphere}.cpp`, `aircraft/c172x/c172x.xml`; LGPL means dynamic linking or reimplementation of the 6-DoF core (the equations are Stevens & Lewis's) |
| `movsim/traffic-simulation-de` | GPL (unverified) | IDM + MOBIL in JS; cite the equations, do not copy |
| `fzi-forschungszentrum-informatik/Lanelet2` | BSD-3 | `lanelet2_traffic_rules/` (per-country rule sets), `lanelet2_routing/` |
| `joonspk-research/generative_agents` | Apache-2.0 | `reverie/backend_server/persona/memory_structures/associative_memory.py`, `persona/cognitive_modules/{retrieve,reflect,plan}.py` |
| `MineDojo/Voyager` | MIT | `voyager/agents/{skill,curriculum,critic,action}.py` |
| `EpicGames/UnrealEngine` | UE EULA | `Engine/Source/Runtime/AIModule/.../BehaviorTree/BTCompositeNode.cpp`, `Engine/Plugins/Runtime/StateTree`, `Engine/Plugins/AI/MassCrowd`, `Engine/Plugins/Experimental/MassTraffic` (City Sample) -- cite |

### 4c. Papers

| paper | where |
|---|---|
| Treiber, Hennecke, Helbing, "Congested traffic states in empirical observations and microscopic simulations" (IDM), Phys. Rev. E 62:1805, 2000 | doi 10.1103/PhysRevE.62.1805 |
| Kesting, Treiber, Helbing, "General Lane-Changing Model MOBIL for Car-Following Models", TRR 1999:86, 2007 | doi 10.3141/1999-10 |
| van den Berg, Guy, Lin, Manocha, "Reciprocal n-Body Collision Avoidance", ISRR 2011 | gamma.cs.unc.edu/ORCA |
| Büttner & Clavet, "Motion Matching -- The Road to Next-Gen Animation", nucl.ai 2015; Clavet GDC 2016 | gdcvault.com/play/1023280 |
| Holden, Kanoun, Perepichka, Popa, "Learned Motion Matching", TOG 39(4) 2020 | doi 10.1145/3386569.3392440 |
| Berndt, "JSBSim: An Open Source Flight Dynamics Model in C++", AIAA 2004-4923 | doi 10.2514/6.2004-4923 |
| Isla, "Handling Complexity in the Halo 2 AI", GDC 2005 | gamedeveloper.com |
| Park et al., "Generative Agents: Interactive Simulacra of Human Behavior", UIST 2023 | arXiv 2304.03442 |
| Wang et al., "Voyager: An Open-Ended Embodied Agent with Large Language Models", TMLR 2024 | arXiv 2305.16291 |
| Poggenhans et al., "Lanelet2", ITSC 2018 | doi 10.1109/ITSC.2018.8569929 |
| Cournoyer, "Massive Crowd on Assassin's Creed Unity: AI Recycling", GDC 2015; Fauerby, "Crowds in Hitman: Absolution", GDC 2012; Dragert, "Census", GDC 2021 | gdcvault.com |

### 4d. What the target can carry, CPU vs GPU

All of this is CPU (2P + 4E cores) and IO; the GPU sees only placements and poses. Re-examined with compute available (owner's correction): the SIM's truth stays on the CPU because the fourth invariant sends a SNAPSHOT from sim to renderer and never a value back, and a compute result reaches the CPU a frame late through a download. Compute may therefore compute a VIEW of an agent (a visual offset, a pose, a skin) but never its position of record. The verdicts:

| candidate | verdict | why |
|---|---|---|
| ORCA in compute | NO for the near rung; a MEASURED CANDIDATE for a "middle" rung of visible-but-contactless walkers, as a view offset from the lane the sim holds | the near rung's positions feed Jolt contacts and mind snapshots the same step: a one-frame-late download puts a wait where a handoff belongs. For 1 000 visible far walkers, GPU ORCA (one thread per agent, neighbours from a grid binned by SORTED index, not atomics) keeps positions on the GPU straight into the instance buffer; the sim keeps lane s. The offset is deterministic (sorted bins, fixed dispatch) and is a look, so a wrong one moves a pixel, never a quest |
| IDM per lane in compute | NO | 1 000 vehicles x IDM = ~50 kflop per step on the CPU; the graph is a CPU structure the autopilot's lookahead reads; compute would move µs of work behind a frame of latency. Reconsider only past 100 000 vehicles, which is not this game |
| navmesh queries (Detour A*) | CPU, always | a priority-queue search with data-dependent branching, ~10-100 µs per query, needed THIS step by the sim; on the GPU it is one thread doing serial work plus a download. Recast's voxelisation is parallel and could be compute at preload, but its output is consumed by Detour on the CPU and region growing has to be deterministic; the IO/compute POOL (CPU) already holds it |
| skinning and pose blending | GPU: skinning in the vertex stage (already); far-rung pose blending (walk-cycle phase per instance) as a compute pass writing the instance's bone matrices | pure view |
| motion matching search | CPU for the near rung (≤ 100 agents, 1 ms); a compute brute-force search over 10 000 frames per agent is possible and would only pay for hundreds of matched agents, which the rungs never ask for | latency and rung count |
| crowd instance culling and LOD | COMPUTE, with the piece cull of section 1 | a walker at the far rung is an instance like a piece |

| thing | shape | cost, origin |
|---|---|---|
| driving, near rung (≤ 10 contacting vehicles, board:2134) | CARLA's five stages collapsed to three on board:2133's graph: localise (lane + s), plan (IDM for gap, MOBIL for lane, a lookahead over junction occupancy and signal state), control (two PIDs or a pure-pursuit lateral) at the physics step | IDM + MOBIL are ~50 flops per vehicle per step; CARLA's TM handles hundreds per tick in one thread (measured by its users, unverified) |
| driving, far rung (100s) | kinematic on the graph: s += v·dt with IDM against the leader on the same edge; no contact, no PID | µs per vehicle |
| walking, navmesh | Recast at PRELOAD per tile from the ground lattice + footprints (IO pool, ~100 ms per 500 m tile on a desktop; on the A18 P-core [SET] < 300 ms); `DetourTileCache` for streaming | memory: ~1 MB per tile |
| walking, steering | Detour corridor + ORCA (RVO2) at 10-20 Hz for the near rung (≤ 100 agents), lane-following on the network graph for the far rung (1 000s) | ORCA: O(k) neighbours via k-d tree; 100 agents x 10 neighbours x 20 Hz = 20 000 LPs/s, < 1 ms |
| animation | motion matching for the near rung only (a database of ~10 MB uncompressed for locomotion; LMM later if memory bites); far rung: a looping walk cycle on a cheap skeleton | search: brute-force over 10 000 frames x 27 features = 0.3 M flops per agent per 10 Hz tick; 100 agents = 30 M flops/s, < 1 ms on one P-core |
| flying | JSBSim's 6-DoF shape reimplemented (LGPL) with the c172 tables as data; pilot = a trim + a route follower | ~µs per step |
| brains | a behaviour tree / state tree per Kind, ticked at the rung's rate; the tree is DATA the scenario declares (`Scenario::Mind::Programme`) | µs |
| schedules | Census-shaped: a mind carries {home, work, haunts} as OSM ids and a timetable; the tree picks the destination from the clock; SmartObjects = OSM amenities advertising verbs (`amenity=cafe` -> `sit`, `drink`) | data |
| LLM minds | on the IO pool; ONE snapshot (≤ 1 000 tokens) per ask at `EverySeconds`; answer parsed to the closed verb set; memory = Park's stream with the same three scores, bounded by `TokenBudget` | see latency below |

LLM latency and cost (measured by third parties, 2026, unverified in this tree): Haiku 4.5 TTFT 0.6-0.8 s, ~85-95 tokens/s, $1/$5 per M tokens; Sonnet 4.6 TTFT 0.85-1.6 s, $3/$15; batch API -50 %, prompt-cache reads at 10 %. Derived: a mind asked every 30 s with a 1 000-token cached prompt and a 100-token answer costs 100 x $5/M + 1 000 x $0.1/M = $0.0006 per ask; 1 000 minds at 30 s = 2 000 asks/min = $1.2/min -- too much. With rungs: 10 near minds at 30 s, 90 middle at 5 min, 900 far minds asleep (schedule only) = 0.33 + 0.3 = 0.6 asks/s = $0.02/min. So the RUNG decides the bill, and "a thousand minds" (board:2136) means a thousand SCHEDULES and ten CONVERSATIONS.

Determinism: an LLM answer is not reproducible (Anthropic's own docs: temperature 0 is not bit-deterministic), so the answer is an EVENT with the step it was asked at and the step it landed; replay reads the log and never the wire (board:2142). The same shape as Doom's LMP (input tics), Trackmania (inputs only, bit-exact physics), Factorio (deterministic lockstep, CRC per tick) -- all readable proofs that an input log plus a deterministic sim replays. Jolt adds `JPH_CROSS_PLATFORM_DETERMINISTIC` (+8 % cost) for cross-compiler determinism; on ONE machine with one binary the default build is deterministic given the same thread count and job order -- and the tree's invariant asks only for that. Broad-phase queries from multiple threads are the one Jolt path that is NOT deterministic; the crowd's neighbour queries must go through the declared-order path.

### 4e. Lab experiments

| question | inputs | solution | PROOF | to C++ |
|---|---|---|---|---|
| does IDM + MOBIL on the OldTown graph stay collision-free and reach capacity? | board:2133's graph exported (edges, lanes, speed by class); 200 vehicles | integrate IDM (a = 1.0, b = 1.5, T = 1.5 s, s0 = 2 m, Treiber's values) at 20 Hz; MOBIL with p = 0.3 | min gap > 0 for all pairs over 10 min; flow at a bottleneck within 20 % of IDM's known ~2 000 veh/h/lane; negative control: T = 0 -> collisions | the two models as `constexpr` parameter sets per behaviour profile |
| does SUMO's signal guess agree with OSM's `highway=traffic_signals` nodes? | OSM extract of OldTown; SUMO `netconvert --tls.guess-signals` (EPL tool run in the lab only) | compare junction sets | > 90 % of SUMO-controlled junctions have an OSM signal node within 30 m; negative control: `--tls.guess` off | which junctions get a signal plan; the phase table's shape |
| does the navmesh cover every OSM footway? | the ground lattice + footprints of one tile; Recast via `pyrecast` or the C demo | build, then sample every OSM `highway=footway` node | > 98 % of footway nodes are on a navmesh polygon; negative control: agent radius 5 m -> coverage collapses | Recast's parameters (cell 0.3 m, height 0.2 m, radius 0.4 m, climb 0.5 m) |
| ORCA vs Detour's sampled avoidance at 100 agents in a 2 m corridor | agents crossing; RVO2 Python bindings | run both; count near-misses (< 0.1 m) | ORCA: 0 collisions; Detour-style: report count; negative control: no avoidance -> collisions | which one, with the number |
| does a compute ORCA view-offset match CPU ORCA, deterministically? | 1 000 far-rung walkers on lanes; RVO2 in Python as oracle; the compute variant with sorted grid bins | run the compute pass 10 times, read back | offsets within 5 cm of RVO2 after 10 s; 10/10 runs byte-identical; negative control: bins built by atomic append -> bytes differ between runs, RED | the middle-rung pass, if the CPU crowd budget is ever exceeded |
| motion matching search cost | a 10 000-frame locomotion database (public: Ubisoft LaFAN1, CC BY-NC-SA -- lab only, NOT shipped; the shipped database must be the engine's own) | numpy brute force and a k-d tree | k-d tree ≥ 10x faster at same result; negative control: random features -> jitter | the feature vector and the search |
| replay of a mind | a 3-mind scenario; 50 asks recorded as events | re-run with the log, hash the agent table per step | identical hashes over 10 000 steps; negative control: withhold the log -> the run REFUSES (board:2142) | the event schema |
| the bill | the rung counts above; the pricing table | arithmetic | $/hour < 2 at 1 000 minds [SET]; negative control: all minds near -> > $50 | the rung policy |

### 4g. Requirements

- Network: board:2133's graph with lanes, speed by class, junction polygons and signal nodes; the country record (section 3) for side and speed defaults
- Physics: board:2127 (Jolt shape) for the near rung; heightfield contact; a vehicle is a body + 4 wheel constraints
- Navmesh: Recast (zlib) vendored, built per tile at preload on the IO/compute pool, streamed with `DetourTileCache`; never built in a frame (board:2124)
- Crowd: RVO2 (Apache) or a rewrite of `Agent::computeNewVelocity` (200 lines); neighbour query over the agent table, not Jolt's broad phase
- Minds: an IO-pool provider with `LatencyBudgetMs` (2 000 [SET]), a closed verb set, an event log with (asked step, landed step, text); a snapshot writer that names OSM things by their tags
- Budgets [SET] per 16.7 ms step on the P-cores: driving 0.5 ms (10 near + 200 far), crowd 1.0 ms (100 near + 1 000 far), animation 1.0 ms, brains 0.2 ms; total 2.7 ms on the SIM thread (board:2130) which is not the frame thread
- Memory: navmesh 1 MB/tile, motion database ≤ 10 MB, agent table 1 000 x 64 B = 64 KB

---

## 5. What else an open-world sandbox needs

| need | one line | the reference that proved it |
|---|---|---|
| **engine note** | granular synthesis over one recorded rev sweep (grains of ~one combustion cycle, resynthesised at the RPM) is what every racing game since DiRT 2 (2009) ships; Crankcase REV (Wwise plugin, commercial) is the product form; `ange-yaghi/engine-sim` (MIT) makes the note from the PHYSICS (combustion, exhaust pipe) and is the only readable "from the machine" source -- expensive, but the shape CLAUDE.md asks for | Forza Horizon 5 (granular at 90 Hz), Codemasters DiRT 2, engine-sim |
| **ambience and propagation** | Steam Audio (Apache-2.0 since v4.5.2, Feb 2024): `phonon.h` C API with direct/occlusion, reflections (ray-traced or baked), pathing, Ambisonics, HRTF; Resonance Audio (Apache-2.0, Google): cheaper, Ambisonic-only. SDL3 `SDL_AudioStream` + `SDL_SetAudioStreamGetCallback` is the device layer; SDL3_mixer (zlib) mixes tracks | Half-Life: Alyx (Steam Audio); the tree's `src/audio/Mixer` already sits on SDL |
| **physics** | Jolt (MIT): body, shape, constraint, fixed-order island solver, `StateRecorder` for determinism tests, ships in Horizon Forbidden West and Death Stranding 2; board:2127 already decided its SHAPE | GDC 2022 "Architecting Jolt Physics for Horizon Forbidden West" |
| **traffic signals and right of way from OSM** | SUMO netconvert: `--tls.guess-signals` (OSM puts signal nodes ahead of the junction), `--tls.join`, right of way from edge priority via `osmNetconvert.typ.xml`, `--lefthand`; OSM `highway=stop`, `give_way`, `traffic_signals:direction` | CARLA's Digital Twin runs exactly this |
| **day/night schedules** | a mind is a timetable over OSM places (home, work, `amenity=*`), Census-shaped; the far rung IS the schedule | Watch Dogs: Legion Census (GDC 2021), Skyrim packages, KCD |
| **save and replay** | scenario id + snapshot at t + event log since (board:2151); the input-log lineage is Doom LMP (1993), Trackmania, Factorio (CRC per tick catches the desync the day it happens) | those three, and Unreal's `DemoNetDriver` checkpoint+delta for the non-deterministic case the tree does not need |
| **telemetry for a person** | Tracy (BSD-3, `wolfpld/tracy`): nanosecond zones, frame view, memory; Remotery (Apache, single C file) if a browser view is wanted; Unreal Insights is the non-readable benchmark. FINDING: Tracy's GPU zones need timestamp queries and SDL_GPU exposes none, so GPU pass costs here are fence-bracketed submissions (one submit per measured pass, `SDL_WaitForGPUFences`) or the difference method -- coarse, and the reason every GPU ms in this report is [SET] until `make shots` measures it that way | Tracy is used by Godot, o3de and most indies; CLAUDE.md's "telemetry is for people" rule means it hangs off `make` as a build flag, never in the frame path by default |
| **country record** | driving side, speed defaults, sign catalogue, marking colour: one struct per ISO country, treaty defaults `constexpr`, OSM overrides | CARLA's `@country` on OpenDRIVE signals; Lanelet2 `traffic_rules` |
| **sky glow and lit streets** | VIIRS VNL radiance per place -> a sky-LUT term; OSM `lit=yes` and `street_lamp` -> punctual lights at EN 13201 levels | every night city in RDR2/Cyberpunk is hand-placed; here it is data |
| **wet roads and puddles** | Lagarde's four wetness states from `precipitation` hours; puddles as a height threshold on the ground stack | Remember Me (DONTNOD 2013), Forza Horizon 4 |
| **specular AA** | Tokuyoshi & Kaplanyan 2019, Filament's two constants; without it a wet street at 720p shimmers | Filament, CoD:WWII |
| **auto-exposure with a physical camera** | histogram compute (Narkowicz, Tardif) over Filament's EV100 formulas; the scenario's `Exposure` triangle is the manual override | Frostbite 2014, Unreal |

---

## 6. What compute changes, and what it does not

SDL_GPU's compute is a general-purpose stage with storage read/write, indirect dispatch and indirect draw; it lacks tile-local memory (so a pass cannot keep a G-buffer on chip the way Metal's imageblocks do) and timestamps. The bandwidth arithmetic at 60 GB/s: a 720p buffer costs 0.06 ms per byte-per-pixel to write and the same to read. A compute pass earns its place when it reads a full-res thing ONCE and writes something SMALL, or when it runs on a CHANGE and not on a frame.

| moves to compute | reads | writes | per | saves |
|---|---|---|---|---|
| piece and instance cull + rung pick + scan compaction -> indirect draws | instance table (20 000 x 64 B = 1.3 MB), Hi-Z pyramid | indirect buffers, a few KB | frame | the CPU piece walk; overdraw (Hi-Z) |
| shadow caster cull per cascade -> indirect | instance table | 3 indirect buffers | frame | the CPU caster walk |
| SDSM depth min/max | depth 3.7 MB | 8 B | frame | tight cascades; possibly one cascade (measure) |
| froxel light binning | 256 lights x 32 B | 8192 x 4 B + records | frame | Filament's ~0.2 ms CPU |
| exposure histogram + EV | quarter-res luminance 0.9 MB | 128 x 4 B, then 16 B | frame | the readback and the one-frame CPU stall |
| star magnitude cull | 9 110 x 16 B | visible list + 1 indirect | on exposure change | rasterising invisible sprites |
| atmosphere LUTs (already compute in the tree) | -- | ~2 MB | sun move / haze change | -- |
| weathering mask bake | piece height/AO | R8 mask per piece | piece build | minutes of CPU per gamma-ton |
| season / wetness parameter rows | phase scalars | instance parameter rows | day / weather change | per-frame uploads |
| snow and puddle masks | tile height + normals | R8 per tile | coverage / weather change | a per-pixel threshold every frame |
| far-rung pose blending; middle-rung ORCA view offset (candidate) | instance rows | bone matrices; offsets | frame | CPU, only past the near rung's budget |

| stays where it was | why |
|---|---|
| one BRDF slab per pixel, no fat G-buffer (D2) | compute must still READ what raster wrote; 80 B/px is 2.5 ms either way |
| pipeline variant choice | a pipeline is an SDL object bound from the CPU |
| ephemeris, cascade matrices, region record, season phase | scalars; double precision; the sim's truth |
| IDM, navmesh A*, near-rung ORCA, motion-matching search, brains, minds | the sim's truth, needed this step, latency-bound; a download is a wait where a handoff belongs |
| Recast build | CPU pool; its consumer is CPU; deterministic region growing |
| Jolt | CPU by design; its own determinism |
| audio | CPU/SDL audio thread |

**Determinism rule for every compute pass** (the fourth invariant applied to the GPU): no result may depend on thread completion order. Allowed: a fixed dispatch reading fixed inputs; integer atomics for COUNTS (commutative); prefix-sum compaction and per-cell loops in declared order for LISTS; float reductions in a fixed tree. Refused: atomic-append lists, float atomics, `SIMULTANEOUS_READ_WRITE` on a texture a neighbouring thread reads. `make shots`' digest is the oracle: a compute pass that moves a digest between two runs of one binary is a bug.

### 6a. Which lab experiments change

| experiment (section) | before | now |
|---|---|---|
| weathering mask (1e) | numpy oracle -> C++ CPU bake | numpy oracle -> COMPUTE bake; proof adds "compute mask == numpy mask within 1/255, 10/10 runs identical" |
| wetness (1e) | one scalar per class | unchanged; the C++ target is a storage-buffer row and a compute update |
| cascade split (2f) | PSSM λ only | + SDSM bounds from read-back depth; SDSM ≥ 2x PSSM min density |
| froxel count (2f) | grid arithmetic | + GPU list == numpy list in ORDER; atomic-append as negative control |
| histogram exposure (2f) | did not exist | new: integer bins equal numpy, byte-identical over runs |
| snow blend, wetness decay (3e) | numpy oracle | unchanged oracle; the C++ target becomes a per-tile compute mask |
| ORCA (4e) | CPU only | + the compute view-offset variant with the sorted-bin determinism control |
| piece cull (new, belongs to board:2122) | -- | scan-compacted visible list == CPU frustum+Hi-Z list in order; atomic append as negative control |
| all the rest (BRDF furnace, sun vs SPA, Bruneton LUTs, night sky, MCD12Q2, Delpierre, country record, marking colour, IDM/MOBIL, SUMO signals, navmesh coverage, motion matching, mind replay, the bill) | -- | unchanged: they are oracles of the sim or of a provider, not of a pass |

### 6b. Revised budgets [SET], 720p p95

| pass | first draft | now | note |
|---|---|---|---|
| opaque lit | 3.0 ms | 3.0 ms | + 0.15 ms cull/compaction compute |
| sun cascades | 1.8 ms | 1.85 ms | SDSM reduction 0.05; a measured drop to 2 cascades would make it 1.35 |
| punctual clustered | 1.0 ms | 1.0 ms | binning moved GPU-side inside it |
| atmosphere | 0.6 ms | 0.6 ms | already compute |
| auto-exposure | 0.1 ms | 0.1 ms | no readback |
| CPU frame thread, render side | ~0.5 ms | < 0.1 ms | piece walk, caster walk, light binning gone |
| sim thread (section 4) | 2.7 ms | 2.7 ms | nothing of the sim moved; a middle-rung ORCA would take 0.3 ms off the crowd line if measured worth it |
| GPU measurement method | timestamps | fence-bracketed submissions / difference method | SDL_GPU has no query API |

---

## Decisions this report takes (for the board to accept or refuse)

| # | decision | reason |
|---|---|---|
| D1 | Ephemeris = `cosinekitty/astronomy` C file (MIT), ±1 arcmin; NREL SPA is the lab's TRUTH, never shipped | licence (SPA non-redistributable) and one-file vendoring |
| D2 | One BRDF slab per pixel; layering at parameter level; clear coat/sheen/anisotropy as compile-time variants | 60 GB/s: an 80 B/px Substrate GBuffer is 2.5 ms of bandwidth before lighting |
| D3 | Sun shadow = 3 cascades x 1024² fitted by SDSM (compute min/max of last frame's depth), PSSM λ = 0.5 as fallback; casters culled per cascade in compute; punctual = Filament's froxel grid (16 slices) binned in COMPUTE in declaration order | Unreal mobile ships 2 cascades; Lauritzen measured SDSM over PSSM; the binning is 2 M tests and belongs beside the depth it fits |
| D4 | Weathering = per-piece mask baked in COMPUTE at piece time (gamma-ton lite) blending parameters; wetness = one scalar per material class from precipitation hours in a storage-buffer row; snow and puddles = per-tile compute masks on change | offline methods yield a mask; the frame pays a lerp; the masks are built when something changes, never per frame |
| D5 | Seasons = MCD12Q2 dates (2001-2020) with Delpierre's DM model as the fallback for years without data; snow = ERA5-Land depth + MOD10A1; all provider answers, 0 ms per frame | the answers have a truth outside the tree |
| D6 | Driving = CARLA's stage shape over IDM + MOBIL on board:2133's graph; walking = Recast/Detour + ORCA; flying = JSBSim's 6-DoF shape with c172 tables | readable baselines, all permissive except JSBSim (LGPL: reimplement the core) |
| D7 | Minds = Park's memory stream + Voyager's skill library shape, on the IO pool, rung-gated so the bill stays under $2/h at 1 000 minds; every answer is a replayed event | cost arithmetic above; the determinism invariant |
| D8 | Audio = SDL3 streams + Steam Audio (Apache) for propagation; engine note by granular synthesis first, engine-sim's physical model as the lab's oracle | what ships vs what CLAUDE.md wants; measure before the second |
| D9 | Telemetry = Tracy behind a `make` flag for the CPU; GPU passes timed by fence-bracketed submissions | BSD-3, zero cost when off; SDL_GPU has no timestamp query |
| D10 | GPU-driven drawing: compute cull + rung pick + PREFIX-SUM compaction into indirect draw buffers, one `SDL_DrawGPUIndexedPrimitivesIndirect` per (pipeline, rung) bucket; the sim's truth (agents, vehicles, paths, minds) never moves to compute | Haar & Aaltonen 2015 for the shape; the fourth invariant for the line: compute may produce a VIEW, never a position of record; scan-compaction keeps the bytes identical between runs |
| D11 | Compute determinism rule (section 6): no completion-order dependence; integer atomics for counts only; scans and declared-order loops for lists | the digest is the oracle |

## Findings (defects and licence traps found while researching)

- GlobalBuildingAtlas LoD1 and height maps are CC BY-NC 4.0: not usable in a product. Overture/Microsoft/Google are ODbL or CC BY and are.
- Open-Meteo's free API is non-commercial and 10 000 calls/day; the DATA is CC BY 4.0. A product mirrors ERA5 or pays.
- NREL SPA is not redistributable; every "pvlib spa_c" wrapper makes the user download it. Astronomy Engine (MIT) is the shipped answer.
- JSBSim is LGPL-2.1: link dynamically or reimplement the 6-DoF core from Stevens & Lewis.
- Yale BSC5's licence is not stated on its host; HYG (CC BY-SA 4.0) is the safe catalogue.
- LaFAN1 (Ubisoft mocap) is CC BY-NC-SA: lab only.
- Black Marble VNP46 v1 products were discontinued after 2025-01-31; VIIRS VNL (EOG) continues and is CC BY 4.0.
- The tree's shaders are `.msl` (board:2152 is the fix); nothing in this report depends on Metal.
- SDL_GPU has no occlusion or timestamp queries (`SDL_gpu.h`, verified): GPU pass costs cannot be read from the GPU; every ms in this report is [SET] until measured by fence-bracketed submissions in `make shots`.
- The first draft dismissed SDSM and GPU light binning by assuming a raster-only GPU; both are compute passes that read once and write bytes, and both are now adopted (D3).
- `Scenario::Weather` carries no precipitation history; wetness needs "hours since rain", one more field the provider fills.
