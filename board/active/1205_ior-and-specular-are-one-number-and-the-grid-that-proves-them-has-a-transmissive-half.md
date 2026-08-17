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

- [x] `KHR_materials_ior` and `KHR_materials_specular` are read, with the format's defaults where a field
  is absent — **and `ior: 0` is legal and means a Fresnel-free surface**, which is not the same as absent
- [x] `F0` is computed once, in `core/Material.h`'s `DielectricF0`, carried in the material row and read
  by the fragment. **The constant is no longer emitted into the shader text**; the C++ name survives only
  so the lobe's own tests can pick a representative dielectric
- [ ] A render case proves it against Cycles — **blocked on `board:1206`**, not on the asset. The number
  is delivered and unproven by a picture, which `board:0079`'s closing line forbids as a CLAIM, so this
  task stays open and the capability table says *implemented, awaiting an environment* rather than
  *delivered*
- [x] **`specularTexture` AND `specularColorTexture` ARE IMPLEMENTED** — read into `MaterialRef`, bound
  as two more images (the contract goes 4 → 6), and folded into `F0` by one macro every shading arm
  uses. The strength image is LINEAR and read from ALPHA; the tint image is sRGB and read from RGB.
  **One corner differs from the format and is named**: the row's `F0` is already capped, so these
  multiply a capped product, which departs from the extension only where the uncapped product exceeds
  1 — needing `specularFactor` above 25 for a dielectric, since the base term is below 1 for every
  `ior`. `SpecularTest`'s largest factor is 1.

- [x] **AND IT IS NOW SHOWN TO CHANGE THE PICTURE, BY ONE PREDICATE.** The consumer carried the uv run
  only where the **colour** image existed — `carried.Uv = where.HasUv && Surfaces[slot].Colour.Rgba` —
  so a material whose only image is this extension's took the layout with **no uv**, entered the arm
  that cannot sample, and its two textures changed nothing. `SpecularTest`'s three textured materials
  each carry `TEXCOORD_0`, so the attribute was there and the layout threw it away.

  **`ReadsAnyImage()` asks every socket**, and the measurement moved for the first time in four
  attempts: channels agreeing to within one code went **23134 → 31454**, so **8320 channels** came
  into agreement. *The peak is untouched at 141.523705 and belongs to something else.*

- [ ] ~~**AND IT IS NOT YET SHOWN TO CHANGE ANY PICTURE.**~~ Superseded by the line above. After the change
  `SpecularTest`'s histogram is **identical to the digit** — 23134 channels in `[0, 1)`, peak
  141.523705 at (551, 380). **Not one channel moved.** The next measurement is named rather than
  guessed: *do the three images decode to anything other than the identity?* `specularTexture` is read
  from **alpha**, and a PNG carrying no alpha samples `1` — which is the identity — so
  `specularTextureGrid.png` at 242 bytes and `WhiteGrid.png` at 205 may be white-and-opaque and change
  nothing by construction. `YellowGrid.png` is the one that must move a pixel if the path is live The extension has a texture half and this task delivered only its factors. [MEASURED] in
  `SpecularTest`: **three materials carry one** — `M2_SpecTex` a `specularTexture`, `M4_whiteTex` and
  `M6_yellowTex` a `specularColorTexture` — and the asset names its own rows *specular texture*,
  *white color texture* and *yellow color texture*. Our side draws them at the factor alone while
  Cycles modulates by the image, and the reference picture carries small marks at the left edge of
  those rows that ours does not.
- [ ] A deliberately wrong `F0` is shown red

## The caveat named before the measurement

**`specularColorFactor` tints `F0` and Blender's Principled has a `Specular Tint` input** — but *the
importer mentions `KHR_materials_specular`* is a grep result, not a capability, and `board:1204` is
explicit that the two are different claims. **The row is exercised the way ior was — a grid whose
declared values differ, compared against what the importer produced — before any of it is believed.**

