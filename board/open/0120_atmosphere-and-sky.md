Type: feature
Area: render
Tags: instrument

**II.5 Atmosphere and sky**

- [x] Bruneton transmittance LUT — built as `MediumTransmittanceStage` over `ParticipatingMedium.h`, the engine's first compute pass; the zenith ray is checked against the closed form and every texel against the C++ twin on the device (`board:1555`)
- [x] Multiple-scattering LUT — built as `MediumMultiScatterStage`, computed on the device from the device's own transmittance table; every texel within 1 % of the C++ twin, f_ms < 1 everywhere (`board:1559`)
- [x] Sky-view LUT — built as `MediumRadianceStage`: 192x108 non-linear dome, blue noon / bright horizon / red sunset / dark night all derived, device chain within 0.1 % of the twin (`board:1560`)
- [x] Sky draw from the LUTs — `SkyStage` samples the dome per pixel; every pixel above the horizon written, blue by derivation, proven in one frame over a built ground (`board:1561`)
- [ ] Irradiance readback that is the scale for everything lit (`IrradianceStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Aerial perspective / haze along the view ray, Koschmieder-derived (`AtmoHaze.h`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Sun disc with limb (`SunStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Moon as a lit sphere with a phase, over the NASA LROC albedo — measured data that is a raster by nature, principle 2 admissible — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Stars at true altitude and azimuth from the HYG catalogue, magnitude-sorted, with B−V colour — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Star magnitudes that do not clip at the display white — `maxY ≈ 1.0` on every night frame
- [ ] Airglow and zodiacal light
- [ ] Milky Way band — needs a source that is measured raster rather than authored; UNSURE whether HYG suffices
- [ ] Moon glow and its halo around the disc
- [ ] Horizon lift at night
- [ ] Mesopic response, so a night is not a dark day
- [ ] Ozone absorption band separated in the model — UNSURE whether the current Bruneton parameterisation carries it
- [ ] Rainbow, halo, sun dog — the reference has none of these either
- [ ] Crepuscular rays through a cloud break
- [ ] Volumetric fog with shadowing (`e_VolumetricFog` + `r_FogShadows` is the reference's; ours must fit inside a stage that already reads the HDR target or it does not get built)
- [ ] Ground fog in a valley at dawn, driven by the terrain's own hollows
- [ ] Fog volumes as declared local shapes — the reference's boxes and ellipsoids; ours would be a function of place instead
