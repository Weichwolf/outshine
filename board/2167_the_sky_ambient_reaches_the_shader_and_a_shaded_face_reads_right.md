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

## Terrain shadow path audit, 2026-09-07

`GroundLattice::Cast` has no caller. Its depth pipeline is configured by
`LightVisibilityStage`, but `LightVisibilityStage::Cast` only draws subject batches.
A successful pipeline creation or unchanged place PNG does not validate terrain casting.
The shadow cache observes `SubjectDraw::Generation()` (`Moved_ + Reshaped_`); terrain
pages and instances need their own invalidation evidence when connecting this path.
Do not attribute the dark shoreline teeth to terrain shadow mapping: that path does
not currently draw them. Normals, source elevations and ambient remain suspects.


## Uniform environment hemisphere correction, 2026-09-07

`Live::StandsEnvironment` now supplies a declared uniform radiance to both hemispheres. Previously only RadianceLinear received it and GroundLinear stayed zero: the shader's 0.5 sky share for a horizontal normal halved the declared radiance. This is not an artificial brightness adjustment: a constant environment has the same radiance in all directions. Sky and ground additions remain separate when the physical sky is enabled.

Separate before/after render evidence: SpecularTest goes from35.0852% within8codes (25346 pixels apart, db3a1b62) to87.0995% (5037 apart,9985b064). Both black-environment normal tests are unchanged:96.9874%/5988pixels and95.0759%/9810pixels. Their unchanged images are the control for declared zero radiance. Logs `build/unlit-standard-after.log`, `build/uniform-environment-after.log`; Specular output opened against previously opened oracle. Still RED, no threshold relaxed.

World regression `build/uniform-environment-places.log`: Husum e6bc1f75 p99 3.03ms; Malcesine eebaea2a p99 4.94ms; both0/120 over16.67ms. Both PNGs opened. Their unchanged teeth, shoreline ramps and excessive terrain darkness remain unacceptable. Actual GPU sky irradiance evaluation, AO and local indirect light are still open; this correction does not satisfy those requirements.


## GPU irradiance is now consumed, and its output is actually bound

The lit and ground fragments now read the atmosphere's six-float irradiance buffer. `Live::LightsFromTheSky` supplies solar illuminance, solar zenith and ground albedo, replacing its duplicate CPU ambient integration. The shader adds E_sky * solarLux / pi above and albedo * (E_sky + max(cosSun,0)*sunTransmittance) * solarLux / pi below, over the declared uniform environment. The atmosphere here is the GPU medium used by the sky, including declared Haze; the old CPU ambient path used unmodified kEarthAir even for the Places' Haze=0. Direct sunlight and exposure STILL use the CPU SunThroughTheAir path; their consistency remains open. Hemisphere interpolation and simple specular Fresnel remain approximations, not directional sky convolution or complete IBL.

Bindings: lit has one active storage buffer after its samplers; ground has class, palette and irradiance buffers. The light uniform now has five vec4 header rows plus its light array (1104 bytes). Irradiance GPU usage includes graphics storage read. The graph declares MultiScatterLut as the actual irradiance input, and transmissive subjects declare their irradiance dependency.

The first connected renders exposed TWO real graph defects:

1. IrradianceBuffer had Format=Handle instead of Table, so EncodePass never bound it as a compute output. In the merged pass, slot0 instead held ClusterKept. My initial suggestion that culling overwrote irradiance was backwards; the six-assertion binding test established which buffer occupied the slot.
2. Independent compute stages with different writable buffers were merged without relocating each kernel's zero-based writable binding table. The compiler now refuses merges that conflict in the writable buffer or texture binding namespace, and checks write-after-read as well as read-after-write dependencies. Independent texture-only and buffer-only writes may still share a pass; this is not a blanket removal of merging.

Irradiance is now a Table with shared six-float layout (`IrradianceLayout.h`). `ComputePassesPreserveStorageBindings` failed one of six assertions before the repair and passes afterward. Full conventions suite:3/3 passed (`build/compute-bindings-before.log`, `build/compute-bindings-after.log`). An initial build-only failure lacked SDL compiler flags for the conventions suite; that harness wiring was corrected before collecting the real negative control. SDL's compute-pass documentation establishes fixed writable bindings and absence of implicit dispatch synchronization: https://wiki.libsdl.org/SDL3/SDL_BeginGPUComputePass

Public ambient measurements now derive from actual GPU readback during inspect, rather than publishing the old CPU calculation at build time. Reading an unsettled irradiance stage refuses. `build/gpu-sky-irradiance-corrected.log` (audit): Husum normalized E_sky approximately(0.022,0.048,0.111), sun transmittance(0.917,0.811,0.665); Malcesine approximately(0.022,0.049,0.115), transmittance(0.929,0.836,0.706). Values are rounded by the instrument. Husum reported sky radiance(913.318,2012.608,4679.032), ground radiance(3667.092,3226.736,1804.136), in the renderer's illuminance-scaled radiometric convention.

