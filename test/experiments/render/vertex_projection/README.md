# Vertex projection arithmetic boundary (#990)

The coverage reference must not treat a CPU float32 matrix replay as the actual GPU
vertex result. This native diagnostic supplies a concrete counterexample before
any such replay can suppress asset IO. It changes no product shader, binding,
floating-point mode or graphics-storage synchronization rule.

## What executes

The runner extracts the position-calculation statements directly from the prepared
native checkout's `litVertex.glsl` and `flatVertex.glsl`, with source hashes and
strict extraction markers. Lit transforms the point, adds the camera shift, then
projects it. Flat adds the shift to the model translation before multiplication.
Both are algebraically equivalent, with different permitted rounding behavior.

The statements run in an actual vertex-stage diagnostic. Its result travels via a
flat varying into an RGBA32F render target and is read back without tone mapping.
The diagnostic uses three equal input points and replaces their raster position
with a fullscreen triangle after recording the projected value. This produces a
64×1 transport surface with a Metal-compatible row pitch. It uses existing RHI
render/readback operations and no graphics shader storage writes.

**This is not a capture of the unmodified world shader's output.** Uniform inputs,
subsequent uses, shader optimization context, clipping and the final raster position
are different. The probe demonstrates arithmetic differences allowed by these
expressions; it does not certify any actual world vertex, complete triangle, root
pixel, terrain 91, temporal lifetime or page decision. Input/output explicitly keep
`qualified_for_page_suppression=false`.

## Cases and controls

Fourteen synthetic inputs exercise shift cancellation and multiply-add rounding.
Another 51 samples take first/middle/last positions from the 17 complete native meshes
in `test/outshine/data/render/patch_selection/native-mesh-subset.json`, with their
recorded model, camera and shift. These are frozen ground 85 inputs, not a general
geographic corpus. The same 65 cases run in four modes:

| Mode | GPU expression | CPU reference |
|---|---|---|
|0|Extracted Lit position|Separate float32 multiply/add operations|
|1|Extracted Flat position|Separate float32 multiply/add operations|
|2|Fixed reverse-order `precise` FMA chain|Ordered `std::fma` chain|
|3|Fixed forward-order `precise` multiply/add|Separate ordered float32 operations|

Observed on Iris Xe/Vulkan: numeric case differences 46/59/0/0; native-input bit
differences 38/51/0/0. These are actual numeric differences, not merely signed zero.
Controls matching in this finite cohort establish no portable arithmetic theorem.
The shader requests no rounding/denormal execution mode and no correctly-rounded
FMA extension. Ordinary GLSL `Fma` must not be promoted to a correctly-rounded
portable contract solely because this GPU happened to match `std::fma`.

For the 48 comparable native corners per mode, an exactly checked downward-rounded
CPU reverse-Z corner bound still exceeds the diagnostic GPU ratio in 7/18/0/0 cases.
Comparison uses positive `w` and `0 <= z <= w`; inapplicable rows are null, with
separate comparable counts. Products of two binary32 values fit exactly in
binary64, so cross-products compare the corner ratios without a tolerance. This
is a single-corner arithmetic witness, not a false page-culling or world-pixel
claim. A larger empirical epsilon is not inferred from these samples.

## Consequence and remaining contract

Default CPU replay is not a GPU guarantee. Before production use, choose and prove
either a shared operation/rounding/denormal contract with actual consumers, a
conservative enclosure for allowed GPU evaluation, or a synchronized actual-GPU
vertex producer. The current backend deliberately rejects graphics storage writes
without a pass synchronization contract; this diagnostic preserves that guard.
Root/world/camera/light validity and shadow/reflection/game/change demand remain
independent requirements. Mode 3 is an arithmetic-contract candidate, not an adopted
renderer change; cost on the real vertex workload and Metal must be checked first.

The inspected Iris properties advertise float32 RTE, denormal and signed-zero/
Inf/NaN preservation, but support does not mean the probe requested those modes.
`VK_KHR_shader_fma` was absent from the inspected extension list. Current Metal
vertex compilation uses safe math; safe math still permits contraction. No backend
flags are changed here.

## Reproduction and evidence

```sh
python3 test/experiments/render/vertex_projection/run.py \
  --native-checkout /path/to/prepared/native/checkout \
  --compile-database /path/to/that/checkouts/compile_commands.json \
  --output /tmp/outshine-vertex-projection-unique-run
```

Use that build's SDK/library environment. The CPU reference needs the actual
compilation database's disabled contraction flag; source, archive, input, generated
shader, binary, commands and terminal results identify the run. Readback rows
contain original values and all four component bit patterns, including mismatches.
Missing GPU times remain marked. Runtime counters include 260 draws, 780 submitted
vertices, 45760 logical uniform bytes and 266240 readback bytes for the default run.
Those are logical submissions/transfers, not measured hardware vertex invocations.
Tiny draw timings are diagnostic costs, not a world/vertex-throughput/FPS benchmark.
Allocation counts and GPU/driver peaks are not fully instrumented.

## Primary precision references

[Vulkan expression evaluation and operation precision](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#spirvenv-evaluation-expressions)
permits rearrangement unless restricted and describes rounding/denormal modes.
[Correctly rounded FMA proposal](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_shader_fma.html)
distinguishes ordinary GLSL FMA from the stronger `OpFmaKHR` contract.
[Apple MSL specification](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf),
2026-06-04, sections 1.6.3/6.6, pages 15–16/206, describes math/contraction controls
and FMA. Pages 16/206 were also inspected visually. These sources motivate explicit
contracts; they do not prove this diagnostic identical to a production shader.
The existing SIGGRAPH 2000 conservative-occluder basis remains documented in
`../fixed_coverage/README.md`.
