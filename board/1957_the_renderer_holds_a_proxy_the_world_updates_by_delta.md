Type: feature
State: open
Parent: 1953
Depends: 1574
Area: render

# The renderer holds a proxy the world updates by delta

**Benchmark** — Unreal: `UWorld` feeds `FScene` through primitive-level DELTAS, so the renderer keeps GPU state across frames. RAGE: `fwEntity` on scene-update lists. **Taking Unreal** — an explicit delta is what makes GPU-side state possible at all.

One world and the rest are views (board:1950) says WHO owns the truth. This says how a view stays
current, and the answer is not "read the world each frame".

**Unreal keeps `FScene` beside `UWorld` and a `FPrimitiveSceneProxy` beside each primitive.** The
game side never touches the proxy; it sends a DELTA -- added, removed, transform changed -- and the
render side keeps everything else across frames, GPU buffers included. That separation is what
makes `FGPUScene` possible at all: state that survives the frame can live on the device.

CURRENT rebuilds a draw list from the world per frame, so nothing can be kept device-side and every
frame pays for what did not change. TARGET's own sentence already demands the opposite: the work a
declaration causes is proportional to what it CHANGED.

**MEASURED, and the waste is narrower than the item assumed.** Two paths place subjects:

    Aim / Pose (GltfStudio)   runs when something RESTANDS -- not once a frame
    Live::Carry (the drive)   runs EVERY TICK, rebuilding every row from the body transform

So a declared subject that merely sits there never re-placed, and a negative control that forced
every row to re-send left `ScoreWhatAnUnchangedFrameSends` green. The per-frame rebuild is real
and it is the drive's, which is the path that matters for the frame budget and the one no case
here can stand -- a drive needs a vehicle asset and a route. That proof is owed to `apps/driver`.

What landed: the renderer KEEPS `Placed_` across frames and gains `PlacementRows` and
`MovePlacement`, both callers send only rows whose sixteen doubles differ from what they last
sent, and `placement rows the renderer has been sent` is published so a case can read it. What is
NOT yet true is the shape -- this is a DIFF at the boundary, and Unreal's `FScene` is fed by a
caller that knows what it moved. A diff is where the cost goes away; the delta is where the
knowledge belongs, and only the second one lets the world stop building the table at all.

- [x] the renderer holds scene state across frames and the world reaches it only through a delta
      proof: outshine/door/ScoreWhatAMovingSceneResends
- [x] a frame that changes nothing issues no scene update
      proof: outshine/door/ScoreWhatAnUnchangedFrameSends
- [x] a frame re-sends what MOVED and not what exists: over a drive, 10 batches are drawn and 9
      rows re-sent -- the ground ring is placed once and drawn every frame. The negative control
      re-sends everything and reads 10 of 10.
      proof: outshine/door/ScoreWhatAMovingSceneResends
- [ ] the DELTA rather than the diff: the caller says what it moved instead of the boundary
      comparing sixteen doubles per row. A diff is where the cost goes away; the delta is where
      the knowledge belongs, and only it lets the world stop building the table at all.
      **MEASURED, and it waits on board:1574 rather than on effort.** Unreal's answer is
      `MarkRenderTransformDirty()` -- the GAME side marks, and `FScene` never compares. Here the
      comparison sits in `Live::Carry` (`SentBody_` against a freshly built matrix, 32 doubles a
      tick) and its caller is `Engine::State::Carries`, which builds that matrix from the
      quaternion BEFORE the diff can decide -- work spent ahead of the decision, which is the
      real cost rather than the compare.
      **AND THE TECHNIQUE IS ALREADY IN THE TREE, one subsystem over.** The audio snapshot is a
      double buffer with an atomic counter -- `Sources[2]`, `Ear[2]`, `std::atomic<unsigned>
      Told` in `EngineHeld.h:228-230`: the writer fills the idle half and bumps the counter, the
      reader takes `Told & 1`. That is RCU's shape in miniature (publish by pointer swap, readers
      never block, no lock on the hot path) and it is what the sim -> render handoff wants too.
      Linux's RCU and seqlock are the reference implementations; the second is cheaper for a
      small value read often, which is what a camera pose is.
      But the picture holds ONE subject: `Advancing.cpp` carries `Freestanding.front()` and
      nothing else, so the table this would save building is one row long. A delta over one row
      is a line of code, not an architecture, and writing it now would prove nothing and would be
      rewritten the day the picture holds five. board:1574 is where that changes.

## MEASURED over the whole Khronos six, and the delta has no case on this corpus

`apps/bench --all --steps 30`, subject stage, the `differ` column being how many of the placement
rows the renderer holds carry DISTINCT values:

    scene            placements   differ
    DamagedHelmet             1        1
    BrainStem                59        1
    ABeautifulGame           49        1
    VirtualCity             167        1
    Sponza                  103        1

**Every scene reads 1, BrainStem included -- and BrainStem is ANIMATED.** Its motion is in the
skin palette and the vertices, not in the placement rows: a node transform that a skin drives
never reaches the table this item is about. So the corpus that measures the renderer cannot
measure this predicate at all -- the sim moves ZERO rows a frame in all five.

That is the honest reason the delta cannot be closed from here, and it REPLACES the reason
written above (which said board:1574 had to hold more than one subject first -- 1574 has since
ticked exactly that, so the old blocker is gone and was never the real one).

What would measure it: a scene where the SIM moves many bodies a tick. `apps/driver` is one and is
not the tool right now; the other is a synthetic bench scene of N moving bodies, which is what
`apps/bench` exists to be. Until one of the two stands, a delta built here would be a line of code
defended by nothing.
