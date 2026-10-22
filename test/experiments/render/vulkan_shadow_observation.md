# Observe regional shadow work without extra GPU commands

Question: which shadow regions are redrawn during the repeated GPU spikes in the
native 1280×720/60 moving-camera benchmark (#15/#4)?

`VulkanShadowObservation.cpp` is an explicit Linux/Vulkan `LD_PRELOAD` diagnostic.
It forwards each intercepted API call once, unchanged. It records the existing
submission timestamp boundaries, viewport/scissor settings, pipeline metadata,
draw arguments and successful submissions.
It adds no Vulkan commands, GPU queries, resource allocations or readbacks.

The Vulkan backend sets one viewport immediately before each draw packet. Four
2048×2048 shadow tiles occupy the atlas at (0,0), (2048,0), (0,2048), (2048,2048).
Negative Vulkan viewport heights are normalized before assigning tile bits 0–3.
`shadow_viewports` counts these packets, including tile-clear draws; it is neither
a triangle count nor the number of draws inside an indirect multi-draw packet.
Other viewport packets remain in `viewports`.

Collection uses fixed tables: 4096 command-buffer handles, 4096 submissions,
4096 pipeline descriptions, 16384 shadow patches and four queues. On the measured
x86-64 build these tables occupy 4620368 bytes;
mutexes, counters, loader/code and process memory are separate. Hot-path collection
uses a mutex and counters, with no formatting or output. The final JSON is emitted
to stderr during library teardown. CPU interception and locking overhead remains
unisolated; this is not a zero-overhead or per-pass GPU timing instrument.

## Reproduce

Use a C++23 compiler with `<print>`, Vulkan headers and the existing native client.
All diagnostic output stays in the system temporary directory. Set `scenario` to
the scenario file being investigated; retain its original content and profile.

```sh
scratch=$(mktemp -d "${TMPDIR:-/tmp}/outshine-shadow-observation.XXXXXX")
${CXX:-c++} -std=c++23 -fno-exceptions -O2 -Wall -Wextra -Werror -fPIC -shared \
  $(pkg-config --cflags vulkan) test/experiments/render/VulkanShadowObservation.cpp \
  -ldl -pthread -o "$scratch/observer.so"
LD_PRELOAD="$scratch/observer.so" build/outshine benchmark \
  --scenario "$scenario" --frames 60 --linear-velocity 6,0,0 \
  --angular-velocity 0,-360,0 --offline --preload-seconds 60 \
  --output "$scratch/benchmark.json" --image "$scratch/final.png" \
  > "$scratch/stdout.log" 2> "$scratch/stderr.log"
```

Extract the final stderr JSON object beginning with `{"dropped"`; loader logs can
follow it. Require `dropped == 0`, one queue, a complete 60-frame benchmark and one
unique successful observed submission per GPU sample. Join the benchmark's
`gpu_times.samples[].submission` to observed `serial`; require `timed` and `ended`
for every matched record. The single-device backend increments its completion
serial once per successful one-command-buffer `vkQueueSubmit2`, starting at one.
Do not join multi-device/queue, missing, overflowed or failed traces by assumption.
Repeat with velocity `0,0,0`, then repeat both modes without `LD_PRELOAD`.

## Native counterexample

Product commit: `37db0b9e38299ba55ed73d758c2b1b4167f5368e`, Iris Xe/i5-1135G7,
Flensburg scenario, 1280×720/60, exactly 60 frames and 360° rotation, offline.
Two interleaved control/observed pairs per mode all completed successfully.
Host load varied from 7.71 to 12.28; builds/lint ran at nice 10. These are not
isolated timing or instrumentation-overhead comparisons.

Both observed runs reproduce exactly the same viewport and region-mask sequences:

| Motion | Shadow packets | Other packets | Tile updates | Frames drawing shadow tiles |
|---|---:|---:|---:|---:|
| Rotation only | 0 | 816 | 0 | 0/60 |
| 6 m/s translation plus rotation | 12119 | 816 | 36 | 27/60 |

For translation the mask histogram is `{0:33, 1:18, 3:7, 7:1, 8:1}`. At zero-based
frame 19, mask 7 draws 3376 shadow packets; total GPU time is 31.86/35.38 ms in the
two observed runs. Frame 29 draws only world-fit tile 3: 3331 shadow packets.
Frames 55/56 each draw tiles 0/1: 550 shadow packets each. These are actual work
counts correlated with full-submission GPU timestamps, not isolated shadow costs.
The existing regional cache works; translation crosses quantized tile boundaries.

All four images per motion mode are byte-identical, including control runs:
rotation SHA-256 `833e3ab3b32c41849d582a14205345910e2ea1d3689db3899b1903bc13912fc8`;
translation SHA-256 `bcb118f8656e0d221f8cd18e5e7bd45f080b9b1c5acdd5c744dc4de35d39383c`.
The reviewed source also reproduces both 60-frame sequences and image hashes with
`VK_LAYER_KHRONOS_validation` forced through the loader: zero API validation errors
or warnings. Its build and focused clang-tidy use the complete actual native
`Device.cpp` compilation database flags, with the observer source and PIC output.

Rotation is world-complete; every translating frame remains unrefined (#844).
No world-completeness, 16.67 ms, worldwide or A18 acceptance follows from this case.

Next decision: reduce work for genuinely changed tiles while preserving shadow
coverage, resolution, offscreen casters, dynamic changes and cache invalidation.
The counts do not justify shrinking regions, suppressing updates or choosing an
incremental-shadow algorithm without a conservative correctness/cost comparison.

## Submitted index work per atlas strip (#1048)

The observer also intercepts graphics-pipeline creation/binding, scissor changes,
and direct/indirect draws. `shadow_patches` groups actual submitted packets by
submission serial, physical atlas tile and scissor rectangle. Indexed work is
reported both as the sum of `indexCount` and as `indexCount * instanceCount`;
these are requested indices, not measured shader invocations or unique vertices.
Direct non-indexed work is separate. Indirect calls count records, but their index
payload remains unknown; do not treat that case as a complete index-work trace.
Unknown pipelines, exhausted tables and arithmetic overflow invalidate admission.
The helper's 13 checks cover grouping, weighting, capacities, overflow, command
reuse, accepted serials and explicit unknown indirect work.

Pipeline binding counts separate one-stream object depth from two-stream terrain
depth in the current native pipeline contract. Zero-stream tile clears remain
separate non-indexed packets. Raw field names retain the binding count rather
than assigning a material/world-class owner. Fragment-stage presence is metadata,
not a fragment invocation count. The four largest direct indexed packets in each
patch retain index range, instance/model slot and pipeline binding metadata.

Current Tokyo input: Native933e89cd0 with the #1051 upload-pressure candidate,
Intel i5-1135G7/Iris Xe, 1280×720/60, full prepared offline cache, exactly 60 frames,
360° rotation, velocity `0,0,-30`. The candidate is a source-verified overlay of the
three changed render translation units and current StructureBuildQueue onto the
fully built `493877d6e` archive, not a fresh whole933 build. Exact commands, hashes,
source/terminal receipts and original inputs remain in
`/tmp/outshine-engine-frame-upload-pressure-28bf6c5d0/` and
`/tmp/outshine-engine-motion-profile-493877d6e/`.

| Work in the measured window | Rotation only | 30 m translation |
|---|---:|---:|
| Frames drawing shadows | 0/60 | 60/60 |
| Submitted patches | 0 | 193 |
| Total instance-weighted index requests | 0 | 1791048021 |
| Median index requests/frame | 0 | 23277022.5 |
| Median updated atlas pixels/frame | 0 | 6144 |

One-stream object pipelines account for 1769282133/1791048021 index requests
(98.8%); terrain accounts for 21765888. The largest object packet in the measured
patches requests 744501 indices for one instance, model slot 2206. This identifies
object caster selection as the next measured target; it does not assign that
packet to a particular generator. The current shadow path draws full original
batch index ranges after its batch-sphere patch test, rather than selecting the
main camera's visible clusters. Offscreen caster correctness remains mandatory.

Both traces have zero dropped records and one queue. Every benchmark GPU sample
joins a unique timed, ended successful submission; packet totals match the
observed shadow viewport counts. The moving trace has no indirect or unknown
pipeline packets. Static control/observed/candidate PNGs are byte-identical
(SHA256 `b74ebc9d94b00b7d7fe3c0226d346637decc6336fcd0e1253be181eada9fd5dd`).
Moving candidate control/observed PNGs are also identical
(`57e427d31a806e0e20433fc9701b49c27941826faace2a866212115236ffa412`). Both
actual final images were inspected. The observer adds no GPU commands; its
CPU/mutex overhead remains unisolated, so these runs prove submitted work rather
than an isolated timing improvement.

All 60 moving frames remain unrefined (#1045); #1051 removes the previous fatal
upload-pressure abort but does not resolve world completeness or 60 fps. The
unobserved moving run reports CPU p99 96.619 ms and GPU p99 51.346 ms under its recorded
host load; neither is a passing budget result. These facts do not justify reduced
content, disabling shadows or another atlas cache. The next native step is finer
conservative caster selection before vertex processing for the actual strip.
