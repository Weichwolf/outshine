Type: feature
Area: render
Tags: scope

**II.6 Clouds**

- [x] Cloud density as one function with two evaluators, C++ and a WGSL transliteration whose constants are emitted from the same place
- [x] Per-deck separable model: wind-advected 2-D coverage FBM × an analytic vertical profile − 3-D erosion
- [ ] Anything that draws a cloud — `Renderer::CreateClouds()` is an empty function and there is no cloud stage
- [ ] Cloud shadow on the ground
- [ ] Cloud lighting: forward scattering, silver lining, powder term
- [ ] Cloud base from the weather ceiling rather than a constant
- [ ] Three decks — low, mid, high — driven by the four GFS cover diagnostics that the provider already carries
- [ ] Cirrus fibres sheared along the wind (the constants exist; nothing draws them)
- [ ] Contrails
- [ ] Storm cell with anvil
- [ ] Cloud advection consistent with the declared wind, so a shadow moves at the right speed
