# Metal command preparation

Owner: [#894](https://github.com/Weichwolf/outshine/issues/894).
The frontend-frustum candidate is [#901](https://github.com/Weichwolf/outshine/pull/901).
Movement-stable world representation remains [#844](https://github.com/Weichwolf/outshine/issues/844).

## Decision

GPU-prepared indirect command buffers reduce CPU submission work in this probe,
but add GPU time and storage. Do not integrate a new prepared-command API as a
solution to the current GPU budget from this result. Keep common multi-draw
boundaries intact; Vulkan already executes a range with native multi-draw when
supported. Metal currently loops over its records. Smaller record counts alone
do not prove a faster complete renderer.

The first A18 Pro input is the final Tokyo pose from native `2dfdaba32`:
6,134 argument records, 1,577 nonempty, 1,428,066 triangles. Original positions,
index order, signed base vertices and current model matrices remain unchanged.
The driver reconstructs the production compacted index ranges from actual
jobs/kept/arguments. It validates counts, spans and effective vertex/model bounds.

Three alternating-order runs with 32 samples/path gave these median ranges.
First samples remain included; setup and readbacks are outside the measurements.
CPU encoding and GPU submission intervals are separate, not additive.

| Path | CPU encode ms | GPU ms |
|---|---:|---:|
| Original indirect loop | 0.309–0.313 | 2.340–2.347 |
| Same loop, ICB-capable pipeline | 0.315–0.320 | 2.339–2.346 |
| Rebuild sparse ICB, then raster | 0.037–0.040 | 2.413–2.426 |
| Reuse sparse ICB, then raster | 0.030–0.032 | 2.401–2.406 |
| Stable prefix, rebuild dense ICB, raster | 0.043–0.045 | 2.414–2.424 |
| Reuse dense ICB, then raster | 0.029–0.033 | 2.385–2.390 |

Sparse preparation saves approximately 0.310−0.040 = 0.270 ms of CPU encoding,
while charging about (2.416−2.344)/2.344 = 3.1% more GPU time. An ICB allocates
4,259,840 bytes for this capacity. The original argument payload is 122,680 bytes;
its allocation can have different alignment. ICB storage is additional and does
not replace the original argument buffer.
The dense variant also allocates 32,768 bytes of slots and an 8-byte execution
range. Both ICB variants coexist in the diagnostic; none is a product memory claim.

## Scope and controls

This is a 1280×720 D32F position-only replay with completed submissions, not PBR,
culling, a full frame, p99 admission or moving-world coverage. The input contains
UInt32 indices, zero base vertices and single-instance records; synthetic general
API coverage and complete material/pipeline/state/lifetime integration remain open.
The 578,295 nonzero depth pixels match bit-for-bit across all six paths, including
the pipeline-capability control. GPU zero-count/reset/restore and partial-count
updates match independent direct-indirect rendering. Partial updates change
62,347 pixels. Read-back dense prefixes and execution ranges preserve source order.
Keeping old commands without preparing again is an explicit negative control.

Preparation must precede the render pass. Both full rebuilding and unchanged-input
reuse are measured; the latter is not presented as a moving-world cost. Metal API
validation passes separately. The runner bounds each offline process to 180 s;
Metal completion uses its synchronous wait API, outside the product frame loop.

## Reproduce

Use macOS Command Line Tools, NumPy and a current native compilation database.
The explicit input directory has the same raw layout as
[`native_visibility_cost.md`](native_visibility_cost.md). Output must be empty
and under system temp. GPU readbacks and generated diagnostics stay there.

```sh
python3 -B -m unittest discover -s test/experiments/render -p test_metal_indirect_commands.py
python3 -B test/experiments/render/metal_indirect_commands.py \
  --input "$INPUT" --output "$OUTPUT" --compile-db "$NATIVE_COMPILE_DB" --rounds 32
```

The receipt retains full original compile flags, commands, source/database/input
hashes and terminal subprocess status. `validation.json` and `cost.json` contain
raw samples and controls; `baseline-depth.bin` and `partial-depth.bin` are Float32.

Method context: [SIGGRAPH 2015 GPU-driven rendering](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
The local SDK declarations in `Metal.framework/Headers/MTLIndirectCommandBuffer.h`,
`MTLArgumentEncoder.h`, `MTLRenderPipeline.h` and `MTLRenderCommandEncoder.h`
define inherited state, GPU encoding and indirect execution ranges. No new shared
engine API, bindings, shaders, assets or default rendering policy follow from this probe.
