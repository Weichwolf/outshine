Type: feature
State: active
Area: scenario, test
Tags: perf, instrument
Supersedes: 1578, 1593

# The scenario suite reports a frame-time distribution over a moving camera

**Benchmark** — Unreal: `stat unit` and the automation suite report frame time distributions, not means. RAGE: telemetry per frame. **Both agree** — a mean hides the frame that missed, and p50/p95/p99 over a moving camera is what a budget is judged on.

*720p60 on this device* stops being a sentence this repository quotes and starts being a
distribution it publishes: p50, p95 and p99 of frame time over a moving camera, whether two runs
of one declaration produced the same pictures, and what residency and memory did across a long
one. The suite exists and its verdict is a distribution with its population, its domain and the
device named; determinism is two runs compared picture by picture; memory is read across 600
frames against a declared ceiling; no sanitiser is in the path.

## THE CAMERA DOES NOT MOVE, and every frame time this tree quotes is a still one

`src/client/PlaceCamera.cpp` sets no motion of any kind -- `grep OrbitDegPerFrame` in it finds
nothing, no view is switched, and the seven places each declare ONE view. `Take()` writes the
picture and then times 120 `advance` + `render` calls **of the same still frame**.

CLAUDE.md's own aim reads *"holding 720p60, measured as p50/p95/p99 over a moving camera and never
as a mean"*. The distribution is there and the mean is correctly refused, but the moving camera is
not: every p50 this repository has ever printed -- 1.74 ms at Heidelberg, 9.80 at Shibuya, 2.29 at
Venice -- is a stationary number, and a stationary camera is the case a renderer is BEST at. It
never rebuilds the world, never streams a tile it has not got, never crosses a boundary.

**And it blocks board:2059.** That item's last box is *a tile already held is DROPPED when the
camera walks away from it*, which cannot be exercised, let alone measured, by a camera that never
walks anywhere. Writing eviction against this instrument would be writing it blind.

`Engine::setView(std::string_view)` already selects among declared views, so a path is a scenario
that declares several and a client that steps through them -- declared rather than coded, and the
picture stays where it is because it is written BEFORE the timed loop.

## THE CAMERA WALKS NOW, and the tail it was hiding is real

The path is DECLARED: twenty-four views along the place's own bearing, 25 m apart, stepped through
with `Engine::setView`. The picture is untouched -- it is written before the timed loop and always
from `station`, and every digest held: Heidelberg 0da91522, Shibuya 732bd2de.

Six places, 120 frames each, 16.67 ms budget:

| place | p50 | p95 | p99 | over |
|---|---|---|---|---|
| CentralPark | 7.23 | 8.42 | 9.73 | 0 |
| Heidelberg | 3.32 | 4.24 | **11.84** | **1** |
| OldTown | 2.88 | 3.65 | **13.56** | **1** |
| Shibuya | 4.11 | 5.12 | 8.20 | **1** |
| Jura | 4.01 | 4.58 | 4.66 | 0 |
| Venice | 3.22 | 4.56 | **9.41** | **1** |

Standing still, Heidelberg's p99 was around 7 ms and nothing missed. **Walking, the p99 is 11.84
and one frame in 120 misses the budget outright.** Four of six places miss at least one. The p50
barely moves and the tail is where the truth was: a still camera never rebuilds the world and
never streams a tile it has not got, which is why it flattered every number this repository has
printed.

## AND THE WORLD FOLLOWED THE SCENARIO WHILE THE PATCHWORK FOLLOWED THE EYE

Two holders of one question. `Picturing.cpp:385-398` already laid the patchwork around the
CAMERA's position, computed from the eye against the anchor. `Advancing.cpp:170-174` restood the
ground stack at `Session.Declared.Ground.Origin` -- the SCENARIO's. While the camera stood still
the two agreed; the moment it walked they did not, and CLAUDE.md names exactly this: *the second
holder is what makes two subsystems disagree about the same place*.

The computation is now `Engine::State::WhereTheEyeStands` and both read it. What that exposes, at
120 frames a place against a 16.67 ms budget:

