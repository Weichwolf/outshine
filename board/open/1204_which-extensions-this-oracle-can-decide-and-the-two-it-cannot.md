Type: task
Parent: 0079
Area: corpus
Tags: oracle, khronos, instrument, scope

**Which extensions this oracle can decide, and the two it cannot**

`board:0079`'s sequence has one criterion it cannot evaluate from its own table: *only where a render can
actually prove it*. **Whether a render can prove a row is a property of Blender's importer**, and it had
never been measured — the occlusion row's third gate was found by accident while implementing something
else. This is that question asked of every row at once.

## The two rows this oracle cannot decide, and they fail in opposite ways

| | how it fails | measured |
|---|---|---|
| **`KHR_node_visibility`** | **LOUDLY.** `CubeVisibility` lists it in `extensionsRequired`, and the importer refuses the whole file: *Extension KHR_node_visibility is not available on this addon version* | Blender 5.2.0, import returns a `RuntimeError` and no scene |
| **diffuse transmission** | **SILENTLY, and this is the dangerous one.** `DiffuseTransmissionTest` lists `KHR_materials_diffuse_transmission` only in `extensionsUsed`, so the importer reads the file, drops the extension and renders a picture that looks like an answer | Blender 5.2.0, import returns `FINISHED`; **20 of 29 materials declare the extension** and **not one of the 32 Principled sockets differs between `Factor 0.0` and `Factor 1.0`**, which differ in the file by exactly `diffuseTransmissionFactor: 1.0` |

**The silent one is worth more than the loud one.** A refused import stops a round; a dropped extension
produces a red case whose natural reading is *our diffuse transmission is wrong*, and the round is spent
on an engine that was right. **Diffuse transmission is impact 6 and is CryEngine's Vegetation shader's
headline feature**, so this row was going to be reached and defended.

## What is NOT claimed here, and why the instrument matters

The starting point was a **grep over the importer's `blender/imp` tree**, which enumerates the extension
names its source mentions:

```
EXT_mesh_gpu_instancing  EXT_texture_webp  KHR_animation_pointer  KHR_draco_mesh_compression
KHR_lights_punctual  KHR_materials_anisotropy  KHR_materials_clearcoat  KHR_materials_dispersion
KHR_materials_emissive_strength  KHR_materials_ior  KHR_materials_iridescence
KHR_materials_pbrSpecularGlossiness  KHR_materials_sheen  KHR_materials_specular
KHR_materials_transmission  KHR_materials_unlit  KHR_materials_variants  KHR_materials_volume
KHR_texture_transform
```

**That list is a hypothesis and not a finding.** `CLAUDE.md` states it: *a grep proves a string absent,
never a capability; the instrument for a capability claim is to exercise the capability.* Both rows above
were therefore **exercised** — imported, and their materials compared against what the file declares. The
list's value is that it says **where to look next**, and it is quoted with its population: the names
mentioned under the importer's own import tree, which is not the same as the names it honours.

- [ ] **The remaining absentees are exercised rather than assumed.** `KHR_mesh_quantization` and
  `KHR_texture_basisu` are both required rows and neither is in the list — but quantization may be
  handled in the accessor decode without ever being named, which is exactly the case the grep cannot
  answer. Each needs an import and a comparison, the way these two got one
- [x] **`KHR_materials_volume` at impact 25 is HONOURED, and exercised rather than assumed.**
  [MEASURED] Blender 5.2.0 on `AttenuationTest`: 11 of 18 materials declare it, and each arrives with a
  `VOLUME_ABSORPTION` node whose `Color` is the file's `attenuationColor` **exactly** — `(0.1, 0.5,
  0.9)` — whose `Density` is 1.0 from an `attenuationDistance` of 1, and which is **linked into the
  material output's Volume socket**. Principled carries `Transmission Weight` 1.0 and `IOR` 1.5. So the
  largest row of tier 2 is not blocked

- [ ] **`KHR_materials_ior` at 17 is NOT yet exercised.** `AttenuationTest` shows Principled `IOR` at
  **1.5, which is the default**, so that reading proves nothing about whether the extension is read.
  `IORTestGrid` declares it and is fetched; the check is one import and a comparison across its grid

## What the volume measurement found that is NOT an exclusion, and matters more

**The three materials `R2_and_R4_ThicknessFac_1.0`, `R2_ThicknessFac_1.5` and `R2_ThicknessFac_2.0`
differ in the file only by `thicknessFactor` — and their `VOLUME_ABSORPTION` inputs are IDENTICAL.**

That is Cycles being right rather than the importer being lazy. **glTF's `thicknessFactor` is a
rasteriser's stand-in for a path length it cannot measure**; a path tracer knows the real distance the ray
travelled and needs no stand-in. So the extension is honoured and the *approximation inside it* is not.

**Which means the volume row will disagree with this oracle exactly where glTF's thickness model departs
from true path length, and that disagreement is neither our defect nor Blender's.** It is a **reduction
question** — rung 2 of the ladder, with a measurement — and not an exclusion like the two rows above.
*Naming it now is the point: it is the caveat that would otherwise be sought after a red case rather than
before one, and `CLAUDE.md` asks for it in that order.*

## What this changes in the sequence

**Both rows leave `board:0079`'s ordering**, on the criterion the section already states, and each becomes
a **declared exclusion naming why the oracle cannot decide it** rather than an item waiting its turn.
`KHR_node_visibility` was the next tier-1 row by impact; it is now the second row to leave for this
reason, after the occlusion texture.

**And the news for tier 2 is good, subject to the box above**: `volume`, `ior`, `clearcoat`, `sheen`,
`specular`, `anisotropy` and `transmission` are all named by the importer, so the largest remaining rows
are not blocked the way these two are.

## Comments

**The pattern behind all three exclusions found so far is one sentence:** the oracle is authoritative
about **light transport**, and every row it cannot decide is a row where glTF states something that is
**not** a light-transport question — an occlusion map that approximates an integral Cycles evaluates
exactly, a node flag that says *do not draw this*, a material term the importer has not learned. *That is
a useful predictor for the rows not yet reached, and it is a prediction rather than a measurement.*
