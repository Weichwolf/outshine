# Subpixel procedural relief and specular response

Owner: #422, material composition #6, goal #189. This is the first model
comparison, before any runtime statistics or shader hookup. It changes no product
shader, light loop, material row, binding, asset or public API. #387/#405 remain
the separate filtered-height/normal-composition delivery.

## Input and independent reference

Use the existing declared 250×80mm running bond, 10mm mortar, 2mm height and 2mm
bevel. These are procedural recipe assumptions. The isolated plane has no window,
band, colour variation, normal texture or micro-occlusion; it tests specular
response only. Roughness0.9 matches the existing native wall fixture;0.2/0.5
are explicit glossy/medium counterexamples, not changed brick recipes. F0 is0.04,
unit point-light irradiance, view0/50degrees, light azimuth0/90degrees and65
light angles from−80 to80degrees. Camera/view changes do not alter the input plane.

The reference clips the real pixel parallelogram against every linear bevel
piece, then integrates the point normal's GGX response with positive Gauss-Duffy
triangle quadrature. It does not use the candidate subcell/box filter. First
gradient moments also match #387's independent polygon area/centroid integral
within1e−11; weights sum to1 within1e−12. Flat interior, zero depth and a constant
slope match all candidates within1e−11. Pixels include axis-aligned, sheared and
1:64 thin footprints from0.2mm to250mm, plus seven moving joint samples.

Each response curve must converge between successive per-axis orders16/32/64/128
within1e−6+1e−4 times its own reference peak; no global bright curve can conceal
a weak one. This is a successive-quadrature check on the sampled directions, not
a rigorous global error bound. Final orders:19 cases32, four64, one128. The
largest successive absolute change is6.318260e−5 on a bright glossy response.
The oracle reproduces the existing shading-normal BRDF and its nl/nv guards;
it is not a physically complete displaced-surface visibility/energy reference.

## Compared models and result

All statistics below are ideal offline reference statistics. No bounded runtime
method for producing them is implied. The baseline gets the exact mean gradient,
so it is more favourable than introducing #405's finite subcell-filter error.

- Mean gradient: resolve one normal, retain authored roughness.
- Unit mean: normalize the mean point normal, retain authored roughness.
- Toksvig: unit mean plus the existing production roughenedBy formula.
- Slope trace: roughness=(r⁴+trace(Cov(slopes)))^(1/4), clamped at1.
- Normal mixture: partition slopes by negative/zero/positive x/y; retain the
  area and normalized mean unit normal of each nonempty group, shade separately
  with the original roughness and sum. One to nine components occur. This is an
  explicit simple experiment, not Han's fitted spherical-vMF algorithm.

The full24-case cohort contains1440 curve comparisons (five methods × three
roughnesses × two views × two azimuths ×24), each65 light directions. RMS below
is over those directions/cases/views/azimuths at each roughness, in linear
specular radiance per unit irradiance; this is not hemispherical energy.

| Roughness | Mean gradient RMS | Toksvig RMS | Slope trace RMS | Normal mixture RMS |
|---|---:|---:|---:|---:|
|0.2|0.41197946|0.26042709|0.26491928|0.00044373|
|0.5|0.01367351|0.00704273|0.00953340|0.00017736|
|0.9|0.00036884|0.00025077|0.00040341|0.00002088|

Do not select an isotropic roughness correction: the slope trace worsens even
the matte cohort RMS. Toksvig improves this absolute aggregate but its worst
glossy weak-curve error reaches22.794 times that curve's peak; slope trace reaches
27.533. Both erase distinct sharp lobes or introduce a broad response elsewhere.
The mixture keeps the distinct responses much better, but its worst peak-relative
error still reaches63.275% at roughness0.2 and9.417% at0.9. A good overall RMS
does not accept those cases. All errors and signed means remain in report.json.

The four-page local PDF was optically opened: near joint, far period, worst
single-lobe sheared corner and worst mixture transverse-light case. The last two
are negative controls, not hidden by the favourable near/far examples. No new
model is selected for production. Next: improve the within-group corner response
and compare a bounded distribution/filter with actual work/GPU cost before
requesting any shared-light-loop change. Normal-map composition, opening/band
covariance, motion beyond these samples and actual lit native/Place images remain.

## Native BRDF check and numerical boundary

The existing immutable NativeMaterialProbe (SHA256
ef6732aa05d6dbd103f6f429e4db77437831cbb1c79cf178eb8170dfcdfdb2a0)
executes the unchanged production metalRoughBrdf.glsl on RTX3060/Vulkan with
validation and synchronization validation. Its retained matching native
headers/archive are separate from current Device API changes.2610 records pass
with no validation errors. The probe cross-compiles to MSL2.1; no Metal execution.

GPU dot products match independently computed inputs within1.148627e−7 against
a fixed3e−7 limit. Given those separately checked float32 intermediate values,
GGX arithmetic matches within2.025397e−5 and the fixed3e−6+1e−4×response limit.
The full float64-vector response differs by up to6.460771e−5 in87 tangent cases:
an exactly zero nl becomes about1e−8 under fused dot evaluation, while nv is
clamped to1e−6. These cases are recorded separately and are not full-vector
parity claims. All non-tangent cases retain the original response limit. No
production guard, oracle value or scene budget was changed to conceal them.

The initial native launch used an invalid unmapped palette index, then lacked
the validation-layer runtime path; both failed before capture. The first direct
float64 response assertion also failed at this tangent condition. Those tool/
conditioning failures are not successful runs; the final scoped checks above
are terminal0. The initial native counterexample records are retained locally.

## Work, limits and reproduction

The final reference rules evaluate965632 quadrature points in total; preceding
convergence rules add work. The largest final gradient/weight arrays alone use
7864344bytes, not process peak. Complete reference/candidate case work takes
about32s on this loaded host; no runtime-method speed claim follows. Single-lobe
models evaluate one specular lobe per light, the mixture1–9, before any statistics
construction. That additional light work needs a measured justification.

Native palette126560B, probes83520B, outputs83520B. The existing consumer requests
five buffers totalling587200B, writes210080B to its upload mapping, issues
293600B of GPU copies and reads83520B back. These are requested resources and
transfer directions, not device/driver peak. Native dispatch time, full-world CPU/GPU/memory,
Metal/A18, load and frame acceptance remain unmeasured. No general GGX variance
or exact GGX convolution follows from LEAN's Beckmann moments.

Use NumPy, Matplotlib, Shapely, glslang, spirv-cross and the existing native
32-byte material probe. Set Vulkan ICD/validation-layer/library paths for the
installed runtime; its Device requests validation. Keep its compiled archive
and headers together. Private outputs stay in system temp:

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/render \
nice -n 10 python test/experiments/render/relief_shading.py \
  --output /tmp/outshine-material-relief-shading \
  --binary "$OUTSHINE_NATIVE_MATERIAL_PROBE"
```

Omit --binary only for the CPU comparison; that does not assert GPU execution.
Exact compile command, binary/source hashes, per-case work and terminal native
captures are retained with the local report. Sources: SIGGRAPH2007 Han et al.,
local PDF S1/Fig1–2; I3D2010 Olano/Baker LEAN, corrected authors' PDF S3–4,
Fig6–8/Eq1–6. Those pages were optically opened; existing papers/primary links
remain in [the reference index](../../../doc/references/README.md). #44's plant
statistics and #422's native material work remain separate deliverables.
