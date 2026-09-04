Type: debt
State: open
Area: engine, render
Tags: architecture, performance, determinism, owner

# The simulation and the picture run on two threads, and meet at a snapshot

**Benchmark** -- Unreal: the game thread and the render thread are two threads by construction;
the game thread hands over `FSceneRenderer` state through `ENQUEUE_RENDER_COMMAND`, and the
render thread runs one frame behind. RAGE: `gameSkeleton` on the main thread, the render thread
draws the previous frame's `drawList`, and the two never share live state. **Both agree**, and
CLAUDE.md's fourth invariant restates them: *SIM · VIDEO · AUDIO · IO run independently and what
passes between them is a SNAPSHOT.*

**Cited beside the two**: Filament, which ships on phones, runs its driver on its OWN thread
behind a `CommandStream` -- the main thread records, the driver thread executes a frame behind,
and the two share nothing but the stream. That is the shape for a 2P+4E part: the snapshot is
a command stream, not a copied world.

## Where it stands, measured 2026-09-04

```
  IO      src/host/Fetching.{h,cpp}       its own threads, notified          holds
  tiles   src/world/ground/TilePool       workers for fetch and mesh          holds
  AUDIO   Engine::mix reads Sources[Told] double-buffered, atomic index       holds
  SIM     Engine::advance                 the caller's thread
  VIDEO   Engine::render                  the SAME thread, serially after it
```

`Engine::advance()` runs `Updates()`, `Falls()`, `Grounds()` and then `Draws()` -- which calls
`Live::Advance` -- and `Engine::render()` runs `Live::Draw` on the same thread afterwards. There
is no render thread and no frame behind: the picture is drawn by the thread that simulated it, in
the same call chain. The GPU runs asynchronously underneath, which is what every single-threaded
loop has, and is not the invariant.

The invariant is breached twice today and only one breach has an item. board:2124 holds the
rebuild, which runs inside the frame; this item holds the OTHER half: even with the rebuild gone,
SIM and VIDEO are one thread, so the frame budget is their SUM and the target's two performance
cores carry one of them.

## What will be true

- [ ] `Engine::advance` and `Engine::render` may run on two threads: the simulation writes a
      snapshot -- transforms, the eye, the lights, what entered and left -- and the renderer
      draws the LAST COMPLETE one, a frame behind
- [ ] Nothing the renderer reads is written by the simulation while it reads it, which
      `NoFramePathCallReachesABlock` extends to: no lock on either path, a swap of two buffers at
      a frame boundary
- [ ] Determinism holds across the split: the same declaration renders the same bytes with the
      threads scheduled differently, measured by `make shots` three times under load
- [ ] `make shots` reports sim p99 and draw p99 as two numbers that no longer add
- [ ] Negative control: pin both to one thread and the frame time is their sum again

## The cores, named

2P + 4E: the simulation and the picture take the two performance cores, one each, and the
bakes (board:2122's `Tasks`) take the efficiency cores. `Tasks` sizes itself as hardware minus the
two the frame keeps, which is that sentence as arithmetic.

## The order

After board:2124: a rebuild that still runs inside `advance` would make the snapshot a second of
stale world, and the split would hide it. The rebuild leaves the frame first; then the frame is
split.

## What will show I was wrong

If the snapshot copy costs more than the parallelism buys at this scene size, the copy is the
wrong shape -- a delta rather than a state -- and the item says which before the split lands.
