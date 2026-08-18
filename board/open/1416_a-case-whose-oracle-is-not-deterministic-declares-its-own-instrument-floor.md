Type: bug
Area: corpus
Tags: oracle, instrument, khronos

**A case whose oracle is not deterministic declares its own instrument floor**

`oracle_samples_differing_between_seeds` is the corpus's own floor: two renders of one scene with the
sampler's seed moved, and **at one sample per pixel of a deterministic emission there is nothing for a
seed to change**, so the metric is enforced at zero. That holds for 147 of the 148 models.

**`SheenWoodLeatherSofa` returns 1497.** [MEASURED] with `EXT_texture_webp` honoured and the case
comparing at last, its two seeds disagree on 1497 samples — so its geometric bound is being measured
against an instrument that moves under it.

## The mechanism, and the first explanation was refuted

**It is a transparent surface whose alpha is strictly between 0 and 1.** Cycles resolves a transparent
BSDF stochastically, so at 1 spp a texel of half alpha is a coin flip and the seed decides it. The
asset's one `BLEND` material is named **`Fringe`**, carries a `baseColorTexture` and **no**
`baseColorFactor` — a soft fabric trim, which is exactly a mask of intermediate alpha.

**THE FIRST READING WAS *IT IS BLEND*, AND THAT WAS MEASURED AND KILLED.** `AlphaBlendModeTest`,
`CompareAlphaCoverage` and `TransmissionOrderTest` each carry a `BLEND` material and each returns
**0** differing samples. So blending is not it; **intermediate alpha from a texture** is, and the
distinction is the whole finding.

## The ladder, and which rung this is

- [ ] **Fix the engine** — nothing to fix: the noise is the reference's, not ours
- [ ] **Reduce the oracle** — this is the rung. The case declares a nonzero instrument floor with this
  measurement beside it, and its geometric bound is judged against that floor rather than against 0.005
- [ ] **Raise the sample count for this case** is a THIRD option and it is not free: 1 spp with a box
  filter at 0.01 px is what makes Cycles evaluate the same predicate a centre-sampling rasteriser does
  (`board:0083`), so averaging the coin flips would change what the case measures
- [ ] **Disqualify** — not reached, and it is per `(case, metric)` when it is

## What the harness needs, which it does not have

**A per-case instrument floor, declared with its measurement.** `worst_disagreement_px` is held against
0.005 for every case and that number is the corpus's own floor for a deterministic oracle. A case whose
oracle is not deterministic needs its own, and there is no field for one.

*Filed rather than worked around: raising the global floor to admit this case is the widened bound this
tree refuses, and leaving the case red teaches nothing about what it can decide.*
