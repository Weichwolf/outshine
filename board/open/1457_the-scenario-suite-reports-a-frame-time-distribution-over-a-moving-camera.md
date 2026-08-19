Type: feature
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

- [ ] **A run is DECLARED, not written in C++** -- a camera path, a frame count, a rate, and which
  scenario stands under it. `src/scenario/Animation.h` already reads channel-and-sampler curves against
  our own properties, so the path is a declaration this tree can already parse
- [ ] **The verdict is a distribution and never a mean**, published with its population, its domain and
  the device it was taken on
- [ ] **Determinism is two runs of one declaration compared picture by picture**, and a difference is a
  failure naming the first frame that differed
- [ ] **Residency and memory are read across a long run**, so a leak is a slope and not an anecdote
- [ ] **NO SANITISER IS IN THE PATH.** A duration measured through a bounds checker is not the shipping
  frame, and `CLAUDE.md` names this for the frame suite already
- [ ] **A run that misses the floor is RED and says by how much**, because a frame budget nobody can fail
  is a quotation

## What this feature may NOT do

**It may not borrow the render suite's verdict shape.** A still at one time decides *wrong pixels*; this
decides *the floor broke, the run was not deterministic, memory grew*. A suite reporting the other's
number would be reporting a figure that does not decide it.
