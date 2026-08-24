Type: task
Parent: 1573
Area: apps, clients
Tags: driver, instrument

# The driver opens a window and publishes its input latency

`board:1573`'s M0, and it is first because every criterion the requirement states is a
measurement at a running window:

- the tangent point inside the frame on `hairpin` routes
- the instrument reading trial, +/-5 km/h in <= 0.5 s
- camera stillness, p99 under one pixel of angular change per frame
- input to present, p99 <= 50 ms

None of them is answerable against an offscreen renderer, and today `apps/driver/window/`
renders offscreen: the only `SDL_CreateWindow` in the tree is the browser's, under `tools/`.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` -- the same bindings the scenario declares, no second spelling.
- [ ] A key moves the car, and the case publishes **input to present** as p50/p95/p99, named as
      PIPELINE latency rather than photon latency, because a photon measurement needs a
      high-speed camera and the tree has none (board:1491).
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] Proving test: the case itself, running inside the runner's budget (board:1778).
      Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.
