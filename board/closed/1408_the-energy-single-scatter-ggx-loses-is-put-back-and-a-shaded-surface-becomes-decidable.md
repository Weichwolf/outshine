Type: task
Parent: 0079
Area: render
Tags: oracle, instrument, khronos

**The energy single-scatter GGX loses is put back, and a shaded surface becomes decidable**

A microfacet model that traces ONE bounce off the facets loses everything that would have left after
two or more, and the loss grows with roughness because a rougher surface shadows itself more. glTF's
Appendix B specifies exactly that one bounce.

**The measurement that motivated it**, from `board:1407`:

| `shaded-sphere-metal` | roughness 0.5 | roughness 0 |
|---|---|---|
| `picture_max_delta_code` | 35.610613 | **0** |
| `linear_channels_differing` | 129 702 | **0** |
| `linear_p50_relative` | 0.058059536 | **0** |
| `linear_p99_relative` | 0.086824776 | **0** |
| channels below the oracle | 129 477 of 129 702 | -- |

**Bit-identical at roughness 0 and 5.8 to 8.7 per cent dark at 0.5, with no angular feature.** The term
that vanishes with the microfacets is the term that was missing.

**The specification permits it, in its normative half.** `spec.adoc` line 2090: *Implementations of the
bidirectional reflectance distribution function (BRDF) itself **MAY** vary based on device performance
and resource constraints.* Appendix B is a reference implementation and the spec says so in its own
uppercase -- which is the distinction `board:1363` drew and refused on, where glTF pins `1 - F(v.h)` and
Blender uses `1 - E(n.v)` for a term the spec does specify.

**Kulla and Conty, *Revisiting Physically Based Shading at Imageworks*, SIGGRAPH 2017.** The
compensation is a second lobe whose shape is fixed by the directional albedo of the first and which has
**no free parameter** -- a derived correction, not a fit to the residual above.

## What must be true

- [x] The albedo is integrated from **our own** `BrdfLobe` and never fetched, so it cannot drift from
  the lobe it compensates
- [x] At roughness 0 the term is **exactly** zero, so the case that is bit-identical stays bit-identical
- [x] Both halves read the same table, generated from the same integral
- [x] The correction is applied to every metal-rough surface and not to a chosen one -- a repair that
  helped one case would be a patch

## The estimator was wrong first, and looking at the table is what caught it

**A uniform quadrature over the light hemisphere was written first.** [MEASURED] at roughness 0.10 it
returned `E(0.5) = 0.22156` and `E(0.1) = 0.01606` for a surface that is nearly a mirror, and at
roughness 0.5 it returned `E(0.1) = 0.886` against `E(0.5) = 0.857` -- not even monotone. A narrow lobe
falls between the samples.

**Every one of those numbers would have become a compensation**, so a smooth surface would have been
brightened by a factor of five. *It was caught by printing the table and reading it, not by the case it
was built for -- and the case would have gone GREENER on the way past, because the metal sphere at
roughness 0.5 was too dark.*

**The replacement samples the distribution itself**, and the two estimators agree where the grid is
valid: `E_avg` at roughness 0.75 is 0.65621 against 0.65608, and at 1.0 is 0.40915 against 0.40908.
*Two independent methods, three digits, and the disagreement confined to exactly the regime one of them
is known to fail in.*

**What the cancellation costs is stated rather than hidden**: sampling the distribution puts `D` in the
numerator and in the density, so this integral is blind to a constant factor inside `BrdfDistribution`.
That is the blindness `TheMicrofacetLobeAddsNoEnergy` uses an independent sampler to avoid; it guards
the normalisation, and this estimator needs to AGREE with the lobe rather than audit it.

## What it predicts, written before the run

**Gain of 4.1 to 11.1 per cent** at the metal sphere's row, computed from the lobe: 11.12 % at
`n.v = 0.95`, 10.84 % at 0.70, 9.85 % at 0.50, 7.42 % at 0.30, 4.07 % at 0.15. Against a measured
deficit of 5.8 % at the median and 8.7 % at p99.

**So `shaded-sphere-metal` should move most of the way in and may overshoot at grazing.** An exact
landing is not predicted and would be more suspicious than a residual: the compensation is a lobe of a
fixed shape, not a per-angle correction.

## The added lobe was built first and the measurement refuted it

Kulla and Conty give the missing energy as a second lobe, `(1 - E(mu_o))(1 - E(mu_i)) / (pi (1 -
E_avg))`, which spreads it evenly over the hemisphere. **It overshot by a factor of four.**

| `shaded-sphere-metal` | before | **added lobe** |
|---|---|---|
| `linear_p50_relative` | 0.058059536 dark | **0.29605869 bright** |
| `linear_p99_relative` | 0.086824776 | **0.72768408** |

**And the residual's own shape had already said so.** A per-pixel relative error of 0.058 at the median
against 0.087 at p99 -- a factor of **1.5** -- is a MULTIPLICATIVE deficit. An added lobe is comparable
to the single-scatter term where that term is strong and dwarfs it where the surface is dim, which
would have made the tail enormous. *The data selected between the two forms before either was written,
and reading that off first is what the round should have done.*

**Cycles' own choice, read from `intern/cycles/kernel/closure/bsdf_microfacet.h` rather than assumed:**

```
missing_factor = (1 - E) / E                       // E at the VIEW's cosine only
Fms            = Fss * E_avg / (1 - Fss (1 - E_avg))
net multiplier = 1 + Fms * missing_factor
```

and `fresnel_f82_Fss(F0, B) = F0 + (1 - F0)/21 - B/126`, which at `B = 0` is **exactly** the closed-form
Schlick average this header derives independently. *Two spellings of one integral, arrived at from
different directions.*

**It depends on the view alone**, so it is a fragment constant and leaves the light loop -- the frame
path pays for it once per pixel rather than once per light.

## What it bought, measured over the same five cases both times

| | p50 before | **p50 after** | p99 before | **p99 after** | bound |
|---|---|---|---|---|---|
| `shaded-sphere-metal` | 0.058059536 | **0.00011407057** | 0.086824776 | **0.0045163848** | **within** |
| `shaded-sphere-metal-smooth` | 0 | **0** | 0 | **0** | within |
| `shaded-sphere-black` | -- | **0.0038684683** | 0.019273589 (p95) | **0.0059721113** | **within** |
| `shaded-sphere` | 0.0057494845 | 0.0058939342 | 0.093481424 (p95) | 0.17504225 | outside |
| `shaded-sphere-smooth` | -- | 0.0090289104 | -- | 1 | outside |

**The median on the metal sphere improved by a factor of 509 and its p99 by 19, and TWO shaded cases
are now inside the picture bound** -- where `board:1363` opened with *the shading arm has never produced
a case inside the bound*.

**The case that must not move did not move at all.** `shaded-sphere-metal-smooth` is still bit-identical:
at roughness 0 the albedo is 1 and the multiplier is exactly 1, as arithmetic and not as a guard.

**The two that stayed outside are the two that carry a diffuse term**, which is `board:1363`'s finding
and the oracle's side of it. *They were not expected to move and they did not, which is what says this
correction went where it was aimed.*

## The prediction this item wrote, and how it fared

*Gain of 4.1 to 11.1 per cent* was written before the run, from the added-lobe form. The multiplier form
gives **4.76 % to 9.67 %** over the same view sweep, against a measured deficit of 5.8 % to 8.7 %.
*Neither number was tuned; the deficit was computed from the lobe before the two were put side by side.*
