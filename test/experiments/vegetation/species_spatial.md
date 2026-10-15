# Spatial species marking experiment (#226)

The actual Koerbersee species-choice plot from PR#256 has independent site
marks. This experiment adds an authored spatial scale without moving/removing
any source site or changing the relative height-weight table. It is not a
competition/succession simulator, calibrated abundance map or accepted plant
recipe. It does not produce a3D Place image.

## Construction and probability contract

Use global ECEF metres, not camera-local ENU or wrapped latitude/longitude.
Prepare M independent physical wavevectors `w_j ~ N(0, I/length_m²)` and
independent coefficient pairs `a_j,b_j ~ N(0,1)`. Store their actual Float64
values in the versioned recipe, rather than relying on a future RNG replay.
The prepared field is

`Z(x) = sum_j[a_j cos(w_j·x) + b_j sin(w_j·x)] / sqrt(M)`.

For any fixed point and wavevectors, `Var[Z(x)] = sum_j(cos²+sin²)/M =1`.
The Gaussian coefficient ensemble therefore gives a standard normal marginal.
Its covariance is `sum_j cos(w_j·(x-y))/M`; averaging wavevector ensembles gives
`exp(-||x-y||²/(2 length_m²))`. A finite prepared field approximates this kernel;
its residual far correlation is not guaranteed zero or exactly isotropic.

Combine it with an independent site normal `G = Phi^-1(u_site)`:
`u = Phi(sqrt(strength)*Z(x) + sqrt(1-strength)*G)`. The ideal pointwise
marginal is uniform, so the existing categorical sampler preserves its target
probabilities over the random ensemble. This is not an exact finite-stand
species quota. Correlated rows have fewer effective independent samples.
The finite53-bit site variate needs a declared zero-endpoint bound2^-54;
Float64/CDF rounding is bounded to keep output below1. Strength0 returns the
original variates exactly. Fixed taxon CDF order influences spatial adjacency;
this remains an explicit limitation of this simple marking model.

The world uses one saved field, not a newly seeded field per tile/region/query.
ECEF avoids artificial dateline/pole/tile seams. Heights/habitat weights can vary
at the sites independently. Geographic pool transitions remain unresolved in
#226; this does not repair abrupt candidate changes. Instantaneous wind/weather
and camera/LOD do not change selected taxa. A runtime cache hit loads saved
indices directly; the replay experiment additionally tests partial regeneration.

## Equal-task comparison and validation

Compare a bounded N×M phase matrix with a streamed per-wave phase evaluator.
Both use the same scalar component-operation and wave-accumulation order, no
BLAS shape-dependent reduction. Require bit-exact variates and selected taxa.
Stream scratch is O(N), phase-matrix scratch O(NM); both do O(NM) arithmetic.
At most1,000,000 positions,1,024 waves; the reference matrix separately caps
16,777,216 elements. These are explicit experiment bounds, not frame budgets.

`species_spatial_checks.py` checks known trigonometric values, transform and
endpoints, analytic feature variance, independent ensemble categorical counts,
near/far covariance, seam-free physical coordinates, immutable recipe reload
with preparation forbidden and exact matrix/subset/permutation replay.
Actual-source runner: `species_spatial_experiment.py --sites <cached-sites.npz>
--scene <cached-scene.json> --baseline <species-mixture-product> --output <temp>`.
`species_spatial_replay.py --product <temp>` uses only saved arrays/parameters;
provider/camera/catalog/PROJ/Bpy/generator imports are forbidden.
`species_spatial_plot.py` compares identical source windows in2560×1440.
Current authored controls are60m length,256 waves and strength0.85; no botanical
source claims these are measured forest parameters.

## Completed actual-source comparison

Frozen `19e25f93c` completed with exit0 on all53,200 original Koerbersee IDs;
43,347 remain forest choices and9,853 remain unsupported nonforest sites. Six
alternating rounds produce bit-exact matrix/stream variates and choices. Median
stream0.590928s versus matrix1.725110s. The streamed path avoids a108,953,600-byte
phase matrix; this is a concrete allocation difference, not measured process-RSS
savings. The whole comparison includes both methods and peaks at288.87MiB;
17.875s wall/17.022CPU-s, with host load recorded in the cost receipt.

Among43,330 nearest-neighbour pairs within30m (median nearest distance10.118m),
the same-species fraction rises from0.261782 to0.560397. This is descriptive,
not an ecological target. Species counts change from
`[257,10320,7679,16266,3644,5181]` to
`[275,9008,7109,16955,3889,6111]` in canonical taxon order. No empirical quota
correction is applied: clustered finite stands are not independent samples.
Fresh process replay reproduces47,284 forest selections exactly for reversed
and partial requests; all saved variates also match, with no provider/catalog/
PROJ/generator import and field/weight preparation explicitly forbidden.

The2560×1440 before/after plot was opened. The independent salt-and-pepper marks
form spatial groups after correlation. Regular source-site spacing remains;
this experiment deliberately preserves positions. Taxon adjacency from a
single ordered latent value is a model limitation; independent per-taxon
competitive marks are a sensible next alternative before ecological promotion.
No plant form, material,3D image or ecological calibration is accepted here.

Current frozen reports/choices/replay/plot:
`/tmp/outshine-vegetation-species-spatial-19e25f93c/`.
Four new independent mathematical controls and six existing mixture controls
pass; Ruff format/check and the fresh-process replay complete successfully.
The model remains experimental rather than a new public library/ecology API.

## Primary sources

SIGGRAPH first: Deussen etal., *Realistic Modeling and Rendering of Plant
Ecosystems* (1998), §4. Figure3/page5 and the population classification were
opened: separate spatial population descriptions from detailed forms. The
paper's ecological simulation is not implemented by this noise field.
Rahimi/Recht, *Random Features for Large-Scale Kernel Machines* (NIPS2007), §3,
Figure1/table/equations2–3 on PDF-page3 optically checked. Their paired
sine/cosine feature map motivates our finite covariance representation; the
Gaussian-coefficient/CDF marking above is our own application and derivation.
[Author copy](https://people.eecs.berkeley.edu/~brecht/papers/07.rah.rec.nips.pdf),
local `doc/references/vegetation/nips/2007-random-fourier-features.pdf`,
SHA256`0745bc8908b05cb795d0775f5cde1643cae55ead1a7a5f6155b5905fc4facd89`.

Cook/DeRose SIGGRAPH2005 *Wavelet Noise* was considered as a spatial-field
alternative ([author PDF](https://graphics.pixar.com/library/WaveletNoise/paper.pdf)).
No shader/displacement texture is evaluated here, so this experiment favours
small saved continuous parameters over a new volumetric noise texture. Neither
spatial-model superiority nor shader noise/aliasing quality is claimed.
