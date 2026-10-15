# Prepared height-dependent species mixtures

[#226](https://github.com/Weichwolf/outshine/issues/226) owns this selection
component. `SpeciesHeightMixture` prepares nonnegative relative weights at
physical height knots. Between knots, raw weights vary linearly; normalization
occurs at the queried height. This differs from interpolating independently
normalized endpoint distributions. Clamped endpoint behaviour is explicit.
It is a mathematical sampler, not a calibrated global ecological model.

```python
from outshine_ref.vegetation import SpeciesHeightMixture, species_site_uniforms

# Mathematical example; these are not botanical height preferences.
mixture = SpeciesHeightMixture.prepare(
    ("species-A", "species-B"), (0, 100), ((3, 1), (1, 3))
)
u = species_site_uniforms([[14, 8653, 5752, 12, 32]], seed=17)
selected = mixture.choose([40], u)
cached = SpeciesHeightMixture.from_recipe(mixture.recipe())
```

Preparation sorts unique taxon IDs, applies one common scale to the full
weight table and stores immutable cumulative weights. A query interpolates
only the total and the cumulative entries visited by paired binary search:
O(N log K) work and O(N) scratch, without an N×K matrix. One variate selects
half-open cumulative intervals. Zero-weight taxa remain unselected; a zero
total returns **−1, unsupported**, with no uniform fallback. This is not an
instruction to remove vegetation: callers retain unsupported sites/reasons.
Relative composition does not determine stand density or a tree line.

The threshold is kept strictly below a positive total, including subnormal
totals where multiplication could round a variate below1 up to the total.
Tests also cover unsigned decreasing input tables, overflow-scale weights,
leading/trailing zeros and immutable independent inputs. Stored tables round
trip exactly through the trusted recipe; `from_recipe` does not call weight
preparation. This is not an untrusted runtime asset decoder or a cache manager.

Limits: 1–2,048 taxa, 2–4,096 strictly increasing finite height knots within
±1,000,000m, at most1,048,576 table entries and1,000,000 queries per call.
These are explicit reference-array limits, not CPU/frame/device guarantees.
Probabilities use Float64; variates have53bits. Tiny unrepresentable weights
are not promised exact real-number probabilities. Camera/LOD are not inputs.

`species_site_uniforms` takes an N×D nonnegative integer array, D1–16, and a
Python integer seed in[0,2^64). Each row hashes independently with BLAKE2b-64:
ASCII `outshine-species-1`, NUL, little-endian uint64 seed, little-endian uint16
arity, then D little-endian uint64 components. Interpret the eight digest
bytes little-endian; its upper53bits divided by2^53 give[0,1). Subsets, chunks,
row permutation and host array endianness preserve values. These random
variates are not unique identities; original site IDs remain authoritative.

## Actual-source comparison and replay

`test/experiments/vegetation/species_mixture_experiment.py` compares this method
with a bounded materialized cumulative-matrix reference at identical variates,
heights and prepared weights. The experiment imports the library winner; only
the alternative remains in its comparison adapter.

Frozen `a6a7edf16` used the unchanged cached Koerbersee stand:53,200 unique IDs,
43,347 forest sites and9,853 nonforest sites outside this pilot. Contact heights
are reconstructed with PROJ from the actual WGS84/ENU scene, in the existing
provider height convention; local ENU-Z is not treated as world altitude.
The geographic prior selects AUT/GER and the WCVP pilot supplies native/certain
candidates. Six explicitly authored forest profiles then propose species.
No forest site in this corpus lacks positive support. These are uncalibrated
model weights, not measured ecological abundance or botanical acceptance.

Both methods choose exactly the same taxa. Median of six alternating rounds:
matrix10.278ms, binary7.608ms; all53,200 site variates70.672ms once. Whole
experimental process0.877s wall/0.911CPU-s,81.11MiB peak RSS includes PROJ,
source/metadata loading and both methods, not just the sampler. Host process
load was recorded; no isolation, frame or A18 claim. Current reports:
`/var/tmp/outshine-vegetation-species-mixture-a6a7edf16/`.

Stored choices retain every original site ID. A separate process reproduces
reversed/subset requests from the stored prepared recipes/heights/IDs. After
library promotion its normal base geometry dependency may be imported;
providers, taxonomy catalog, PROJ and scene/generator preparation remain absent.
This distinguishes dependency imports from prohibited source reconstruction.
An actual runtime cache hit can consume the saved species indices directly.

The2560×1440 private site/composition plot was opened: the proposals vary with
terrain height within existing forest polygons. Independent site draws still
lack ecological patch/clustering effects. This is not a3D Place image.
No public vegetation image is replaced by this numerical/plot evidence.

## Sources and remaining scope

Deussen etal., *Realistic Modeling and Rendering of Plant Ecosystems*, SIGGRAPH
1998 §2/Fig.1 motivates separate distribution and shared forms; local PDF
pages3/4 were also opened. `species_ecology_pilot.json` links InfoFlora's
qualitative elevation/habitat accounts for oak, beech, spruce, Scots/Swiss
pine and larch. Its numerical knots/weights are our declared model choices.
They are not copied measurements or a globally valid altitude classifier.

Climate/soil/moisture, nonforest habitats, cultivated pools, continuous
geographic transitions, spatial groups, bound plant recipes and rendering
remain open. Native stand/cache/LOD integration and device costs remain #48.
