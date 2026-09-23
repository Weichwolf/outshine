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
synchronously. Subject submission takes 29.13 ms in the baseline: 11.42 ms
packing and 16.32 ms device handoff. A later measured handoff spent 4.91 ms
on indices, 7.20 ms on vertex streams and 1.55 ms on draw tables. The whole
geometry phase peaks at 34.35 ms; 8/4528 frames exceed 16.67 ms. Residency reports 320 MB
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
Separate CPU planning/packing, index upload and vertex/table upload into at
least three advances; the baseline pack plus index alone can exceed one frame.
`SubjectProxy` now separates `PreparePlacement`, `BeginPlacementUpload` and
`FinishPlacementUpload`; the scene-owned scratch checks Shape identity through
submission. The synchronous `SubmitPlacement` composes those steps for the
existing path. `SubjectDraw` and `SceneRenderer` expose generation-bound
begin/finish calls, including empty subjects.
The paused state retains the prior complete draw; stale tickets fail. The
focused GPU case checks those contracts, but its linear image is black and is
not an independent visual oracle. Place and glTF images must prove visibility
after pacing. `RuntimeScene` now advances plan, binding, CPU packing, index
upload, stream/table upload and finalization in separate candidate frames;
its one-shot path loops over the same stages. The native test compares direct
and interrupted digests and pixels; the place pacing test passes normally and
with NDEBUG. Individual upload ranges are still unbounded.
`GroundBuildState` now gives class upload, geometry admission and cooking
separate advances. The current Wien capture retains `2fc0aec4`: class upload
10.78 ms, longest complete geometry slice 15.95 ms, longest inner phase CPU
packing 10.98 ms. Six of 4471 full frames still exceed 16.67 ms; worst sim
35.39 ms and draw 78.62 ms require separate attribution. Malcesine remains
`07ca3a25`, longest geometry slice 1.71 ms. These captures demonstrate a
reduction, not a guaranteed bound: an earlier class upload took 17.55 ms.

## Implementation order

1. In `src/render/SubjectProxy.*`, retain stable draw/index preparation and
   separated index and stream submissions. Replace full upfront channel
   packing with bounded span packing. `RuntimeScene` already owns `Stood_`,
   `Shaped_` and `Scratch_`; a
   move-only placement job may borrow them for exactly that candidate's life.
   Keep `Scratch_.Draws`, indices and positions fixed from begin through finish:
   `SubjectDraw::MeshTicket` pins their addresses. Recreate callback contexts
   locally on each advance, never persist pointers to their stack storage.
   Record packing and staging bytes per advance.
2. In `src/render/stages/SubjectDraw.*` and `SubjectResidency.*`, accept bounded
   index/vertex stream ranges into candidate-owned buffers; track completion
   and submission lifetime. Reject invalid offset/size and an incomplete mesh.
3. In `src/engine/RuntimeScene.*`, retain the phase sequence after
   `ShapeCookJob`; verify the prior complete scene stays resident until final
   success and `GroundWorldCandidate` publication remains atomic. Bound work
   inside the phases rather than introducing a second build route.
4. Stress class upload, admission and index/stream submission with larger
   inputs. Bound any over-budget unit by measured bytes/work, and measure full
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
