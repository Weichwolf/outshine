Type: bug
State: open
Area: engine, world
Tags: measured, performance, owner
Depends: 2122

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
      proportional to one tile, not the ring
- [ ] 0 of 120 frames over 16.67 ms on all nine places WITH the camera walking
- [ ] The frame draws the last complete world: a case captures a frame during a rebuild and it
      is the previous world, whole, never a half
- [ ] Negative control: call `Grounds` synchronously from `Updates` again and `OverBudget` goes
      RED at the tile crossing

## Ruled out, measured

- making the rebuild cheaper (b18fb9df, 7c98b7d3): 2211 -> 1132 ms; right to do and not the
  answer
- a still camera: the frame column went green and the engine did not change
