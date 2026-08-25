Type: feature
State: open
Area: scenario
Tags: perf, instrument

**The scenario suite reports a frame-time distribution over a moving camera**

`test/run.sh scenario` runs declared runs and answers with p50, p95 and p99 of frame time, whether two
runs of one declaration produced the same pictures, and what residency and memory did across a long one.
**It is the fourth constraint becoming a measurement**: *720p60 on this device* stops being a sentence
this repository quotes and starts being a distribution it publishes.

## Its first members are the corpus already on disk, and that is the whole point

`board:1209` asks which film the suite should carry and recommends splitting a `.blend` by object,
packaged by shot. **That question is about the suite's most demanding member and it does not gate its
existence.** 148 cases are prepared, and `Clients::Live` already stands one up, advances it and takes its
body with it -- so a camera moving over `ABeautifulGame` is a legitimate scenario run today, at no fetch
cost and no export loss.

**AN INSTRUMENT WITH MEMBERS BEATS AN INSTRUMENT WAITING FOR A SUBJECT.** A film exercises the compositor
at a scale nothing here reaches; a corpus case exercises the *instrument*, and until the instrument
exists a film would be a subject with no ruler. Build the ruler, then bring the film to it.

## What must be true

- [x] **The suite exists and has two members**, `test/scenario/`, declared in `test/run.sh` like every
  other layer
- [ ] **A run is DECLARED, not written in C++** -- a camera path, a frame count, a rate, and which
  scenario stands under it. `src/scenario/Animation.h` already reads channel-and-sampler curves against
  our own properties, so the path is a declaration this tree can already parse
- [x] **The verdict is a distribution and never a mean**, published with its population, its domain and
  the device it was taken on
- [x] **Determinism is two runs of one declaration compared picture by picture**, and a difference is a
  failure naming the first frame that differed
- [x] **Memory is read across a long run**, 600 frames, and the claim is a declared ceiling with its
  own derivation -- the term it has to clear was measured, not supposed
- [ ] **RESIDENCY IS NOT MEASURED AND HAS NO INSTRUMENT.** `Renderer` publishes draw and batch counts
  and no byte accounting at all, so what the DEVICE holds is unanswerable here. It is named rather
  than approximated, because a figure taken from the process's memory and called device residency
  would be the exact defect an instrument's domain paragraph exists to prevent
- [ ] **A leak is judged by EQUALITY once `board:1462` lands**, because suspend and quick resume put the
  horizon at 21 600 000 frames and no run this suite can afford is statistical enough to reach it
- [x] **NO SANITISER IS IN THE PATH.** A duration measured through a bounds checker is not the shipping
  frame, and `CLAUDE.md` names this for the frame suite already
- [ ] **A run that misses the floor is RED and says by how much**, because a frame budget nobody can fail
  is a quotation

## What its first two members measured, and both found engine defects the corpus could not

[MEASURED] `ABeautifulGame`, 1280x720, 80 frames after a declared warmup, serialised on the device.

| | before the round | after |
|---|---|---|
| serialised frame p50 | 10.97 ms | **6.21 ms** |
| serialised frame p99 | **23.24 ms** -- missing the budget | **7.65 ms**, 46 % of it |
| the engine's own side p99 | 4.1179 ms | **0.3113 ms** |
| samples of the frame carrying the subject | 258 of 102 480 | **9213** |
| two runs of one declaration, at one frame | 0 of 102 480 samples differ | unchanged |

**`board:1459`** -- a transmissive pass erased the opaque scene, because `SceneTransmissive` cleared to
alpha one and the composite reads `1 - front.a`. A chess set was sixteen pawn tops in the dark.

**`board:1460`** -- the near-plane precondition walked 934 309 vertices on every `Aim`, so **turning the
camera cost sixteen times the whole of the rest of the frame path**, and the memory traffic cost the
device three times again what the arithmetic cost the CPU.

**NEITHER WAS REACHABLE FROM THE OTHER SUITES.** The render suites draw one frame per pose, so a
per-frame cost disappears into a case total; every corpus case declares zero transmission bounces, so the
composite never ran. *This is the argument for the fourth constraint stated as two measurements rather
than as a plan.*

## What the round learned about the instrument itself

**A SERIALISED FRAME IS AN UPPER BOUND AND THE CLAIM IS MADE IN ONE DIRECTION ONLY.** With no two frames
in flight, CPU and GPU never overlap, so the figure is a sum of terms a shipping frame runs at once:
holding the budget serialised DECIDES that it holds, and missing it decides nothing. **A pipelined run
with two frames in flight is the measurement that would judge a failure, and this suite does not take it
yet** -- that is the largest thing still open here.

**THE FRAMES THAT PAY FOR THE STAND-UP ARE NOT FRAMES OF THE RUN**, and the exclusion is declared rather
than absorbed into a percentile. [MEASURED] the largest of them is 659 ms against a p50 of 7.8; a run
that dropped its worst frames silently would be quoting a distribution it had cut.

## What this feature may NOT do

**It may not borrow the render suite's verdict shape.** A still at one time decides *wrong pixels*; this
decides *the floor broke, the run was not deterministic, memory grew*. A suite reporting the other's
number would be reporting a figure that does not decide it.
