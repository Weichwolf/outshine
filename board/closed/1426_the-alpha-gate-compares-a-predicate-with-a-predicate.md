Type: bug
Area: corpus
Tags: oracle, instrument

**The alpha gate compares a predicate with a predicate**

Our frame's alpha is a predicate by construction -- coverage is 0 or 1 and cannot be anything between.
**The oracle's is a predicate only at ONE sample per pixel.** Above one the reference antialiases and
its alpha at a silhouette pixel is the fraction of samples that hit: a continuous coverage, which is a
different quantity from a predicate however close the two look.

[MEASURED] `PointLightIntensityTest` renders at **256** samples and is the only case in the corpus that
does -- **144 of 145 take one**. Its oracle carries alpha **0.383** over 806 pixels on a subject whose
every material is `OPAQUE` with no transmission, so there is nothing transparent to find and what the
gate was reading is the reference's own edge filtering.

## The rule now names which side it constrains, twice over

- [x] **The floor constrains the ORACLE's departure** and not ours, because ours cannot depart
  (`board:1422`)
- [x] **The gate is enforced only where the reference's alpha is a predicate too**, read from the
  case's own `renders.default.samples`

**Neither is a widened bound.** A blended surface is tens or hundreds of codes and still fires; what
stops firing is a comparison between two different quantities.

## Comments

**This case also showed the other half of the same asymmetry.** Five of its stated invariants were
enforced at zero against us and merely REPORTED for the oracle -- and [MEASURED] the oracle misses every
one of them by MORE: 137 374 against our 133 149, 137 373 against 133 155, 45 794 against 44 386, 45 794
against 44 392, 45 792 against 44 386. The invariant is the ASSET's claim about a correct render, so a
reference that satisfies it less well than we do cannot be the thing we are failed against. Each carries
a declared reduction with both numbers.
