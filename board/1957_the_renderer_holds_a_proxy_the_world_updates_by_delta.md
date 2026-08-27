Type: feature
State: open
Parent: 1953
Depends: 1950
Area: render

# The renderer holds a proxy the world updates by delta

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
