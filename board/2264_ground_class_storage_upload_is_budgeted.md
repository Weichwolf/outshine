Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: rendering, engine
Tags: streaming, gpu, realtime, ownership

# Ground classification reaches the GPU without a frame-sized upload

## Defect and evidence

The Wien candidate uploads 50,694,364 bytes of class structure through
`GroundStorage::Replace` in one call. The current shot retains `2fc0aec4`, but
`ground candidate: class upload` took 26.65 ms and 7/4671 full frames exceeded
16.67 ms. This call allocates both GPU buffers, maps and clears one full-sized
transfer buffer, copies both payloads, and submits them together.
`SceneRenderer::SetGroundClasses` then makes another full CPU copy through
`SceneResources::SetGroundClassification`. The timings do not yet separate
allocation, mapping, copies, submission and CPU retention in the 26.65-ms
sample. Two later runs did: allocation 0.05–0.10 ms, staging 2.19–5.20 ms,
submission 0.015 ms, restore copy 5.65–8.31 ms; whole call 10.62–10.91 ms.
These runs do not explain the earlier peak. Subject stream submission is WI 2263.
Shared immutable source ownership now removes the second 50.7-MB copy in the
engine path. Wien remains `2fc0aec4`: allocation 0.073 ms, staging 2.604 ms,
submission 0.019 ms, source retention below 0.001 ms, whole class step 2.698 ms;
p99 9.97 ms, 6/4657 frames late. Malcesine remains `07ca3a25`, class step
1.131 ms, no late frame. Both PNGs were opened. The GPU staging transfer is
still one full-size call, so a cold or larger input can exceed the budget.
With paced staging, Wien retains `2fc0aec4`: largest range 1,048,576 bytes,
allocation 0.047 ms, longest staging 0.200 ms, longest complete class slice
0.205 ms, aggregate class CPU time 1.402 ms, frame p50/p95/p99
3.14/4.85/9.69 ms, 7/4680 late, peak heap 1080 MB. Malcesine retains
`07ca3a25`: longest class slice 0.123 ms, aggregate 0.552 ms, frame
p50/p95/p99 2.44/6.24/9.60 ms, 0/1447 late, peak heap 668 MB. Both PNGs
were opened: Wien still has flat urban material/lighting, Malcesine has
artificial vertical shore walls; neither is caused by this upload change.
The former 26.65-ms peak has not been reproduced or attributed. Frame outliers
remain, so this WI claims bounded class staging, not a 60-Hz frame guarantee.

## Contract and ownership

The ground candidate owns immutable `ClassStructure` words and palette until
the class upload commits or cancels. Its private `SceneRenderer` owns one
move-only upload transaction; `GroundStorage` owns candidate GPU buffers,
staging and submission fences. `SceneResources` keeps immutable source bytes
alive through aliased shared ownership of the native class words and a moved
palette vector; it must not duplicate the 50.7-MB class blob. The renderer
receives only spans plus opaque lifetime owners, not `ClassStructure`. Neither
consumer sees partial classification. A complete, validated pair of buffers
and its retained source becomes visible together; on error or stale
revision the prior complete pair remains bound and all pending resources retire
after their last GPU use. No borrowed source span outlives its candidate.

Use a phase/cursor API inside the renderer: admit sources, allocate class and
palette storage, fill and submit bounded byte ranges, retain the source owners,
poll the final GPU fence without waiting, then commit both bindings.
Keep the pending source in `SceneRenderer::WorldContent` and the buffers,
1-MiB reusable transfer buffer, byte cursors and fence in `GroundStorage`.
One advance submits at most one aligned class or palette range. Reuse staging
only after `SDL_QueryGPUFence` signals; `GpuSubmission` needs an injectable
query hook. SDL releases GPU buffers and transfers when safe, so canceling a
candidate can release pending handles without a simulation-thread wait.
`GroundStorage::Replace` loops over the same transaction and may explicitly
wait for its fence in the synchronous caller. The candidate uses Begin/Advance
and keeps `GeometrySubmission` in its class phase until final fence and commit.
Start with at most 1 MiB of payload per advance: 50.7 MB needs at least 49
advances, about 0.82 s at 60 Hz before overhead. This is an initial budget,
not a measured guarantee. Keep `GroundStorage::Replace` as a synchronous
wrapper over the same transaction for callers that explicitly request it.
The ground candidate uses the paced path. Preserve four-byte alignment,
zero-filled minimum buffers, exact words/palette order and GPU command order.

## Implementation and falsifiable acceptance

1. Attribute allocation, map/zero/copy, submit/fence and source retention in
   `GroundStorage`, `SceneRenderer`, `SceneResources` and `Laying`. Source
   retention is now a shared-owner move; SDL retires GPU handles asynchronously,
   with no simulation-thread retirement wait. Measure the longest complete
   frame slice. If one
   full GPU-buffer allocation exceeds 16.67 ms, page the storage and shader
   lookup; moving that stall between phases does not satisfy this WI.
2. Advance one immutable candidate through bounded ranges. Validate offset,
   length, alignment, generation and source lifetime. Cancellation, map,
   acquire or submit failure leaves the previous pair readable. A retry begins
   a fresh transaction. The candidate never waits for a fence on the simulation
   thread; the explicitly synchronous `Replace` caller may wait.
3. Extend `GroundStoragePublishesCompleteUploads` with inputs on both sides of
   a chunk boundary, exact GPU readback, padded empty buffers, injected failure
   after a submitted range and cancellation. An incomplete range must leave the
   old published pair readable. Extend `GroundClassificationBelongsToItsWorld`
   for retained source lifetime and atomic commit. Compare synchronous and
   paced output.
4. Render Wien and Malcesine through outshine-client. Open PNGs, retain
   `2fc0aec4` and `07ca3a25`, and report class-stage maxima, full-frame
   p50/p95/p99, late-frame count and CPU/GPU peaks. Run `make format`, focused
   suites and `LINT_JOBS=2 make lint` on the committed code.
