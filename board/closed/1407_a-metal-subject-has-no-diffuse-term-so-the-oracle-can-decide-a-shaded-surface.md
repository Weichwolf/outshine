Type: task
Parent: 0079
Area: corpus
Tags: oracle, instrument, khronos

**A metal subject has no diffuse term, so the oracle can decide a shaded surface**

`board:1363` measured why every shaded case is outside the picture bound and reduced nine cases to ONE
term: Cycles couples its diffuse to `1 - E(n.v)`, the specular layer's directional albedo, where glTF
Appendix B specifies `1 - F(v.h)` on the half-vector. **It also measured, and this is the part that was
not followed up, that with the diffuse term removed the two sides agree to `-0.000143` across every view
angle** -- two to three orders under the diffuse residual, with no view dependence and the opposite sign.

**glTF gives a metal no diffuse component at all.** So a subject at `metallic = 1` removes the disputed
term by the specification's own rule, and the arm that has never produced a case inside the bound has a
configuration in which the disputed term does not exist.

**This is what the four layered extensions have been waiting for without anybody saying so.**
`KHR_materials_sheen` (board:1385), `KHR_materials_clearcoat` (board:1388), `KHR_materials_anisotropy`
(board:1390) and `KHR_materials_iridescence` (board:1389) are all built, and **not one of them is
exercised by any case in the corpus** -- 107 of 148 cases replace every material with a flat emission,
12 more with emission by index, 10 with a single emission and 7 with a Diffuse BSDF, and the 9 that do
shade declare none of the four. Every one of the four modifies the SPECULAR path and none touches the
diffuse term.

**And Blender's Principled carries all four**, read from the node and not recalled: `Anisotropic`,
`Anisotropic Rotation`, `Tangent`, `Coat Weight`, `Coat Roughness`, `Coat IOR`, `Coat Tint`,
`Coat Normal`, `Sheen Weight`, `Sheen Roughness`, `Sheen Tint`, and **`Thin Film Thickness` and
`Thin Film IOR`**, which are `KHR_materials_iridescence`'s two parameters under Blender's names.

## What must be true

- [x] **The premise is measured before anything is built on it.** `test/outshine/render/shaded-sphere-metal`
  is `shaded-sphere` with ONE number changed -- `metallicFactor` 0.0 to 1.0 -- and its residual is binned
  against `n.v` exactly the way `board:1363` binned the dielectric one
- [x] **The conductor Fresnels are compared as functions and not as one number.** glTF Appendix B is
  Schlick with `F0 = baseColor`; Blender's Principled uses an F82-tint conductor Fresnel, which dips near
  82 degrees where Schlick rises monotonically. **If they disagree, the disagreement is a SECOND named
  term about the specular path**, and that is a finding rather than a setback
- [ ] **The first layered-extension case follows the premise and not the other way round.** No case is
  authored on this route until the route has a number
- [x] **A base colour of 0.5 is [SET] for sensitivity**: at F0 near 1 every Fresnel model agrees
  trivially, and at 0.5 the `(1 - F0)` term that carries the angular shape is at its largest

## What this is NOT

**It is not a reduction of the oracle and it does not touch the ladder.** Nothing is declared
undecidable and no threshold moves: it is a choice of SUBJECT in which the undecidable term is absent.
*`board:1363`'s conclusion -- that the oracle is the wrong side for a shaded dielectric -- stands
untouched, and this neither repairs nor weakens it.*

**It is not a claim that the shading arm is unblocked.** It is a claim that one configuration of it is
worth measuring, and the measurement has not been made yet.

## Three of the four are carried faithfully, and that was measured rather than assumed

[MEASURED] on Blender 5.2.0 by importing a glTF declaring all four and reading the Principled node back.

| declared | reaches Cycles as |
|---|---|
| `anisotropyStrength` 0.65 · `anisotropyRotation` 0.5 rad | `Anisotropic` **0.65** · `Anisotropic Rotation` **0.0796**, which is 0.5 radians in Blender's turns |
| `clearcoatFactor` 0.7 · `clearcoatRoughnessFactor` 0.2 | `Coat Weight` **0.7** · `Coat Roughness` **0.2** · `Coat IOR` 1.5, which is the fixed index glTF specifies |
| `sheenColorFactor` (0.9, 0.3, 0.1) · `sheenRoughnessFactor` 0.6 | `Sheen Tint` **(0.9, 0.3, 0.1)** · `Sheen Roughness` **0.6** · `Sheen Weight` 1.0, glTF's sheen carrying its strength in the colour |
| `KHR_materials_iridescence` | **partly** -- see `board:1389` for the two properties this oracle drops |