| place | p50 | p95 | p99 | over |
|---|---|---|---|---|
| CentralPark | 7.23 | 7.58 | 18.51 | 2 |
| Heidelberg | 3.70 | **26.12** | **2493.00** | 8 |
| OldTown | 3.01 | 6.00 | **1831.54** | 2 |
| Shibuya | 4.02 | 7.50 | **1413.01** | 3 |
| Jura | 4.06 | 4.79 | 14.29 | 1 |
| Venice | 3.30 | 4.19 | **920.13** | 2 |

**A frame that crosses a tile boundary costs up to 2.5 SECONDS.** Four of six places have a p99 in
whole seconds. The p50 does not move at all, which is why a mean or a median would have shown
nothing -- CLAUDE.md refuses both for exactly this reason and was right.

**It is the REBUILD, not the streaming.** The world's fields grow from 180.8 MB to 182.3 MB over
the whole 600 m walk and `world: times a round stopped at that ceiling` stays 0 -- nothing is being
fetched. The seconds are spent re-laying a world that was already resident, serially, which is
board:2056's subject measured on its own case for the first time.

## THE INSTRUMENT DID NOT REPEAT BECAUSE I HAD PUT A GPU STALL IN EVERY FRAME

`ReadPyramid` -- added the same night to prove the depth pyramid was not reading zero -- sat in
`Engine::render()`, ungated. It reads 640x360 floats back from the device every frame, and a
readback is a pipeline stall.

    Heidelberg p50   with it 4.0 ms     without 2.58, 2.58, 2.59

**1.4 ms a frame, a third of the p50**, and the variance with it. Gated on declared audits:

| place | with the stall | without |
|---|---|---|
| CentralPark | 7.33 / 18.94 / 27.95, 8 over | **5.87 / 6.31 / 8.30, 1 over** |
| Heidelberg | 3.79 / 12.93 / 478.25 | 2.56 / 11.74 / 467.19 |
| OldTown | 3.14 / 5.97 / 290.46 | 2.10 / 3.16 / 271.77 |
| Shibuya | 4.01 / 7.53 / 755.15 | 3.41 / 6.87 / 705.38 |
| Venice | 3.30 / 4.44 / 475.92 | 2.44 / 3.18 / 435.19 |
| Jura | 4.09 / 4.63 / 4.99 | 3.19 / 3.42 / 10.45 |

**So CentralPark's unrepeatability was mine.** It carries the most geometry, so it carried the
stall hardest -- 8.30 against 27.95, one frame over budget against eight. The "warm-up" written
here two hours ago was wrong as well, and both readings are corrected rather than left standing.

The readback itself was RIGHT to write: without it nobody would have noticed that the pyramid read
0.000 under a perfectly stable digest. What was wrong was leaving a measuring instrument inside the
thing it measures.

## AND THE GATE STILL RECORDS ONE RUN

Three consecutive runs, same binary, same tree:

    Heidelberg    p99 453.01  479.87  468.73     over budget  5,  5,  5
    CentralPark   p99  29.10   22.64   17.64     over budget 50, 15,  3

Heidelberg repeats to 3 per cent. CentralPark falls monotonically and its over-budget count goes
fifty, fifteen, three -- warm-up rather than the scene, and it is the place carrying 3.9 M building
triangles. A single run is what a case records, so for that one place the row is whatever state the
machine happened to be in.

**A distribution that does not repeat is not a distribution.** This item owes a repeatability
statement beside the numbers: how many runs, and which places the figure is good for. Until then a
p99 from CentralPark cannot be compared with another p99 from CentralPark, which is exactly the
mistake made once already in board:2056 and corrected there.

## AND THE VARIATION WAS READ FROM THE WRONG FRAME, the moment the camera moved

`shot.VariationAlongRows` was read AFTER the timed loop, from whatever was in the framebuffer.
While the camera stood still that was the same frame as the saved picture; the moment it walked it
described the LAST frame of the path while sitting beside the FIRST frame's digest. It fell from
2.327 to 2.180 and that is how it was caught. Read before the walk now.

