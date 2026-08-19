Type: bug
Area: render
Tags: khronos, oracle

**The clearcoat Fresnel takes the view angle the extension names**

`KHR_materials_clearcoat` evaluates its layering Fresnel at **`NdotV`** and says why in its own words:

> we compute the microfacet Fresnel term with NdotV instead of VdotH. That means that we ignore the
> orientation of the microsurface.

```
clearcoat_fresnel = 0.04 + (1 - 0.04) * (1 - abs(VdotNc))^5
coated_material   = mix(material, clearcoat_brdf, clearcoat * clearcoat_fresnel)
```

This engine evaluated it at **`v.h`**. Under one distant light the half-vector angle barely moves across a
subject, so the base was attenuated by nearly the same factor everywhere -- where a coat's reflectance
runs from 0.04 head-on to 1 at grazing.

## The measurement demanded the term before the specification named it

[MEASURED] `shaded-sphere-metal-clearcoat`, ours over the oracle by view angle, before and after:

| `n.v` | px | oracle | before | after |
|---|---|---|---|---|
| 0.9 - 1.0 | 8813 | 0.49161 | 0.758 | 0.758 |
| 0.7 - 0.9 | 14850 | 0.13593 | 0.995 | 0.995 |
| 0.5 - 0.7 | 11049 | 0.04106 | 1.020 | **1.012** |
| 0.3 - 0.5 | 7349 | 0.02447 | 1.083 | **1.013** |
| 0.1 - 0.3 | 3597 | 0.01901 | 1.332 | **0.982** |
| 0.0 - 0.1 | 476 | 0.01421 | **2.132** | **0.819** |

| | before | after |
|---|---|---|
| `linear_p95_relative` | 0.213744 | **0.017590152** |

**The sign is what pointed at it**: too dark where the coat should transmit, too bright where it should
reflect, and crossing in the middle -- the signature of a constant attenuation standing in for a
view-dependent one. *The naive reading was the opposite and was refuted first: a physically layered coat
transmits the base twice and would be DARKER at normal incidence, not brighter, so the layering operator
could not explain the sign.*

**The parameters were ruled out before the code was touched.** Blender's importer gives the oracle
`Coat Weight 0.8`, `Coat Roughness 0.1`, `Coat IOR 1.5`, which is this file's `clearcoatFactor 0.8` and
`clearcoatRoughnessFactor 0.1` exactly.

## What it did not fix, and it is a separate question

**The `n.v` 0.9-1.0 bin did not move: 0.758 before and after.** At normal incidence both sides put the
coat's Fresnel at 0.04, so the base is weighted 0.968 on ours and the coat adds `0.8 * 0.04 = 0.032`
times its lobe -- a 3 % effect where 24 % is measured. `board:1435` carries it as an open row.

## The population, quoted with the number

[MEASURED] **13 cases in the corpus declare `KHR_materials_clearcoat`** -- `ClearCoatTest`,
`ClearcoatWicker`, `CompareClearcoat`, `ClearCoatCarPaint`, `ToyCar`, `CarConcept`, `StainedGlassLamp`,
`PotOfCoals`, `AnisotropyBarnLamp`, `CommercialRefrigerator`, `TextureTransformMultiTest`,
`PotOfCoalsAnimationPointer` and `AnimationPointerUVs`. **All twelve that prepare are still green after
the change**; the thirteenth is the case Blender's importer refuses.

## Comments

A second deviation from the same paragraph is NOT repaired here and is named instead: the extension gives
the coat the GEOMETRIC normal -- *if clearcoatNormalTexture is not given, no normal mapping is applied to
the clear coat layer, even if normal mapping is applied to the base material* -- and this shader passes
the coat the base's `nl`, `nv` and `nh`, which are the SHADING normal's. On a subject with no normal map
the two are the same, which is why this case cannot see it and why nothing is claimed about it here.
