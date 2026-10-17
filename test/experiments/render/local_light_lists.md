# Point-light list counterexamples and method comparison

Refs #448; goal #190. This is a method experiment, not a runtime light-list
implementation or acceptance of thousands of lights. The public counterexample
uses the real Engine/Geometry/renderer API: 15 points plus the declared Sun render
4,096 nonempty pixels, while 16 points plus Sun fail at `setGeometry` because the
subject refuses a seventeenth light. Both cases keep total point candela fixed.
`local_light_limit.cpp` returns 1 for this existing defect, 0 only when both render,
and 2 for SDL initialization failure. It is intentionally outside normal tests.

## Inputs and reference

`local_light_inputs.py` retains 256 Vienna axis source IDs, coordinates and endpoint
DEM priors from Transport's versioned `vertical-transition-cases.json`. Benchmark
lamps are explicitly procedural: every 16 m, both sides, 1 m outside half the
source width, 6 m above interpolated endpoint priors. The 148 unspecified widths
use an explicit 3 m fallback. These are neither observed lamps nor constructed
road/terrain surfaces. Fixture SHA-256:
`6ace4da0733eaac7bc201b19403505d2c834fcbab254a74c000e45dab182be1b`.

The base scene contains 2,536 source-derived finite point lights, range 32 m,
RGB candela (600, 390, 180), plus three authored controls: behind-eye, near and
in the unbounded final depth cell. The dense scene selects 8,189 source-derived
lamps at 4 m spacing plus those controls. Overlap is a separate authored scene
with 1,021 coincident lights, range 128 m, plus controls. Global sets every 127th
source light to range zero; this means unbounded, preserving the public contract.
All source-derived and authored IDs remain in each report.

The reference evaluates every light at every receiver with the existing point
inverse-square/range attenuation and ideal diffuse albedo 0.5. Inputs are
FP32-quantized; CPU evaluation is FP64. Geometric support is checked even where
the receiver normal makes diffuse radiance zero. This does not test Spot,
directional/atlas ownership, full material BRDFs, transparency composition or
shadowing, and is not a general conservative FP32 culling proof.

## Compared methods

The logical camera is 1920×1080, sampled at 96×54 points on each view-depth plane
25/250/500/5,000 m. Compare the complete sum, 64 px tiles, and 24 linear or
logarithmic depth cells. Cluster edges start at 0.1 m, reach 4,096 m and append
an infinite tail. No opaque depth reduction is used; transparent receivers may
query their own depth. A conservative projected sphere-box interval finds cells;
optional sphere versus cell-AABB testing refines them in real arithmetic.

Range-zero lights are stored once globally. Finite lights produce stable-ID CSR
lists. A 16 MiB index cap is an experimental control, not a production/device
budget. Overflow discards partial lists and evaluates every emitter globally;
the diagnostic reports this expensive exact fallback, without a per-cell cap.

On Linux x86-64, NumPy 2.2.4, two concurrent single-BLAS-thread processes under
host load, the twelve recorded variants have zero missing-support queries and
zero relative radiance error (1e-5 denominator floor). Selected CPU work:

| Base street method | Light/receiver tests | Logical GPU payload, bytes |
|---|---:|---:|
| Full sum | 52,648,704 | 162,496 |
| Tiles, 64 px | 1,065,388 | 269,616 |
| Linear, 64 px | 212,426 | 371,108 |
| Logarithmic, 64 px | 120,230 | 495,248 |
| Logarithmic, 32 px | 101,772 | 1,407,960 |

Dense 8,192 lights select 333,015 of 169,869,312 possible pairs. Overlap selects
3,937,143 pairs and needs 4,560,316 logical bytes; forcing a 4,096-byte index cap
preserves the full 21,233,664-pair sum. Turn, orthographic, range-zero and unrefined
variants are separate controls. Plane occupancy is recorded so empty far planes
cannot masquerade as a culling test. The orthographic 250/500 m planes are empty;
its near and tail controls remain nonempty. No completeness claim follows from
empty planes.

This selects both tiles64 and log64 for native comparison, not a device winner:
NumPy grouping makes the tile query faster despite more light tests. Building
log64 is about 2.44 s with allocation tracing, so this Python implementation is
not a frame-time solution. Its traced build peak is 3,015,399 bytes; process
VmHWM is 55,140,352 bytes. Reports distinguish major live arrays, traced build
peak/live allocation count and whole-process RSS. They do not claim cumulative
allocation/copy counts, driver/GPU peak, upload cost or a device/frame budget.
Those measurements belong to the native alternative and runtime integration.

## Reproduction

Requires Python 3 and NumPy; plots additionally need Matplotlib. Private outputs
must name a directory outside the repository. Keep thread settings and load in
any comparison. Run each variant in a fresh process:

```sh
python3 -m unittest discover -s test/experiments/render -p test_local_light_grid.py
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 python3 test/experiments/render/local_light_compare.py --mode log --output /tmp/outshine-local-light-comparison/street-log
```

Repeat `--mode brute|tiles|linear`, `--camera turn|ortho`, `--scene dense|global|overlap`,
`--no-refine`, `--tile 32`, and `--scene overlap --index-bytes 4096`. The PDF plot
expects the base variants named street-brute, street-tiles, street-linear,
street-log and street-log32 under the input directory:

```sh
python3 test/experiments/render/plot_local_light_compare.py --input /tmp/outshine-local-light-comparison --output /tmp/outshine-local-light-comparison/comparison.pdf
```

Compile the C++ counterexample with the normal public client compiler/link flags
and `build/liboutshine.a`, against the matching frozen headers and shader package;
run with `SDL_VIDEODRIVER=offscreen` where required. Never mix old native archives
with current headers. Exact invocations/hashes and terminal results are in the
linked PR's receipt. Production/shared bindings await consumer agreement in #448;
the existing native stage port #328 remains the only renderer transition.

First source: [Olsson et al., SIGGRAPH 2012, Tiled and Clustered Forward Shading](https://www.cse.chalmers.se/~uffe/tiled_shading_siggraph_2012.pdf),
one-page PDF and transparency Figures 1/2 visually inspected. Supplement:
[Clustered Deferred and Forward Shading, HPG 2012](../../../doc/references/lighting/hpg/2012-clustered-shading.pdf),
PDF page 3, Figures 2/3 visually inspected. Our full-camera grid and measured
fallback are an explicit variant; published timings are not Outshine's proof.
