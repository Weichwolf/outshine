Type: task
Parent: 0056
Area: render
Tags: perf

**The plan derives store-ops and transient lifetimes, the way it already derives load-ops**

`Renderer.cpp:616,630`: every attachment stores unconditionally -- velocity, normal, identity
and depth are written to memory even in plans where nothing reads them again. The plan knows
last-use at compile time; deriving store/discard and transient aliasing from the table is
FrameGraph's defining trick, and the `Touched_` load-op tracking is already the first half of
the mechanism. On a TBDR GPU an unnecessary store is pure bandwidth.

- [x] derived at plan compile: `Stored(resource)` = declared as an output, or read by any held
      stage, or attached in more than one pass, or the surface -- and the renderer sets
      STORE/DONT_CARE from it for colour and depth alike.
      `test/unit/render/plan/APassAttachesEachTargetOnce.cpp` decides both directions: a plain
      subjects plan drops the shading normal and the surface identity (24 B/px of tile
      write-back), and the parity harness DECLARING the normal as an output is enough to keep it
      -- the consumer decides what survives the pass, in the declaration and nowhere else
- [ ] measured: frame-time delta over the full windowed drive against today's reference
      population (1 469 414 frames, p50 1.865 / p95 4.525 / p99 6.115 ms, 2026-08-22) -- rides
      with the next complete window run

---

Parked: the derivation stands and is committed; the measurement box (frame-time delta over a
full windowed drive against the 1 469 414-frame reference) needs a quiet machine and the driver
is at rest until the architecture review runs clean.
