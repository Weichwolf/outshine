Type: feature
State: open
Area: gltf
Tags: scope, khronos
Supersedes: 0106, 1387, 1390, 1391, 1393, 1394, 1397, 1405

# The engine reads all of glTF 2.0 and every ratified KHR extension

**Benchmark** — Khronos states the format and ships the corpus and the validator, so the vendor is the oracle. Unreal and RAGE both read many formats and neither reads one COMPLETELY. **Taking the vendor** — this is the one capability someone else can certify.

glTF is the only content surface, so what the engine may assume about content is the whole
format. [MEASURED] at the Khronos registry: 20 ratified KHR extensions; `kHonouredExtensions`
(src/import/Document.cpp) names those whose BEHAVIOUR is built, which is the rule this feature
keeps — generator bakes or renderer implements, never a field that parses and does nothing.

| missing | shape | what it is |
|---|---|---|
| `KHR_materials_volume` | data | thickness, attenuation — the inside of a transmissive body; needs transmission |
| `KHR_materials_dispersion` | data | index of refraction per wavelength; needs volume |
| `KHR_materials_anisotropy` | data | built and unproven — the lobe stands, `anisotropyTexture` is not read and no corpus case shades one |
| `KHR_materials_sheen` textures | data | the two factors are honoured, `sheenColorTexture`/`sheenRoughnessTexture` are not; `SheenCloth` is made of them |
| `KHR_draco_mesh_compression` | structural | decides the dependency question first: a host package or ours. Off the frame path |
| `KHR_texture_basisu` | structural | KTX2/Basis transcoded to the DEVICE's own compressed format, or it buys nothing |
| the rest of the thirteen | — | one row each when it is taken |

Archived and in-development extensions are outside this feature, named so their absence is a
choice.

## What will be true

- [ ] Each row above closes with behaviour and a case, and `kHonouredExtensions` grows only then.
- [ ] `data:` URIs decode — base glTF, and the one gap the reader names about the core.
- [ ] **The base is audited once by enumeration**, the specification's property tables against
      the reader, each DROPPED property becoming its own item. Three found by reading were
      repaid (`ShapeAllowed`'s fallthrough, POSITION min/max, T/R/S output counts); the walk
      itself is still owed.
- [ ] The honoured list and the registry are compared by a case, so a newly ratified extension
      shows as a red count.
