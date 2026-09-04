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

**AND A THIRD, WHICH THREE RUNS CALLED CLEAN.** Kaiserberg drew `984cb74e` three times
across 2026-09-03 against `7530376c` on roughly twenty-five other runs, with the
tree's road code byte-identical across the pair (the change under test was a pure
dissolution of `Network::Weave`, compared character by character). So three runs of
`shots --all` are not enough to call a place deterministic -- they were enough to
clear Kaiserberg and it wanders anyway. Whatever count this item finally quotes, it
is the count at which the WHOLE set has been quiet, not the count at which one place
was.

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

## The two are ONE defect, and the measurement says so

Once `preload` refuses on a stack that stopped at its ceiling, and `PlaceCamera::Draw`
stops measuring a place that did not preload, the nine places split exactly along the
line the digests had already drawn:

| place | preloads | digest across every run |
|---|---|---|
| Shibuya | **no** | wandered |
| CentralPark | **no** | wandered |
| OldTown, Heidelberg, Venice, Jura, ZurichPlan, Kaiserberg, Koehlbrand | yes | steady |

Seven of nine load and seven of nine are steady; two do not load and those two are the
two that wander. Kaiserberg is the caveat: it preloads and still drew a second digest
twice, so preloading is necessary and not yet sufficient -- it stands close enough to
the ceiling that the round it stops on still varies.

## Where it stands, 2026-09-03

`shots --all`, three runs, after the ceiling began refusing out loud, `Settle` freed
96 MB, and `Restand` stopped ingesting while tiles were in flight:

| place | run 1 | run 2 | run 3 |
|---|---|---|---|
| OldTown, Heidelberg, CentralPark, Venice, Jura, ZurichPlan, Kaiserberg, Koehlbrand | identical | identical | identical |
| Shibuya | refuses | refuses | refuses |

**Eight of nine hold a steady digest across three runs, and the ninth does not draw at
all.** CentralPark was one of the two that wandered.

**AND THE COMMAND SHAPE REACHES THE PICTURE AT THREE PLACES, not one.** Measured
2026-09-03 with the ring cut in:

| place | variation inside `--all` | the suite, one place per run |
|---|---|---|
| Kaiserberg | 0.477 | green (bar is 1.0) |
| Koehlbrand | 0.596 | green |
| CentralPark | 0.959 | red |

The suite runs each place in its own process and reads a picture the same place does not
draw inside `--all`. Two of the three cross the case's bar in one shape and not the other,
so **the bar is being decided by what ran before**, which is the whole of what this item is
about, one level up from a tile.

Two things this does NOT settle. Shibuya still holds 537 951 538 bytes against a
536 870 912 ceiling, so its determinism is untested rather than proven. And CentralPark
draws `d56ae2e1` inside `--all` against `3cdca8d5` over sixteen runs of `--rows
CentralPark` alone -- steady within each shape of the command and not across them, so
what ran BEFORE a place still reaches it. That is the same sentence as the original
defect, one level up.

## Preloading is necessary and NOT sufficient: there is a second source

Measured 2026-09-03, after `Settle` freed 96 MB and CentralPark began to preload:

| run | CentralPark | triangles |
|---|---|---|
| 1 | `d56ae2e1` | 3 932 159 |
| 2 | `3cdca8d5` | 3 932 159 |
| 3 | `d56ae2e1` | 3 932 159 |

It preloads, it holds the same count of triangles every time, and it still draws two
pictures. So the second source is the ORDER the tiles were meshed in, not the amount.

`TileWatermark::Ask` sorts its candidates by `(distance^2, zoom, X, Y)` and is right to.
But the CANDIDATE SET is whatever tiles have arrived by the time the round runs. With A
(near) and B (far) both resident, A is meshed first; with only B resident, B is meshed
first and A follows. Same tiles, same triangles, different order in the vertex buffer --
and for coplanar faces the draw order decides the pixel.

**This is the invariant, stated exactly**: work is combined in a DECLARED order and
never in completion order. The fix is the same `FlushLevelStreaming` shape this item
already argues for: fetch the whole ring FIRST, then mesh it in the watermark's order,
so the candidate set is the ring rather than whatever landed.

## AND THE MEASURES CANNOT SEE IT

Two CentralPark runs drawing `3cdca8d5` and `d56ae2e1`, every published measure diffed.
Of roughly 340 numbers, FIVE differ, and not one of them is geometric:

| measure | `3cdca8d5` | `d56ae2e1` |
|---|---|---|
| asks that repeated a posted job | 266 | 200 |
| heap: bytes LIVE right now | 1 415 061 248 | 1 415 012 096 |
| heap taken under tile-worker | 1 784 761 744 | 1 749 021 504 |
| heap taken under tile-carrier | 80 016 272 | 79 426 448 |
| heap taken under frame-tells | 699 856 | 696 880 |

Everything else -- triangles, vertices, class features taken, street stations, water
surfaces, ring extents, lighting -- agrees to the last digit. Same geometry, different
picture. **The instrument does not see the defect**, which is CLAUDE.md's "a measure
that cannot see" and means the next step is a MEASUREMENT rather than a repair.

What the five do say: `tile-worker` differs by 36 MB of churn between the two runs, so
the workers divided the work differently. The invariant names that case exactly --
work that ran on more than one thread, combined in completion order -- and the place to
look is what the tile pool hands back and in what order, not the watermark.

The one-line repair this item argued for (`if (Vectors_->PendingTiles() > 0) return;`
before ingesting, so the candidate set is the ring rather than whatever landed) is IN,
and it is right by the invariant, but six runs after it CentralPark still draws both
pictures. The candidate set was not the mechanism.

## And the ceiling cannot see a quarter of what is held

Measured at Shibuya, 2026-09-03:

| held | MB | inside `HeapBytes()`? |
|---|---|---|
| raised building geometry | **360** | yes |
| OSM features | 181 | yes |
| land classes | 70 | yes |
| footprints | 22 | yes |
| streets and water | 1.3 | yes |
| **the frame copies the renderer reads** | **124** | **NO** |
| **actually held** | **759** | the ceiling sees 634 |

`GroundStack::HeapBytes()` sums `Cls_ + Footprints_ + WaterBodies_ + Ways_ + Vectors_`.
`World.WallPlaces`, `WallFacing`, `RoofPlaces` and `RoofFacing` live in
`Engine::State::World` and are not in that sum, so a ceiling of 512 MB is enforced
against 634 of 759 MB. **A ceiling blind to a quarter of the spend is not a ceiling**,
and this is CLAUDE.md's "a measure that cannot see" with a number on it.

And the geometry itself: 377 487 360 bytes over 2 732 059 triangles is **138 bytes per
triangle**, against 108 for the three unshared 8-float corners plus a 3-index run --
about 28 per cent of capacity overhang from geometric growth.

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

## Measured 2026-09-04

Shibuya is the one place that does not preload, and by how little is the point:

```
539509334 bytes held against a ceiling of 536870912 -- OVER BY 2638422 (0.49%)
```

So the case reports UNPREPARED rather than a number, and `make shots` writes no digest for it --
which is why the eight digests quoted in every commit are eight and not nine. Half a percent
decides whether this place can be measured at all.