**So the route is not blocked on the importer.** Whether the two sides then AGREE is a different question
and the one this item exists to ask; what is settled is that the values arrive.

## The prediction, written down before the render so it can be wrong

**Both models are pinned at the same two ends.** At normal incidence a conductor Fresnel IS `F0`, and
`F0` is the base colour on both sides; at exact grazing both go to 1. **So a disagreement can only live
in the middle**, and F82-tint's whole construction is a dip located at `cos(theta) = 1/7` -- the angle
it is named for. If the residual is a bump centred near `n.v = 0.143` and vanishing at both ends, the
mechanism is named before it is measured. If it is monotone in `n.v` the way the dielectric one was,
something else is going on and this prediction was wrong.

**A residual of zero would also be a finding**, and the one that opens the corpus: it would mean Blender's
default specular tint reduces F82-tint to Schlick for an untinted metal.

## The conductor Fresnels are the SAME FUNCTION for an untinted metal, read from Cycles' source

`intern/cycles/kernel/closure/bsdf_util.h`, fetched rather than recalled:

```
fresnel_f82(cosi, F0, B) = saturate(Schlick(F0, cosi) - B * cosi * (1 - cosi)^6)
fresnel_f82tint_B(F0, tint) = Schlick(F0, 6/7) * (7 / (6/7)^6) * (1 - tint)
```

**`tint` is Blender's `Specular Tint`, and its default is white.** At `tint = 1` the factor `(1 - tint)`
is zero, so `B` is zero and `fresnel_f82` **is Schlick, term for term** -- which is glTF Appendix B's
conductor Fresnel exactly. *The F82 dip is a departure the artist opts into, not the model's baseline.*

**So the prediction above is answered before the render and it was the good outcome**: the residual on
`shaded-sphere-metal` should NOT contain a bump at `n.v = 1/7`, because the term that would make one has
a coefficient of zero.

## Which leaves one named candidate, and it is not the Fresnel

**Cycles preserves multiple-scattering energy and Appendix B does not.**
`bsdf_microfacet_setup_fresnel_f82_tint` calls `microfacet_ggx_preserve_energy(kg, bsdf, wi,
fresnel_f82_Fss(f0, b))`; glTF's specular BRDF is single-scatter GGX with no compensation term at all.
Multiple scattering ADDS energy and its magnitude rises with roughness.

**So the prediction for this case, written before its render**: the oracle is BRIGHTER than us, by an
amount that grows with roughness and is largest away from the highlight, with **no feature at
`n.v = 1/7`**. If instead the residual is at the F82 angle, the reading of `Specular Tint`'s default
above is wrong. If it is neither, there is a third term nobody has named.

**And that candidate has a rung on the ladder rather than a reduction.** Multiscatter compensation is a
term this engine could carry -- it is a published, derived correction with no free parameter -- so
*fix the engine* is available here in a way it was NOT for the diffuse coupling, where implementing
Cycles' model would have meant rendering something glTF does not specify.

## The premise holds, and the residual has a shape the Fresnel cannot make

[MEASURED] `test/outshine/render/shaded-sphere-metal`, one number from its sibling.

| | `shaded-sphere` grey dielectric | `shaded-sphere-black` | **`shaded-sphere-metal`** |
|---|---|---|---|
| `worst_disagreement_px` | 0 | 0 | **0** |
| `picture_max_delta_code` | 48.275985 | 6.8036277 | **35.610613** |
| `linear_p50_relative` | 0.0057494845 | -- | **0.058059536** |
| `linear_p95_relative` | 0.093481424 | 0.019273589 | **0.085641026** |
| `linear_p99_relative` | -- | -- | **0.086824776** |
| channels below the oracle | -- | -- | **129 477 of 129 702** |

**There is no angular feature and that is the finding.** p50, p95 and p99 sit at 0.0580, 0.0856 and
0.0868 -- a factor of **1.5** between the median and the tail, where the dielectric case spans a factor
of **16** between the same two. A near-constant relative deficit is not what a Fresnel disagreement
looks like, and there is no bump at `n.v = 1/7`: **the conductor Fresnels agree, exactly as Cycles'
source said they would.**

