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
- [ ] **the clearcoat** -- 0.213744 at p95 relative and yet **1 code** in the picture, which is the pair
  in the table most worth explaining: a fifth of the radiance and nothing anyone can see
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
