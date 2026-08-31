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

## AND THE FIRST 27x CAME FROM A CONSTANT, NOT FROM CORES

With the rebuild finally visible on a moving camera, its phases said where the seconds were:

    rebuild: the buildings, streets and water took   2557.498 ms
      of that, the streets and the water             1996.332 ms   78 per cent
      of that, walking it into the proxy              300.268 ms
      of that, the census over every triangle          61.925 ms
    rebuild: of the streams, PACKING them                0.000 ms

**Packing the streams -- this item's goal names it as 748.6 ms of a single thread -- measures
zero.** The seconds are the streets: 20 205 ways, 93 729 vertices, and about 186 000 ground
samples at 10.7 microseconds each.

`GroundStream::TileAt` (TerrainLoader.cpp:277) LINEAR-SCANS a cache of `kGroundSlots` tiles and a
miss DECODES AND STITCHES a whole DEM tile. The constant was **12**. A street walk touches far
more than twelve tiles, so it thrashed.

Swept, and the curve is a THRESHOLD rather than a slope:

| slots | streets and water |
|---|---|
| 12 | 1996.3 ms |
| 24 | 1843.6 ms |
| 48 | 728.3 ms |
| **96** | **71.2 ms** |
| 192 | 72.6 ms |
| 256 | 72.4 ms |

The working set is between 48 and 96 tiles and above 96 more buys nothing, so **96 is derived**.
It costs 1.55 MB against 0.19 MB -- 1.36 MB for 27x on the dominant phase.

| place | p99 before | p99 after |
|---|---|---|
| Heidelberg | 2493.00 | **478.25** |
| OldTown | 1831.54 | **290.46** |
| Shibuya | 1413.01 | **755.15** |
| Venice | 920.13 | **475.92** |
| Jura | 14.29 | **4.99** |
| **CentralPark** | 18.51 | **27.95** |

**CentralPark did NOT get worse -- that reading was noise taken for signal, and the correction is
the more useful finding.** Three consecutive runs of the same binary on the same tree:

    CentralPark   p99 29.10   22.64   17.64      over budget 50, 15, 3
    Heidelberg    p99 453.01  479.87  468.73     over budget  5,  5, 5

Heidelberg repeats to 3 per cent. **CentralPark falls monotonically across runs and its
over-budget count drops from fifty to three** -- that is warm-up, not geometry, and it is the
place with 3.9 M building triangles.

So the instrument is repeatable for five of six places and NOT for the heaviest, while the gate
records exactly ONE run. Every p99 comparison in this item holds for the five and is void for
CentralPark. **A distribution that does not repeat is not a distribution**, and this instrument
does not yet say which of its places it can measure.

**What this does to the item.** Four cores would have quartered the rebuild. A constant
twenty-seven-folded the phase that held 78 per cent of it. The scheduler is still owed -- 760 ms
of rebuild is still a frame that misses by 45x -- but the goal's stated cause was wrong twice over
now, and a planner must be built against what the measurement says rather than what the sentence
said.

## AND WHAT IS LEFT IS NOT PARALLEL WORK, IT IS REDUNDANT WORK

With the DEM cache derived, the same rebuild reads:

    rebuild: the buildings, streets and water took   653.113 ms
      of that, the census over every triangle         72.045 ms
      of that, the streets and the water              66.911 ms      (was 1996)
      -> the buildings                              ~514     ms
    rebuild: of that, walking it into the proxy      267.208 ms
      of that, standing and submitting INSIDE Build  201.710 ms

The buildings are now the largest single item. What they actually did that round:

    buildings: floats in the soup                 15 788 288
    buildings: the field's last delta began at    15 786 752
    buildings: and ran for                             1 536      <- 512 corners
    buildings: corners the soup holds              1 973 536

**One ten-thousandth of the soup changed and all of it was re-walked.** 1 973 536 corners at
roughly 260 ns each is the ~514 ms, and `Grounds()` builds a fresh `outshine::Geometry` every
time -- the whole world is copied to add 512 corners.

**This is the same shape as the DEM cache and it changes what this item is.** Four cores would
make a redundant copy four times faster and it would still be redundant. The reference both
benchmarks actually implement is not "walk it in parallel": Unreal's proxies are updated
INCREMENTALLY per primitive and RAGE re-submits only what moved. A planner is owed for work that
must happen; this work must not happen at all.

So the order this item should run in is now measured rather than assumed:

1. the rebuild touches only what CHANGED -- the field already knows, it publishes the delta;
2. what remains after that is the work a planner is for, and its size is not yet known because
   nobody has seen a rebuild that only does new work.

## AND THEN I PROFILED THE INSTRUMENT FOR HALF AN HOUR

The rebuild's phase measures publish only under `--audit`, and `--audit` COSTS:

    Heidelberg without --audit    p99   455 ms
    Heidelberg with    --audit    p99  1018 ms

