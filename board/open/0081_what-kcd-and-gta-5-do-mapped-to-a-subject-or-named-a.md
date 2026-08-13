Type: feature
Area: render
Tags: oracle, khronos, instrument

**I.26.8 What KCD and GTA 5 do, mapped to a subject or named as a gap**

*Owner's ruling, 2026-08-12, restated because it had narrowed in transit: **the renderer must be good at
everything KCD and GTA 5 do** — not at everything the glTF format exercises. `CLAUDE.md` already carries
the rule; § I.26.6 applied it to a **feature list** and that list was bounded by what a file format can
carry, which silently dropped every workload-shaped requirement. This section is the workload half.*

**Every feature gets a basic, isolated, synthetic case before any complex scene exercises it.** *Owner's
ruling, 2026-08-12: **a basic glTF test case for every GL feature KCD or GTA 5 uses**, and "basic" is
the operative word. It is the one-new-thing rule extended across the whole feature surface instead of
across the first ten rungs only — **a forest that fails tells you nothing if instancing, alpha test and
shadow cascades were never isolated first**.*

**Two kinds of case, and the second is the one with no assets to find.**

| | |
|---|---|
| **A · format features** | glTF expresses them; a Khronos sample usually exists and is **fetched** |
| **B · renderer techniques** | glTF cannot express them, so no sample can be found — the case is **authored in Blender and exported**, minimal by construction |

- [ ] **For kind B the case is authored, and that closes the escape hatch this section first left open.** *"No clearable asset expresses this" is **no longer an admissible reason to skip a feature** — if no asset exists we author one. The only remaining admissible exclusion is **"neither KCD nor GTA 5 does this"**, stated per feature with its reason.*
- [ ] **Authoring in Blender means the oracle comes free**, which is the whole reason kind B is tractable: the scene is built in the reference renderer, so the reference render already exists, and the exported glTF is the subject our side reads. A found asset would have to be *hoped* to isolate the right thing; an authored one **is** minimal because we made it so
- [ ] The authoring scripts are **offline data preparation committed beside what they produce** — `CLAUDE.md`'s one open door, the same one the corpus fetcher goes through. A `.py` per case, its `.blend` and `.gltf` derived and untracked (§ I.26.10)
- [ ] **The glTF file is the *subject*; the feature under test may be ours entirely.** A basic SSR case is still a basic glTF scene — the file carries a sphere and a plane, the reflection technique is 100 % ours, and Cycles renders what the reflection should look like. Nothing about kind B requires glTF to know what the technique is