**And the sign is the predicted one.** We are darker in 129 477 of 129 702 differing channels -- the
oracle carries energy we do not.

**So the two predictions this item wrote down before the render both held**, and what is left is the one
named candidate: multiple-scattering compensation, which Cycles applies to its conductor closure and
Appendix B does not specify.

## The discriminator for it, and its prediction, written first

`test/outshine/render/shaded-sphere-metal-smooth` is the metal case with the ROUGHNESS changed to 0 and
nothing else. **Multiple scattering between microfacets is zero when there are no microfacets**, so a
compensation term has nothing to add there on either side.

**Prediction: the residual collapses.** If it survives at 5.8 to 8.7 per cent, the mechanism is NOT
multiple scattering and the candidate list is empty again -- which would be the more interesting result
and the one worth having asked for.

## The magnitude was computed from our own lobe, not read off the residual

[MEASURED] the directional albedo of `BrdfLobe` at the case's declared row -- roughness 0.5, F0 0.5 --
integrated with a hemisphere sampler written independently of the lobe's own, so a factor inside `D`
survives the ratio instead of cancelling.

| `n.v` | `E` of our single-scatter lobe | deficit against a lossless layer of the same Fresnel |
|---|---|---|
| 0.95 | 0.45596 | **8.8 %** |
| 0.70 | 0.44495 | **11.0 %** |
| 0.50 | 0.43981 | **12.0 %** |
| 0.30 | 0.45164 | 9.7 % |
| 0.15 | 0.49112 | 1.8 % |

**Against a measured gap of 5.8 % at the median and 8.7 % at p99.** The measurement sits BELOW the
computed loss, which is the right side of it: a compensation term restores the lost energy through
further bounces and every bounce is attenuated by the Fresnel again, so the recovered fraction is
`F_avg`-weighted and not the whole of it. At `F_avg` around 0.65 the two agree.

*Quoted as a corroboration of the mechanism and not as a fit -- nothing here was tuned to the residual,
and the deficit was computed from the lobe before the two numbers were put side by side.*

## The discriminator collapsed to ZERO, and that is the first shaded case inside the bound

[MEASURED] `test/outshine/render/shaded-sphere-metal-smooth`, the metal case with the roughness alone
changed to 0.

| | roughness 0.5 | **roughness 0** |
|---|---|---|
| `picture_max_delta_code` | 35.610613 | **0** |
| `linear_channels_differing` | 129 702 | **0** |
| `linear_p50_relative` | 0.058059536 | **0** |
| `linear_p99_relative` | 0.086824776 | **0** |
| picture bound | outside | **within** |

**Bit-identical, not merely close.** `board:1363` opened with *the shading arm has never produced a case
inside the bound*; this is the first, and it says the two renderers agree TERM FOR TERM on the GGX
distribution, the Smith visibility, the conductor Fresnel, the light, the camera and the geometry --
because at roughness 0 all of those are what is left.

**So the residual at roughness 0.5 is multiple scattering and nothing else**, by the strongest form of
the argument available: the term that is absent at roughness 0 is the only term that changed.

## And the specification settles the rung, in its normative half

`spec.adoc` line 2090, the core specification and not an appendix:

> *Implementations of the bidirectional reflectance distribution function (BRDF) itself **MAY** vary
> based on device performance and resource constraints.*

**Appendix B is a reference implementation and the spec says so in its own uppercase.** So carrying an
energy-compensation term is INSIDE the specification rather than a departure from it -- which is exactly
the distinction `board:1363` drew and refused on: there, glTF specifies `1 - F(v.h)` and Blender uses
`1 - E(n.v)`, two different formulas for a term the spec pins. Here both sides agree what the model is
and one of them evaluates it with less loss.

**The rung is therefore `fix the engine`, the first one, and no reduction is needed.**

## Closed on its own question, and the one box left open belongs elsewhere

**What this item asked was whether a metal subject makes the shading arm decidable, and the answer is
yes**: `shaded-sphere-metal` is inside the picture bound at p50 0.00011407057 once `board:1408` restores
the missing energy, and `shaded-sphere-metal-smooth` is bit-identical.

**The remaining box -- the first layered-extension case -- is not this item's work.** It needs the
generated fixture to be able to DECLARE an extension, which its material vocabulary
(`baseColourFactorRgba`, `metallicFactor`, `roughnessFactor`) cannot express. That is a task of its own
and it now has everything it was waiting for.
