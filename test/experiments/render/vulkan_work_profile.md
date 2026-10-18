# Native Vulkan work probe

Issue #756. Explicit private instrumentation of the existing native client;
production sources, public API, pass ordering, bindings and shaders stay unchanged.
Prepare an up-to-date native build and its full compile database first. Supply the
actual client link argv as a JSON array, as for `scene_storage_probe.py`.

```sh
python3 test/experiments/render/vulkan_work_profile.py \
  --runtime-root /path/to/native-checkout \
  --link-command /tmp/outshine-client-link.json \
  --output /tmp/outshine-vulkan-work-tokyo --place Tokyo
python3 test/experiments/render/vulkan_work_profile_checks.py
```

The output directory must be new and below `/tmp`. Compilation preserves all
actual per-unit flags and substitutes six private objects before the real archive.
Exact source anchors fail closed. The receipt binds source/header/archive/client
hashes and complete compile/link commands. `--validation` enables the existing
Khronos layer and debug messenger in this private build. Without `--place`, only
build the probe. With a Place, capture offline in a fresh process, with exactly the
declared frames and a full turn. `--overrides` requires explicit `render.fps` and
otherwise accepts the client's scenario override JSON. The default is
1920×1080/60. The 120s diagnostic preload limit permits complete images; it never
turns an actual load above ten seconds into a passing load measurement.

Raster passes carry timestamp intervals and actual input-assembly, vertex and
fragment invocation counters. Terrain containing `groundPatchLattice` instead
measures its individual draw packets: no nested statistics query or split render
pass. Compute dispatches carry actual compute invocation counts, pipeline labels,
workgroups and the existing native timing record. Workgroups are declared groups,
not a replacement for actual invocation counters. Raster `calls` counts recorded
DrawPackets; one indirect packet may expand into many draws.

Counters for absent stages, empty draws and terrain parent statistics are `null`,
not measured zeros. Hardware timestamps retain valid bits, period and availability;
interval validation uses the existing native wrap/host-bound contract. The analyzer
checks child containment, complete compute attribution and exact submission-total
agreement with GPUROW. A sky pass identifies the measurement window; preparation
stays in raw records. Custom plans without that pass are outside this probe's scope.

One client render thread is supported. Limits: 16 live command traces, 256 query
slots per trace, 256 retained completed traces, 4096 pipeline registrations,
64 distinct pipelines per raster pass, 1024 bytes per registered label and 4096
bytes per pass label. Query/capture overflow stays explicit and fails analysis.
Unsubmitted commands cancel their trace; completed query pools are released after
the existing recording lifetime says they are unused. No added GPU wait or
blocking readback. Fixed-size trace storage and retained strings add process memory.

Formatting and output happen at context destruction, after capture. `VK_WORK_COST`
separates timed CPU hook scopes from cold output; timestamp measurement overhead
itself is not calibrated. GPU queries also add work. Compare matched
uninstrumented runs separately; diagnostic timings do not prove the frame budget.
Pass/dispatch intervals may overlap and must not be added into a frame breakdown.

Primary contract: Vulkan-Docs `chapters/queries.adoc` (statistics, query scope,
availability, timestamps) and `chapters/features.adoc` (`pipelineStatisticsQuery`).
On the measured RTX3060 the feature is supported, the queue has 64 valid timestamp
bits, and timestamp period is 1ns. Check the actual device on other machines.