Causal render sequence retained:
- Connected shader but broken compute output: Husum4cc56e51, Malcesine8bc3f82b; near-black facades, visibly rejected.
- Corrected graph: Husum1bfb2f74, Malcesine9ca45b13. Both opened, plus enlarged facade/shore and cliff crops under build/irradiance-control, against reopened webcam photographs.
- Negative control set only packed SkyLux to zero: Husum4cc56e51, Malcesine9bffff8a, both opened. Terrain/subject indirect illumination disappears. `build/irradiance-control/without-gpu-light.log`; source restored in finally, exact bytes verified afterward.
- Positive rebuild restores1bfb2f74 and9ca45b13: p993.11ms and4.79ms, both0/120 over16.67ms (`build/gpu-sky-irradiance-restored.log`). Earlier audit run p994.92/5.61, also0/120, not averaged with this run.

Material controls unchanged: Unlit100%; Specular87.0995%/5037pixels apart; NormalTangent96.9874%/5988; Mirror95.0759%/9810 (`build/gpu-sky-irradiance-materials.log`). No thresholds changed. Three remain RED.

Visual verdict: not accepted. The physically connected sky contribution is now demonstrable, but Fels faces remain far too dark, shoreline teeth and cliff bands remain, and the render has none of the webcam's surface detail/reflection. Need direct-sun frame/normal investigation, consistent direct atmospheric attenuation/exposure, directional irradiance/AO/local bounce, terrain error/shoreline work. No claim of optimality or full WI completion.


### Geographic azimuth, 2026-09-07 continuation

The local geographic frame is East-Up-South: north is -Z. The camera already used this convention, but Live::TowardTheKey and the duplicate StandsKeyLight direction used +cos(elevation)*cos(bearing) for Z. Ephemeris azimuth is clockwise from north; this mirrored the sun's north/south component. A shared EastUpSouthDirection now serves camera and light, with Z=-cos(elevation)*cos(azimuth). StandsKeyLight reuses TowardTheKey. Public Scenario::Light documents the direction toward the source and the geographic azimuth convention. Explicit imported glTF beam directions retain their meaning: the corpus inverse conversion now uses atan2(-direction.x, direction.z).

Negative regression with the old sign: AzimuthUsesGeographicNorth failed north and south Z, two of thirteen assertions (build/azimuth-before.log); east, west and zenith controls passed. Corrected implementation passes all thirteen; conventions suite4/4 (build/azimuth-after.log). The camera direction is unchanged. Material controls after the inverse translation retain previous results: NormalTangent96.9874%, Mirror95.0759%, Specular87.0995%; still RED, no oracle or threshold changes (build/azimuth-materials.log).

First two-place result: Husumc98da96a p993.10ms, Malcesine56b80c58 p996.96ms; both0/120 over16.67ms (build/azimuth-places.log). Both full PNGs and enlarged shore/cliff crops opened. Husum harbour facades now receive direct sunlight as in the photograph; Malcesine's left slope is lit. The previous corrected-GPU baselines1bfb2f74 and9ca45b13 retain the mirrored sun for comparison. No control remains active.

All-nine run completed (build/azimuth-all-places.log). Each row measures120 frames; every row has zero frames over16.67ms:

| Place | PNG digest | p99 ms |
|---|---|---:|
| DarmstadtWest |416186dd|2.94|
| Wien |1f2b2d88|8.36|
| Rosenheim |594b48d6|4.45|
| Husum |c98da96a|3.11|
| Olympiaturm |2c28e855|8.60|
| Graz |700bad1a|5.12|
| Koerbersee |304fa840|7.39|
| Malcesine |56b80c58|4.85|
| Feldkirch |2597dcc3|5.77|

Every final PNG was opened, along with every corresponding webcam photograph in comparison sheets and a2x crop per place (build/azimuth/review). This is a static performance pass, NOT visual acceptance or moving-camera proof. Verdict remains rejected: Husum has shoreline ramps/teeth instead of quay walls; Malcesine blue vertical curtains and horizontal bands; Feldkirch a near-black abrupt terrain wall and discontinuities; Koerbersee visibly coarse ridges and abrupt material patches; Graz floating fragments and a missing/misplaced hill relative to the photo. Urban facades remain uniform prisms with no detail; Wien/Rosenheim are too dark compared with the photographs. All skies lack the photographed cloud/light distribution. Distinct geometry, material and atmospheric causes require their own controls; the sun sign does not fix them.


