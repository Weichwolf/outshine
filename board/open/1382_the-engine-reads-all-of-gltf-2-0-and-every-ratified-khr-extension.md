Type: feature
Area: gltf
Tags: scope, khronos
Supersedes: 0106

**The engine reads all of glTF 2.0 and every ratified KHR extension**

**glTF is the backbone and not an implementation detail**, so what the engine may assume about content
is the whole format -- not the subset one corpus happens to exercise. `0106` listed the extensions a
particular picture needs and was correct and too narrow: it made the format's coverage a function of
what was being drawn that month.

## The population, counted from the registry rather than remembered

[MEASURED] at the Khronos glTF registry: **20 ratified KHR extensions.** The reader honours **7**:
`KHR_lights_punctual`, `KHR_materials_emissive_strength`, `KHR_materials_ior`,
`KHR_materials_specular`, `KHR_materials_unlit`, `KHR_materials_variants`, `KHR_texture_transform`.
**Thirteen are missing**, and each is one task under this feature.

*In-development and archived extensions are deliberately outside this feature.* An unratified extension
can still change, and building against a moving specification buys a rewrite; `KHR_materials_pbrSpecularGlossiness`
and `KHR_xmp` are archived and are their own decision. **They are named here so their absence is a
choice rather than an oversight.**

## What "implemented" means, and it is the existing rule

`kHonouredExtensions` is the claim, and an extension is added to it **in the round its behaviour is
built** -- not when its fields parse. That rule already stands in `src/gltf/Document.cpp` and this
feature does not relax it. **Generator bakes, or renderer implements; there is no third path where a
field is read and nothing does anything with it.**

**Three shapes and the precedents already exist**, so no task starts from nothing:

| shape | what it does | precedent |
|---|---|---|
| **data** | adds numbers to a material or texture row; composed at the reader; defaults are the consumer's identity, so absence and presence-with-defaults are one computation | `KHR_texture_transform` (`board:1177`) |
| **selection** | maps something to something else and is resolved away before a draw list exists | `KHR_materials_variants` (`board:1188`) |
| **structural** | changes what geometry or which nodes exist at all | none yet -- `KHR_mesh_quantization` and `KHR_node_visibility` are the first |

## What must be true

- [ ] **Thirteen tasks, one per missing ratified extension**, each closing with behaviour and a test
- [ ] **`data:` URIs decode**, which is base glTF and the one gap the reader names about the core
- [ ] **glTF 2.0 base is audited against the reader once**, so a silently-ignored core feature is found
  by enumeration rather than by an asset
- [ ] **The honoured list and the registry are compared by a test**, so a newly ratified extension shows
  up as a red count rather than as nothing

## Comments

**The oracle does not bound this.** [OWNER] *What Blender cannot do is built in Outshine anyway, and
correctness is judged directly.* `KHR_node_visibility` is the first case of it: Blender 5.2's importer
refuses the extension outright, and that decides which INSTRUMENT judges the result, never whether the
capability is built.