| Workload | Kind | Basic case | Integration scene |
|---|---|---|---|
| PBR metal-rough, normal · occlusion · emissive maps | A | `MetalRoughSpheres` · `NormalTangentTest` · `SciFiHelmet` | built world |
| alpha `OPAQUE`/`MASK`/`BLEND` | A | `AlphaBlendModeTest` | forest |
| instancing | A | `SimpleInstancing` — syntax; **authored count sweep** — scale | forest |
| skinning · morph · interpolation modes | A | `RiggedSimple` · `AnimatedMorphCube` · `InterpolationTest` | film |
| `KHR_lights_punctual` | A | `DirectionalLight` · `PointLightIntensityTest` | built world at night |
| cameras and projection | A | `Cameras` (perspective **and** orthographic) | every rung |
| `KHR_texture_transform` | A | `TextureTransformTest` | built world |
| vertex colours · second uv set | A | `BoxVertexColors` · `MultiUVTest` | forest |
| clearcoat = **car paint** | A | `ToyCar` · `ClearCoatCarPaint` (CC0) | vehicle |
| transmission + volume + ior = **glass** | A | `TransmissionTest` · `CompareVolume` · `IORTestGrid` (CC0) | built world · vehicle |
| sheen = **fabric** | A | `SheenCloth` (CC0) | character |
| anisotropy = **brushed metal** | A | `AnisotropyDiscTest` (CC0) | vehicle |
| `emissive_strength` | A | `EmissiveStrengthTest` | night |
| `specular` | A | `SpecularTest` | built world |
| **cascaded shadow maps and cascade transitions** | **B** | **authored**: one long ground plane, one occluder, sun low, camera dollying so a shadow crosses a cascade boundary | built world |
| **SSR and reflection probes** | **B** | **authored**: reflective sphere over a plane with a second object off-screen — off-screen is what separates SSR from a probe | built world |
| **water reflection and refraction** | **B** | **authored**: a flat surface with ior 1.33 over a textured floor, a partly submerged rod for the refraction break | river · sea |
| **decals** | **B** | **authored**: one projected quad on a curved surface, to isolate depth bias and normal reorientation | built world |
| **volumetric fog and god rays** | **B** | **authored**: a light through a slotted occluder into a fog volume | forest · built world |
| **motion blur** | **B** | **authored**: a wheel at declared angular speed, one shutter interval | vehicle |
| **depth of field** | **B** | **authored**: three subjects at declared distances, one declared focus and f-number | any |
| **bloom and tonemapping** | **B** | **authored**: an emissive disc at declared luminance against black | night |
| **TAA** | **B** | **authored**: a static subpixel-detail chart plus the same in motion — the only way ghosting is separable from softening | forest |
| **wet surfaces** | **B** | **authored**: one plane, roughness and specular swept as a wetness parameter | weather |
| **LOD transition** | **B** | **authored**: one subject, declared LOD ladder, camera dollying through each switch distance | forest |
| **overdraw** | **B** | **authored**: N alpha-tested quads stacked at declared depth complexity | forest |
| **hair and cloth** | **B** | **authored**: a card-based hair clump and a draped cloth — geometry we emit, since glTF carries no hair primitive | character |
| **particles: fire, smoke, dust** | **B** | **authored**: a declared quad emitter, fixed seed, fixed times — the particle system is ours, the case pins its output | weather |
| **large-scale terrain** | **B** | **authored**: a declared heightfield meshed at kilometre scale, since no clearable asset was found | world |
| **crowds of skinned characters** | **B** | **authored**: `Fox` instanced at declared counts — a throughput claim one character cannot make | crowd |
| **day/night, dynamic sun and moon · night sky** | — | **excluded, and the reason is the admissible one**: these are not features a subject can carry, they are our own scenario axes (§ I.25) — a time sweep is a run, not a file. The features *under* them (punctual lights, emissive strength, tonemapping) each have a case above |
| `KHR_materials_iridescence` · `dispersion` · `unlit` | — | **excluded: neither KCD nor GTA 5 does this.** No thin-film, no spectral dispersion, and unlit is emissive-only which `Material::Emission` already spells |

- [ ] **The count that matters: 14 kind-A cases fetched, 17 kind-B cases authored, 2 exclusions by scenario axis, 3 exclusions by neither-reference-does-it.** No entry is a gap any more, which is the change this ruling makes — a gap was a place the ladder quietly stopped, and an authored case is a task with a size
- [ ] **Kind B is where the real cost sits and it should be stated rather than discovered**: 17 minimal `.blend` files, each with a script, an export and a reference render. They are small scenes — the Cycles floor measured in § I.26.4 is 2.087 s/frame on a six-quad scene — so the reference cost is minutes, and the cost is **authoring judgement**, not compute
- [ ] Every kind-B case declares **what would make it fail**, because a minimal scene with no failure signature is a picture nobody can read: the cascade case fails as a visible seam at a declared distance · SSR fails by missing the off-screen object · decals fail by z-fighting or by a normal that did not reorient · TAA fails as ghosting behind the moving edge · LOD fails as a pop at a named distance
- [ ] **Where the technique has no counterpart in Cycles at all — TAA, SSR as an approximation, LOD selection — the oracle renders the *ground truth* and the case reports the *approximation error***, which is § I.26's bias-curve shape applied to techniques instead of to bounces. A path tracer's reflection is the correct answer SSR is approximating, so the comparison is meaningful even though parity is impossible
