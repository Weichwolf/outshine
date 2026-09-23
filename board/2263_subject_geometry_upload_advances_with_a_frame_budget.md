Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: engine, rendering
Tags: streaming, gpu, realtime, ownership

# Subject geometry upload advances with a frame budget

## Defect

Wien `2fc0aec4` has 857730 subject vertices and 1748976 indices. After
resumable shape cooking, `RuntimeScene::AdvanceGeometryBuild` calls `Build`
synchronously. Its subject submission takes 29.13 ms in the 2026-09-23 run:
11.42 ms packing and 16.32 ms device handoff. The whole geometry phase peaks
at 34.35 ms; 8/4528 frames exceed 16.67 ms. Subject residency reports 320 MB
offered across 146 uploads during the capture, not one 320-MB upload. The
candidate is private until publication, so partial preparation need not be
visible to the active world. A shorter shot or larger frame allowance is no fix.

## Contract and ownership

`RuntimeScene` owns one move-only subject preparation job for the candidate.
It holds the immutable shaped geometry and a stable draw/index plan until
completion or cancellation. The job owns its cursors and any CPU staging it
needs; it must not retain the stack-local `ChannelPack`, `EmitPack`, `SubjectMesh`
or callbacks currently created by `SubjectProxy::Place`. Use shape spans and
bounded scratch, not a second full copy of every subject stream.

`SubjectDraw`/`SubjectResidency` own candidate GPU buffers, transfer buffers,
submission tracking and retirement. They may accept index and vertex stream
chunks over several advances. They expose a ready state only after all ranges,
draw tables, materials and validation are complete. The active renderer keeps
its prior complete subject throughout preparation. Failure or stale candidate
revision discards the job and its private GPU products; existing world stays
renderable. GPU resources retire only after their last submitted use.

`RuntimeScene::AdvanceGeometryBuild` returns pending while either shape cooking
or subject submission is unfinished. Do not repeat completed packing or `Build`
on each call. Keep the glTF one-shot build entry point working through the same
job with an explicit unlimited budget. No public API or format-specific path is
needed. Maintain existing draw-run order, material slots, cluster jobs, index
rebasing and geometry digest for an identical source snapshot.

## Implementation order

1. In `src/render/SubjectProxy.*`, separate stable draw/index planning from
   per-stream packing. Own callback context or replace it with bounded span
   packing. Record packing and staging bytes per advance.
2. In `src/render/stages/SubjectDraw.*` and `SubjectResidency.*`, accept bounded
   index/vertex stream ranges into candidate-owned buffers; track completion
   and submission lifetime. Reject invalid offset/size and an incomplete mesh.
3. In `src/engine/RuntimeScene.*` and `SceneRenderer.h`, advance the private
   subject job after `ShapeCookJob` and bind it once. Preserve the old complete
   scene until final success. Keep `GroundWorldCandidate` publication atomic.
4. Bound the most expensive unit by measured bytes/work, then measure full
   frame p50/p95/p99, warm/cold transition and CPU/GPU peaks on Wien. If an SDL
   allocation or submit still blocks, isolate and bound that operation rather
   than moving it into an unmeasured phase.

## Falsifiable acceptance

- One-shot and interrupted schedules produce identical draw runs, indices,
  stream bytes, cluster tables, digest and PNG for a one-part ground subject and
  a multi-part glTF subject. Changing budget changes completion timing only.
- A stale revision, invalid range, failed upload and cancellation leave the
  previously published world intact; no incomplete subject can draw.
- Wien and Malcesine use the public client, retain their completed image
  digests, and show no geometry-submission unit over the declared frame budget.
  Open the PNGs and report full-frame distributions separately from job slices.
- Run `make format`, focused subject/ground and NDEBUG pacing tests, then
  `LINT_JOBS=2 make lint`. Report the checked commit and actual measured maxima.
