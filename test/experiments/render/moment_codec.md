# Roughness-moment precision alternatives (#824)

This follows [the first production-GGX counterprobe](material_filter.md).
It compares UNORM16 in existing R/G channels with FP16 numerically. It changes
no product shader, image format, binding, cache or geometry. Hardware texture
sampling and complete atlas/ThinSheet/normal/raster behavior remain unproved.

## Contract and reference

Store premultiplied moment `M=A*r^4`, opaque coverage A and premultiplied
metalness B. Existing images keep B/A and normal variance at their current
owners. For UNORM16, let `q=round(65535*M)`, R=lowbyte/255, G=highbyte/255.
Then `(256*G+R)/257=q/65535`. This **linear** reconstruction commutes with common
bilinear/trilinear weights in real arithmetic, including byte-carry boundaries.
It adds no texel bytes. Finite arithmetic and actual sampler precision still
need independent checks; this argument does not create a texture-format proof.

The Python reference produces a 4×4 Float32 field and its 2×2 box mip, clamps
eight samples to edges and uses common bilinear/trilinear weights. There are
960 source cases: ten r values 0…1 (including 0.01/0.02/0.04/0.06/0.12/0.2),
eight coverages 0…1, constant/hole/mixed-roughness fields and four UV/LOD states.
These are controlled fields, not geographically independent world samples.

Five modes separate unencoded Float32, existing UNORM8, UNORM16 R/G,
FP16-moment/A8 control, and RGBA16F. The FP16/A8 mode isolates coverage error;
it is not a proposed native image format. Actual B/A statistics are checked
separately; the subsequent BRDF stays dielectric with fixed F0=0.04 and fixed
normal to isolate roughness. Opaque contribution is `A*RGB`, not a complete
ThinSheet, alpha-mask or background compositor.

## Native observations

The unchanged cea66e2a9 NativeGroundLitterProbe on **Intel Iris Xe (TGL GT2)**
executes 4,800 channel queries then 100,800 unchanged product GGX/energy queries
(shader source 0eb2e739d). No native consumer is built by this experiment.
The private IO shader uses the existing four buffers and 32-byte records.

Channel checks pass against independent Float64 reconstruction; maximum absolute
difference is 1.1642e-7. The two R/G reconstruction/filter orders differ by at
most 5.9605e-8. B and A, including normalized B/A, are **bit-identical** between
UNORM8 and UNORM16 on all 960 cases. Half subnormal values survive this numeric
unpack/Float32 arithmetic control. None of this exercises FP16 texture sampling.

For constant fields, decoded perceptual roughness is:

| r / opaque A | UNORM8 | UNORM16 R/G | FP16 M + A8 | RGBA16F |
| --- | ---: | ---: | ---: | ---: |
| 0.12 / 1 | 0 | 0.1208965 | 0.1199920 | 0.1199920 |
| 0.12 / 0.01 | 0 | 0 | 0.1153963 | 0.1201749 |
| 0.2 / 1 | 0 | 0.2000686 | 0.2000083 | 0.2000083 |
| 0.2 / 0.01 | 0 | 0.1897739 | 0.1919590 | 0.1999082 |

UNORM16's zero interval at full coverage reaches approximately r=0.05256;
binary16 reaches r=0.01314 when subnormals are retained. Premultiplication by
small A expands both intervals: the no-zero condition involves **A*r^4**, not
r alone. A8 independently loses opaque coverage below roughly 1/510. In this
matrix 120 cases lose positive A under A8, versus zero with half coverage.
At r=0.12/A=0.01, maximum sampled opaque-contribution RGB error is 0.9221
for UNORM16, 0.3467 for FP16 M/A8 and 0.00499 for RGBA16F. These are native
linear unit-irradiance responses, not displayed pixel errors or a new tolerance.

**112 unencoded small-r BRDF responses are nonfinite.** They remain recorded,
with finite-comparable counts per case, and are excluded only from explicitly
finite-only numerical error statistics. Independent literal r=0.01 inputs with
N=L=V show the same +Inf without any codec; [#829](https://github.com/Weichwolf/outshine/issues/829)
owns this separate cancellation defect. The quantized modes erase these small
moments and report finite zero lobes; that does not preserve the material.

## Decision and costs

UNORM16 R/G is a useful byte-neutral candidate for a bounded domain. It does not
cover every valid small-r/coverage case. RGBA16F improves the sampled low-coverage
cases, while retaining a lower underflow boundary and requiring a native image
path. No global winner, format change or lower-resolution material path is
admitted. Native texture precision/mip interpolation and #829 remain explicit.

Logical MR texel bytes are 4 for either UNORM8 or UNORM16 R/G, and 8 for
RGBA16F. Existing five RGBA8 optical images cost 20 bytes per mip texel; expanding
only MR gives 24 (+20%), before padding, uploads or driver reserve. Multiply by
the actual mip/view/atlas counts; this is not an observed residency budget.
R/G adds a constant linear reconstruction after the same fetch; B/A, coverage,
ThinSheet and normal consumers must migrate together under the owner contract.

The numeric decode stage requests 2,458,112 buffer bytes and copies 1,229,056;
the BRDF stage requests 29,030,912 and copies 14,515,456. Each stage has five
buffers; the processes run sequentially, so their peaks are not added. Driver/
OS/native-child peaks are incomplete; reported Python RSS is separate. GPU times
are unmeasured, and these verification buffers are not a runtime-cache design.

## Reproduction

```sh
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_moment_codec.py
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/render/probe_moment_codec.py \
  --binary "$NATIVE_CONSUMER" --tables "$TABLES" --output "$FRESH_SYSTEM_TEMP"
.venv/bin/python test/experiments/render/plot_moment_codec.py \
  --input "$FRESH_SYSTEM_TEMP/report.json" --output "$SYSTEM_TEMP/precision.png"
```

NumPy, Matplotlib and glslangValidator are required. Reports preserve all source
cases, encoded input words, native outputs, commands/hashes and nonfinite counts.
The angular/coverage chart was opened locally; no README/doc image is published.
SIGGRAPH-first PBR background remains [Karis 2013](../../../doc/references/materials/siggraph/2013-unreal-engine-4-shading.pdf),
pages 2–3 visually checked in the preceding comparison. Half/UNORM bit conversions
follow the [Khronos GLSL built-ins specification](https://github.khronos.org/Vulkan-Site/glsl/latest/chapters/builtinfunctions.html#floating-point-pack-and-unpack-functions);
the actual unpack arithmetic above is measured, not inferred from texture support.
