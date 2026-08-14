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

## The execution order, and the criterion that produces it

**The owner, verbatim:** *implement the glTF features one by one ordered by complexity or impact.
animations when required.*

**IMPACT IS ALREADY IN THIS TABLE AND I HAD NOT READ IT.** The `Proven by` column carries a per-feature
model count — *144 of 146*, *133 of 146*, *46 of 146* — so **45 of the 50 rows already have their impact
measured**; five carry none and are named below. *The number the ordering needs was in the artefact being
ordered.* **The denominator is 146 here and the pinned index enumerates 148** — a two-model discrepancy
that is recorded rather than smoothed, and that `board:1172`'s declared set settles.

**Impact-first is wrong at the top of this table, and measurably so.** The four highest rows — indexed
`TRIANGLES` 144, `NORMAL` 133, `TEXCOORD_0` 118, base colour 108 — **are already supported and proven**.
Ordering by raw usage sequences work that is done. So:

> **Impact means: how many in-scope models are blocked SOLELY by this feature.** Usage is the proxy the
> table holds; the ordering key is usage **restricted to rows not already supported-and-proven**.

**Which is why `board:1174`'s re-measurement comes first and is not merely tidy: the ordering criterion is
undefined on a row whose population is unknown.**

**Complexity's honest unit is which layers move**, and `board:0078` carries the precedents:

| | |
|---|---|
| **reader only** | a field parsed and carried — `multiple scenes`, `KHR_xmp_json_ld` |
| **reader + draw list** | a vertex stream or a draw parameter — `COLOR_0`, `TEXCOORD_1`, primitive modes, skinning |
| **a lobe** | a term in the BRDF and numbers in the material row — `sheen`, `clearcoat`, `specular` |
| **a resource and a stage** | `transmission`/`volume`, which need the scene colour behind the surface |

## The rule where they disagree, and it is not *impact first, complexity as tie-break*

**Two tiers, worked to exhaustion in order; impact orders within a tier.**

> **Tier 1 — reader-only and reader+draw-list rows. Tier 2 — lobes and new-stage rows.**

**The reason is the owner's own execution rule rather than a preference: *one by one* requires that a
feature can be FINISHED before the next starts.** A tier-1 row is one dispatch — a field, a stream, a
test, a citation. A new-resource row is a multi-round project, and starting one stalls the sequence for
everything behind it. **A bounded look-ahead is the wrong instrument for this** because it schedules by
how long a queue is; the tier rule schedules by whether an item can be completed at all, which is the
property the owner's sentence actually constrains.

**Impact zero is a finding about the feature, not a low rank**: a row no in-scope model needs is a
candidate for the declared-out list in `board:1172`, and it leaves the sequence rather than sitting at the
bottom of it.

## The first three, and the first one keeps its place on the criterion rather than on its name

- [ ] **1 · the animation interpolations — `LINEAR` · `STEP` · `CUBICSPLINE`.** Impact **16 in-scope
  models blocked by one missing consumer**, the largest blocked-count on the board; tier 1, reader plus
  draw list; `core/Keyframes.h` and `gltf/Track` already exist and nothing drives a draw from a time.
  **`board:1169` is mid-flight and it earns first place on the measure, not because it was named earlier.**
  *Animations when required* — and they are required by sixteen models
- [ ] **2 · the occlusion texture. Impact 46 of 146 — the highest unmet impact in the table — and it is
  supported-and-unproven in the worst way.** [MEASURED]: `src/gltf/Document.cpp` parses `occlusionTexture`
  and the mapped shader arm samples `orm.g` for roughness and `orm.b` for metalness and **never `orm.r`**.
  The map is read, carried, and thrown away. **Tier 1 in cost — a material row field, an ambient term and
  a strength multiply — against 46 models**, which is the best impact-per-layer ratio on the board
- [ ] **3 · `TEXCOORD_1`.** Impact 9, tier 1, a second uv stream and the material rows that select it; the
  row already carries an `UNSURE` on which of the material's textures may name it, which is the question
  the task settles

**Behind those three, in tier 1 by impact**: `KHR_texture_transform` 15 · `COLOR_0` 7 · skinning 6 · morph
targets 4 · non-indexed 3 · `KHR_node_visibility` 2 · **`POINTS`/`LINES`/`STRIP`/`FAN` 2** and **multiple
scenes 1**, the last two being the rows whose `REFUSED` the owner's scope ruling overturned
(`board:1174`). **Tier 2 by impact**: `volume` 25 · `ior` 17 · `clearcoat` 13 · `sheen` 10 · `specular` 9
· `anisotropy` 7 · diffuse transmission 6 · `transmission`, whose count the table does not carry.

**Five rows carry no model count and cannot be ordered until they do**: `node hierarchy, TRS and matrix` ·
GPU instancing · vertex quantization · `KHR_materials_transmission` · the interpolation row itself. **Four
of the five are high-usage or already-built**, so the missing number is a gap in the table rather than a
sign of low impact — and it is `board:1174`'s to fill.

