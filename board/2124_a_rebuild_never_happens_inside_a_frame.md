Type: bug
State: active
Area: engine, world
Tags: measured, performance, owner

# A rebuild never happens inside a frame, and the ring recentres by what changed

**Benchmark** -- Unreal streams asynchronously (`FIoDispatcher`, level streaming builds
off-thread and registers the result) and never blocks a frame on it; RAGE's streaming threads sit
beside `sysTaskManager` for the same reason. **Both agree**, and CLAUDE.md's fourth invariant
restates them: the simulation hands the renderer a delta, the old geometry keeps drawing until
the new exists, the swap happens at a frame boundary, complete.

## Where it stands, measured 2026-09-04

```
  Engine::State::Updates()     ends with Grounds(false), every frame        Advancing.cpp:214
  Grounds                      patchwork, classify, Paves -- synchronous     Laying.cpp:2079
  Restand                      meshes buildings, water, ways inline           GroundStack.cpp:126-128
  the step that crosses a tile ~1132 ms at Kaiserberg (was 2211)             7c98b7d3
  what a recentre rebuilds     the WHOLE ring, not what entered and left     dba32c6b
  the instrument               a still camera, so p99 reads 2.6-5.3 ms and 0 of 120 -- and
                               "this did not make the engine faster"
```

`RingWanted` is split out of `Grounds` (08253818): the cheap decision -- does the ring move --
is a seam, and the expensive half behind it still runs on the frame path. No amount of making
that half cheaper fixes this: 1132 ms cut by ninety per cent is 113 ms in one frame, seven
frames missed.

Measured 2026-09-04 at Shibuya with the bake on a worker (board:2122): while tiles landed
two a frame, `PlacePiece` (upload + `Retable`) on the frame path read sim p99 46 ms, worst
102 ms; with every tile landed, sim p99 1.0 ms. The landing is a rebuild by another name.

Measured 2026-09-04 after the cook and the digest moved into the worker's bake (board:2122's
`BakedTile` carries `Walls`, `Roofs`, `Digest`): at Shibuya a landing costs the frame
0.3-0.7 ms per piece for the upload, and `Retable` 3 ms per frame in which a piece landed
(20 ms the first time, while its tables grow). The cook read 2-4 ms per piece before and is
gone from the frame. What `Retable` still does is rebuild the WHOLE cluster table -- 67 000
jobs and spheres for 98 pieces -- for one piece that entered; that is the "one tile costs the
ring" of the pool, and the answer is Nanite's: a piece's jobs are a RANGE in a persistent
table, written once at placement, and the table is uploaded by the range that changed.

## The solution

Two halves, and board:2122 supplies the first:

**Per tile, off the frame.** With pieces baked by workers and handed over whole, a recentre by
one tile ERASES the pieces that left and ENQUEUES the ones that entered; nothing that stayed is
touched. The frame's `Grounds` becomes: place the pieces that finished, in the watermark's
declared order, under the counted budget (board:2105). The world-grained passes -- `Classify`
over every vertex, `Paves` over every lane -- are what board:2115 and board:2101 turn into
per-tile work; until then they run on the same worker pool as the bake, and the frame draws the
LAST COMPLETE world.

**Then the camera moves again.** `make shots` puts the walk back (board:2092) and the frame
budget is measured where it is missed.

## What will be true

- [ ] `Grounds` runs off the frame path; `the step's own time, most` never carries a rebuild
- [ ] A recentre by one tile costs one tile: a case measures the work of a one-tile move as
      proportional to one tile, not the ring -- the cluster table included (measured above)
- [ ] 0 of 120 frames over 16.67 ms on all nine places WITH the camera walking
- [ ] The frame draws the last complete world: a case captures a frame during a rebuild and it
      is the previous world, whole, never a half
- [ ] Negative control: call `Grounds` synchronously from `Updates` again and `OverBudget` goes
      RED at the tile crossing

## Landed 2026-09-05: a stitched field is a POOL job, and the halo stops stitching on the frame

With board:2115 closed the ground's lay stood at ~700 ms at Kaiserberg, and 620-680 ms of it
was the HALO: every sheet's rim asks its neighbours' stitched fields, and a stitch reads up to
thirteen raw grids through one cache on the main thread. Unreal's `FIoDispatcher` and RAGE's
streaming threads both put exactly this kind of work -- decode, assemble, hand over whole --
beside the frame, and this tree's pool already runs its mesh jobs that way. So a stitched
field is a pool job (`TilePool::Field`, `Rank::Field`): a worker stitches on its own
`TerrainTiles`, the result is a `shared_ptr<const TerrainField>` in the job's result, the
stream asks the pool (`GroundStream::StitchedField`, non-blocking, and `StitchedFieldAwaited`)
and holds what lands in its budgeted cache. The halo ASKS every neighbour field of every sheet
first (`HeightSheets::AsksFields`, a post per tile, 6 workers stitch in parallel) and only then
walks the sheets waiting on each -- deterministic, because a stitched field is a pure function
of its bytes whichever thread computed it, and the sheets are walked in their declared order.

