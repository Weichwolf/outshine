Type: feature
Area: gltf
Tags: oracle, khronos, instrument

**I.26.5 Not a ladder but a matrix with a dependency order, and the integration scenes sit at the joins**

*Owner's ruling, 2026-08-12: **"21 rungs" is the wrong shape.** Once every feature KCD or GTA 5 uses
gets its own basic isolated case (§ I.26.8), the structure is a **feature matrix with a dependency
order**, not a line. The table below is the matrix's **spine** — the path a first implementation walks
— and it is no longer the whole thing.*

**The ladder grows wide before it grows tall.** Every basic case for a feature **precedes** the complex
scene that combines it, and the complex scenes are **integration** rather than difficulty: the forest
integrates instancing, alpha test, two-sided transmission, LOD and overdraw, each of which has its own
isolated case first. So the order is:

```
  basic cases          integration scene        what a red means
  ───────────────      ─────────────────        ────────────────
  A: alpha modes   ┐
  B: overdraw      ├──▶ forest (§ I.26.7)  ──▶  a combination, because every part is green
  B: LOD, A: inst. ┘
  A: metal-rough   ┐
  A: transmission  ├──▶ built world        ──▶  a combination
  B: cascades, SSR ┘
  A: skinning      ┐
  A: interpolation ├──▶ film (§ I.26.4)    ──▶  a combination
  spine rungs 1–20 ┘
```

- [ ] **An integration scene is never run before every basic case it combines is green** — the ladder's own rule, restated for the matrix. Rung *n* waiting for *n−1* was the linear form of it; the general form is *a join waits for its inputs*, and it is what keeps a red naming one thing
- [ ] **A basic case is owned by exactly one feature and a feature is owned by exactly one basic case.** Two cases for one feature means neither is the answer when they disagree; one case for two features is the bundling the one-new-thing rule exists to prevent — which is why `DamagedHelmet` was refused for carrying five
- [ ] **The spine below is the walk order for a first implementation, not the matrix.** It is kept because somebody has to start somewhere and because rungs 1–4 genuinely must come first — the reader, the projection and the raster convention are prerequisites of every case in both kinds

**The spine, twenty-one rungs, easy to hard**

*Owner's ruling, 2026-08-12, in two steps: **implement from easy to hard, motion last** — then refined,
**simple animations can be in between; difficulty decides the stages**. The ordering principle is
therefore **how many independent things can be wrong at once**, and each rung adds exactly one. Motion
enters at rung 6, as early as it honestly can, because a moving cube whose static form does not match
teaches nothing — and because motion adds a **time axis to every comparison**, so a residual under an
unsettled static rung is unattributable between a geometry error and a sampling-time disagreement. Every
animated rung sits above the static rung it depends on. The synthetic floor is **generated here**
(principle 2); everything else is fetched under § I.26.1's manifest.*

