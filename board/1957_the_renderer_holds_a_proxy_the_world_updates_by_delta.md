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

- [ ] the renderer holds scene state across frames and the world reaches it only through a delta
- [ ] a frame that changes nothing issues no scene update, proven by a case counting the deltas
- [ ] moving one instance costs one delta and not a rebuild, proven by the same case
