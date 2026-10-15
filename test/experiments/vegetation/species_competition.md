# Competing spatial species marks (#226)

PR#258's one latent CDF rank creates groups, but taxon order influences which
species border each other. This experiment gives every taxon an independent
world-anchored field, using the existing Fourier/Gaussian construction. It does
not simulate resource competition between actual plants.

At each site interpolate each nonnegative species weight from the prepared
height table. For independent spatial uniforms `u_i`, choose the maximum of
`log(weight_i) - log(-log(u_i))`. Equivalently this is the minimum independent
exponential race time `-log(u_i)/weight_i`; in the ideal continuous ensemble,
its winning probability is `weight_i / sum(weights)`. Zero weights do no field
work and never win; zero total remains unsupported (`-1`). Log scores handle
positive subnormal weights without dividing by a tiny weight. The same finite
endpoint bound as the previous experiment applies. Float64 prepared cumulative
roundoff/tiny unrepresentable weights remain the library's explicit limit.

Each taxon uses separate BLAKE2b-64-derived field and site seeds. Message:
ASCII`outshine-species-race-1`, NUL, little-endian uint64 world seed, ASCII role
`field` or `site`, NUL, UTF-8 canonical taxon ID. Digest parameter is8bytes,
interpreted little-endian. Canonical taxon IDs, rather than array indices,
identify fields. These are pseudo-random seeds, not unique identity hashes or
a mathematical independence guarantee. Genuine independent variates are the
probability theorem's assumption. Alias resolution belongs to the catalog;
renaming/changing a canonical identity intentionally changes its field.

Prepared recipes save actual field coefficients and per-taxon site seeds.
Neither field generation nor height-weight preparation recurs on partial
replay. World seed/fields are shared across regions, never reseeded by camera,
LOD or tile. A finished cache product simply stores the resulting taxon indices.
This experiment creates no new general asset/cache manager or runtime API.

The maximum is independent of field iteration order. A numeric tie selects the
smallest canonical taxon index explicitly. Adding a zero-weight taxon leaves
named winners unchanged. Existing height-dependent abundance and support are
retained as pointwise ensemble probabilities, not exact finite-stand quotas.
Finite Fourier covariance, geographic pool transitions, height/habitat fitting,
regular source positions, plant form and rendering remain separate limitations.

## Verification and comparison

`species_competition_checks.py` tests canonical seed messages, direct exponential
race scores, subnormal/zero weights, independent Gaussian-to-Gumbel categorical
marginals, reversed field order, added zero-support species, fixed tie policy,
prepared partial replay and no field work for unsupported species.

`species_competition_experiment.py --baseline <saved-spatial-product> --output
<system-temp>` consumes the prior actual-source positions/heights/IDs without
provider work. Compare streamed per-taxon maxima with a materialized N×K score
reference under identical fields. Both use the already measured per-wave
streamed evaluator. The matrix is explicitly bounded to16,777,216 elements;
1..128 prepared taxa and at most1,000,000 sites bound this experiment.
No claim that O(NK) competing fields are as cheap as one categorical rank.
Three alternating repeats and a full reversed-field run must give identical
indices. Fresh-process replay forbids provider/catalog/PROJ/generator imports
and patches field/weight constructors to reject reconstruction.

Use the existing source-site plot with explicit titles, identical window and
colours,2560×1440. This is a distribution comparison, no3D Place image or new
optically accepted species. Actual-source results are pending.

## Sources

SIGGRAPH first: Deussen etal.1998 §4, population versus detailed form, and the
previously filed supplementary Rahimi/Recht2007 §3 paired Fourier features
(derivation and optical checks in `species_spatial.md`).
Maddison/Tarlow/Minka, *A* Sampling*, NIPS2014, §2/PDF-page2: Gumbel-Max and
Eq.2 for categorical probability were opened optically. Only the discrete
Gumbel-Max rule is used here, not the paper's continuous A* search algorithm.
[Proceedings PDF](https://papers.nips.cc/paper_files/paper/2014/file/937debc749f041eb5700df7211ac795c-Paper.pdf)
filed under `doc/references/vegetation/nips/2014-a-star-sampling.pdf`,
SHA256`8e775c95aa4cf563ba1f6903a95b1c55794fcb56057d27bf0ab13d44ab8d24a8`.
This is our own spatial application; no reference implementation is bundled.
