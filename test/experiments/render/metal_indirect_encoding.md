# Metal indexed indirect encoding

Issue: #1079. This experiment measures backend submission, not world rendering.
The product Metal/Vulkan API and GLSL are unchanged.

```sh
python3 test/experiments/render/metal_indirect_encoding.py --output /tmp/outshine-icb-normal
python3 test/experiments/render/metal_indirect_encoding.py --validation --output /tmp/outshine-icb-validation
```

The output directory must be new and inside system temp. It contains the compiler
command, source/binary hashes, logs, structured report and a rendered PNG witness.
Requires macOS, Command Line Tools and the Metal/Foundation/ImageIO frameworks.
No Xcode application, network request or engine build is needed. Standard, warning
and optimization flags come from the shared declarations in `test/run.sh`.

## Compared work

A native loop submits one indexed-indirect Metal call per record. The other path
encodes a Metal ICB on the GPU and executes its range once. Both consume the same
private GPU-generated argument buffer. An independent CPU argument producer supplies
a third rendered image. Every image must agree exactly; the largest case must
also contain nonempty coloured coverage.

Cases: 0/1/7/128/1024/8192 records, UInt16/UInt32 indices, 64-byte argument offset,
32-byte argument stride, 32-byte index offset, zero counts, zero/one/two instances,
changing records, positive/negative base vertex and nonzero base instance.
Twenty pairs per case alternate loop/ICB order. The first four warm up; medians
use the remaining sixteen samples. GPU time includes argument generation and
readback for both paths, plus ICB generation for the candidate.

The CPU/GPU indexed-argument layout is statically checked against the Metal SDK.
Sources of the API contract: the installed SDK's MTLIndirectCommandBuffer.h,
MTLRenderCommandEncoder.h and MTLArgumentEncoder.h. The Metal compiler checks the
MSL command_buffer/render_command operations directly.

## A18 Pro result and decision

Original `f81132553` run from 2026-10-22, 8192 records; its reports record the
original compiler flags. Times are milliseconds.
These are whole command-encoding CPU medians and command-buffer GPU medians.

| Indices | API validation | CPU loop | CPU ICB | GPU loop | GPU ICB |
|---|---|---:|---:|---:|---:|
| UInt32 | off | 0.583271 | 0.021292 | 0.621104 | 0.930125 |
| UInt16 | off | 0.368375 | 0.013021 | 0.284729 | 0.430438 |
| UInt32 | on | 7.650313 | 0.043646 | 0.519083 | 0.737667 |
| UInt16 | on | 7.419562 | 0.043771 | 0.536563 | 0.875771 |

Independent earlier processes varied: ordinary unvalidated CPU encoding was
approximately 0.35–0.58 ms at this count. The large validated CPU gain must not be
reported as normal application acceleration. In the UInt16 row above the paired
CPU difference is 0.368375−0.013021=0.355354 ms; the additional GPU work is
0.430438−0.284729=0.145709 ms. These asynchronous times are not added into an
invented frame result. One 8192-command ICB allocated 5,685,248 bytes on this device.
The reference test also owns an ICB; that duplicate is test scaffolding.

All twelve cases passed 240 exact image pairs and the CPU argument controls in
both modes. Omitting the ICB base-vertex adjustment makes the comparison fail;
the altered control stays private. The PNG witness was opened.

No product rewrite follows from this result. The existing interface requires a
render pass when drawing, while GPU ICB construction needs an earlier compute
phase. A new preparation contract has a real maintenance/memory/GPU cost. Revisit
for a demonstrated CPU bottleneck with matching arguments and larger workloads;
retain the ordinary path as the current implementation, not a new unused ICB path.
There is no p99/world/device-budget admission from this synthetic experiment.
