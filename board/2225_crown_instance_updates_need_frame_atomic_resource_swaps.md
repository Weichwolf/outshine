Type: bug
State: active
Parent: 2191
Area: engine, render, flora, test
Tags: streaming, ownership, transaction

# Crown instance updates need frame-atomic resource swaps

## Problem

`VegetationStreaming::Step` accepts a finished atlas and then creates prototypes or replaces instance rows
on the published `RuntimeScene`. `Render::ImpostorInstances::Update` can fail after earlier groups changed. A frame can
therefore contain a mix of old and new crown resources, while the CPU group state has already
advanced. Rebuilding the complete world candidate per foliage update would reupload terrain and
unrelated pieces, violating the streaming budget.
Creating crowns inside a resumable Ground candidate is also invalid: a later ground-revision
change can abandon its resources after `VegetationStreaming` retained their handles.
Frame encoding must continue from the published renderer state while such a candidate is open;
drawing its partial indirect tables caused an invalid Metal indirect-draw access in Malcesine.

## Decision

Give `RuntimeScene` and `SceneRenderer` a bounded resource-update transaction: validate every replacement,
allocate/uploads into inactive GPU resources, submit one ordered swap at a frame boundary, and only
then commit the matching `VegetationStreaming` state. A rejected transaction retains every prior prototype,
instance row and stable handle. Keep atlas IO/preparation outside the render transaction. Retire old
GPU resources after their final submitted frame.

The transaction describes changed resources only. It must not clone native geometry, terrain pages
or unaffected pieces. Resource handles remain stable across the swap and are invalidated only by an
explicit release.
Until that transaction exists, GPU crown creation and updates wait while a Ground candidate is open;
atlas IO/preparation remains independent. This serializes publication without losing CPU work.
Renderer frame execution always binds the published state; candidate mutation remains invisible
until the world transaction publishes.

## Proof

- Reject the second of several crown prototype or instance-row uploads; verify every prior row,
  resident count and rendered frame remains unchanged.
- Retry the same update and verify one frame-boundary publication with no leaked resources.
- Hold different published and candidate geometry across a frame; the image must remain the
  published image. The same oracle must fail when frame execution selects the candidate.
- Update a dense forest while terrain streams; measure CPU/GPU p50/p95/p99, peak resource count and
  upload bytes. Show cost proportional to changed crown resources, not complete world size.
