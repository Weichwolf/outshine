Type: task
Parent: 0079
Depends: 1206
Area: gltf
Tags: khronos, oracle, instrument

**IOR and specular are one number, and the grid that proves them has a transmissive half**

The first dispatch of tier 2, and the point of the item is that **it is not tier-2-shaped work**.
`KHR_materials_ior` at impact 17 and `KHR_materials_specular` at 9 both set the **same quantity** — the
dielectric normal-incidence reflectance `F0` — so they are one number reaching one lobe that already
exists. No new resource, no new stage, nothing the compositor or the renderer gains a noun for.

> `F0 = specularColorFactor · min( ((ior - 1)/(ior + 1))² · specularFactor , 1 )`

**That is why they are taken before `volume` at 25**, which is genuinely a resource-and-stage row: it
needs the scene colour behind the surface, and `board:0079`'s tier rule is about what can be **finished**
before the next thing starts, not about what is biggest.

## What is there, measured rather than recalled

| | |
|---|---|
| `Material::Ior` and `Material::Transmission` | **carried and thrown away.** `core/Material.h` holds both; the only readers anywhere are `core/SurfaceState.h`, which classifies, and `gltf/Emit.cpp`, which serialises. **Nothing under `src/render/` reads either** — the sole mention is a comment in `MetalRoughBrdf.h` naming the constant F0 at IOR 1.5 |
| `KHR_materials_ior` and `KHR_materials_specular` in the reader | **absent.** The reader knows six extensions and neither is among them |
| `Material::Metalness` | **present**, which makes `board:0079`'s line *metalness has no field at all* stale — it was added and the table was not told |

**The oracle honours both, exercised and not grepped** (`board:1204`): [MEASURED] Blender 5.2.0 on
`IORTestGrid`, **16 of 23 materials declare `KHR_materials_ior`** over four distinct values — 1.0, 1.33,
1.76, 2.42 — and Blender's Principled `IOR` carries every one of them to the digit.

## The asset separates the two halves, and that is what makes this dispatchable

`IORTestGrid`'s materials are named for their own contents, and the naming is the design:

```
IOR1.33_Black_R0_M0_T0_S0.25    baseColor black, metallic 0, roughness 0, transmissionFactor 0
IOR1.33_White_R0_M0_T1_S1       transmissionFactor 1
```

**The `Black … T0` row is OPAQUE**: no diffuse to speak of, a mirror lobe at roughness 0, and the only
thing varying across its five materials is the IOR feeding `F0`. **So the quantity this task delivers is
isolated by the asset itself**, with no transmission in the path.

- [x] **A smaller asset does carry the specular half**: `SpecularTest` declares
  `KHR_materials_specular` **and nothing else** — no transmission, no volume, no ior — over 23 of its 24
  materials. So the transmissive half of `IORTestGrid` is not what blocks this task.

- [ ] **What blocks it is `board:1206`, and it is a bigger finding than this task.** Both assets are
  **black panels at roughness 0**, which is a mirror whose only visible content is what it reflects —
  they are built for image-based lighting, and this suite's `light` vocabulary is `none | gltf | sun`,
  the schema's whole enumeration. Under a sun a roughness-0 lobe is nearly a delta, so `F0` would decide
  the brightness of a point; meanwhile **Cycles renders the factory world**, a uniform grey that IS a
  light, and our subject arm consumes no environment at all. **A case authored today would be red for a
  reason that is neither our arithmetic nor Blender's**, and its natural reading — *our specular is
  wrong* — is the one that costs the round

## Done when

- [ ] `KHR_materials_ior` and `KHR_materials_specular` are read, with the format's defaults where a field
  is absent — **and `ior: 0` is legal and means a Fresnel-free surface**, which is not the same as absent
- [ ] `F0` is computed once, from the formula above, and reaches `MetalRoughBrdf` — the constant at
  `MetalRoughBrdf.h:32` **disappears in the same round**, because a default that survives beside a
  computed value is the second spelling of one number
- [ ] A render case proves it against Cycles — **blocked on `board:1206`**, not on the asset. The number
  is delivered and unproven by a picture, which `board:0079`'s closing line forbids as a CLAIM, so this
  task stays open and the capability table says *implemented, awaiting an environment* rather than
  *delivered*
- [ ] A deliberately wrong `F0` is shown red

## The caveat named before the measurement

**`specularColorFactor` tints `F0` and Blender's Principled has a `Specular Tint` input** — but *the
importer mentions `KHR_materials_specular`* is a grep result, not a capability, and `board:1204` is
explicit that the two are different claims. **The row is exercised the way ior was — a grid whose
declared values differ, compared against what the importer produced — before any of it is believed.**
