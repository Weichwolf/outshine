Type: feature
Area: gltf
Tags: khronos, perf, instrument

**I.26.6 What the corpus requires of this engine, one line per capability**

*Owner's ruling, 2026-08-12: **on every feature ask whether Kingdom Come: Deliverance or GTA 5 uses it;
if so Outshine must implement it, and if not it is a `REFUSED` line with that reason.** Both references
are already `CLAUDE.md`'s, so this is the existing standard applied to a list rather than a new one. The
capability is the requirement; **glTF's spelling of it is how the corpus proves we have it.** Where a
reference reaches the effect by another means that is written down rather than rounded — KCD is a
CryEngine fork that predates several of these extensions, so "KCD does fuzzy cloth, but not via a sheen
BRDF" is a finding about what we owe, not a technicality. The corpus was measured at SHA `2bac6f8c…`
by parsing every `.gltf` in it, so the counts below are the corpus's own and not a recollection.*

| Capability | Proven by | KCD / GTA 5 | Verdict |
|---|---|---|---|
| indexed `TRIANGLES` | `Box` — 144 of 146 | both | required |
| non-indexed | `TriangleWithoutIndices` — 3 of 146 | both | required |
| `NORMAL` | 133 of 146 | both | required |
| `TANGENT` as an attribute | `SciFiHelmet` — 34 of 146 | normal mapping in both | required |
| `TEXCOORD_0` | 118 of 146 | both | required |
| `TEXCOORD_1` | `MultiUVTest` — 9 of 146 | second parameterisation for occlusion and decals | required · `UNSURE` on which reference uses it for what |
| `COLOR_0` | `BoxVertexColors` — 7 of 146 | CryEngine vegetation bending and terrain blending | required |
| skinning (`JOINTS_0`/`WEIGHTS_0`/`skins`) | `RiggedSimple` — 6 of 146 | both, every character | required |
| morph targets | `AnimatedMorphCube` — 4 of 146 | facial animation; CryEngine's Facial Editor is morph-target based | required |
| node hierarchy, TRS and `matrix` | all | both | required |
| GPU instancing | `SimpleInstancing` (`EXT_mesh_gpu_instancing`) | vegetation in both; GTA 5 instances transparent geometry in 11 draw calls | required |
| vertex quantization | `KHR_mesh_quantization` | both ship quantized vertex streams; our 32 B vertex is already a budget | required |
| perspective camera | 12 of 146 | both | required |
| orthographic camera | `Cameras` — 1 of 146 | every cascade is an orthographic projection | required |
| base colour factor and texture | 108 of 146 | both | required |
| **metallic**-roughness factor and texture | `MetalRoughSpheres` — 63 of 146 | both | required · **`Material` has no metalness field** |
| normal texture | 55 of 146 | both | required |
| occlusion texture | 46 of 146 | both | required |
| emissive factor and texture | 27 of 146 | GTA 5 at night | required · declared and unreached today |
| alpha `MASK` | 16 of 146 | GTA 5 discards below 0.75; KCD foliage | required · **load-bearing for vegetation** |
| alpha `BLEND` | 13 of 146 | both | required |
| `doubleSided` | 40 of 146 | foliage in both | required |
| sampler wrap, filter, mip | 92 of 146 | both | required |
| `KHR_texture_transform` | 15 of 146 | atlases and scrolling uv in both | required |
| `KHR_materials_emissive_strength` | `EmissiveStrengthTest` — 5 | HDR emissive at night | required |
| `KHR_lights_punctual` | `DirectionalLight`, `PointLightIntensityTest` — 10 | both | required · **a subsystem, not a feature** |
| `KHR_materials_specular` | `SpecularTest` — 9 | dielectric F0 in any PBR | required |
| `KHR_materials_ior` | 17 | water and glass in both | required |
| `KHR_materials_transmission` | 33 — the most-used extension in the corpus | glass in both | required |
| `KHR_materials_volume` | 25 | GTA 5's water absorbs with depth, which is the same integral | required · the glass case is the finding |
| `KHR_materials_clearcoat` | `ToyCar` — 13 | GTA 5 car paint; CryEngine's car paint does colour shifting over a coat | required |
| `KHR_materials_sheen` | `SheenCloth` — 10 | **CryEngine's Cloth shader has a fuzzy layer with its own gloss** — the effect, by another means | required |
| `KHR_materials_anisotropy` | 7 | **CryEngine's Hair shader has had anisotropic highlights with directionality maps since 3.6**; brushed metal and rims in GTA 5 | required |
| `KHR_materials_variants` | `MaterialsVariantsShoe` — 7 | GTA 5 vehicle liveries and modkits | required · cheap, a material-row swap |
| diffuse transmission | `DiffuseTransmissionTest` — 6 | **CryEngine's Vegetation shader's headline feature is translucency (light transmittance)** | required as a **material model**, and `Material::Transmission` is half of it. Proven by the extension's flat test asset as format conformance — **never by a foliage comparison**, which Band III owns |
| `KHR_node_visibility` | `CubeVisibility` — 2 | both | required · trivial |
| `KHR_texture_basisu` | 1 | both ship block-compressed textures; ASTC and BC are native on this device | required |
| `KHR_animation_pointer` | `AnimatedColorsCube` — 5 | animated emissive and uv in GTA 5 | required · `clients/Animation.h` is the mechanism, the glTF spelling is later |
| `LINEAR` · `STEP` · `CUBICSPLINE` | `InterpolationTest` — the only asset with all three | both | required · `core/Keyframes.h` and `core/CatmullRom.h` already carry them |
| `KHR_materials_iridescence` | 10 | neither — no thin-film anywhere in either | **REFUSED** |
| `KHR_materials_dispersion` | 4 | neither | **REFUSED** |
| `KHR_materials_unlit` | 5 | the effect is emissive-only, which `Material::Emission` already spells | **REFUSED** · no second material model |
| sparse accessors | `SimpleSparseAccessor` — 1 | a file compaction, no runtime capability | **BUILT, and outside the 39** — the `REFUSED` verdict is overturned by the ruling below the table |
| `POINTS`/`LINES`/`STRIP`/`FAN` | `MeshPrimitiveModes` — 2 of each at most | debug drawing only; the picture is triangles | **REFUSED** — we draw none of them. The reader carries `mode` all the same; the refusal is the consumer's (§ I.26) |
| multiple scenes | `MultipleScenes` — 1 | neither | **REFUSED** — we render one. The reader reports the whole `scenes` array and the file's default; choosing one is the consumer's (§ I.26) |
| `KHR_draco_mesh_compression` | 1 | neither ships Draco; both use their own formats | **REFUSED** — a large vendored decoder for no picture |
| `EXT_meshopt_compression` | 1 | as above | **REFUSED** |
| `EXT_texture_webp` | 1 | neither | **REFUSED** — a second image decoder buys no comparison |
| `KHR_xmp_json_ld` | 2 | metadata, not rendering | **REFUSED** |
| `KHR_materials_pbrSpecularGlossiness` | 1 | archived by Khronos | **REFUSED** |

