Type: task
State: active
Area: generators
Tags: memory, optimisation

# A corner is stored ONCE

**Benchmark** — Unreal: `FStaticMeshRenderData` is INDEXED and its build welds vertices whose position, normal and UVs agree within a tolerance; a soup never reaches the device. RAGE: `grmGeometry` is indexed for the same reason. **They agree, so the matter is closed** — this item is about doing it.

## What was measured

Shibuya, 8 789 636 building triangles, so **26 368 908 corners in an unindexed soup**:

    identical in POSITION alone            20 824 134    5 544 774 remain
    identical in POSITION AND NORMAL        9 548 539   16 820 369 remain

A renderer cannot weld on position alone -- a hard edge needs two normals at one point, and
collapsing them rounds every building corner. **The honest, shading-preserving win is the second
row: 26.4 M corners become 16.8 M, which is 36 per cent of every vertex stream.**

Six streams cross per subject -- position, normal, emitted radiance, uv, colour and the previous
pose -- so 36 per cent comes off all of them. Measured beside it: 3.5 GB of graphics allocations
during Shibuya's load, on a target that holds 8 GB for everything including the operating system.

The index run does NOT grow: it is 26.4 M entries either way, 106 MB, and it is already there.

## Where it belongs

**In the generator, not at the hand-over.** `BuildingMesh::Site` pushes three corners per triangle
into one soup; it should push into a keyed table and emit an index. That is where the information
is -- a wall panel's four corners are known to be four, and the soup turns them into six. Doing it
afterwards means hashing 26 M corners to recover what the mesher never had to lose, which is the
same shape of mistake as the roof/wall split this tree just removed: the engine spent 11.4 s
re-deriving by normal direction what the generator already knew.

**The measurement is already wired.** `render.audits` publishes both rows above, and
`outshine-client shots --audit <place>` turns it on for one run. It is off by default because it
costs 11.3 s of a 19 s load, which is why this number went unmeasured for so long.

## What will be true

- [ ] the building mesher emits an INDEXED mesh and the soup is gone
- [ ] `solid: building corners identical in POSITION AND NORMAL` reads ZERO, because there are none
- [ ] Shibuya's graphics allocations fall by about a third and the picture digest does NOT move --
      the same triangles, the same shading, fewer copies of the same corner

## What this does NOT cover

**Position-only welding is refused** and the reason is the second row above: it would take 26.4 M
corners to 5.5 M, which is a further 67 per cent, and it would round every hard edge in the city.
That is a different picture, not a smaller one.

**The terrain is not in this.** These numbers are the BUILDING soup. Whether the ground's tiles
carry the same redundancy is unmeasured and belongs in its own item rather than assumed here.