| # | Asset | Source · licence | The one thing it adds | Judgeable on | Fixture |
|---|---|---|---|---|---|
| 1 | triangle | **ours**, generated | the reader, the projection, the raster convention | coverage | 1 |
| 2 | quad, off both axes | **ours**, generated | depth varying across the frame | coverage · depth | 2 |
| 3 | ±1 m cube, 12 tris | **ours**, generated | indices, winding, back-face culling | coverage · depth | 3 |
| 4 | UV sphere 32×16 | **ours**, generated | a curved silhouette — every boundary pixel its own sub-pixel offset | coverage · depth | 4 |
| 5 | **Menger sponge, level 1–2** | **ours**, generated | genus and depth complexity at *known* topology | coverage · depth | — |
| 6 | **`BoxAnimated`** + `InterpolationTest` | Khronos · CC-BY-4.0 / CC0-1.0 | **time, and nothing else** — one object, TRS, no light | coverage · depth | — |
| 7 | cube lit by one `SUN` | **ours**, generated | the first radiance number | + direct radiance | 5 |
| 8 | sphere lit by the same | **ours**, generated | the whole `cos θ` sweep in one image | + direct radiance | 6 |
| 9 | albedo + emissive spheres | **ours**, generated | channel linearity and the emissive path | + direct radiance | 7 |
| 10 | **`SimpleTexture`** → `TextureCoordinateTest` | Khronos · CC0-1.0 | uv, sampling, base-colour sRGB decode, at 1 texel per pixel | coverage · direct radiance | 10 |
| 11 | **`NormalTangentTest`** | Khronos · CC0-1.0 | tangent-space handedness | + direct radiance | — |
| 12 | **`AlphaBlendModeTest`** | Khronos · CC-BY-4.0 | **format conformance**: `OPAQUE`, `MASK` with `alphaCutoff`, `BLEND` and its ordering | conformance — **not** a coverage rung | — |
| 13 | **`AnimatedMorphCube`** → `MorphStressTest` | Khronos · CC0-1.0 / CC-BY-4.0 | vertex-level animation over a rung already green | coverage · depth | — |
| 14 | cube under the factory point light | **ours**, generated | inverse-square falloff — the literal default lighting | + direct radiance | 8 |
| 15 | cube on a plane, sun at 30° | **ours**, generated | a cast shadow and the first non-zero interreflection | + shadow and indirect | 9 |
| 16 | **`RiggedSimple` → `RiggedFigure` → `Fox`** | Khronos · CC-BY-4.0 (+CC0-1.0) | skinning, at rising joint counts | coverage · depth | — |
| 17 | **`MetalRoughSpheres`** | Khronos · CC-BY-4.0 | the full roughness × metalness sweep | + direct radiance | — |
| 18 | **`SciFiHelmet`** | Khronos · CC0-1.0 | one real asset's material stack | coverage · direct radiance | — |
| 19 | **`ABeautifulGame`** → Barcelona Pavilion | Khronos · CC-BY-4.0 · eMirage · CC-BY | scene scale — draw counts, occlusion, many materials | coverage · depth | — |
| 20 | **quaternion-Julia isosurface, high subdivision** | **ours**, generated | the stress rung: triangle count with *unbalanced* spatial distribution | coverage · depth | — |
| 21 | **the film** (§ I.26.4) | Barcelona Pavilion + `Fox` + our camera path | everything at once, over time, with a per-frame reference | the difference series | — |

