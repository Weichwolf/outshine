Type: bug
State: open
Area: engine, world, render, client
Tags: measured, performance, owner
Supersedes: 2106, 2120
Depends: 2124, 2122

# Every place holds 16.7 ms at p99 with a MOVING camera, and stands in under a second

**Benchmark** -- Unreal holds the frame by taking work OFF the frame path (`FIoDispatcher`, cooked
landscape) and `stat unit` attributes a hitch to the system that caused it; RAGE's streaming
threads sit beside `sysTaskManager` and a replay plays a drive back frame for frame, so a hitch is
a defect and never weather. **Both agree**: nothing that takes seconds runs where a frame runs,
and a frame time is measured while the camera MOVES, because a still camera is the case a
renderer is best at.

## Where it stands, measured 2026-09-04

`make shots --all`, STILL camera, digests bit-identical to `build/shots/reference/`:

| place | p99 ms | over | waited s | streamed s | peak heap MB |
|---|---|---|---|---|---|
| OldTown | 2.79 | 0 | 1.8 | 1.0 | 273 |
| Heidelberg | 3.27 | 0 | 2.4 | 1.4 | 411 |
| Shibuya | -- | -- | REFUSES | 541 530 534 B against 536 870 912 | -- |
| CentralPark | 6.53 | 0 | 5.2 | 3.0 | 965 |
| Venice | 3.79 | 0 | 2.1 | 1.2 | 469 |
| Jura | 3.75 | 0 | 2.1 | 1.3 | 279 |
| ZurichPlan | 5.70 | 0 | 3.6 | 2.0 | 562 |
| Kaiserberg | 4.33 | 0 | 3.2 | 1.8 | 549 |
| Koehlbrand | 4.37 | 0 | 2.7 | 1.4 | 387 |

```
  the rebuild a moving camera trips        ~1132 ms in ONE frame at Kaiserberg (7c98b7d3)
  the road fit, of that                    114 ms OldTown, 201 ms Kaiserberg (was 620)
```

No place stands in under a second: `waited` runs 1.8 to 5.2 s, of which 1.0 to 3.0 s is the
network and the rest is standing the world after the bytes arrive.

The green column is the INSTRUMENT's: `PlaceCamera.cpp` stopped walking the camera because the
walk timed the ring rebuild (board:2124) and called it a frame. The engine is not faster. A
moving camera comes back the day the rebuild is off the frame path, and this item is what that
day is measured against.

## The instrument, and what it still cannot see

`make shots` is the client and the cases score its rows. Three gaps, each a concrete change to
`src/client/PlaceCamera.cpp` and `test/harness/shared/ClientShot.h`:

| gap | today | the change |
|---|---|---|
| no frame-budget oracle | the row carries `P99` and `OverBudget`; no case CHECKs either | `outshine/places` refuses a place whose `OverBudget > 0` once the walk is back, and holds `P99` as a ceiling that may only fall |
| built is not compared with drawn | `cull: indices the subject cull kept` is published (`Advancing.cpp:338`) and not in the row; `TellsWhatCrossed` (`Laying.cpp:2015`) is dead and its body stands inline at `:2527` | put `Handed`, `Kept` beside `Triangles` in the row; refuse when `Kept == 0 && Triangles > 0`; delete the dead function |
| the timed frames are not digested | one digest, of the still before the timing loop | one digest OVER the 120 frames (a hash of the frame hashes), so a nondeterministic `advance()` on a settled world is visible |

## What will be true

- [ ] Every one of the nine places reads `0 of 120 over 16.67` with the camera walking, once
      board:2124 has put the walk back
- [ ] Every one stands in under 1.0 s of `waited`, quoted per place from `shots --rows`
- [ ] The row carries handed, kept and a walk digest; the case refuses on a zero and on a moved
      walk digest
- [ ] `P99` and `waited` are recorded ceilings that may only fall
- [ ] Negative control: put one rebuild back inside a frame and `OverBudget` goes red

## Ruled out, measured, so nobody repeats them

- a wall clock in the ingest (`kStreamBudgetMs`) -- gone; it was A mechanism of the digest
  wander, not THE mechanism (board:2105 holds the rest)
- indeterminate padding under `memcmp` in the medium stages -- gone; every settled comparison
  is `operator== = default`
- the elevation tiles decoding off the planet at CentralPark -- `TerrainGrid::FromTerrariumPng`
  refuses and fills (`HeightIsOnEarth`, median ± 20·MAD); it was real and it was not the empty
  picture
- the reference-line fit at 78 µs a call -- 5.7 µs now; what is left is the golden-section
  radius search (`Alignment.cpp:227-244`), arithmetic and not allocation, and it moves with the
  road derivation (board:2101)

## What will show I was wrong

If the walk returns and p99 is still over budget with the rebuild off the frame, the cost is in
the drawing and this item was pointing at the wrong half.
