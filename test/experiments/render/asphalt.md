# Dry asphalt: filtered component study

Contract #781, parent #6. The component study is independently checked;
outshine_engine owns its later product integration. No production shader,
SurfacePattern, asset, source classification or world geometry is changed.
The current prototype is a filter/response baseline, not final asphalt art.
Its soft round grains still need an appearance comparison with irregular,
angular aggregate and a defensible grain-density/size distribution.

## Current component verification

The final unchanged Python cohort, independent box replay and phase checks were
completed by outshine_research on the saved head 7eb9b484d; PR #787 links the
terminal receipts. The current merge preserves all seven Python blobs exactly.
The following original receipts retain their own source and device scope.

Apple A18 Pro / Metal also executes both 1,856-point compute modes and all eight
actual raster pairs through matching native 7bdb46175 consumers. Maximum field
error is 1.668707e-6 against the unchanged 3e-6 limit; every parameter and direct-MR
bound passes. All eight PDF pages were opened. Soft near grains remain an art
limitation; the far filter converges to the authored material mean.

Metal supplies no individual dispatch timestamps here. The optional `--profile`
invocation remains failed/unproved; its reported 0.668750 ms command batch covers
32 dispatches plus uploads/readback and is not the earlier RTX kernel sum.
The plain numerical invocation passes. Python 3.9 rejected `zip(strict=True)`;
the current run uses Python 3.14, NumPy 2.5.4 and SciPy 1.18.1.
Commands, source/archive hashes and failed attempts remain in
`/tmp/outshine-pr787-metal/` and the PR. No device/frame/material adoption follows.

## Existing product paths and missing contract

RoadSurfaceBuilder carries separate transport surface semantics for named
alignment/routes. Its asphalt is linear [0.11,0.11,0.10], roughness0.92.
Actual Places use Corridors::HandsThePavingOver/BeginTransfer: one streets
material with vertex-coloured pavement, kerbs and sides. Marking that entire
draw as asphalt would be wrong. Neither RGB nor slope establishes a material.
The existing world catalogue has asphalt [0.1256,0.1193,0.1104], roughness0.65,
grainSizeM0.008; both real recipes are included in the native raster.

A product hookup needs explicit native face role/surface semantics and a
bounded, stable metric frame. The possible extra material selector has Light's
narrow ACK in #781/6250829520; no full shared/product contract is adopted.
Cache/atlas/copy consumers and actual Places remain unimplemented/unverified.
No new vertex/uniform bytes have been selected. The CPU phase counterexample
proves a need for a coordinate contract, not that existing actual tiles violate
one: at100km/1000km absolute FP32 metres, indistinguishable inputs require
at least2.05045/2.55534sRGB-LSB error for some tested grain pairs. Integer cells
plus bounded local FP32 recover the model; this is not an executed engine ABI.

## Model, independent reference and fixed selection

Each jittered cell contributes w*(G_sigma-0.25*G_2sigma), sigma0.12 cell,
w0.75..1.25, site coordinates0.2..0.8. Every complete kernel has zero area
integral. A24mm cell pitch produces an approximately7mm Gaussian core; this
is an authored procedural assumption, not measured asphalt reconstruction.
Colour and roughness use the same field, amplitudes0.035 linear RGB /0.02.
The 7x7-source-cell reference checks its9x9 tail on4096 points; positive box
quadrature converges16/32/64/128 within1e-6 field. No albedo gamma averaging.

The predeclared limits remain maximum1sRGB-LSB colour and1/255 roughness over
all declared cases. Expanded coverage:29 footprints x64 phases =1856 pixels,
including motion, different cells, mineral centres/rims, rotation, shear,
1:64 footprints and zero footprint. The original17-phase motion-only pass did
not establish a winner; centre/rim probes exposed the single-Gaussian error.

| Method | Failed pixels /1856 | Worst colour LSB | Worst roughness steps |
|---|---:|---:|---:|
|Point + maximum-width fade|164|6.911144|2.539788|
|Isotropic Gaussian|72|2.593677|0.926593|
|Full Jacobian + maximum-width fade|11|1.242621|0.436715|
|Four Gaussian tiles + maximum-width fade|1|1.011636|0.345612|
|Full Jacobian + minor-axis fade|10|1.242621|0.436715|
|Four Gaussian tiles + minor-axis fade|0|0.950556|0.328615|
|Four aspect-oriented Gaussian tiles + minor-axis fade|0|0.552230|0.188814|

The Gaussian pixel approximation uses covariance(dx*dx'+dy*dy')/12. It is
not an exact box integral. Four tiles split2x2, or4x1/1x4 for an axis-length
ratio greater than2. The fade2..4 cells requires BOTH footprint axes to be
unresolved, using the stable small singular value. No increased sample count
was needed for that correction. The point/wrong-fade counterexamples remain.
Native GLSL uses nonnegative cross-product determinant/quadratic identities
instead of cancellation-prone matrix subtraction; no full covariance inverse.

