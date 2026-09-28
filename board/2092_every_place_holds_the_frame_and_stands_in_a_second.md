Type: bug
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: engine, client, render
Tags: performance, measured, budget

# Every place meets the frame budget while the world streams

## Problem and evidence

Still frames do not prove moving streaming or target-device cost. Local Hockenheim
warm/offline traces have p99 near 13 ms but sporadic long render/fence waits. Batching
reduced upload attempts without resolving the stall: historical p99 12.702 -> 12.738 ms,
15 -> 16 late frames, maximum render 24.889 -> 74.051 ms. PNG matched exactly.
Do not optimize another submit counter without identifying actual GPU/fence work.
FrameMeasurements exposes HOST phases; per-pass GPU execution is not yet measured.

## Executable next slice and owners

- client/ScenarioCapture owns trace/capture orchestration; engine/FrameMeasurements
  publishes bounded owner data; render/SceneRenderer owns submission/fence accounting.
  Reuse current route traces and successful submitted-camera serials, not a second renderer.
- Reproduce the Hockenheim fence outlier on identical warm inputs. Correlate same-frame
  command submission, upload bytes, passes and queue history with a native GPU capture.
  Record profiler/toolchain/source provenance. Local SDL_gpu.h currently exposes fence
  polling but no timestamp-query API; choose a measured backend/profiler path without
  pretending CPU encode/fence time is GPU execution. No library-owned platform globals.
- Then add matched city, isolated forest, sky-dominant and mixed traces using the public
  client. Share build/resolution/time/weather and pacing; declare missing native foliage
  or clouds rather than interpreting absent content as performance headroom.
- Report complete frame p50/p95/p99, max and over-budget count; CPU simulation/preparation,
  GPU passes, upload/IO, queues and CPU/GPU bytes separately. Do not sum asynchronous p99.
  Resolve per-pass/cross-family calibration into 2314 before implementing a budget planner.

## Binding budget and acceptance

- 60 Hz gives 1000/60 = 16.6667 ms/frame. City and forest receive the same total render
  envelope; sky/clouds/atmosphere use it too. No class-specific extra time. Sparse scenes
  need not saturate the envelope. Quality, memory and streaming are independent gates.
- Still/jump/drive/flight/backtrack, cold/warm cache and deterministic replay are separate.
  Manifest records source/build/settings; a scene with fewer drawn products cannot win
  by deleting quality. At least 120-frame transition windows plus complete route traces.
- A 120-frame window has zero frames beyond its declared target; full traces report all
  hitches. Local measurements do not close A18 Pro/8 GB/720p60 acceptance. 2143 owns soak.
- First Playable and target-quality readiness are measured separately. Existing <1 s
  initial-world target remains unproved; name provider/network assumptions explicitly.
- Inject synchronous rebuild or excessive upload work: timing oracle must FAIL. Disable
  required drawing: completeness/quality oracle must FAIL even if frame times improve.
- make format; focused client timing/trace, render submission and streaming suites;
  full lint with clang-tidy. Render via outshine-client, open affected PNGs.

Other streaming/LOD WIs are consumers and repair owners, not prerequisites for measuring
their current defects. 2314 owns the common policy; 2260 the complete camera-lap proof.