## Comments

**The wrong-arm mistake was found by forcing the number to zero, not by reading the code.** The
modulation went into `mappedShade` first, which is the arm for a surface carrying a colour or normal
image. Forcing that arm's `F0` to **zero** left the picture identical to every digit — so the case never
enters it, and the extension's own textured materials declare no base-colour image either. *A capability
put in the arm the asset does not take is indistinguishable from one that is absent, and only the
mutation says which.*

**And the arm was only the first of two gates.** After the modulation reached every arm the picture was
STILL identical, because the layout feeding those arms had already dropped the uv run — one predicate
asking about the colour image alone. **Four measurements came back identical to the digit before the
cause was found**, and each one was a real elimination rather than a wasted round: the mirror ray, the
grazing Fresnel, minification by resolution, and the wrong shading arm. *The chain broke on a predicate
that had been correct for as long as every image was a colour image.*

## The 141-code peak is not this task's, and five measurements say so

**It is on a LABEL PLATE, not on a specular panel.** Cropped and magnified around (551, 380) at 10x,
the pixel sits at the leading edge of the plate reading *yello[w]* — `LabelMat`, the one material in the
file with a `baseColorTexture` and the only one this extension does not touch.

| eliminated | how |
|---|---|
| environment visibility | mirror ray through the existing BVH — identical to the digit, reverted |
| grazing Fresnel | measured against Cycles at six view angles; it TRACKS Schlick, 0.997–1.21 |
| ~~minification~~ | **THIS ELIMINATION WAS OVER-CLAIMED AND IS WITHDRAWN — see below** |
| the extension's textures | now live and moving 8320 channels, and the peak did not move |
| **a geometric offset** | **the first covered column is IDENTICAL in both pictures at rows 360, 370, 380, 390 and 400 — delta 0 everywhere.** Both sides cover the pixel |

**So both draw the plate in the same place and disagree about its TEXEL**: the reference is 0 and ours
is 142 at a pixel one column inside the plate's leading edge. That is a texture-sampling difference at
the image's own edge — a wrap or edge-handling question on `LeftLabels.png` — and it belongs to
`board:1130`'s neighbourhood rather than to `KHR_materials_specular`.

**The case therefore cannot be scored on this task's work alone**, and saying so is the point: `1205`'s
number and its textures are both delivered and both exercised, and the case stays red for a cause that
five measurements have now placed outside it.

## The minification elimination was wrong, and the error is in the inference and not the number

**What was measured is true**: 141.523705 at 1280×720 against 141.533904 at 320×180 — the peak does not
move with resolution. **What was concluded from it does not follow.**

**A peak between a black texel and a white one is SATURATED BY CONSTRUCTION.** `LeftLabels.png` is text:
its contrast is the maximum the format can carry, so any pixel straddling a glyph edge yields the same
maximum delta at any resolution. Quadrupling the raster reduces how MANY pixels straddle an edge; it
cannot reduce how far apart the two sides are at one that does. *The instrument I read was insensitive
to the hypothesis I was testing.*

**And the file asks for exactly what this engine does not have.** [MEASURED] `SpecularTest`'s one sampler
declares `minFilter: 9987` — `LINEAR_MIPMAP_LINEAR` — and `board:1130` states that **no texture in this
engine has mipmaps**. A 512×512 label strip minified onto a plate a hundred pixels wide is where that
absence shows first, and Cycles filters it with a mip chain while this engine point-samples one level.

- [ ] **The right test is the COUNT above the bound, normalised by covered pixels, at two resolutions**
  — a peak saturated by contrast cannot fall, but the population straddling an edge must. *Named rather
  than run, because it belongs to `board:1130` and not to this task.*

**So the fifth elimination stands and the third is withdrawn**: the peak is on a label plate, both sides
cover it, they draw it in the same place, and they disagree about its texel under a `minFilter` this
engine does not implement.
