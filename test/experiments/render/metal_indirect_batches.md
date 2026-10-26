# Metal indirect batches: measure before changing the command model

Owner #16; native integration #531. The current Metal backend emits one indirect
API call per slot, even when GPU visibility sets instanceCount to zero. Vulkan
can submit compatible slots in one multi-draw call. A range count, retained batch
count and number of nonempty GPU draws are different quantities.

This standalone native probe compares command submission, not world rendering.
It uses 3,305 slots, one indexed triangle per active slot, disjoint 8-pixel right
triangles in a 1280×720 RGBA8 target. The raster convention covers 8×7/2 = 28 pixels
per triangle. Nominal 0/1/10/100% selects exactly 0/34/335/3,305 active slots.
No scene, shader complexity, lighting, depth, MSAA or world-selection cost is modelled.

| Mode | Submitted API calls | Executed range | Pipeline permits ICB |
|---|---:|---:|---|
| regular_loop |3,305|3,305 indirect arguments|no|
| loop |3,305|3,305 indirect arguments|yes|
| icb |1|3,305 command slots|yes|
| compact_icb |0 or1|only active commands|yes|

The two ICBs are built on the CPU before measurement and remain alive through all
GPU completions. Inactive full-ICB slots are explicitly reset. A zero-instance ICB
draw is invalid; the GPU validation layer caught that in the initial prototype.
Compact commands preserve the original baseInstance and primitive order. Both
loops use the same arguments. The regular pipeline is a separate control for the
ICB-capable pipeline flag; other pipeline/shader settings are identical.

## Reproduction and verification

Build the normal native compilation database, then run with Python ≥3.10 and Pillow:

```
TMPDIR=/tmp python3 test/experiments/render/metal_indirect_batches.py \
  --output /tmp/outshine-metal-indirect-batches
```

The probe inherits all current Metal compilation-database compiler flags and
links only Foundation/Metal. The driver records source/shader/binary hashes,
actual commands, successful terminal statuses and an analysis database. Output
must be an explicit system-temp directory outside the checkout.

Each validation mode performs 24 rounds in alternating forward/reverse mode
order: 4 occupancies ×4 modes ×24 =384 complete renders. No first round is removed.
Every render must match its occupancy reference byte for byte and contain exactly
28×active coloured pixels. This catches collapsed baseInstance positions even
when all modes make the same mistake. A private mutation collapsing every cell
actually exits4 with incorrect coverage; the unchanged shader succeeds. The last
actual render is saved as a PNG and was opened. Raw readback files are removed.

GPU times come from the completed command buffer; missing/nonfinite timestamps
fail the probe. CPU encoding includes command-buffer/pass creation, state, draw
calls and endEncoding. Commit is separate. Host duration ends after GPU completion.
Readback and image verification use another completed submission outside these
timings. They are diagnostic costs, not part of a claimed game-frame budget.

## A18 Pro result and decision

Measured with validation disabled; the separate GPU-validated run also passes.
Median ranges across repeated complete runs, in milliseconds; every round is retained:

| Active slots | Regular loop CPU encode | ICB CPU encode | Regular loop GPU | Full ICB GPU | Compact ICB GPU |
|---:|---:|---:|---:|---:|---:|
|0|0.131|0.004–0.005|0.25–0.27|0.259–0.260|0.014|
|34|0.131|0.005|0.21–0.27|0.260–0.261|0.088–0.099|
|335|0.131|0.005|0.271|0.27–0.28|0.137–0.138|
|3,305|0.131|0.005|0.239–0.289|0.417–0.531|0.415–0.532|

Complete raw samples retain maxima and first samples; medians are not p99 admission.
The normal and ICB-capable loop controls give similar encoding times. These
microcosts do not explain the multi-millisecond world-pass/first-frame spikes.
At full occupancy ICB execution is slower despite less CPU encoding. Sparse
compaction helps, but this probe excludes dynamic GPU selection, ICB generation,
compaction and its barriers. CPU prebuilding is not a dynamic-world solution.

With this descriptor/capacity, each ICB reports 2,293,760 allocated bytes versus
81,920 allocated bytes for the 66,100-byte argument buffer: 2,293,760/81,920 =28×.
The probe holds both alternatives for comparison; an implementation need not.
Building both ICBs plus filling arguments costs about 0.62–0.72 ms outside the
measured window, not a per-ICB or per-frame cost claim.

Decision: retain the production loop for now. An ICB integration needs measured
whole-frame benefit with actual shaders, GPU-generated visibility, preserved
primitive order and explicit resource ownership. No shared API, product pipeline,
shader binding, Place, budget or test threshold changes in this experiment.

Raw runs, negative control and exact receipts:
/tmp/outshine-metal-indirect-cost-6df1fd602/. Full compilation flags pass focused
clang-tidy; source formatting, Ruff and documentation checks are separate.

The existing [GPU-Driven Rendering Pipelines, SIGGRAPH 2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
slide 12, motivates multi-draw/cluster batching and warns about primitive order and
index-storage tradeoffs. Its costs are not Metal measurements. The installed
Metal SDK's MTLIndirectCommandBuffer/MTLIndirectCommandEncoder declarations define
inheritance and draw contracts; real device validation checks their use here.
