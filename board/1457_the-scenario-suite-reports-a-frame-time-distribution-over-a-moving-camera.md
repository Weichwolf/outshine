Type: feature
State: open
Area: scenario, test
Tags: perf, instrument
Supersedes: 1578, 1593

# The scenario suite reports a frame-time distribution over a moving camera

**Benchmark** — Unreal: `stat unit` and the automation suite report frame time distributions, not means. RAGE: telemetry per frame. **Both agree** — a mean hides the frame that missed, and p50/p95/p99 over a moving camera is what a budget is judged on.

*720p60 on this device* stops being a sentence this repository quotes and starts being a
distribution it publishes: p50, p95 and p99 of frame time over a moving camera, whether two runs
of one declaration produced the same pictures, and what residency and memory did across a long
one. The suite exists and its verdict is a distribution with its population, its domain and the
device named; determinism is two runs compared picture by picture; memory is read across 600
frames against a declared ceiling; no sanitiser is in the path.

**The blocker this item carried is gone, and it left an instrument nobody reads.** At
35829990 `Renderer::DrawsInto` takes the first present mode the device offers that does not
queue -- MAILBOX, then IMMEDIATE, then VSYNC -- and refuses by name if it takes none
(src/render/SceneRenderer.cpp:968-985). `[[nodiscard]] bool Queued() const` (src/render/SceneRenderer.h:81)
says which it got.

Two things are wrong with it and both are this item's:

- **No caller.** `grep -rn 'Queued()' src include apps test` finds the definition and nothing
  else. A distribution that does not print the mode it was taken under is the state this item
  was opened to end.
- **It answers for a swapchain that does not exist.** `SDL_GPUPresentMode Presenting_ =
  SDL_GPU_PRESENTMODE_VSYNC;` (SceneRenderer.h:235) is the default, and the offscreen path never
  assigns it, so an offscreen renderer -- every headless run in the tree -- reports `Queued() ==
  true`. A number that answers where it was never measured is worse than no number.

## What will be true

- [ ] The present mode is DECLARED beside `fps` rather than chosen by a preference list in the
      renderer, and the distribution names the mode it was taken under. `Queued()` has a reader
      or it is deleted, and it refuses to answer where nothing presents.

- [ ] **A run is DECLARED, not written in C++** — a camera path, a frame count, a rate and which
      scenario stands under it. `apps/bench --scene NAME` and `--all` name Khronos's own six from
      the corpus this tree already fetches, which is the first half: WHICH scenario is a switch
      rather than a recompile. The camera path and the rate are still C++.
- [ ] **Residency is measured.** The renderer publishes draw and batch counts and no byte
      accounting at all, so what the DEVICE holds is unanswerable here — named rather than
      approximated, because a figure taken from process memory and called device residency is
      the exact defect a domain paragraph exists to prevent.
- [ ] **Every GPU pass publishes its span to a READER** — a per-pass duration measured on the
      DEVICE and a consumer, so a cost that moves between passes is attributable without a
      profiler. The reader is built first and the probe second.
      **The READER is built and half the probe with it, in that order.** `Renderer::Spent(stage)`
      carries a per-stage `{TookMs, Draws, Triangles}` recorded around `EncodeStage` -- the one
      place every stage passes through -- and `apps/bench` reads it and divides. What it measures
      is the CPU's ENCODE span, not the device's execution: Unreal separates the two for a reason
      and `stat unit` shows Game, Draw and GPU as three numbers. So this predicate still owes the
      DEVICE side, which is SDL_GPU timestamp queries around each pass, and the row it lands in
      already exists.
      **And a duration alone was never the point**: every stage reports its WORK beside its time,
      because power is work over time and a millisecond without its population cannot be compared
      between two scenes or two machines. That is CLAUDE.md's own rule about a number carrying
      its population, applied where it had never been applied.
      proof so far: `apps/bench --all` prints a rate per stage per scene.
- [ ] **A run that misses the floor is RED and says by how much**, because a frame budget nobody
      can fail is a quotation.
- [ ] The store-op derivation (`Stored(resource)`, landed) is MEASURED: the frame-time delta over
      a full declared drive against the standing reference population.

**board:1989 hands this item its measurement.** A GPU-driven change wants a frame time before and
after, and this tree measures none anywhere -- `Core::Live::Took*` are BYTE counts, not durations.
So "before and after" has nothing to read and board:1989 closed without it, recording instead the
CPU term it actually removed: the vertex uniform is pushed once per PASS where it was pushed once
per model slot, which is once per PART. `outshine/door/ScoreWhatASecondSubjectDoes` reads
`one subject pushes 1 vertex uniform(s), two push 1` beside `one subject draws 1 batch(es), two
draw 2`. Until this item stands, that is the only shape of performance claim this tree can defend.
