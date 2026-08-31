Type: task
State: open
After: 2013
Area: base
Tags: concurrency, determinism, performance

# ONE scheduler runs the tree, and it combines in DECLARED order

**Benchmark** — RAGE: `sysTaskManager` owns the worker threads and every subsystem posts to it;
streaming threads stand BESIDE it because a fetch blocks and a blocked worker is a held slot.
Unreal: `FTaskGraph` over `LowLevelTasks` does the same, and `FIoDispatcher` is the separate IO
side. **They agree on the shape and on the split**, so the matter is closed: one compute pool sized
by cores, one IO pool sized by outstanding requests, and nothing owns threads of its own.

## What is here already, because writing a second would be the worst outcome

`outshine::Work::Graph` (`src/base/Graph.{h,cpp}`, 180 lines) IS a task graph: `Adds(Does, void*)`,
`After(step, earlier)`, `Runs()`, a worker pool of `Hands()` threads, an owed-count per step and a
ready queue. **Two files reach it: `src/audio/Mixer.h` and `src/audio/BusGraph.cpp`.** Nothing else
in the tree knows it exists.

    Held_[kMostSteps]   64 steps, fixed
    After[kMostAfter]    8 dependencies a step, fixed

Beside it, four other places own threads with no planner between them:

    src/host/Fetching.{h,cpp}        a fetch pool
    src/world/ground/TilePool.{h,cpp} Threads_ AND Carriers_, two groups
    src/world/ground/ClassBuilder.h   one thread
    src/base/Graph.{h,cpp}            the graph's own hands

Eleven `std::thread` sites in four groups, and `grep -rln thread src/generators/` finds NOTHING:
every generator is serial.

## What was measured

Shibuya's rebuild, 776 ms in all:

    walking it into the proxy      749.8 ms
      of which the device takes      1.2 ms
    the ground ring                133.4 ms
    the census over every triangle  38.0 ms
    cutting it into clusters         7.1 ms

**THIS NUMBER WAS READ WRONG and board:2057 holds the correction.** 778.9 of the 907.8 ms is
`Live::Stand` integrating the atmosphere on the CPU to reach one ambient radiance; the channel
packing this item named is 0.018 ms. The phase name said "walking it into the proxy" and I did not
ask what the phase CONTAINED.

What still stands for this item, and it is not a millisecond count: every generator is serial, four
places own threads with no planner between them, and the tree's one task graph is bounded at 64
steps and reached by the audio mixer alone. The number this item should be spent against has to be
measured AFTER board:2057, on a rebuild that is not 86 per cent atmosphere.

## What will be true

- [ ] `Work::Graph` is the tree's ONE scheduler and its step and dependency counts are not fixed at
      64 and 8; the four thread-owning places post to it instead of owning hands
- [ ] the IO side stays SEPARATE and is sized by outstanding requests rather than by cores, because
      a fetch blocks and a blocked worker is a slot doing nothing
- [ ] a generator stays a SERIAL function and the planner runs many of them -- no generator threads
      itself
- [ ] every combine indexes by the step's DECLARED id and never by completion order, and the places
      suite's six digests are unchanged across ten runs with the pool at 1, 2 and 6 hands
- [ ] Shibuya's rebuild falls with cores rather than standing at 776 ms

## What this does NOT cover

The frame path. `Live::Advance` and the render stages are not moved onto the planner by this item;
what is moved is the REBUILD, where the 748.6 ms is. A frame-path scheduler is a different question
with a different bound (no alloc, no lock, no unbounded block) and it comes after a measurement
that says the frame needs one -- today's frame is 9.6 ms of a 16.7 ms budget.

## THE CASE IS MEASURED NOW, on a moving camera, and it is SECONDS

The number this item's goal quotes -- 748.6 ms of one thread packing channels -- was disproven the
day it was written: 778.9 ms of Shibuya's 907.8 ms was atmospheric integration on the CPU, and
channel packing was 0.018 ms. What was missing was a case where the rebuild happens AT ALL while a
frame is being timed, because `make shots` timed a still camera and a still camera never rebuilds.

board:1457 gave the places a declared walking path and made the ground restand where the eye
stands rather than where the scenario was declared. With that, 120 frames a place against a
16.67 ms budget:

| place | p50 | p95 | p99 | over |
|---|---|---|---|---|
| Heidelberg | 3.70 | **26.12** | **2493.00** | 8 |
| OldTown | 3.01 | 6.00 | **1831.54** | 2 |
| Shibuya | 4.02 | 7.50 | **1413.01** | 3 |
| Venice | 3.30 | 4.19 | **920.13** | 2 |
| CentralPark | 7.23 | 7.58 | 18.51 | 2 |
| Jura | 4.06 | 4.79 | 14.29 | 1 |

**A single frame that crosses a tile boundary costs up to 2.5 seconds**, and the p50 does not move
at all -- the whole cost lives in the tail, which is why this was invisible for as long as it was.

**It is not streaming.** The world's fields grow 180.8 MB to 182.3 MB across the entire 600 m walk
and no round stops at the memory ceiling: nothing is being fetched. The seconds go into re-laying a
world that is already resident, on one thread, which is precisely what this item exists to fix.

So the goal's sentence -- *fertig, wenn der Neuaufbau mit Kernen skaliert* -- finally has a
measurement to be judged against, and it is a p99 rather than a mean.
