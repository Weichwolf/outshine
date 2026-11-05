# GPU-compacted indices and programmable vertex pulling

Owner #1955. Device experiment, not a scene benchmark or a frame-budget claim.
Design reference: [GPU-Driven Rendering Pipelines, SIGGRAPH2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
its index compaction / manual vertex-fetch alternatives, and Riccio2013 in its bibliography.

The native Metal probe compares8,192 indexed indirect calls with one indexed
indirect draw. A compute kernel reads the same visibility arguments, packs visible
indices and writes the final draw count. The new vertex shader decodes a virtual
cluster/local-vertex ID, reads its source-vertex offset and instance, then fetches
positions and colour. Source vertices and instance IDs use independent permutations;
the source triangles alternate orientation. Negative baseVertex and a physical
index-buffer offset remain in the reference path.

Each synthetic cluster has three vertices in a64-slot virtual range. **This is not
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
single-instance triangles,14 alternating rounds,2 warmup rounds; means use12
samples per mode. Every result must match the reference bytes, including warmup.
Nonempty input must produce coloured pixels; GPU timings must be finite/positive.
The driver exits nonzero on any violation. Keep binaries and output in system temp.

## A18 Pro, 2026-11-05

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