Ninth number this session that described something other than its name -- and the first one built
rather than found.

**The blocker this item carried is gone, and it left an instrument nobody reads.** At
35829990 `Renderer::DrawsInto` takes the first present mode the device offers that does not
queue -- MAILBOX, then IMMEDIATE, then VSYNC -- and refuses by name if it takes none
(src/render/SceneRenderer.cpp:968-985). `[[nodiscard]] bool Queued() const` (src/render/SceneRenderer.h:81)
says which it got.

Two things are wrong with it and both are this item's:

- **No caller.** `grep -rn 'Queued()' src include apps test` finds the definition and nothing
  else. A distribution that does not print the mode it was taken under is the state this item
  was opened to end.
- **It answers for a swapchain that does not exist.** `SDL_GPUPresentMode Presenting_ =
  SDL_GPU_PRESENTMODE_VSYNC;` (SceneRenderer.h:235) is the default, and the offscreen path never
  assigns it, so an offscreen renderer -- every headless run in the tree -- reports `Queued() ==
  true`. A number that answers where it was never measured is worse than no number.

## What will be true

- [ ] The present mode is DECLARED beside `fps` rather than chosen by a preference list in the
      renderer, and the distribution names the mode it was taken under. `Queued()` has a reader
      or it is deleted, and it refuses to answer where nothing presents.

- [ ] **A run is DECLARED, not written in C++** — a camera path, a frame count, a rate and which
      scenario stands under it. `apps/bench --scene NAME` and `--all` name Khronos's own six from
      the corpus this tree already fetches, which is the first half: WHICH scenario is a switch
      rather than a recompile. The camera path and the rate are still C++.
- [ ] **Residency is measured.** The renderer publishes draw and batch counts and no byte
      accounting at all, so what the DEVICE holds is unanswerable here — named rather than
      approximated, because a figure taken from process memory and called device residency is
      the exact defect a domain paragraph exists to prevent.
- [ ] **Every GPU pass publishes its span to a READER** — a per-pass duration measured on the
      DEVICE and a consumer, so a cost that moves between passes is attributable without a
      profiler. The reader is built first and the probe second.
      **The READER is built and half the probe with it, in that order.** `Renderer::Spent(stage)`
      carries a per-stage `{TookMs, Draws, Triangles}` recorded around `EncodeStage` -- the one
      place every stage passes through -- and `apps/bench` reads it and divides. What it measures
      is the CPU's ENCODE span, not the device's execution: Unreal separates the two for a reason
      and `stat unit` shows Game, Draw and GPU as three numbers. So this predicate still owes the
      DEVICE side, which is SDL_GPU timestamp queries around each pass, and the row it lands in
      already exists.
      **And a duration alone was never the point**: every stage reports its WORK beside its time,
      because power is work over time and a millisecond without its population cannot be compared
      between two scenes or two machines. That is CLAUDE.md's own rule about a number carrying
      its population, applied where it had never been applied.
      proof so far: `apps/bench --all` prints a rate per stage per scene.
- [ ] **A run that misses the floor is RED and says by how much**, because a frame budget nobody
      can fail is a quotation.
- [ ] The store-op derivation (`Stored(resource)`, landed) is MEASURED: the frame-time delta over
      a full declared drive against the standing reference population.

**board:1989 hands this item its measurement.** A GPU-driven change wants a frame time before and
after, and this tree measures none anywhere -- `Core::Live::Took*` are BYTE counts, not durations.
So "before and after" has nothing to read and board:1989 closed without it, recording instead the
CPU term it actually removed: the vertex uniform is pushed once per PASS where it was pushed once
per model slot, which is once per PART. `outshine/door/ScoreWhatASecondSubjectDoes` reads
`one subject pushes 1 vertex uniform(s), two push 1` beside `one subject draws 1 batch(es), two
draw 2`. Until this item stands, that is the only shape of performance claim this tree can defend.
