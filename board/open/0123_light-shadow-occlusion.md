Type: feature
Area: render
Tags: oracle, perf, instrument

**II.8 Light, shadow, occlusion**

- [x] Sun as the directional source, its radiance from the atmosphere model
- [x] Sky as an area source through the irradiance LUT
- [x] Four cascaded shadow maps
- [x] Screen-space ambient occlusion at a 0.9 m radius, half resolution
- [ ] One lighting model spliced into every lit surface (`SurfaceLight.h`), so a second one cannot appear — **the implementation named here was deleted with the WebGPU renderer at `0161f88`**; the stage survives in the catalogue and `Renderer::Executable` refuses it by name (`render/Renderer.cpp:102-127`), so this is scope again and not a silent loss
- [x] Auto exposure from measured irradiance, with gain and white point read by the tone chain
- [x] ACES-Narkowicz tone curve with no free parameter
- [x] Temporal antialiasing with a Halton(2,3) jitter
- [ ] Nothing in the frame occludes between 1 m and 20 m — the whole of a tree. Cascade 3 is 1.2 m per texel and SSAO reaches 1 m
- [ ] Coarse world-space sky visibility over the cluster DAG's own bounds, per vertex — the cheap candidate, no new pass
- [ ] Voxel cone tracing in the AO pass's existing slot, only if the cheap candidate demonstrably cannot produce the term
- [ ] Sky visibility at 1.5 m under a closed canopy inside the band an LAI of 4.5–5.1 implies
- [ ] Ambient specular in an enclosed place — NO SUBSTITUTE: under a canopy, in a gorge, indoors, the reference hand-places a baked probe, which is measured appearance of an authored scene and principle 2 forbids it. In the open the sky LUT is the correct substitute and is better founded
- [ ] Baked environment probes — REFUSED, principle 2
- [ ] Baked lightmaps — REFUSED, same
- [ ] **The two micro-relief bounce terms move out of the lighting model and into the material row.** `render/stages/SurfaceLight.h:33 kGroundBounce = 0.12` and `:40 kSelfShelter = 0.35` are *correctly* documented as the mean reflectance of Central European land cover and as the sky fraction a clod or a sward hides from a point between them — both are statements about **a surface**, and both are currently engine constants spliced into every lit surface, water and glass included (the bug tasks in `board/`). They are two scalars, they switch no pipeline state, and the material row is defined as exactly that (`CLAUDE.md`, *the core dictates the pipeline*). Ground keeps 0.12/0.35 and a manufactured surface declares 0 — at which point a Lambertian configuration becomes **spellable**, which is what rung 3 of § I.26 needs and what nothing in this tree can express today
- [ ] Point and spot lights as a list the core lights from — **and § I.26 scene 8 is what first requires it**: Blender's factory key light is a 1000 W point at 7.244 m, so matching the literal default lighting is this line, not new scope invented for a test
- [ ] Emissive surfaces contributing to that list — `Material` has the field and `SurfaceState::Emits()` derives from it; nothing emits
- [ ] Shadow from a point light
- [ ] Contact hardening on a shadow
- [ ] Shadow proxy: a cheap single-material representation per caster — free, because the LOD ladder already produces one
- [ ] CPU coverage-buffer occlusion culling with authored occluder meshes — REFUSED: authored *and* CPU-bound, the wrong direction on wasm32
- [ ] GPU occlusion culling against the depth of the previous frame
- [ ] Vegetation tinted toward the ground class colour with range — the single mechanism that makes a distant foliage field read as one mass; the reference ships it at 50…80 m
