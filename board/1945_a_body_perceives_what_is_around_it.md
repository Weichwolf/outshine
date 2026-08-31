Type: feature
State: active
Progress: perception
Area: sim
Tags: benchmark, target

# A controller perceives what is around it through bounded spatial queries

**Benchmark** — Unreal: perception is a component making bounded queries against the scene. RAGE: the same, per task. **Both agree** — a controller perceives through bounded spatial queries and never by reading the world directly.

Both benchmarks have this and outshine has NONE of it. Unreal: overlap and sweep queries against
a scene structure, plus AI perception components. RAGE: `phBound` against a broadphase. CLAUDE.md
names the seam already -- a controller PERCEIVES through *bounded spatial queries: bounds ·
ground · sight* -- and only the ground half exists.

- [x] the ground half: a bounded query answers what a point stands on and what it grips with
      proof: outshine/physics/ScoreWhatAWheelFindsOffTheMadeSurface
- [ ] a body's occupied volume is queryable by another body, bounded, with no search over all
      bodies (board:1925)
- [ ] `Underfoot` composes it: a wheel over a second body's roof stands on the roof (board:1925)
- [ ] a mind asks what is AHEAD within a distance and gets bodies back, so following and
      yielding are expressible (board:1925)
- [ ] a sight query answers what is visible from a point, so a mind can be blocked by what
      stands between
- [ ] presence is a rung: a measurement materialises a body from a field (board:1597)
