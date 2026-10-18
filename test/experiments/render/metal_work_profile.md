# Native Metal Place work profile

This diagnostic instruments the native Metal backend through an injected local library.
It does not replace the renderer, alter shaders or change generated assets. Build the
native client first. Command Line Tools and an Apple GPU with stage-boundary timestamp
sampling are required; Xcode is not required.
Compiler, language, warnings and Objective-C flags come from the native Metal unit in
`compile_commands.json`; prepare it with `make db` using the repository toolchain.

```sh
python3 test/experiments/render/metal_work_profile.py \
  --output "${TMPDIR:-/tmp}/outshine-metal-work-tokyo" --place Tokyo
```

`--overrides` accepts the same complete JSON override object as the client.
`--client` selects its binary; `--runtime-root` selects the working directory that
owns its shader artifacts. This allows the shared probe to diagnose a native
integration build while leaving the main checkout's Place outputs untouched.
Use a fresh output directory per comparison. Outputs stay in the system temporary
directory: compile/client logs, injected library and a source/client-bound JSON receipt.
The client still captures exactly the declared frame count and one full turn.

The receipt separates preload submissions from measured frames by their sky pass;
the number of measured submissions must match the successful `ROW`. A custom
plan without the sky pass is outside this probe's scope. All submissions remain in
the raw records. Shader names identify passes; phases of a shared pass cannot be
attributed separately to its individual shaders.

- Renderpass attachments sample vertex/fragment stage boundaries. Resolved Apple
  timestamp deltas are nanoseconds; do not apply the CPU mach timebase to them.
  Hardware calibration on A18 Pro matched these intervals against command-buffer
  GPU elapsed time. Phases/passes can overlap; do not add their durations into a frame.
- Compute-encoder attachments sample start/end without splitting the original
  encoder. Its pipeline set can contain several dependent kernels: that interval
  cannot be attributed individually to them. Dispatch groups and declared threads
  are recorded separately; padded/early-returning threads are not actual hardware
  invocations. Overflow or unavailable counts stay incomplete, not zero work.
- Direct draw counts include instancing. Indirect argument buffers are copied after
  the recorded draws and decoded after completion. Indirect arguments must remain
  unchanged between their draw and this final copy, as in the current native Place path.
  These are submitted triangle references, including degenerate/clipped triangles,
  not actual vertex-shader invocations, visible triangles or resident geometry.
- Sampling is bounded to 64 combined encoders, 65,536 indirect calls and eight readback buffers
  up to 1 MiB per command. Missing samples/indirect arguments remain explicit. The
  supplied native client records triangle-list draws on one render thread.
- Counter allocation, argument copies, Objective-C hooks and completion logging add
  work. Compare an uninstrumented run separately; these timings do not prove a frame
  budget. Neither this profiler nor its files belong in the production hot path.

Primary API reference: the installed Metal SDK headers `MTLCounters.h`,
`MTLRenderPass.h`, `MTLRenderCommandEncoder.h`. Cost decisions belong to #15/#728;
native pipeline integration belongs to #328. No change to the shared backend API.

`compute_encoders` and raw `METAL_COMPUTE`/`METAL_COMPUTE_WORK` records join the same
measured submission set as raster passes. Peak-frame profiles include both kinds;
missing timestamp values are `null`. Preparation remains in raw data and is excluded
from frame aggregates. `metal_work_profile_checks.py` covers mixed overlapping
intervals, preparation, invalid/missing data and duplicate/inconsistent records.

On the existing A18-Pro Körbersee control,1280×720/30,30frames/360°, the first
extended capture keeps PNG076343f4 and matches all command durations to GPUROW.
Mean encoder intervals: subject cull/scan/compact0.470ms; terrain depth-pyramid/
cull/scan/compact0.362ms; later depth pyramid0.592ms. Each has30 valid measured
samples and no unavailable dispatch counts. They are not an additive breakdown.
The much larger terrain raster interval remains the primary observed cost;
this evidence does not justify rewriting the already small compute selection.
