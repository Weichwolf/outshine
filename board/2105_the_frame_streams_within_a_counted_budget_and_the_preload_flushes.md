Type: bug
State: open
Area: engine, world
Tags: measured, performance, determinism, owner
Supersedes: 2109
Depends: 2122

# The frame streams within a COUNTED budget, and every timed frame is digested

**Benchmark** -- RAGE: `CStreaming::Update()` services a BOUNDED number of requests per frame;
`LoadAllRequestedObjects()` is the separate blocking form used on entering a scene. Unreal:
level streaming is asynchronous with a per-frame budget, and `FlushLevelStreaming(Full)` is the
blocking form the automation harness calls before every screenshot. **Both agree there are TWO
modes and they are different functions.** They differ in the frame bound -- Unreal in
milliseconds, RAGE in requests. **Taken: RAGE.** A time budget reads a clock, and this tree's
determinism is compulsory.

## Where it stands, measured 2026-09-04

| | | |
|---|---|---|
| preload refuses loudly | DONE | `Engine.cpp:265-283` on `Overflowing()` and on patience |
| an unpreloaded place is not measured | DONE | `PlaceCamera.cpp:305-311` |
| ingest waits for the whole ring | DONE | `GroundStack.cpp:113` returns while any tile is pending |
| the frame's budget is counted | NO | `GroundStack::Restand` loops `kVectorTiles` (49) passes, one tile per field per pass, from the frame path (`Advancing.cpp:196`) |
| the ceiling refuses | NO | `GroundStack.cpp:115-122` `break`s silently and sets `Overflowing_`; only preload reads it |
| the ceiling can see | NO | `HeapBytes()` misses the frame copies (board:2104) |
| Shibuya | refuses | 856.7 MB against 512 MB with the ceiling lifted -- board:2122 |
| eight places, three runs | steady | one still digest each; the 120 timed frames are unhashed |

The wall clock is gone and the medium-stage `memcmp` is gone (board:2092 holds the record). What
remains of the wander is the ORDER tiles are meshed in: `TileWatermark::Ask` sorts the candidate
set by `(distance², zoom, x, y)`, and `Restand` waits for the ring, so the sinks consume a
declared order today; `BuildingField.cpp:363` still reads `field.Tiles()` directly and is the one
site left to check.

## The solution

`Restand(at, TileBudget)`: the frame passes a declared count (one tile), the preload passes the
ring. The ceiling REFUSES -- returns `unexpected` with the bytes and the bound -- instead of
breaking out of a loop nobody told. With board:2122 the frame's form only PLACES pieces a worker
finished, so its budget is a count of placements and the mesh cost is not in the frame at all.

And the instrument closes the gap board:2109 named: `make shots` writes one digest over the 120
timed frames beside the still's, so a nondeterministic `advance()` on a settled world is visible
on the day, and the walk digest is ready for the day the camera moves again (board:2092).

## What will be true

- [ ] `Restand` takes the budget it may spend, counted in tiles; the frame passes one
- [ ] The 512 MB ceiling refuses with its reason and reads the tagged heap, frame copies included
- [ ] Every timed frame is digested; `shots --all` three times agrees on the still AND the walk
      digest for all nine places, Shibuya included
- [ ] Negative control: set the counted budget so low the walk never catches up, and the walk
      digest moves

## Ruled out, measured

- the candidate set (`PendingTiles() > 0` early return) -- in, right by the invariant, and
  CentralPark still drew two pictures six runs later; the measures could not see the
  difference (five heap numbers of 340), so the next step was a measurement, not a repair
- three runs are not enough to call a place deterministic: Kaiserberg drew a second digest on
  the twenty-eighth run
