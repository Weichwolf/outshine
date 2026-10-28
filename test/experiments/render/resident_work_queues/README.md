# Resident work queues

Native GPU experiment for #1523, before the shared renderer/backend contract changes.
It expands explicit root selection into bounded `(root, local cluster first)` chunk
references. Selection is supplied by the test; it does not prove frustum, LOD,
deformation or coverage selection. Sizes include the 19 boundary/observed batch
lengths from `test/harness/render/NativeComputeProbe.cpp`, including 1,939 clusters,
with eight instance copies. The independent CPU enumeration checks every selected
reference, all-visible/sparse/empty/unknown cases, reset, guard words and overflow.
An omitted-reset control must fail. Overflow remains invalid work, not a successful
shortened list; this experiment does not provide the renderer fallback.

```sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json python3 -B \
  test/experiments/render/resident_work_queues/run.py \
  --build build --output /tmp/outshine-engine-resident-work-<commit>
```

The driver uses the full current native device test compile/link commands. Build
and shader work runs with nice 10; native checks run at normal priority. Outputs
must be in the system temp directory. It records the GPU name, source hashes,
commands and terminal results. Product shaders, bindings and dispatch API remain
unchanged. This is a queue correctness/cost-input experiment, not a frame or device
budget acceptance: source data, templates, final selected lists, logical prefix
storage, index copying, encoding and synchronization must all be paid by the
complete renderer. Atomic queue order does not itself prove final draw order.

The motivating instance→chunk→cluster pipeline is shown on PDF pages 16–17 of
`doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf`.
