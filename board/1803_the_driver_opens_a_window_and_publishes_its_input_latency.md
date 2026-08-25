Type: task
State: open
Parent: 1573
Area: apps, clients
Tags: driver, instrument

# The driver opens a window and publishes its input latency

Every criterion the product states is a measurement at a RUNNING window — the tangent point
inside the frame on a hairpin, an instrument reading trial at +/-5 km/h in <= 0.5 s, camera
stillness at p99 under one pixel of angular change per frame, input to present at p99 <= 50 ms.
None is answerable against an offscreen renderer, and the only `SDL_CreateWindow` in the tree is
the browser's, under `tools/`.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` — the same bindings the scenario declares, no second spelling (board:1862).
- [ ] A key moves the car, and the case publishes input-to-present as p50/p95/p99, named as
      PIPELINE latency rather than photon latency, because a photon measurement needs a
      high-speed camera and the tree has none.
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.