**No task is filed for any row here.** The order is the artefact; a task arrives when its row is
dispatched, with its own test and its own citation. **Fifty ready tasks worked one at a time is
forty-nine items that are ready and not being done**, which is the shape two rounds were spent undoing.

## Row 2 is NOT tier 1, and no task is filed for it — the occlusion texture is gated twice over

**The specification settles what it multiplies, quoted rather than recalled**: the occlusion value is read
from the **red** channel, **"Direct lighting is not affected"**, it indicates *areas that receive less
indirect lighting from ambient sources*, and `strength` applies as
`1.0 + strength * (occlusionTexture - 1.0)`.

**So a spec-correct implementation attenuates indirect light, and this engine has none in the path that
would use it.** [MEASURED] in the catalogue: `Stage::Subjects` reads **`{kNoEdge}` — nothing at all**,
while `Terrain`, `Buildings`, `Water` and `Models` each read `IrradianceBuffer`. **The ambient term exists
in the plan and the subject path does not consume it.**

**And the oracle has nothing to attenuate either.** [MEASURED] over every render declaration in the
corpus — **61 declarations across 36 cases — `bounces.max` is 0 in every one.** A reference with zero
bounces carries no indirect light, so **even with an ambient term on our side there would be nothing to
compare against.**

| | |
|---|---|
| implement it correctly today | **changes zero pixels** — nothing to multiply, on either side |
| implement it so pixels move | **multiplies direct light**, which the specification forbids in one sentence |

**A feature whose correct implementation is a no-op cannot be proven by a render**, and the owner's rule
is *supported and **tested***. **So the row is re-ranked rather than dispatched**, and no task is filed —
this table's own rule is that a task arrives when its row is dispatched, and dispatching one that cannot
terminate is worse than leaving the row where it is.

**It is gated on two things and one of them is already filed:**

- [ ] **The subject path gains an indirect term** — `Stage::Subjects` reads `IrradianceBuffer`, which the
  catalogue already expresses for four other stages, so this is an edge rather than an invention
- [ ] **An oracle recipe with indirect light**, which is `board:1150` and **needs a parameter that item
  does not yet carry** — see the note added there. `board:0087`'s reason for lowering these materials to
  emitters was that *a Diffuse BSDF at one sample per pixel is a Bernoulli draw on the visible sky
  fraction*; at the integrating recipe's sample count that objection dissolves, which is precisely why
  occlusion belongs behind `1150` and not in front of it

**Where the proof will live when it is unblocked**: `water-bottle`, `boom-box`, `corset` or `lantern` —
the lit metal-rough cases whose ORM maps carry a red channel. **`scifi-helmet` cannot prove it** at any
point: its scene declares `light: none` and its oracle is lowered to an emitter, so nothing shades.

## The revised first three

- [ ] **1 · the animation interpolations — DELIVERED** by `board:1169`; `board:1175` carries the reduction
  question for the two interpolations still open
- [ ] **2 · `KHR_texture_transform`, impact 15.** Tier 1 — a reader field and a uv transform in the shader
  — **and genuinely absent**: [MEASURED] zero occurrences of `texture_transform` in `src/`. It is
  ratified, so it is in scope under the owner's ruling; it moves texels visibly, so a render proves it;
  and it is the highest-impact unmet tier-1 row once occlusion is gated
- [ ] **3 · `TEXCOORD_1`, impact 9** — a second uv stream and the material rows that select it, with the
  row's own `UNSURE` on which textures may name it settled by the task

**The occlusion row keeps its impact of 46 and loses its place.** *Impact is what makes a row worth doing;
being provable is what makes it dispatchable*, and the ordering rule was missing the second half until
this row demonstrated it.

## Row 2 delivered, and row 3 confirmed with a second reason

**`KHR_texture_transform` landed** (`board:1177`, closed). Its case is red at **9.5930063** against the
bound's `6.4354338`, **attributed and not absorbed**: the mip-chain attribution **refuted itself by
measurement** — `kChainIsReadable` takes the metric **9.593 → 186.118** — and what survives is the
sub-texel weight term at a **whole** 2⁸ division, `255·12.92/256 = 12.8695`, with `texture-coordinate-test`
at `10.295625` by the same arithmetic. **`board:1151` now has two cases pointing at one derivation**,
which is worth more to it than either alone.

**ROW 3 IS `TEXCOORD_1` AND IT NOW HAS A SECOND REASON.** Impact 9 of 146 was the first; the second is
that it **unblocks 9 of `TextureTransformMultiTest`'s 27 cells** (`board:1180`), which is a third of a
case the corpus already contains. Tier 1: a second vertex stream, the layout that carries it, and the
material rows that select it.

**AND IT INHERITS A BOUNDARY IT MUST NOT QUIETLY REMOVE.** `board:1177` established that a texture
reference whose `texCoord` names set 1 on a subject carrying one uv set is a **named refusal**. **When row
3 lands, that refusal must narrow, not disappear**: a transform naming set 1 on a subject that *has* set 1
resolves; on a subject that does **not**, it stays a refusal. **The refusal is about the subject's
attributes, never about the engine's capability**, and turning it into a fall-back to set 0 would make a
missing attribute render as a plausible picture — which is the silent-success class this tree files
against.
