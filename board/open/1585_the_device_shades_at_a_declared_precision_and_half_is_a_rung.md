Type: issue
Area: render
Tags: perf

**The device shades at a declared precision, and half is a rung of the same ladder the twin already climbs**

**No shader in this engine computes in `half`.** `grep -rn '\bhalf' src/render` finds nothing:
every MSL kernel — the subject's `shadeRow` with its 16-light loop and BVH shadow ray
(`SubjectDraw.cpp:422-533`), the BRDF/sheen/iridescence lobes, tonemap, temporal resolve, sky —
carries `float` throughout, and the uniform structs are all-float rows
(`SubjectDraw.cpp:132-160`).

**The plan's precision dial does not reach arithmetic.** `ScenePrecision { Half, Float }` exists
(`RenderPlan.h:14`) and `render.precision` selects it — but compiling the plan only widens
attachment FORMATS to RGBA32F (`RenderPlan.cpp:183-190`). There is no path by which a plan makes
a kernel compute narrower or wider; "Half" names the storage default, never the ALU.

**On the declared target this is the fast currency left on the table.** The platform is one
device, an A18 Pro at 720p60 on five GPU cores (`CLAUDE.md`), and Apple's shader cores run
`half` at twice the float ALU rate with half the register pressure — register pressure is what
sets occupancy, and occupancy is what hides the texture latency the fragment-bound stages
(subject shading, tonemap, resolve) already pay. Every serious mobile/console renderer keys
precision per kernel; MSL was built for exactly this mix.

**The engine already owns the discipline this needs.** The precision ladder today is double
(C++ twin, the reference) → float (device), adjudicated by the picture bound at its 0.005 px
floor. A `half` rung is the SAME ladder extended one step down, gated the same way — per kernel
family, each demotion accepted only when the bound holds, published as criteria beside the
cases. Not a blanket conversion: position/depth reconstruction, velocity, the BVH traversal and
large-UV derivatives stay float on the well-known overflow/precision grounds (half's max is
65504 and its ulp at 1.0 is ~5e-4).

## Done when

- [ ] shading and post kernels carry a declared arithmetic precision per family, and the plan's
      `render.precision` (or a sibling dial) selects it — the dial stops being storage-only
- [ ] each demotion is adjudicated by the picture bound against the oracle suites, results
      published beside the cases
- [ ] frame p50/p95/p99 over the windowed drive, before and after, ride with the item

Ties to board:1580: if the physics kernels move to one shared source, the precision becomes a
parameter of that source rather than a third hand-kept copy.
