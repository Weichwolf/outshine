Type: feature
Area: render
Tags: instrument

**II.3 Ground materials and surface**

- [x] Seventeen ground materials with linear albedo whose chromaticity is sourced (Munsell renotation, ECOSTRESS spectra) and whose luminance is locked to a broadband value
- [x] Roughness, specular scale, grain size, height amplitude, coarse and fine detail scale per material
- [x] Litter class per material, overridable per template — beech litter under spruce is a defect the botanist calls
- [x] Litter coverage, contrast, edge reach, constructed-edge flag
- [x] Slope maximum per material, so a class cannot sit on a wall
- [ ] Sward closure folding the grass colour into the terrain albedo beyond the blade fade — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [x] Alpine limit: a rock template selected by slope band and elevation
- [ ] High-frequency detail as a noise function, explicitly greyscale, cut at a declared range — the reference's rule, and the only legal form a detail map takes here
- [ ] Height-driven blend between classes so pebbles poke through dirt instead of cross-dissolving
- [ ] Class-boundary mixing width measured in pixels at the comparison rung
- [ ] Near-ground luminance variance off the floor
- [ ] Wetness as a material state — darkening, specular rise, puddles in depressions
- [ ] Snow cover as a material state with a slope and aspect mask
- [ ] Frost, ice, mud, ruts, trampled paths
- [ ] Tracks and desire lines where things walk repeatedly
- [ ] White limestone and rock patches reading as snow at 36 N in August — a tonal defect in the existing table
- [ ] Deferred decals for local dressing — REFUSED in the reference's form (authored textures); the procedural substitute is a material row plus a noise function