```
                       before      after, the first lay     after, fields held
  Kaiserberg haloing   657 ms      548 ms                   18 ms
  OldTown haloing      700 ms      543 ms                   34 ms
  the nine digests     unmoved
```

**Corrected 2026-09-05.** Commit 2f128d4a claimed 657 -> 18 ms; the 18 ms was a RELAY's
halo over fields already held, published last because the dropped field jobs (the third
defect below) forced that relay after the first lay had copied its rims. With the handoff
repaired the first lay at a place waits on the workers and no relay follows: Kaiserberg 548 ms
for 300 field jobs that cost 1 009 ms of worker time (3.4 ms a stitch, six workers) behind
870 raw fetches; OldTown 543 ms, 288 jobs, 950 ms. The field jobs rank after the mesh jobs
by design -- a mesh makes a tile drawable, a field only stitches its rim -- so the halo's
wait begins where the last mesh lands. That wait is the preload's, before any frame; a lay
whose fields are held costs the 18 and 34 ms, and a one-tile move asks only the new edge's
fields. The ledger publishes the field jobs, their drops and their worker time beside the
mesh jobs' so the number is a client's to read.

Three pool defects the gate found the same day, each measured before it was touched:

- **the pool's kept list held every result, held or consumed.** Each field stitch on a worker
  fetches its raw grids through the pool, and every fetch result took a slot in the one
  kept list (1 024) beside the mesh results that landed first -- so after the second lay
  the mesh results were the oldest, fell out, and the asking lay saw every tile pending
  again; the pool re-meshed the whole ring in the background and the client read 128 bare
  tiles at Kaiserberg where the picture was right. A result consumed on its poll leaves no
  trace now; the kept list bounds only what is held, and field results keep their own
  bounded list. Measured: 122 frames of `resident 128` after the second lay, 0 bare
- **six workers decoded the same raw grids six times.** Each worker's `TerrainTiles` had its
  own 16-slot decoded cache, and the queue hands neighbouring tiles to neighbouring workers,
  so a raw grid a stitch needs was decoded once per worker; the preload read 7.2 s waited
  against 5.5 before the field jobs. The decoded cache is one object with its own lock
  (`Ground::DecodedCache`, 32 MB [SET, three working sets of a 3 x 3 stitch neighbourhood at
  264 KB a grid]), shared by the pool's workers; the stream's stitch pool keeps its own.
  Measured: Kaiserberg waited 7.2 -> 5.0 s (5.5 before the field jobs), peak heap 282 ->
  302 MB, the cache's own bytes
- **a fetch job handed its bytes over through the LRU byte cache, not in its result.** The
  carrier ran the fetch into a scratch landing, `Remember`ed the bytes in the pool's 64 MB
  byte cache and put a bytes-less result in `Done_`; the worker that had parked on that
  fetch then polled the result and READ THE CACHE for the bytes. A three-level halo pulls
  in ~700 raw tiles, so by the time the parked job ran again its entry was the LRU victim:
  the worker's ask came back pending with the fetch neither done nor posted, the job was
  dropped, the halo copied the rim, the next relay re-posted it and the same thing happened
  again. Measured at ZurichPlan: 716 entries in the 64 MB cache, 956 landings lost, 23 rims
  copied on every relay, the place refused after 15 s with `0 of 100 tile(s) still
  pending` -- a refusal that named nothing, so it now names the bare tiles and the copied
  rims too. A handoff through a cache is a read of live state where a snapshot belongs
  (the invariant on the front page); the result carries its landing (`Result::Landed`) and
  `Bytes` takes it from the consumed result. The cache serves only a repeat ask. And the
  kept-list repair above had landed in the CARRIER loop, which runs no field job, while
  the worker still pushed every field result into the held list: one `Lands(key, holds)`
  now serves both loops, so the two lists cannot be maintained apart again

What the frame's lay still carries at Kaiserberg: the streets and the water 2 783 ms (board:2101's
move to the workers), pressing 230 ms, the soup's BVH 149 ms, the whole rebuild 2 896 ms --
and the instrument that will show it is the walk (board:2092).

## Ruled out, measured

- making the rebuild cheaper (b18fb9df, 7c98b7d3): 2211 -> 1132 ms; right to do and not the
  answer
- a still camera: the frame column went green and the engine did not change

## Decided 2026-09-04: the order the rebuild leaves the frame in

1. buildings bake on `Tasks` workers from a snapshot and are PLACED in post order, N per frame
   (board:2122's next block) -- the first synchronous mesh leaves `Restand`
2. the ground becomes a GPU height field (board:2115): `Grounds`' gather, classify, yield and
   the ring upload go; a tile crossing uploads one height tile
3. the road derivation moves to the generator (board:2101) and runs per tile on the same workers
   over the same snapshot; the ribbon pieces are placed like buildings
4. what `Grounds` keeps is the SEQUENCE: place finished pieces, write finished stamps, hand the
   frame its snapshot -- and the camera walks again in `make shots`
