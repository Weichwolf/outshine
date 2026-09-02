Type: bug
State: open
Area: engine, world
Tags: measured, performance, determinism, owner

# The frame streams within a COUNTED budget, and the preload FLUSHES until the world is whole

**Benchmark** -- RAGE: `CStreaming::Update()` runs once per frame and services a BOUNDED number of
requests; `CStreaming::LoadAllRequestedObjects()` is the separate, blocking form used when a scene
is entered and never during play. Unreal: level streaming is asynchronous with a per-frame budget,
and `UWorld::FlushLevelStreaming(EFlushLevelStreamingType::Full)` is the blocking form the
automation harness calls before every screenshot comparison. **Both agree there are TWO modes and
that they are different functions.** They differ in what bounds the frame mode: Unreal bounds it in
MILLISECONDS (`s.LevelStreamingActorsUpdateTimeLimit`), RAGE bounds it in REQUESTS.

**Taken: RAGE.** A time budget is read from a clock, so what a frame finishes depends on how fast
the machine was that second -- and this tree's determinism is COMPULSORY, not aspirational. The
budget is counted in tiles.

## Measured 2026-09-02, `shots --all`, three runs

Two of nine places draw a DIFFERENT picture from the same declaration:

| place | run 1 | run 2 | run 3 |
|---|---|---|---|
| Shibuya | `e14aeb7e` | `eda96c41` | `eda96c41` |
| CentralPark | `da5dfb89` | `da5dfb89` | `53b0a600` |

The other seven are bit-identical across all three. OldTown is bit-identical across six runs at
349 766 triangles. The two that wander are the two densest.

And the same runs say eight of nine places break the frame budget, each on ONE outlier:

| place | p50 ms | p99 ms | over 16.67 | worst at frame |
|---|---|---|---|---|
| OldTown | 2.42 | 2196 | 2 of 120 | 80 |
| Heidelberg | 2.99 | 3375 | 4 of 120 | 68 |
| Shibuya | 3.55 | 3725 | 5 of 120 | 45 |
| CentralPark | 5.78 | 515 | 2 of 120 | 115 |
| Venice | 2.94 | 2482 | 2 of 120 | 55 |
| Jura | 3.30 | 13.9 | 1 of 120 | 99 |
| ZurichPlan | 6.13 | 6.82 | **0 of 120** | -- |
| Kaiserberg | 4.35 | 4713 | 4 of 120 | 8 |
| Koehlbrand | 3.43 | 4287 | 3 of 120 | 50 |

ZurichPlan is the control: where nothing streams during the walk, no frame is late.

## What is wrong

`Engine::advance` -> `Updates()` (`src/engine/Advancing.cpp:191`) calls `GroundStack::Restand(at)`,
and `Restand` (`src/world/ground/GroundStack.cpp:118`) turns

    for (int pass = 0; pass < kVectorTiles /* 49 */; ++pass) { Ways_.Ingest(); WaterBodies_.Ingest(); Footprints_.Build(); }

Forty-nine tiles meshed synchronously inside one frame. That is the unbounded term CLAUDE.md's
fourth invariant forbids on the frame path, and it is the whole of the p99 column above. The same
call is what the preload uses, so the two modes are one function with one budget that is wrong for
both: too large for a frame, and abandoned too early for a preload.

Abandoned too early, and SILENTLY: the loop also carries

    if (HeapBytes() > kHoldsBytes /* 512 MB */) { ++Overflowed_; break; }

so once the ceiling is touched the stack stops ingesting, `Drained()` stays false, `Ingested()`
stays false, `preload` runs to its patience and returns a refusal -- and `PlaceCamera::Draw` records
`Preloaded = 0` and measures the place ANYWAY. `HeapBytes()` sums `std::vector` CAPACITIES, and a
capacity is the geometric growth of however many `push_back`s ran, so WHERE the ceiling is touched
depends on how many tiles had arrived when the pass ran. That is arrival order reaching a
termination condition, which is the second half of the wander.

## What will be true

- `Restand` takes the budget it may spend, counted in tiles, and the frame passes a small one
- the preload's form turns until `Drained()` or until a pass makes no progress, and REFUSES loudly
  rather than leaving a half-built world behind
- a place that does not preload is not measured; `Preloaded = 0` is a refusal, not a footnote
- the 512 MB ceiling refuses rather than breaking out of a loop nobody told

## The measurement that shows this was wrong

`shots --all` three times: every digest identical, in every column. `0 of 120 over 16.67` for every
place, not just ZurichPlan. If the counted budget is set so low that the walk never catches up, the
picture at frame 119 differs from the picture at frame 0 by more than the camera moved -- that is
the negative control, and it goes red by comparing those two frames.
