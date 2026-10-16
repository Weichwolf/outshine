# Adaptive normal groups: rejected runtime candidates

Owner #422, parent #6, goal #189. Follow-up to [the independent GGX
comparison](relief_shading.md), with exactly its24 cases,288 reference curves and
65 directions per curve. No product shader, material, light loop, binding or
asset changes. The input clouds are converged offline quadrature products, not
bounded runtime statistics.

The selection limits were recorded in #422 before execution: maximum error
at most2% of each curve's reference peak, RMS at most0.5%. Weak curves remain;
these are component-selection limits, not a Place/visual/frame acceptance.

## Methods and decision

Preserve the original nine negative/zero/positive slope groups. For larger
capacities split the selected group's unit-normal covariance along its largest
eigenvector, using a deterministic stable weighted median. Compare selection by
area times angular variance with selection by angular variance alone. A uniform
plane stays one group. Weights sum to1 and normals remain unit within1e−12;
the nine-group baseline reproduces #426 within1e−11. Flat, constant-slope and
zero-depth responses retain the original within1e−11 at every capacity.

Also apply the existing Toksvig formula separately inside each group, using its
mean unit-normal length. This retains distinct directions and only broadens
their unresolved interiors; it is neither global isotropic roughening nor an
exact GGX convolution. The comparison does not add that treatment to the native
cost kernel because it already fails the CPU selection limits.

| Capacity | Area point failures | Angular point failures | Area local Toksvig failures | Angular local Toksvig failures |
|---|---:|---:|---:|---:|
|9|20|20|19|19|
|17|6|6|11|12|
|33|4|5|11|11|
|65|4|4|9|9|
|129|4|4|3|3|

Counts are failed curves out of288; one failure rejects a candidate. At129,
point-group worst peak-relative error remains30.663%/30.142% for area/angular;
local Toksvig remains11.629%/12.830%. More point groups do not monotonically
improve weak glossy curves. The three-page local PDF was optically opened:
all capacities/limits, retained weak glossy/matte counterexamples, native cost.
No model is selected. Do not increase group counts without a different error
model or actual visible need; finite within-group response and horizon/visibility
assumptions need explicit treatment. The oracle remains the current guarded
shading-normal BRDF, without displaced-surface micro-occlusion.

## Actual native work

The existing immutable32-byte material consumer executes the unchanged
metalRoughBrdf.glsl and original alpha=r²/a2=alpha² arithmetic on RTX3060/Vulkan.
Statistics are already resident and reused; this is an optimistic shading-only
cost, not statistics construction or a whole light-loop/frame measurement.
All six115200-probe batches pass validation/synchronization validation and
numeric bounds. Timed/plain captures are byte-identical, forward and reverse.

| Method | Mean evaluated lobes/probe |32-dispatch GPU sum forward/reverse (ms)|
|---|---:|---:|
|Mean gradient|1|3.151232 /3.144320|
|9 groups|3.25|3.291904 /3.280704|
|17 groups|6.375|3.442432 /3.434208|
|33 groups|11.041667|6.448224 /6.385248|
|65 groups|20.375|10.066688 /10.040864|
|129 groups|39.041667|17.366976 /17.399520|

These are real dispatch timestamps, not fence time; host/GPU use was not
isolated. Do not compare absolute times to earlier batches as a code speedup or
regression. The129 case evaluates4497600 lobes per dispatch and still fails
quality. Requested resources are five buffers,14752192–14779680B; upload writes,
GPU copies and readback are separately recorded. Device/driver peak is unknown.

The initial float64 point-response limit failed: narrow GGX amplifies float32
dot rounding (maximum raw difference0.0004819221). Retain that failure; do not
silently raise the original3e−6+1e−4×response arithmetic limit. A separate
original GPU probe checks all1489 unique tested normal vectors against all130
light/view pairs:193570 records, max dot error1.743239e−7 against the fixed3e−7
limit. Propagate this checked uncertainty through GGX: radiance increases with
nl/nh and decreases with nv/vh for these inputs. Positive weights preserve the
resulting interval. Native outputs exceed that interval by at most2.402e−9,
inside the unchanged arithmetic remainder. This is scoped numeric evidence on
the tested inputs, not a universal vendor precision or full-vector parity claim.
All three SPIR-V probes cross-compile to MSL2.1; no Metal execution.

## Construction and reproduction

Area allocation to129 groups across24 cases visits2851490 points in1796 group
statistics calls and859 stable sorts over1885837 elements. It makes3592 named
indexed array copies/91247680B. This count excludes covariance/sort scratch;
the traced build peak20972048B includes tracked Python/NumPy temporaries after
input clouds are resident, not untracked native-library scratch or process peak.
Build time1.736s includes tracing on this loaded host. It is not a runtime method
speed claim. All original inputs, weights and groups remain reproducible.

Use NumPy/Matplotlib/Shapely, glslang, spirv-cross, a valid Vulkan validation
runtime and the existing native material consumer with its matching header/
archive closure (binary SHA ef6732aa… from #426). Explicit system-temp paths:

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render \
nice -n 10 python test/experiments/render/relief_distribution.py \
  --reference /tmp/outshine-material-relief-shading \
  --output /tmp/outshine-material-distribution-area --strategy area
```

Repeat with --strategy angular and a different output. Then run
relief_distribution_native.py with --source pointing at the area output,
--output at another temp directory, --binary at the retained consumer and
--profile. relief_distribution_plot.py consumes --area/--angular/--native and
an explicit --output PDF. Environment/runtime setup follows the linked study.
Reports retain input/source/binary hashes, exact commands, failures and work.

Sources remain the optically reviewed SIGGRAPH2007 Han normal-distribution
filtering PDF S1/Fig1–2 and LEAN I3D2010 S3–4/Fig6–8/Eq1–6. The covariance split
here is a local experiment, not Han's spherical-vMF fit. No Frame/Place/Metal/A18
claim follows. #422 stays open for a justified bounded filter and actual lit
integration; #405/#416 remain independent product deliveries.
