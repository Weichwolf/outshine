Type: task
Parent: 1455
Area: scenario
Tags: instrument

**An animated scenario advances, and its picture changes with the pose**

A scenario carrying animation poses itself, rebuilds its geometry and hands it over **because it moved**;
one carrying none hands over nothing after the first frame. Both halves are read from the device, so the
claim is about pixels and not about a counter.

## Why this is a task and not a property somebody assumed

`Clients::Live::Advance` was written in the round that closed the cost defect and **nothing exercises its
animated arm**. `git grep` over `test/viewer/` finds no call of `At()` or `Frames()`; the still arm is
proven by the teardown case and the moving one by nothing at all. **A path that cannot be shown to run is
a path that can stop running silently** -- and this one stops by drawing the same frame forever, which is
the failure a still image cannot show.

## What it measures, and the population is quoted with every number

[MEASURED] `BoxAnimated` and `Box`, 1280x720, 102 480 samples on a 3x3 stride over the whole surface.

| | |
|---|---|
| frames on the grid | **223**, derived from the animation's own length against the declared 60 |
| frame 1 against frame 2 | **1932 of 102 480 samples differ** -- it moves |
| frame 1 against frame 1 one lap later | **0 differ** -- the grid is the document's and a lap is deterministic |
| a still frame against the next | **0 differ** -- the body is still drawn, not merely no longer uploaded |
| an animated advance | p50 **0.3154 ms**, p95 0.4543, p99 **0.6070**, max 0.7659 |
| a still advance | p50 **0.0654 ms**, p95 0.1452, p99 0.2006, max 0.2420 |
| **what a pose costs** | **0.2500 ms p50** -- the one legitimate per-frame upload in this engine, and 3.6 % of the frame budget at p99 |

## What must be true

- [x] **A subject the document declares animation for reports more than one frame**, derived from the
  animation's own length and the declared rate rather than from a number a consumer passed in
- [x] **Two advances of an animated scenario produce two different pictures**, read back and compared over
  a declared population -- and the population is quoted, because a count of differing pixels over a
  neighbourhood nobody named decides nothing
- [x] **The same two advances of a STILL scenario produce the same picture**, which is what says the
  saving is real and not a body that stopped being drawn
- [x] **The pose the engine derives at frame n is the pose the file declares at n/fps**, so the grid is
  the document's and not the runtime's
- [x] **The cost of an animated advance is published beside the still one.** A pose that costs a
  resubmission is the one legitimate per-frame upload in the engine, and what it costs is a number a
  scenario budget will need

## Two defects it found in the round that wrote it, and one of them was mine

**THE FIRST POSE HAD NO PREVIOUS POSE AND THE STUDIO REFUSED IT.** `Live` captured the previous pose
before building the current one, so at frame 0 it differenced a 320-vertex body against an empty one --
*no vertex has a place it moved from*, which is exactly right and is why the studio says it. At the first
pose the previous one is THIS one, because nothing has moved yet; every later frame captures before the
build, which is what makes the pair a motion. **No animated scenario had ever been stood up**, so nothing
had asked.

**THE TEST COMPARED TWO DIFFERENT FRAMES AND CALLED IT DRIFT.** `Open` poses frame 0 and the first
`Advance` renders frame 1, so the picture read first is frame 1's. Compared against frame 0 it reported
**2115 of 102 480 samples differing** and read as an engine defect. It was two frames of an animation,
correctly drawn. *The caveat-first rule is what caught it: the harmless explanation was sought before the
defect was filed, and the harmless explanation was the whole of it.* A lap is counted from the frame that
was read.