- [ ] **The denominator is the required rows of the table above and the percentage is read against it** — *features the references use* — never against every glTF extension that exists. *Recounted 2026-08-12 against the table's own rows: **39 required and 11 refused**, 50 rows in all. The line read 40 and 11, which is one more required row than the table has ever carried; the refusal count is right. Every percentage in this section is therefore read against 39, and the two lines below are the first to say so.* The sparse-accessor row is **outside** the denominator in both directions — it is not a picture capability, so it enters no numerator either
- [ ] **The sparse-accessor `REFUSED` is overturned, and the reason it was refused is the part worth keeping.** *"A file compaction, no runtime capability"* is **true and was the wrong test**: nothing in this section's own rule — *does KCD or GTA 5 use it* — can answer a question about a file's encoding, because neither ships glTF at all. What decides it is the cost of the two answers: refusing means refusing `SimpleSparseAccessor` **whole**, and § I.26.6's closing line requires every capability to be proven by a named corpus asset, so a refusal here deletes an asset from the corpus rather than a feature from the picture. Implementing it cost **37 lines** (`src/gltf/Document.cpp` `ApplySparse`) and **cannot produce a wrong picture** — it either resolves the overrides or refuses the file. Built and ticked under § I.26 above
- [ ] **Today the engine reads 10 of the 39 through glTF — 25.6 %** — and the baseline that stood here was `0 of the 40, because there is no glTF reader`, which was true when it was written and is now stale by one commit (`e658b21`). **The recount, row by row against `src/gltf/`, so the next round can check it rather than trust it.** Read: indexed `TRIANGLES` · non-indexed · `NORMAL` · `TANGENT` · `TEXCOORD_0` · `TEXCOORD_1` · `COLOR_0` · node hierarchy with TRS and `matrix` · perspective camera · orthographic camera. **Not read, and the reason is one fact each**: skinning and morph targets need `skins`, `targets` and `mesh.weights`, and none of the three is parsed · GPU instancing, quantization, `basisu` and every `KHR_materials_*` need an `extensions` object, and the reader parses none · everything from base colour to `doubleSided` needs `materials`, `textures`, `images` and `samplers`, and `Primitive::Material` is an index into an array that is never read · `KHR_animation_pointer` and the three interpolations need `animations`. **The five attribute rows — `NORMAL`, `TANGENT`, `TEXCOORD_0`, `TEXCOORD_1`, `COLOR_0` — are read for one reason, and it is a design decision rather than five features**: an attribute is a name and not a slot, so every semantic a file carries crosses at once
- [ ] **6 of those 10 are held by an asserted claim and 4 cross untested**, which is the number worth watching rather than the 10. Held: indexed `TRIANGLES`, `NORMAL`, `TEXCOORD_0`, the node hierarchy, and both cameras. Untested: **non-indexed** (four primitives in the fixture carry no `indices` and nothing asserts `Indices == -1`), `TANGENT`, `TEXCOORD_1`, `COLOR_0` — all four cross only because the attribute path is generic, and a generic path that no case names is a capability nobody has looked at. The closing line of this section already requires a named corpus asset per capability; these four are where it bites first
- [ ] **As raw capability, independent of the boundary, the engine expresses 18 of the 40 — 45 %** *(the denominator is 39, so the fraction is 18/39 = 46.2 % if the numerator still holds. It was not re-counted this round and is deliberately left standing rather than restated — a numerator nobody checked, divided by a denominator somebody did, is a worse number than the one it replaces)* — and the number is generous by construction, because it counts *the engine can express this quantity*, not *a glTF value reaches it*. Present: indexed triangles · `NORMAL` · a tangent frame (`core/TangentFrame.h`) · `TEXCOORD_0` (`core/ChunkVtx.h`) · instancing (`render/stages/ModelDraw.h`) · perspective and orthographic projection · base colour · roughness · alpha `MASK` and `BLEND` (`Material::Coverage`) · `doubleSided` · sampler settings · transmission and ior (`core/Material.h`) · `LINEAR`, `STEP` and `CUBICSPLINE`. **Absent: metalness has no field at all**, and neither do occlusion, clearcoat, sheen, specular, anisotropy or volume
- [ ] **`Material` gains a metalness field**, because it is the one required quantity of the metallic-roughness model that has **no spelling** in this tree: `core/Material.h` carries albedo, roughness, coverage, transmission, ior and emission and nothing that separates a conductor from a dielectric. It switches no pipeline state, so it is a material row entry by the core's own rule, and rung 17 is what first requires it
- [ ] **A light list, and it is a subsystem rather than a feature.** `render/Gpu.h:22 SceneLight` binds one irradiance pair, one cascade buffer and one shadow atlas — *"one light, one scale, one set of cascades, so no surface can end up lit by a second sun"*. That invariant is **right for the sun and wrong as the whole lighting model**; § II.8 already owes point and spot lights as a list, and rungs 14 and 21 are what pull it. The shape to prefer keeps the sun's uniqueness unspellable while making the list ordinary: the sun stays a distinguished binding, the list is a second one, and no surface can bind two suns because there is still only one field for it
- [ ] Skinning, morph targets, `COLOR_0` and `TEXCOORD_1` are **four new vertex-stream capabilities and one new vertex layout question**: our layout is `pos3 · uv2 · nrm3` at 32 B with a declared 24 B second (`core/ChunkVtx.h`), and neither has a spelling for joints, weights, a second uv or a colour. That is a design question this section raises and does not answer — a fifth layout per capability is the wrong answer, and so is one fat layout
- [ ] `Material` gains an **occlusion strength** entry with the same argument as metalness — it is a number, it switches nothing, and 46 of 146 corpus assets carry an occlusion texture. It is **not** the same quantity as `render/stages/AoStage`'s screen-space term and the two must not be summed silently
- [ ] The § I.26 reader line's *"Skinning, morph targets and `animations` out of scope"* is **superseded by this section**: all three are required capabilities, pulled by rungs 6, 13 and 16 respectively. *The earlier line was right about the reader's first subset and wrong as a scope statement, and the difference is the ruling above*
- [ ] Every required capability is **proven by a named corpus asset and only by that asset** — a capability with no asset behind it is untested scope, and an asset that proves nothing is bytes we fetch for no reason
