Type: feature
State: open
Area: scenario, test
Tags: perf, instrument
Supersedes: 1578, 1593

# The scenario suite reports a frame-time distribution over a moving camera

*720p60 on this device* stops being a sentence this repository quotes and starts being a
distribution it publishes: p50, p95 and p99 of frame time over a moving camera, whether two runs
of one declaration produced the same pictures, and what residency and memory did across a long
one. The suite exists and its verdict is a distribution with its population, its domain and the
device named; determinism is two runs compared picture by picture; memory is read across 600
frames against a declared ceiling; no sanitiser is in the path.

**A blocker landed this hour and it is one line.** `Renderer::DrawsInto`
(src/render/Renderer.cpp:959-968) now claims the swapchain with

```cpp
SDL_SetGPUSwapchainParameters(Device_.Get(), presents, wanted, SDL_GPU_PRESENTMODE_VSYNC)
```

as part of fixing the sRGB transfer (board:1889, closed). VSYNC is not a property of a colour
space and it is not the plan's to choose: a windowed run clamped to the display's refresh cannot
report a p50 below the refresh interval, and the p95/p99 it does report measure the compositor's
queue rather than the engine's frame. The present mode belongs in the render declaration beside
`fps`, defaulting to whatever the measurement needs, and `SDL_GPU_PRESENTMODE_IMMEDIATE` is what
a distribution is taken under.

## What will be true

- [ ] The present mode is DECLARED. A frame-time distribution is taken with the swapchain in
      immediate mode, and the declaration says which mode it was taken under.

- [ ] **A run is DECLARED, not written in C++** — a camera path, a frame count, a rate and which
      scenario stands under it.
- [ ] **Residency is measured.** The renderer publishes draw and batch counts and no byte
      accounting at all, so what the DEVICE holds is unanswerable here — named rather than
      approximated, because a figure taken from process memory and called device residency is
      the exact defect a domain paragraph exists to prevent.
- [ ] **Every GPU pass publishes its span to a READER** — a per-pass duration measured on the
      device AND a consumer (a telemetry row, a frame-attribution line), so a cost that moves
      between passes is attributable without a profiler. The reader is built first and the probe
      second; an instrument nothing reads is deleted on sight.
- [ ] **A run that misses the floor is RED and says by how much**, because a frame budget nobody
      can fail is a quotation.
- [ ] The store-op derivation (`Stored(resource)`, landed) is MEASURED: the frame-time delta over
      a full declared drive against the standing reference population.