### Orthographic lighting continuation

Corrected the shared BRDF view vector to use parallel rays for orthographic cameras. Full evidence is in board2096's orthographic shading rays continuation: NormalTangent5988 ->2086 differing pixels, Mirror9810 ->1741; both still RED. A GPU axial-camera-motion invariance test fails under the old shader (maximum linear difference0.264038086) and passes under the correction. The perspective Places repeat reveals alternating terrain-edge digests without source edits; see board2154. Direct atmospheric medium consistency, directional sky/AO/local bounce and the rejected shoreline/cliff appearance remain open.


## Declared atmosphere and general conventions continuation, 2026-09-07

Live::DeclaredAir now supplies Hazed(kEarthAir, Declared_.Haze) to BOTH CPU SunThroughTheAir and Renderer::SetMedium. The air cache compares its actual float cosine and Medium; its old metre tolerance on a cosine is gone. SamePicture includes Haze so changed declarations invalidate the picture. ClockedExposureUsesDeclaredAir negative: Haze0/1/2 all85072.454561337lux, fails response assertion. Positive:84931.058194878 /85072.454561337 /85127.545410845lux,7assertions pass; conventions7/7. Logs declared-air-before/after and declared-air-negative-assertions. Increased aerosol does NOT simply darken total horizontal irradiance: scattered light redistribution is a counterexample. This is consistency evidence, not independent absolute photometry. Dynamic redeclaration invalidation not separately tested; ground albedo/observer height consistency still open.

All9 make shots terminal46251 exit0, build/declared-air-all-places.log. Name,digest,p99ms: DarmstadtWest,e3480f35,3.21; Wien,fca0b043,5.85; Rosenheim,94287023,4.75; Husum,84ab9c53,3.32; Olympiaturm,a95d6751,4.04; Graz,0ad02ce2,5.11; Koerbersee,552e2a97,9.28; Malcesine,90ecdd5f,4.72; Feldkirch,0ca91bb4,5.80. All0/120 over16.67ms. Earlier2Placeaudit44687: Husum3.51/Malcesine5.72ms same digests. Every all9 comparison PNG (render+webcam) AND every2x detail PNG in build/declared-air-review opened visually. Graz initially truncated tool result was reopened successfully. Rejected: Husum shoreteeth/ramp, Malcesine blue vertical curtains/horizontal band, Feldkirch nearly black abruptwall, Graz floating geometry and hill spatial mismatch. Koerbersee coarse ridges and abrupt material borders. Cities flat prisms/darkfacades and absentvegetation; allsky/weather mismatch. Performance is a pass, visual acceptance is NOT.

Materials terminal41748 exit2, build/declared-air-materials.log: Unlit100%8ef62b10; Normal98.9505%2086px51a01e96; Mirror99.1261%1741pxf2a3cadf; Spec87.0995%5037px9985b064. All4PNG reopened. Three remain RED against strict99.99%, no oracle change.

### Spot angular attenuation

Public native Spot light path rendered linear cone attenuation, inconsistent with Khronos's steep-then-level curve. subjectLighting.glsl now multiplies attenuation by angular*angular. Standard: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles . The squared interpolation is the reference implementation, not the sole allowable curve.

NEW SpotFalloffMatchesKhronos.cpp renders three65x65 native lit planes via public Engine/Geometry: pointcontrol, spotcosine0.75 between innercos1/outercos0.5, spotaxis. Odd dimensions put center sample at planeorigin, equalposition/distance/BRDF cancels in ratio. Expected midpoint=((0.75-0.5)/(1-0.5))^2=0.25, axis1. Negative actualGPU0.5midpoint/1axis:1/15fails (build/spot-falloff-before.log, spot-falloff-negative-assertions.log). Aftershaderfix15/15pass, fullconventions8/8 (build/spot-falloff-after.log). Terminal4030negative exit2,75963positive exit0. No transient controls active. No further Places run after spotlight-only correction; all9above belongs to declaredair correction. No source/board edits during gates.

Remaining goal unchanged: native texture binding, normalScale/occlusionStrength, alpha preservation, instance normal/mirror and Standing.ScaleXyz consumer audit, vertical datum; directional sky/AO/localbounce; terrainDEM projected1px bound/crackfree/noSkirts/motion; all9visual acceptance. TerrainGrid::FlattenOutliers tilemedian/MAD may flatten realfeatures but this is UNPROVEN; actual69badHusumsamplespreviouslyverified, do not remove sanitization blindly. Materialcorpus3red; nondeterministicPlacesedgesopen board2154. No completion claim. Current turn made concrete progress and is NOT blocked.
