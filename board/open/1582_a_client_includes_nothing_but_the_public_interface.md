Type: feature
Area: clients
Tags: scope

**A client includes nothing but `include/outshine/`**

**The owner's rule, 2026-08-22:** no client may include anything except `include/outshine/*`.
Today `tools/driver/` includes Journey.h, Ribbon.h, TerrainLoader.h, Body.h, CurlTransport.h --
the internals wholesale -- and the viewer likewise. The public interface (`Outshine.h`,
`Scenario.h`) must grow until the drivers need nothing else, which is the same motion as
board:1573 (declarative game) and board:1581 (the actor chain): what a client needs is a
declaration, a run loop, pixels and instrumentation hooks.

- [ ] the include sets for `tools/` in `test/run.sh` and the `Makefile` shrink to
      `-Iinclude` (+ the host transport seam), so a breach cannot compile
- [ ] every capability the drivers use today reaches them through `include/outshine/` -- laying
      a route, riding, reading the ground, screenshots, the stills instrumentation
- [ ] a claims case proves it: no `#include` outside `include/outshine/` in any client source

## Comments

Enforced the build way, like the engine's own layering: when the include set cannot express the
breach, the rule needs no reviewer.

---

Groomed with the round-3 finding: the parity law's C++ door (Store/Register/Column, Assembly)
lives under src/scene + src/clients, so the moment this item's build enforcement lands, the
assembly API has no public entrance. The plan, aligned with the SOLL door: the scene brick's
HEADERS move under include/outshine/ (the precedent is Scenario.h, which lives there entire),
Engine gains Scene() returning the store, and the drive's fold (board:1581) consumes that same
door -- so enforcement, entrance and fold land as one move, not three patches.
