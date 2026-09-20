Type: bug
State: active
Parent: 2191
Area: engine, render, flora, test
Tags: streaming, ownership, transaction

# Crown instance updates need frame-atomic resource swaps

## Problem

`WorldCrowns::Step` accepts a finished atlas and then creates prototypes or replaces instance rows
on the published `RuntimeScene`. `CrownPieces::Update` can fail after earlier groups changed. A frame can
therefore contain a mix of old and new crown resources, while the CPU group state has already
advanced. Rebuilding the complete world candidate per foliage update would reupload terrain and
unrelated pieces, violating the streaming budget.

## Decision

Give `RuntimeScene` and `SceneRenderer` a bounded resource-update transaction: validate every replacement,
allocate/uploads into inactive GPU resources, submit one ordered swap at a frame boundary, and only
then commit the matching `WorldCrowns` state. A rejected transaction retains every prior prototype,
instance row and stable handle. Keep atlas IO/preparation outside the render transaction. Retire old
GPU resources after their final submitted frame.

The transaction describes changed resources only. It must not clone native geometry, terrain pages
or unaffected pieces. Resource handles remain stable across the swap and are invalidated only by an
explicit release.

## Proof

- Reject the second of several crown prototype or instance-row uploads; verify every prior row,
  resident count and rendered frame remains unchanged.
- Retry the same update and verify one frame-boundary publication with no leaked resources.
- Update a dense forest while terrain streams; measure CPU/GPU p50/p95/p99, peak resource count and
  upload bytes. Show cost proportional to changed crown resources, not complete world size.
