Type: feature
Area: render
Tags: instrument

**II.5 Atmosphere and sky**

- [ ] Bruneton transmittance LUT (`TransmittanceStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Multiple-scattering LUT (`MultiScatterStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Sky-view LUT (`SkyViewStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [ ] Sky draw from the LUTs (`SkyStage`) — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
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
