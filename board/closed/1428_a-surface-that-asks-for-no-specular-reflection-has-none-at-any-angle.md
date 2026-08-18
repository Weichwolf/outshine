Type: bug
Area: render
Tags: khronos

**A surface that asks for no specular reflection has none at any angle**

`KHR_materials_specular` states its two halves together and the engine carried only the first:

```
dielectric_f0  = min(0.04 * specularColor, 1) * specular
dielectric_f90 = specular
```

`specularFactor` reached F0 and F90 stayed at unity, so a panel declaring `specularFactor = 0` kept the
entire grazing rim the factor exists to remove.

[MEASURED] on `SpecularTest`'s first row -- black base colour, `metallicFactor 0`, `roughnessFactor 0`,
`specularFactor 0`, which leaves the grazing term as the only thing any pixel can show:

| | before | after | oracle |
|---|---|---|---|
| mean linear radiance over the panel | 0.01059 | **0.00000** | **0.00000** |
| its peak, at the rim | 0.24228 | 0 | 0 |

## The repair reached nothing on its first reading, and that was the finding

The first pass threaded F90 through `BrdfFresnel`, its device twin, the material row and all five
fragment call sites -- and the case came back **identical to every digit**: `linear_channels_differing`
122775 both times, `linear_p50_relative` 0.067951999 both times. *`CLAUDE.md` says a change that alters
the picture by design cannot reproduce it to six decimals, and that identical is a finding.* It was: the
rim was never drawn by the lobe's Fresnel at all.

`roughnessFactor 0` makes `a2` zero and `brdfLobe` returns zero for it, so the microfacet specular was
already nothing. The rim came from the environment term, which had **its own copy of Schlick written out
in longhand** beside the shared one:

```
const float fresnel = grazing * grazing * grazing * grazing * grazing;
const float3 specularEnvironment = f0 + (1.0 - f0) * fresnel;
```

A second spelling of an expression is the defect `CLAUDE.md` names as *every statement has exactly one
place*, and this is what it costs: the row's own number could not reach a line that recomputed it. It is
now `brdfFresnel(f0, f90, nvClamped)` and there is one Fresnel in the engine.

## What is proven

- `test/outshine/shader/BothHalvesOfTheBrdfAgree.cpp` sweeps F90 as an input, with a row at 0.5 and a
  row at 0.0 beside the unity rows -- a parameter the twins share but no sample varies is a parameter
  neither half is compared on
- `core/Material.h`'s `DielectricF90` is the one place the quantity is derived, beside `DielectricF0`

## Comments

The factorisation is why the pair suffices and no third number is needed: with `f0' = s*f0` and
`f90' = s`, Schlick reads `s*f0 + (s - s*f0)*(1-vh)^5 = s * (f0 + (1-f0)*(1-vh)^5)`, so the extension's
`weight * fresnel` and its base's `1 - weight * max(fresnel)` both fall out of the two numbers already
carried.

`SpecularTest` remains red on two metrics and neither is this one -- `board:1429` and `board:1430` carry
them.
