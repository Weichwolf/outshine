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

- [ ] **The premise is measured before anything is built on it.** `test/outshine/render/shaded-sphere-metal`
  is `shaded-sphere` with ONE number changed -- `metallicFactor` 0.0 to 1.0 -- and its residual is binned
  against `n.v` exactly the way `board:1363` binned the dielectric one
- [ ] **The conductor Fresnels are compared as functions and not as one number.** glTF Appendix B is
  Schlick with `F0 = baseColor`; Blender's Principled uses an F82-tint conductor Fresnel, which dips near
  82 degrees where Schlick rises monotonically. **If they disagree, the disagreement is a SECOND named
  term about the specular path**, and that is a finding rather than a setback
- [ ] **The first layered-extension case follows the premise and not the other way round.** No case is
  authored on this route until the route has a number
- [ ] **A base colour of 0.5 is [SET] for sensitivity**: at F0 near 1 every Fresnel model agrees
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
