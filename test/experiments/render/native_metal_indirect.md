# Native Metal indirect submission

Decision and device evidence: [#1827](https://github.com/Weichwolf/outshine/issues/1827).
This experiment measures command encoding and complete GPU submissions. It does
not replay World geometry, PBR, frame p99, asynchronous frame reuse or asset residency.

```sh
python3.12 -B test/experiments/render/native_metal_indirect.py \
  --checkout /path/to/built/outshine \
  --output "${TMPDIR:-/tmp}/outshine-metal-indirect-run"
```

Use a fresh system-temp output. The launcher preserves the complete current
`MetalIndexedDraw.mm` compilation flags, emits a probe CDB and links only the
standard Metal/Foundation frameworks. No engine rebuild or product change.

| Input | Values |
|---|---|
| Output | 1280×720 RGBA8, transparent clear |
| GPU-produced draw records | 1, 1024, 8192; native 20-byte indexed argument ABI |
| Requested sections | 1, 145; contiguous ranges, alternating vertex/material bindings |
| Visibility phases | All, quarter, empty, half; seven rotating-order rounds |
| Reference | Individual native indirect draws, ordinary pipeline |
| Pipeline control | Same draws with ICB-compatible pipeline |
| ICB | One GPU preparation before render, inherited bindings per section |
| Optimized ICB | Same preparation plus native blit optimization |

Both references and ICBs consume the same private GPU argument buffer. Nonzero
FirstIndex, negative BaseVertex and FirstInstance are exercised. Every active
phase must cover pixels; empty phases must clear everything, including after full
phases. All four modes must produce identical RGBA bytes for each control.
The deliberate `--omit-reset` native control must fail with stale-command pixels.
It changes only the experimental shader macro; it never changes a product shader.

The launcher runs API validation on and off in separate processes. Raw reports
include the actual device class, resource allocations, setup time and every sample;
logs retain Metal's validation message. CPU encoding excludes submit/fence wait and
readback. GPU timestamps cover argument generation, ICB preparation/optimization and
rendering together. ICB allocation is reported separately and excluded from per-sample
times. Medians include first samples and all four visibility phases; raw per-phase
results remain available. These distributions are not complete game frames.

`receipt.json` binds source/launcher/binary hashes and terminal commands, including
the expected negative exit. `summary.json` groups costs without summing CPU/GPU or
overlapping counter stages. For source checks:

```sh
clang-tidy -p "$OUTPUT" test/experiments/render/native_metal_indirect.mm
```

The method addresses native submission from the
[GPU-driven rendering design](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
It does not establish the selection, occlusion, PBR or HLOD parts of that design.
The experiment supports UInt32 indices and fixed argument stride; other index types,
general layouts, command/resource lifetime and bounded runtime caching need separate
qualification before any engine integration.
