Type: bug
Area: render
Tags: oracle, instrument

**Each shaded sphere names the model pair its residual is made of**

The eight generated spheres of `board:1363` exist to isolate one shading layer at a time, and read
together they now do something no single case can: they say **which layer costs what**, against an
instrument one of them proves is sound.

[MEASURED] same subject, same camera, same light, one layer changed:

| sphere | picture p99 | channels differing | p95 relative |
|---|---|---|---|
| **metal, smooth** | **0 codes** | **0** | **0** |
| metal | 1 code | 129 675 | 0.0013726554 |
| black | 1 code | 129 702 | 0.0056239884 |
| black + iridescence | 1 code | 129 702 | 0.045389304 |
| metal + clearcoat | 1 code | 129 702 | 0.213744 |
| dielectric, rough | 2 codes | 129 702 | 0.093656913 |
| dielectric, smooth | 2 codes | 129 702 | 0.28468061 |
| metal + iridescence | 5 codes | 129 701 | 0.21144544 |
| metal + sheen | 18 codes | 129 702 | 1.6047504 |

## The smooth metal is the load-bearing row

**It agrees with Cycles to the last bit of f32, over 129 702 channels.** So `linear_channels_differing`
at a bound of zero is not an unreachable demand and the instrument is not broken: where the BRDF reduces
to a mirror and `F0` is the metal's own colour, two independent renderers land on identical floats.
*Every other row's residual is therefore a difference in what is being evaluated, not in how precisely.*

**And the metal rows corroborate `board:1363` without being asked to.** That item measured the diffuse
coupling -- Cycles attenuates its diffuse by `1 - E(n.v)` where glTF Appendix B specifies `1 - F(v.h)` --
and predicted that removing the diffuse term drops the residual by 7.1x. Here the prediction is visible
in the shape of the table: **every metal row is at 1 code or better and both dielectric rows are at 2**,
because a metal has no diffuse term for the two models to disagree about.

## One layer is sourced and declared; the rest are named and are not

- [x] **sheen** -- glTF specifies the **Charlie** distribution with the Conty-Kulla visibility, in closed
  form, and `src/render/stages/SheenLobe.h` implements it line for line; Cycles implements **Zeltner,
  Burley and Chiang, *Practical Multiple-Scattering Sheen Using Linearly Transformed Cosines*, 2022**,
  cited in `intern/cycles/kernel/closure/bsdf_sheen.h`. Both fetched at the source. Declared on the case
- [x] **iridescence** -- and it is **NOT a model pair**. Cycles' `fresnel_iridescence_channel` in
  `intern/cycles/kernel/closure/bsdf_util.h` cites *Belcour and Barla, A Practical Extension to
  Microfacet Theory for the Modeling of Varying Iridescence*, which is the paper
  `src/render/stages/IridescenceLobe.h` implements. **One paper, two substrates**: Cycles takes
  `substrate_n`, `substrate_k` and `F82`, a complex index, where `KHR_materials_iridescence` derives a
  REAL index from F0 and states *it's only an approximation for metals ... assumed to be `0.0`*.
  **The case set already carried the discriminator**: the film over a dielectric substrate
  (`metallicFactor 0`) is **1 code and within the bound**; over a conductor (`metallicFactor 1`) it is
  **5 codes**. Declared on the conductor case
- [ ] **the rough metal lobe** -- 0.0013726554 at p95 relative and 1 code in the picture, against a
  perfectly smooth sibling at zero. The candidate is our Kulla-Conty energy compensation (`board:1408`)
  against Cycles' own multiple-scattering GGX, and it is a candidate and not a finding
- [ ] **the clearcoat** -- MEASURED against the view angle, and it is two effects rather than one. The
  parameters are NOT the cause: Blender's importer gives the oracle `Coat Weight 0.8`, `Coat Roughness
  0.1`, `Coat IOR 1.5`, which is this file's `clearcoatFactor 0.8` and `clearcoatRoughnessFactor 0.1`
  exactly. The residual against `n.v`, radiance in linear:

  | `n.v` | px | oracle | ours | ratio |
  |---|---|---|---|---|
  | 0.9 - 1.0 | 8813 | 0.49161 | 0.37247 | **0.758** |
  | 0.7 - 0.9 | 14850 | 0.13593 | 0.13530 | 0.995 |
  | 0.5 - 0.7 | 11049 | 0.04106 | 0.04190 | 1.020 |
  | 0.1 - 0.3 | 3597 | 0.01901 | 0.02532 | 1.332 |
  | 0.0 - 0.1 | 476 | 0.01421 | 0.03029 | **2.132** |

  **`board:1441` closed the grazing half of this** -- the extension evaluates its layering Fresnel at
  `NdotV` and this engine used `v.h`, so `p95 relative` fell from 0.213744 to 0.017590152 and the
  0.0-0.1 bin from 2.132 to 0.819. **What remains is the head-on bin alone, unmoved at 0.758**, where
  both sides put the coat's Fresnel at 0.04 and a 3 % effect is measured at 24 %. The table below is the
  before state and is kept because the sign is what identified the term:

  24 % dark where the coat's Fresnel is at its minimum and 2.1x bright at grazing, with the middle
  agreeing to half a percent -- and the same sphere without a coat agrees everywhere at p95 relative
  0.0013726554. The format's operator is a mix, `base*(1 - clearcoat*F) + clearcoat*F*coatLobe`, and this
  engine implements it; Cycles layers the coat as a closure. **But a layered coat transmits the base
  twice and would be DARKER, not brighter**, so the sign at `n.v` near 1 is not explained by the layering
  and the cause is not named. `board:1429` does not reach it either: at normal incidence Schlick and the
  exact Fresnel agree
- [ ] **the dielectric rows** -- `board:1363`'s coupling, already measured there

## What must be true

- [ ] every red sphere either declares a reduction naming its cause **with both sides fetched at the
      source**, or is repaired -- and *cause* rather than *model pair*, because the iridescence row turned
      out to be one model over two substrates and the wording would have prejudged it
- [ ] no reduction is written for a pair that has only been recalled

## Comments

**The rule this item was written under**: a reduction is an argument, and an argument needs both sides
quoted. Five pairs above are plausible and unverified, and plausible is exactly what `CLAUDE.md` means by
*flüssigkeit ist verdächtig* -- so they are listed as work rather than declared as findings.

## The black-iridescence row is now its own question and is NOT covered by the above

Its substrate is a **dielectric**, where the extension's index conversion is exact and both sides
evaluate the same paper on the same real index -- so the substrate argument does not reach it. Its
picture passes at 1 code; what fails is bit-exactness, at **p95 relative 0.045389304** against the plain
black sphere's 0.0056239884. **A factor of eight, and it is not last-bit noise**: two implementations of
one paper differ somewhere inside it, and nothing in this round says where.
