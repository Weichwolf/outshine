# GPU-compacted indices: native vertex fetch and pulling

Owner #1955. Device experiment, not a scene benchmark or a frame-budget claim.
Design reference: [GPU-Driven Rendering Pipelines, SIGGRAPH2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
its index compaction / manual vertex-fetch alternatives, and Riccio2013 in its bibliography.

The native Metal probe compares8,192 indexed indirect calls with one indexed
indirect draw. A compute kernel reads the same visibility arguments, packs visible
indices and writes the final draw count. The new vertex shader decodes a virtual
cluster/local-vertex ID, reads its source-vertex offset and instance, then fetches
positions and colour. Source vertices and instance IDs use independent permutations;
the source quads alternate orientation. Negative baseVertex and a physical
index-buffer offset remain in the reference path.

Each synthetic cluster now has four shared vertices and six indices. Pulling uses
a64-slot virtual range; native fetch keeps canonical indices and a uint32 model
slot per source vertex. Both merged paths use one indexed draw. **This is not
the current engine cluster format.** ClusterDag defaults to128 triangles per
cluster; that does not imply at most64 unique vertices. Native integration must
measure actual unique counts and build a suitable packed reference layout before
switching shaders. Do not reinterpret existing indices or assume this probe's limit.

## Reproduce on Metal

```
clang++ -std=c++23 -fno-exceptions -fobjc-arc -O2 -Wall -Wextra \
  -Wpedantic -Wshadow -Werror -ffp-contract=off \
  -framework Foundation -framework Metal \
  test/harness/render/MetalVertexPullProbe.mm -o /tmp/outshine-vertex-pull
/tmp/outshine-vertex-pull 0
/tmp/outshine-vertex-pull 64
/tmp/outshine-vertex-pull 4
/tmp/outshine-vertex-pull 1
MTL_DEBUG_LAYER=1 /tmp/outshine-vertex-pull 1
```

The argument is the visibility divisor;0 means empty. Target256×128,8,192
single-instance quads,14 alternating rounds,2 warmup rounds; means use12
samples per mode. Every result must match the reference bytes, including warmup.
Nonempty input must produce coloured pixels; GPU timings must be finite/positive.
The driver exits nonzero on any violation. Keep binaries and output in system temp.

## Historical three-vertex fixture, A18 Pro, 2026-11-05

No competing build; ordinary timings below exclude validation. GPU clocks affect
absolute numbers: these are one comparison, not a stable speedup bound.

| Active | CPU loop → pull, ms | GPU loop → pull, ms |
|---|---:|---:|
|0|0.516 →0.017|0.842 →0.038|
|128 (`8192/64`)|0.443 →0.013|0.849 →0.042|
|2048 (`8192/4`)|0.417 →0.012|0.918 →0.114|
|8192|0.400 →0.012|0.750 →0.184|

All four cases and a separate dense Metal-validation run passed. This models
command/vertex-reference execution, **not** production materials, shadow passes,
LOD transitions, blending order, simulation or source/asset IO. Atomic compaction
is valid here because triangles occupy disjoint cells; order-sensitive production
passes need their own contract. Next compare real resident geometry, material/layout
buckets, packed references and bounded residency; retain the current product path
until actual Places show a gain.


## Native vertex fetch, four-vertex fixture, A18 Pro, 2026-11-05

`native` binds a Metal vertex descriptor (`Float2`, per-vertex stride8), retaining
canonical indexed fetch; a uint32 lookup selects the model independently of the
source permutation. Indices reuse vertices0 and2 for the second triangle. The same
compute pass compacts canonical indices for native fetch and virtual indices for
pulling. Original draws retain negative baseVertex and physical index-buffer offset.

| Active | CPU loop / native, ms | GPU loop / pull / native, ms |
|---|---:|---:|
|0|0.356 /0.011|0.843 /0.041 /0.040|
|128|0.358 /0.010|0.858 /0.043 /0.083|
|2048|0.384 /0.011|0.948 /0.128 /0.129|
|8192|0.364 /0.010|1.128 /0.345 /0.348|

All four cases and a separate dense Metal API-validation run exited0. All three
paths produced identical bytes in every round; nonempty inputs produced colour.
Validation timings are excluded. Logs/binary: `/tmp/outshine-master-1955-native-vertex-fetch*`.
No competing native build during measurement; GPU clocks remain variable. A later
dense run measured GPU0.612 /0.187 /0.181 ms (loop /pull /native); compare
within one interleaved run, not absolute numbers across runs.

This establishes native fetch plus model lookup for a tiny flat-colour workload,
not a native-over-pulling speedup or a scene/frame-budget result. It does not test
production normals, PBR, motion, shadows, instancing or asset/residency costs.
[`static_vertex_batches.py`](static_vertex_batches.py) defines the single-model
source-range admission and preserves instanced/shared-mesh draws without vertex
expansion. Next: real resident input and existing vertex/material/pass contracts.