An independent erf-box reference integrates each original Gaussian exactly
along one pixel axis; orthogonal axes use two exact erf intervals, shear uses
positive1D Legendre quadrature along the other axis. It agrees with the retained
converged2D point quadrature on1856 cases to3.071e-14 absolute. The first native
far-image16/32 point quadrature failed convergence and was rejected; using the
erf integral retained the same model/limits and removed the expensive reference
alias. It is not the candidate's Gaussian-pixel approximation.

## Executed native evidence and limits

Retained matching NativeMaterialProbe and NativeFacadeProbe-a56 execute Vulkan
on host trixie, Intel i5-10400 / RTX3060. This is NOT the new Intel i5-1135G7 /
Iris Xe or A18-Pro1280x720/60 acceptance. The binary/source generations are
historical component consumers; no current complete engine build is claimed.

Six118784-probe runs (64 repeated cohorts) pass arithmetic checks: max field
error1.728312e-6 against3e-6; parameter arithmetic3e-7. Timed/plain outputs are
byte-identical; verified duplicate outputs are hardlinked. Actual32-dispatch
GPU sums point/filtered are0.880736/1.552384ms forward and0.881312/1.550528ms
reverse. Background Xorg/host work is present; these are resident component
work, not an isolated speedup, whole light loop, frame or load prediction.

Filtered active pixels evaluate36 cell sites and72 exp calls instead of9/18
for the point baseline. Early far fade evaluates none. Each probe input/output
is32B,3,801,088B each for118784 records; the palette is1280B. Requested inputs,
GPU copies and readback are not driver/allocator/device peak memory. Allocation
counts, full transient copies, actual full-frame impact and native product
residency remain unmeasured. No timing limit is raised from these results.

Sixteen actual160x96 draws yield eight old/new pairs: near, transition, sheared
and far for both existing recipes. All eight PDF pages were optically opened.
Worst parameter colour0.087996sRGB-LSB and roughness0.029847/255; far shaded
images are unchanged. Direct MR-core response changes visibly in resolved
images. The fixture has fixed normal/up, view and one unit white light with
nl0.8; no full shared lighting, shadows, environment or material-loop claim.

The additional direct-response error interval includes roughness variation:
16 analytic source-distance intervals bound Gaussian sums, plus a geometric
tail. The resulting field envelope bounds all roughness, which lies above the
maximum of D(a2) at this fixed half-angle. Both D and correlated Smith then
monotonically decrease. Diffuse colour integrates linearly; specular lies
between the endpoint responses. With3e-6 arithmetic/normal remainder, the
largest exposure4 display bound is0.630855sRGB-LSB. This is deliberately a
bounded interval, not a false exact-GGX-filter claim or other-angle guarantee.

Scripts are studies: inspect recorded passes_parameter_limits and
passes_direct_mr_bound as well as exit status. All eight current records are
true. Broad source/motion/anchor/other-light/Place tests remain. At handover the
last CPU cohort predated the final optional parameter-helper/path edits. That
gap is closed by the final unchanged-source cohort above; historical native
reports still pin their own executed source hashes.

## Reproduction and sources

Python 3.10+ dependencies: NumPy, SciPy, Shapely, Matplotlib and lib/src on PYTHONPATH.
Use explicit system-temp outputs. asphalt_filter.py --output DIR reproduces
CPU cases; asphalt_phase.py --output FILE gives the indistinguishability
counterexample. asphalt_native.py --binary NativeMaterialProbe --output DIR
--repeats 64 --profile runs the immutable32B consumer. asphalt_raster.py --binary
NativeFacadeProbe-a56 --output DIR captures actual raster and the8-page PDF.
GLSLANG/SPIRV_CROSS name pinned binaries. Vulkan ICD, validation-layer directory
and shared-library paths must match the retained consumer installation.

Local complete inputs, logs, hashes, report.json/native.json/raster.json and
PDF are under /tmp/outshine-material-road-aggregate-92ec9cd90 on trixie.
Consumers remain /tmp/outshine-material-masonry-visible-f3335e510/;
Python environment /tmp/outshine-material-reference-env/.

Primary sources: Worley, SIGGRAPH1996, [A Cellular Texture Basis
Function](https://cedric.cnam.fr/~cubaud/PROCEDURAL/worley.pdf), page1 optically
read; Lagae et al., SIGGRAPH2009, existing local sparse-Gabor PDF page6/Fig6/
Eq17-23 optically read. This prototype uses jittered Gaussian differences,
not Worley F1, a full Gabor implementation, or copied third-party code.
