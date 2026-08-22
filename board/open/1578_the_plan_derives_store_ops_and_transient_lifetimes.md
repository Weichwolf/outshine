Type: task
Parent: 0120
Area: render
Tags: perf

**The plan derives store-ops and transient lifetimes, the way it already derives load-ops**

`Renderer.cpp:616,630`: every attachment stores unconditionally -- velocity, normal, identity
and depth are written to memory even in plans where nothing reads them again. The plan knows
last-use at compile time; deriving store/discard and transient aliasing from the table is
FrameGraph's defining trick, and the `Touched_` load-op tracking is already the first half of
the mechanism. On a TBDR GPU an unnecessary store is pure bandwidth.

- [ ] store-op = STORE only when a later pass reads the resource, DONT_CARE otherwise, derived
- [ ] measured: bandwidth or frame-time delta over the windowed drive, population named
