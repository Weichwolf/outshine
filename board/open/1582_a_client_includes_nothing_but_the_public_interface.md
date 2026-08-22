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
