Type: bug
State: active
Parent: 2190
Depends: 2191
Priority: P1
Area: render, engine, test
Tags: gpu, ownership, state

# Sampled images publish complete immutable mip chains

## Defect

`SubjectResidency::UploadMip` allocates one staging buffer and submits one raw
command buffer per level. `BoundImage` retains only texture and sampler, then
`SubjectDraw::BindSurface` makes it bindable immediately. A failing later level can
leave a partially initialized image; no owner represents submitted-but-incomplete
upload work. SDL command ordering cannot replace an explicit engine publication and
lifetime contract.

## Decision

Replace `BoundImage` with move-only `SampledImage`. It owns texture, sampler, one
packed upload buffer and its submission fence until completion. Encode every mip into
one aligned upload allocation; record all levels in one copy pass and submit exactly
once. Any allocation/map/copy/submit failure destroys only the candidate and exposes
no image. `static_assert` requires nonthrowing moves for the published owner.

`WorldContent` candidates may contain `PendingSampledImage` products. The engine
state transition owned by WI 2191 polls their fences on the render thread; it never
waits in a frame. Until every required image is ready, the previous published world
remains drawable. First declaration reports a defined pending/no-world outcome rather
than sampling a partial product. Completion performs one nonthrowing world swap.
Cancellation, device loss and shutdown release fences and staging on their owning
thread; stale completions cannot publish over a newer revision.

## Implementation order

1. Add packed-chain copy recording and failure injection for each level boundary.
2. Add `SampledImage` lifetime/fence ownership and prove destruction before and after
   completion is safe.
3. Integrate the pending-publication state in WI 2191; do not add a second transaction
   framework in SubjectDraw or WorldCandidate.
4. Prove A remains visible while B uploads, B appears once only after all image fences,
   B failure keeps A, and immediate B retry succeeds. Measure no GPU wait or unbounded
   allocation in the frame path.

## Acceptance

- [ ] No material table can bind a partial mip chain.
- [ ] One image chain has one upload submission and retains staging through its fence.
- [ ] A→pending-B→B, late upload failure, cancellation, shutdown and stale completion
      preserve ownership and revision contracts through public Engine tests.
- [ ] Device injection, relevant public tests and `make lint` pass.
