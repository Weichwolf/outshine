# Source-triangle raster audit

This experiment separates fixed-function rasterization from vertex-expression
uncertainty. A read-only storage buffer supplies original float32 clip points;
the vertex shader only loads a point and writes `gl_Position`. It draws the
original two triangles of each rectangle into a 512×512 D32 reverse-Z target.
No inner-patch triangles, fragment shader or world shader are substituted.

There are 4,096 rectangles, including 16 calibration cases. The runtime seed
controls their constant depths. Homogeneous powers span −126 through 126,
including subnormal components and very large finite values. Every source has
positive `w` and `0 < z < w`; those mathematical conditions alone do not qualify
an implementation's raster arithmetic.

The native exact-input patch reference tests its claimed whole pixels against
the actual source D32 buffer. Missing samples and nonzero values below the
conservative depth are reported separately. `--isolate-case N` renders only the
original two triangles of case N, at the same location, with the same supplied
points. This distinguishes individual failures from overlapping erroneous
primitives in the batch. Normal isolated controls are necessary as well.

A second batch rescales each source by an exact power of two to bring `w` into
[1, 2). `verify.py` independently checks all 49,152 exact projected-coordinate
and depth ratios for equality using Python fractions. It also verifies a whole
central pixel strictly inside each exact rectangle and derives its downward
float32 depth independently of the native patch implementation. This finite
comparison is not a portable raster error proof or a proposal to change product
shaders.

```sh
python3 test/experiments/render/source_raster/run.py \
  --native-checkout /path/to/prepared-native-checkout \
  --compile-database /path/to/its/compile_commands.json \
  --output /tmp/outshine-source-raster-result \
  --isolate-case 10 --isolate-case 19 --isolate-case 20 \
  --isolate-case 1573 --isolate-case 1715
```

The output must be empty and under the system temporary directory. The native
archive and compilation database must belong to the same prepared checkout.
Builds and this correctness probe run at nice 10; no GPU-time or FPS claim is
made. Receipts record source/native commits, dirty status, hashes, commands and
terminal outcomes. Each source buffer is 262,144 logical bytes; depth and
readback are each 1,048,576 bytes. These are not driver allocations or peaks.
Terminal success means the audit ran and checked its inputs/outputs, not that
the proposed mathematical bound held. Inspect `observed_bound_holds` and the
separate verification report. Page suppression always remains unqualified.

`mesh_audit.py` consumes the existing #1023 patch and #1025 indexed-source D32
outputs without rerunning either device experiment. It requires matching clean,
successful input/native receipts. Fine sample centres are assigned exactly to
half-open coarse cells; every positive patch depth is compared with the actual
Lit/Flat source depth. This is a finite 17-mesh fixture check, not continuous
raster coverage, a whole-world check or permission to suppress asset pages.

```sh
python3 test/experiments/render/source_raster/mesh_audit.py \
  --patch-run /tmp/path/to/1023/final \
  --depth-run /tmp/path/to/1025/final-cost \
  --output /tmp/outshine-mesh-raster-audit.json
```

The [Vulkan floating-point requirements][fp] distinguish nonshader computations
from SPIR-V arithmetic. Shader FDiv error requirements do not automatically
bound fixed-function perspective division, clipping or depth interpolation.
The [coordinate transformation][coordinates] and [rasterization][raster]
chapters complete that boundary. Finite clip coordinates alone, a guessed
universal epsilon, or a positive sample cohort do not establish that contract.

[fp]: https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-floating-point
[coordinates]: https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html
[raster]: https://docs.vulkan.org/spec/latest/chapters/primsrast.html