More than half of every phase number above is the price of looking. Two attributions followed from
it, both argued with arithmetic, both wrong:

| guessed | the arithmetic offered | what the test said |
|---|---|---|
| `CarryIntoTheFrame` is the 514 ms | 1 973 536 corners at 260 ns | made incremental -- **no gain** |
| the three vertex censuses are the 619 ms | three passes over 2 M | gated -- **no gain** |

The three censuses are about 30 ms, not 600. `CensusOverEveryTriangle` -- already gated -- is the
560 ms that `--audit` adds.

**So where the remaining ~450 ms of a non-audit rebuild goes is NOT KNOWN, and this instrument
cannot say without changing it.** That is the next step and it is a measurement problem before it
is a scheduler problem: a phase timer that costs nothing when nobody is reading it, or a profile
taken outside the frame.

Kept anyway, on principle rather than for the gain: the three censuses now run only when they were
asked for. A number nobody ordered should cost nothing. Digest 0da91522 unchanged, 8 PASS.

## THE PRODUCT PATH, PROFILED AT LAST -- and the measurement problem was that I passed two flags

`--measures` prints the phase timers; `--audit` additionally runs `CensusOverEveryTriangle`. I had
always passed both, so every phase number above carried the census. Passing only `--measures`:

    rebuild: the buildings, streets and water took      72.1 ms     (653 with --audit)
      of that, the census over every triangle            8.0 ms
      of that, the streets and the water                64.1 ms
    rebuild: of that, walking it into the proxy        268.1 ms     <-- the cost
      of that, standing and submitting INSIDE Build    201.8 ms
    rebuild: shaping what was built                     59.7 ms

**The geometry is not the cost. Handing it over is.** Buildings, streets and water together are
72 ms; walking the result into the proxy is 268, and 202 of that is inside `Build` -- shaping the
`outshine::Geometry` into what the renderer takes and submitting it.

So the three earlier attributions in this item -- `CarryIntoTheFrame`, the vertex censuses, and by
implication the whole "buildings are 514 ms" reading -- were all measuring the audit census. They
are corrected here rather than left standing: **the buildings phase is 72 ms, not 653.**

What this item is actually owed, measured on the product path:

    standing and submitting INSIDE Build   201.8 ms
    shaping what was built                  59.7 ms
    the streets and the water               64.1 ms

That is where a planner would earn its keep, and it is `Live::Build`'s submission path rather than
anything in `Grounds()`.

## THE SHAPE WAS BUILT TWICE PER REBUILD, and 152 ms of Shibuya's hand-over was the second one

Once the product path could be read, the 405 ms of `standing and submitting INSIDE Build` split:

    settling placements and lights   152.167 ms
    laying the surface                 0.870 ms
    streams to the device            252.122 ms

and `settling placements and lights` came to within 4 microseconds of `stand: shaping it a second
time` (152.163). That is not a coincidence -- `Live::Stand` opens with `Reshape()`, and `Live::Build`
had already called it 400 lines earlier. `Reshape()` is a pure function of `Held_` and `ShapeParts_`
and neither moves between the two calls, so the second one rebuilt an identical `Render::Shape`.

Two of those three numbers could not be read at all before this: `Restand(Geometry&&)` ZEROED
`StandMs_` and `SubmitMs_` right after `Build` returned. That zeroing was correct when it was
written -- 7f90617b removed a second stand-and-submit from `Restand` itself and the zeroes said
"this path no longer does it twice" -- but `Build` stands and submits INSIDE, writing those same
three fields, and the zeroes then deleted `Build`'s own measurement. The sentence stayed true; the
numbers stopped being.

**The fix is a stamp on the source, not a deleted call.** `Stand()` is also reached from the frame
path, where `Held_` really has moved, so removing its `Reshape()` would be right for one caller and
wrong for the other. `Core::Posed` now carries `Changed()`, a counter bumped by every mutator that
`Reshape` reads through -- `Clears`, `Reads`, both `Carries`, `PoseInto`, and a new `Appends` that
replaces the non-const `Assembled()` door so no mutation can slip past the counter. `Reshape()`
records the stamp it shaped and returns when it matches.

    Shibuya, product path             before      after
    stand: shaping it a second time  152.163     0.001 ms
    settling placements and lights   152.167     0.003 ms
    walking it into the proxy          565.5     410.7 ms

Picture 732bd2de, bit-identical.

**What is left, and it is one item rather than a scheduler:** `and the streams to the device`
250.9 ms, which is `Render::Place`. Inside it, `SubjectProxy.cpp:520` walks EVERY BYTE of every
index and every attribute stream through an FNV loop one byte at a time to publish
`restand: the geometry handed over, digested`. Shibuya hands over 587 MB. That is a DIAGNOSTIC on
the product path, the same shape of defect as the depth-pyramid readback, and it is measured next
rather than assumed.