- [ ] **`DamagedHelmet` is refused twice over and rung 18 is `SciFiHelmet` instead.** Its normal, occlusion, emissive, base-colour and metallic-roughness textures are **CC BY-NC 4.0** (theblueturtle_, 2016, in the model's own `metadata.json` at the pinned SHA) — non-commercial, which is not a free cultural work and which would put a use restriction on this repository's test suite. And it bundles **five** new things at once, which is exactly what the one-new-thing rule exists to prevent. `SciFiHelmet` is CC0-1.0 entire, carries `POSITION · NORMAL · TANGENT · TEXCOORD_0` with base-colour, metallic-roughness, normal and occlusion textures, and adds **occlusion** alone over rung 11 — one thing
- [ ] **`Sponza` is refused on licence, and the refusal is not close.** The Khronos copy is *"© 2016, Crytek. Cryengine Limited License Agreement"* — a proprietary EULA, not a Creative Commons licence. Rung 19's scene-scale role is carried by `ABeautifulGame` (CC-BY-4.0, ASWF and Ed Mackey, 42.0 MB) and by **Barcelona Pavilion** (eMirage, CC-BY, 24.7 MB, a Blender demo archive), which is the better of the two for us: it is **architecture**, which is what GTA 5 is the reference for, and it is hard-surface and image-textured, so it survives glTF export
- [ ] **`BrainStem` is refused on licence** — *"© 2017, Smith Micro Software, Inc. Poser EULA"*. Its skinning role is carried by `RiggedSimple` → `RiggedFigure` → `Fox` at rung 16, which is a **rising joint count** and therefore a better ladder than one asset
- [ ] **`BoxTextured` is refused and `SimpleTexture` takes rung 10.** `BoxTextured` is *"CC-BY 4.0 International with Trademark Limitations"* plus a Cesium trademark, and it is on Khronos's own Khronos’s own Models-issues list list of models with licence or ownership issues — together with `AntiqueCamera`, `BoxTexturedNonPowerOfTwo`, `CesiumMan`, `CesiumMilkTruck`, `PrimitiveModeNormalsTest` and `RecursiveSkeletons`, all seven of which are out by that list alone. `SimpleTexture` and `TextureCoordinateTest` are CC0-1.0 and carry the same uv path
- [ ] **`Duck` is refused**: SCEA Shared Source License 1.0 (Sony, 2006) — not a Creative Commons licence and not on the allow-list
- [ ] **The fractal is confirmed as the right stressor, and it is two generators rather than one — the instinct was right about *what* and wrong about *which*.** A subdivided sphere is the **best case for every spatial partition**: convex, genus 0, uniform vertex density, one component, near-spherical cluster bounds at any cut, depth complexity 2. It cannot fail a cluster build. What it cannot produce, and what our one cluster DAG is judged on (`core/ClusterDag.h`), is **unbalanced occupancy** and **topology that survives simplification**
- [ ] **A Menger sponge is rung 5 because its topology is *known*.** At level n it is 20ⁿ cubes with a genus that is closed-form, so *"the LOD closed three holes"* is a **number** rather than an impression — and screen-space error alone cannot see a closed hole, because the silhouette barely moves while the shading changes completely (`Real-Time Rendering` 4e, ch. 19 on LOD error metrics). It also gives **depth complexity** along its axes, which a convex body cannot measure at all, and **exact coplanarity at many depths**, which is the deliberate-tie fixture § I.26 already asks scene 1 for
- [ ] **A quaternion-Julia isosurface is rung 20 because its distribution is *unbalanced*.** A 4D Julia set sliced to 3D and meshed at rising grid resolution gives dense filigree in some regions and emptiness in others, with wildly uneven triangle size — which is close to the worst case for a surface-area-heuristic partition, where a sphere is the best (`Real-Time Collision Detection`, ch. 6 on BVH construction and the SAH). The measurable product is the ratio of **cluster bound volume to enclosed geometry**, taken on both bodies: the sphere gives the floor, the Julia set gives the ceiling, and our DAG sits between them
- [ ] Both fractals are **functions, not files** (principle 2), and both give arbitrary triangle counts at a declared parameter — which is what makes rung 20 a *stress* rung with an axis rather than one big model
- [ ] The synthetic floor — triangle, quad, cube, sphere, sponge, Julia — is generated by the same code that emits the ten fixtures, so a coverage failure at rung 1 has **one** cause: we control every vertex position exactly and none of them came from a file

## Comments

**The matrix's honest state, read from the assets rather than from the directory names.** 34 cases over
19 `.gltf` on disk (some share an asset; some subjects are generated).

**Declared today:** `KHR_lights_punctual` ×14 · `KHR_materials_emissive_strength` ×5 ·
`KHR_materials_volume` ×3 · `KHR_materials_transmission` ×3 · `KHR_materials_unlit` ×2 ·
`alphaMode: MASK` ×4 · `alphaMode: BLEND` ×1 · `TANGENT` ×11.

**Absent entirely: `COLOR_0`, `TEXCOORD_1`, `JOINTS_0`, `WEIGHTS_0`** — no vertex colours, no second UV
set, no skinning — and no animation of any kind, which is `board:1128`.

**The gap, ordered by what the two named targets actually use** rather than by what glTF offers:

| missing | needed by | why it is not optional |
|---|---|---|
| animation · skinning · morph | characters, anything that moves | `board:1128`; the reader parses it and no case renders it |
| `TEXCOORD_1` | KCD's terrain and interiors | a lightmap or baked AO on a second UV set is how a built world gets cheap indirect light |
| `COLOR_0` | KCD's vegetation | per-instance tint is how a forest stops looking like one cloned tree |
| `KHR_texture_transform` | both | authored assets atlas and offset constantly; a renderer that ignores it draws the wrong texel |
| `KHR_materials_clearcoat` | GTA 5's cars | car paint is the canonical clearcoat surface |
| `KHR_materials_sheen` · `anisotropy` | cloth, brushed metal | present in the Khronos corpus, absent here |
| `KHR_draco_mesh_compression` · `KHR_texture_basisu` | any real asset pipeline | not picture features — **fetch-path features**, and a case that cannot be read is a case that cannot be judged |

**`alphaMode: MASK` is covered ×4, which matters more than the count suggests**: foliage cutout is *the*
vegetation technique and a SpeedTree-class leaf is alpha-tested. That box is ticked before the foliage
stage reaches for it.

**Caveat on this enumeration**: it says what the corpus *declares*, not what the engine *does with it* —
`KHR_materials_volume` appearing three times does not mean transmission is right, and three of those
cases are among the thirteen outside the picture bound.

## The extension band, and the three families it splits into

*Added on the owner's ruling that **glTF 2.0 with extensions** is what a modern engine must process
(`board:1172`). The matrix above is the core spine; this is the band beside it, and it is grown here
rather than filed as a second matrix.*

**The engine anticipated part of this and it is measured rather than assumed.** The reader already knows
four extension names by occurrence in `src/` — `KHR_lights_punctual` 13, `KHR_materials_unlit` 8,
`KHR_materials_emissive_strength` 3, **`KHR_materials_transmission` 1** — and `Material` already carries
`Transmission`, `Ior` and `Unlit` beside the metal-rough row, with `SurfaceState` distinguishing
`Refractive` (`Transmission > 0 && Ior > 1`) from `ThinTransmissive`. **One occurrence is the signature of
*supported and unproven***, which is `board:1174`'s middle population and where transmission almost
certainly sits.

**Three families, and their dependency orders are unrelated — which is why they are not one band.**

- [ ] **Transmission and volume** — `KHR_materials_transmission` · `volume` · `dispersion`. **The only
  family that needs something the render plan has not got: a read of the scene colour behind the
  surface.** `board:0088` already names these assets unreducible for the oracle, and `SurfaceState`
  already sorts them, so the draw-order half is anticipated
- [ ] **The extra lobes** — `specular` · `ior` · `sheen` · `anisotropy` · `clearcoat` · `iridescence`.
  Each is a term in the BRDF and a few more numbers in the row. **Their cost is not the mathematics, it is
  the permutation count** — `board:1156` measures 15 fragment arms times three target families today, and
  a lobe that can be present or absent multiplies that again
- [ ] **Reader-level extensions** — `KHR_draco_mesh_compression` · `EXT_meshopt_compression` ·
  `EXT_mesh_gpu_instancing` · `KHR_texture_transform` · `KHR_materials_variants` ·
  `KHR_animation_pointer`. **This family is not a renderer question at all.** Two of them are decoders,
  and `CLAUDE.md` allows no vendored third-party tree — *a dependency is a package the host provides, or
  it is ours* — so **Draco and meshopt are a dependency decision before they are an implementation**, and
  that decision is the owner's

## Does any of these collide with *a material is a row of numbers*? — no, and the two near misses are named

`CLAUDE.md`: **the core dictates the pipeline; a material is a row of numbers with no field that can
switch pipeline state; generator bakes or renderer implements, and there is no third path where content
ships a program.**

**No extension in the registry asks content to ship a program**, so none collides with the rule as
written — that door was closed by glTF 2.0 itself when it removed shaders from content, which is the same
sentence `CLAUDE.md` cites. **Two come close enough to state:**

- **Transmission needs a scene-colour read, which is not a material question and is not a violation.** It
  is a **resource and a stage** — the plan already has `Reads`/`Writes`/`Contributes` and already refuses
  reading a target of the pass one is in, so the answer is an explicit scene-colour copy with an edge,
  exactly as `SceneLinear` aliases `SceneHdr` today. **A catalogue row, not an exception.**
- **`KHR_materials_unlit` selects a shading arm from a declared field**, which reads like a field
  switching pipeline state — and is already built as `Row.Unlit`. **The rule forbids content shipping a
  program, not a declared enumeration choosing among arms the renderer implements.** It is the precedent
  every later lobe extension follows, and it is stated here so the next one is not argued from scratch

**What would be a real collision, so it is recognisable if it ever arrives**: an extension whose behaviour
is a function the file supplies rather than a value it declares. Nothing in the registry does that today.
