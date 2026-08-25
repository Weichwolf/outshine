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
`(action id, kind, value)` without allocating (src/engine/Engine.cpp:419-420) and hands the
declared action's NAME to `Host::Calls` (:428), where the client decides what it means. The
engine offers `Takes(view)` and `Views()` (include/Outshine.h:48-49) so a client can act on a
view without the engine knowing what for, and `apps/viewer` answers `next-view` through them.

What is left is the other half. `apps/driver` offers NO host at all, so
`if (S_->Offered == nullptr) { return false; }` (src/engine/Engine.cpp:433) drops every key
before it is translated, and the four bindings `f31.scenario` declares -- `throttle`, `brake`,
`steer-left`, `steer-right` -- reach nothing. There is also no window to press a key in: the
driver runs headless.

**AND THE SEAM HAS LOST ITS RELEASE EDGE.** `if (S_->Pumping && event.type ==
SDL_EVENT_KEY_DOWN)` (src/engine/Engine.cpp:431) filters to the press alone; the door used to
take `KEY_DOWN || KEY_UP`. `InputPump::Translate` still answers a release with value `0.0f`
(src/engine/InputPump.cpp:87-88) and nothing ever calls it with a release, so that branch is
dead code and a throttle pressed once is a throttle held for ever. A button is an edge PAIR or
it is a latch, and no case in the tree presses a key and lets it go.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` — the same bindings the scenario declares, no second spelling (board:1862).
- [x] The engine names no action of its own. The pump translates and `Host::Calls` carries the
      declared name to the client (src/engine/Engine.cpp:428).
- [ ] A key moves the car. `throttle`, `brake`, `steer-left` and `steer-right` reach the pilot's
      seat as a driver INPUT that overrides the plan, through a host `apps/driver` offers.
- [ ] The case publishes input-to-present as p50/p95/p99, named as PIPELINE latency rather than
      photon latency, because a photon measurement needs a high-speed camera and the tree has
      none.
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] A key that is RELEASED reaches the client with value 0. Proving case: one press and one
      release through `Engine::Handles` produce two `Host::Calls` for the one action, 1 then 0.
- [ ] Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.
