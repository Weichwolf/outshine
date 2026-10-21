# Subject index compaction workgroup comparison

Owner: #1069, parent #16. The existing one-group-per-cluster kernel copies kept
index words to the already scanned offsets. It uses no shared memory or barrier.
The inspected SIGGRAPH 2015
[GPU-driven rendering slides](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
slides 11/13, remain the cluster-selection reference; workgroup choice follows
the actual native job distribution and measured device execution below.

The original Tokyo snapshot has 50,330 jobs and 17,758,773 index references across
all supplied ranges. Of those jobs, 42,186 have 384 indices; short clusters are
not the majority. Three private variants compare 32/64/128 lanes with the same
selection, scan, resources, group count and kernel body. For any positive lane
count L, each index i has exactly one writer `(lane=i%L, iteration=i/L)`.
The standard-library reference checks this mapping for all 128 distinct original
job lengths. It predicts unchanged copy counts and 1,610,560 / 3,221,120 /
6,442,240 launched invocations respectively. Those operational counts do not
substitute for issued SIMD instructions, actual visibility or measured GPU time.

The existing native scan/compact fixture also uses representative original job
lengths 3,18,72,381,384, with skipped jobs, three batches, partial tails and guard
words. All three variants pass 120,913 native checks each: output words, scanned
slots, arguments and untouched guards agree with the independent CPU reference.
This fixture has deterministic UInt32 source words; it does not export every
original Tokyo source-index word. Full-world image comparison is separate.

## Iris result

i5-1135G7/Iris Xe, Clang22/libc++, fresh offline processes, identical original
1280x720 output/60 profile, fill0.6/TAA, 240 km sight, vegetation=no, 60 frames,
360-degree rotation and 30 m translation. These are the unchanged explicit
scenario inputs, not worldwide content or frame-budget admission.

| Lanes | Actual invocations/frame | Compact median ms | Compact p95 ms | GPU frame median ms |
|---|---:|---:|---:|---:|
| 128, first control | 6,442,240 | 1.475286 | 1.606354 | 19.799270 |
| 64 | 3,221,120 | 1.302812 | 1.564062 | 19.977343 |
| 32 | 1,610,560 | 1.404036 | 1.623542 | 20.227395 |
| 128, closing control | 6,442,240 | 1.476198 | 1.641510 | 19.750104 |

64 lanes halves launched invocations and lowers the compaction median about
11.7% relative to either bracketing control. 32 lanes is slower than 64 despite
fewer invocations. Whole-frame medians do not show a gain; the first control also
has a 12.822 ms compaction outlier. Query overhead is uncalibrated and intervals
can overlap. Per-run host load, raw timestamps and all 60 unique exact joins to
benchmark GPU submission durations remain recorded. Each run validates 61 traces/
1,209 samples with no omitted or cancelled capture. All images have the original
SHA256 `57e427d31a806e0e20433fc9701b49c27941826faace2a866212115236ffa412`
and the 64-lane image was opened. All moving frames remain unrefined (#1045).

## Common candidate and verification

Master approved the bounded comparison and synchronized group contract in
[#47](https://github.com/Weichwolf/outshine/issues/47#issuecomment-6324899205).
The common candidate changes GLSL local_size and SubjectCompact.Shape.GroupX to64.
Dispatch remains one group per job. Reflection agrees on every descriptor,
buffer type/offset/access and dimension except the intended local X size.
Shader byte identity includes that size. No device branch, resource, layout,
barrier, queue budget or asset format is added. Driver/register/RSS savings are
not measured. The product SPIR-V is byte-identical to the measured 64-lane probe.

All 59 archive units in the recorded ComputeShaders.h dependency closure are
rebuilt; unchanged archive members are reused. The official build-metadata
generator supplies CalibrationBuild.h. The complete native compute fixture,
actual pipeline/reflection contracts including rejection of stale128metadata,
and camera-movement consumer pass 121,090 checks. Four focused actual-CDB analyses
and format/diff checks pass. This is not a fresh whole-engine build or broad gate.
A fresh uninstrumented product run has 60 complete CPU/GPU samples and the same
opened image hash; CPU-p99 67.723 ms/GPU-p99 29.786 ms, unrefined60/60. It does not
establish an end-to-end speedup. Master owns Metal/A18 and bundled integration.

Originals and commands: `/tmp/outshine-engine-compact-lanes-2ec263b57/`, including
`model.py/json`, `build.py`, `comparison.json`, `native-terminal.json`,
`reflection.json` and `product/{build,checks}-terminal.json`. Prototype setup
initially omitted the shipped asset path; the failed control and cancelled next
run contain no measurement window. Corrected variants resolve the same frozen
asset root; invalid attempts are excluded from this table and retained compactly.
