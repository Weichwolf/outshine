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
optically accepted species. Completed actual-source results follow below.

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

## First completed actual-source comparison

Frozen `9c6993496` completes with exit0 on all53,200 unchanged IDs;43,347 forest
choices and9,853 unsupported nonforest sites remain. Stream/matrix and full
reversed-field traversal choose exactly the same taxa. Three alternating rounds:
stream median3.420350s; score-matrix3.476795s. Their costs are similar at six taxa;
no meaningful speedup is claimed. Per-taxon field evaluation costs more than the
one-field model from PR#258; this buys independent spatial species marks, not
an identical joint distribution. Zero-weight sites avoid field/hash work.
Whole comparison31.183s wall/28.404CPU-s,86.67MiB peak RSS; actual host load is
recorded. This is cold plan work, not a runtime/frame/device/cache-load budget.

Same-species nearest-neighbour fraction within30m changes from0.560397 (shared
rank) to0.690561 (competing taxa), with43,330 pairs and10.118m median distance.
Neither statistic is a calibrated ecological target. Canonical species counts
change from`[275,9008,7109,16955,3889,6111]` to
`[223,10181,8073,16752,3352,4766]`; pointwise ensemble probabilities remain the
specified weights, but this finite correlated stand is not a quota sample.
Fresh-process replay reproduces47,284 reversed/subset forest choices exactly,
without source/provider/catalog/PROJ/generator import or constructor work.
The2560×1440 actual-source before/after plot was opened: distinct groups remain,
and the right-hand result is independent of field iteration order. Regular
source spacing and lacking3D botanical verification remain visible limitations.
Six new controls and ten preceding spatial/mixture controls pass.
Current reports/choices/replay/plot:
`/tmp/outshine-vegetation-species-competition-9c6993496/`.

The focused delivery on frozen `ee2189054` corrects the receipt's neighbour
labels to distinguish shared-rank and competing fields. Its complete actual
rerun and fresh replay also end with exit0; all saved choice columns exactly
match the first run. At higher recorded host load, medians are3.955267s stream
and4.227258s matrix. External terminal receipt:37.052s wall/29.679CPU-s and
102.25MiB peak RSS; the in-process report is a pre-exit snapshot, not that
terminal peak. No timing threshold is fitted to either run. Current choices,
the identical inspected plot, report and replay are retained under
`/tmp/outshine-vegetation-species-competition-ee2189054/`.
