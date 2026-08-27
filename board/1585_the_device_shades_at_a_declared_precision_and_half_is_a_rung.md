Type: issue
State: open
Area: render
Tags: perf

# The device shades at a declared precision, and half is a rung of the ladder the twin already climbs

**Benchmark** — Unreal: `EShaderPlatform` and feature levels DECLARE what a device shades at; half precision is a declared rung (`min16float`), not something detected at runtime. RAGE: per-platform shader variants. **Taking Unreal** — a declared capability can be refused; a detected one can only be worked around.

`ScenePrecision { Half, Float }` is declared (src/render/plan/RenderPlan.h) and `render.precision`
selects it — and compiling the plan only widens attachment FORMATS. No path makes a kernel
compute narrower: `grep -rn '\bhalf' src/render` finds nothing. A declaration the engine accepts
and does not execute, which is board:1862's defect in the render column.

On the declared target it is the fast currency left on the table: Apple's shader cores run
`half` at twice the float ALU rate with half the register pressure, and register pressure sets
occupancy, which is what hides the texture latency the fragment-bound stages already pay.

## What will be true

- [ ] The plan's precision reaches ARITHMETIC, per kernel, or the dial is deleted.
- [ ] Each kernel that narrows is gated by the picture bound at its 0.005 px floor — the same
      ladder as double (reference) -> float (device), one step further down.
- [ ] The frame cost is published before and after, p50/p95/p99.
