Type: task
State: open
Parent: 1573
Area: apps, clients
Tags: driver, instrument

# The driver opens a window and publishes its input latency

Every criterion the product states is a measurement at a RUNNING window — the tangent point
inside the frame on a hairpin, an instrument reading trial at +/-5 km/h in <= 0.5 s, camera
stillness at p99 under one pixel of angular change per frame, input to present at p99 <= 50 ms.
None is answerable against an offscreen renderer.

**The engine no longer names content, and no key still moves the car.** At 35829990
`Engine::Acts` is gone: `Engine::Handles` translates an SDL key to at most two
`(action id, kind, value)` without allocating (src/clients/Engine.cpp:419-420) and hands the
declared action's NAME to `Host::Calls` (:428), where the client decides what it means. The
engine offers `Takes(view)` and `Views()` (include/Outshine.h:48-49) so a client can act on a
view without the engine knowing what for, and `apps/viewer` answers `next-view` through them.

What is left is the other half. `apps/driver` offers NO host at all, so
`if (S_->Offered == nullptr) { return false; }` (:422) drops every key before it is translated,
and the four bindings `f31.scenario` declares -- `throttle`, `brake`, `steer-left`,
`steer-right` -- reach nothing. There is also no window to press a key in: the driver runs
headless.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` — the same bindings the scenario declares, no second spelling (board:1862).
- [x] The engine names no action of its own. The pump translates and `Host::Calls` carries the
      declared name to the client (src/clients/Engine.cpp:428).
- [ ] A key moves the car. `throttle`, `brake`, `steer-left` and `steer-right` reach the pilot's
      seat as a driver INPUT that overrides the plan, through a host `apps/driver` offers.
- [ ] The case publishes input-to-present as p50/p95/p99, named as PIPELINE latency rather than
      photon latency, because a photon measurement needs a high-speed camera and the tree has
      none.
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.
