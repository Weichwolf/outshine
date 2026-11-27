# Established GPU timing and scene comparison

Primary sources checked 2026-11-19; implementation work: [#1956](https://github.com/Weichwolf/outshine/issues/1956).

| Source | Method | Interpretation boundary |
|---|---|---|
| [Vulkan specification: timestamp queries](https://docs.vulkan.org/spec/latest/chapters/queries.html#queries-timestamps) | Asynchronous GPU timestamp queries, availability and queue/device timestamp capabilities | Use supported stages/valid bits and timestamp period. API-stage intervals need not be exclusive shader execution time. |
| Apple [gpuStartTime](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpustarttime), [gpuEndTime](https://developer.apple.com/documentation/metal/mtlcommandbuffer/gpuendtime.md) | Completed command-buffer GPU timeline | Completion is required for valid values; a command-buffer interval is not a per-pass hardware utilization sample. |
| [GPU-Driven Rendering Pipelines, 2015](../geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf) | Account for culling, compaction and submission as actual work | Lower submitted triangle count alone does not establish a lower full-frame time. |

- Report full frame cadence/latency, CPU work, explicit waits and GPU intervals separately. Subtracting a wait from CPU work cannot make a missed frame deadline pass.
- Use the same declared scene, motion, cache state, resolution and sampling scope before/after; include startup and incomplete frames explicitly.
- Pinned input replays and paired comparisons help assess algorithm cost and scope across prototypes and application paths.
- GPU algorithms may live in shared GLSL kernels dispatched by Python through a GPU runner. Measure dispatch/work, readback and resource use separately; CPU NumPy timing is not GPU timing.
- Keep shaders once: the checked kernel is also the later runtime kernel where practical. Backend command encoding/lifetime is engineering and needs native verification.
- No GPU-timing or benchmark algorithm requires new research. Missing device evidence is missing evidence, not a new performance threshold.
