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

**MEASURED 2026-08-25 at d5a562cd: a key reaches an action and NO key moves the car.** The pump
is wired — `Engine::Handles` translates an SDL key event to at most two (action id, kind, value)
without allocating (src/clients/Engine.cpp:405-412) — and then the id is un-interned back to a
`std::string` and handed to

```cpp
bool Engine::Acts(const std::string &named) {
  if (named != "next-view" || !S_->Views || S_->Views->Count() < 2) { return false; }
```

(src/clients/Engine.cpp:779). `apps/driver/src/f31.scenario` declares five bindings —
`throttle`, `brake`, `steer-left`, `steer-right`, `next-view` — and exactly one of them has an
effect, hard-wired in the ENGINE by its name. Four translate to an id and are dropped.

Two defects in one line: a content-level action named in engine code (*content = data, engine =
verbs*), and a string compare on the input path after the catalogue already interned the id
(*values over strings*). The interning is defeated by its own caller.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` — the same bindings the scenario declares, no second spelling (board:1862).
- [ ] A key moves the car. `throttle`, `brake`, `steer-left` and `steer-right` reach the pilot's
      seat as a driver INPUT that overrides the plan, and `Engine::Acts` dispatches on the
      interned action ID, never on a spelling.
- [ ] The case publishes input-to-present as p50/p95/p99, named as PIPELINE latency rather than
      photon latency, because a photon measurement needs a high-speed camera and the tree has
      none.
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.
