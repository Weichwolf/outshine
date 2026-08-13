Type: feature
Area: generators
Tags: perf

**I.12 Physics — one system for walking, driving, flying, swimming**

- [x] Substitute contact body as a cylinder with radius, height, mass, contact material (`generators/Body.h`)
- [ ] A contact representation with its own rungs, selected by distance to the observer rather than by the draw's screen-space error — a body far enough to be one impostor cell still needs a correct standing surface, and the two criteria are not the same
- [ ] Collision geometry evicted with the tile that owns it, and its bytes in the ledger under their own name
- [x] Occupancy claimed through a sink, so a proposal and a placement are one type
- [ ] **A segment or joint fails under load.** Not aerodynamic fidelity — the bar is that the engine can tell whether a leg breaks. A declared body's joints and segments carry a load limit, the solver reports the load, and exceeding it is a **state change on the body**, not a log line: the same declaration that carries a walking human's knee carries an undercarriage leg and a branch. This is what makes a hard landing, a fall and a collision have consequences without a second system to model them
- [ ] The failure threshold is declared per material and per section, so it is derived from what the body is made of rather than set per body
- [ ] Rigid-body state: position, orientation, linear and angular velocity, inertia tensor
- [ ] Integrator with a fixed timestep and an interpolated render pose
- [ ] Broad phase over the one spatial index, never a second index
- [ ] Narrow phase: sphere, capsule, box, convex hull, triangle soup (Ericson, ch. 4–5)
- [ ] Contact manifold generation and persistent contact caching
- [ ] Contact solver: restitution, friction with a declared material pair table
- [ ] Joints: hinge, ball, slider, fixed, motorised, with limits
- [ ] Force sources as a declared list, so a wheel, a propeller and a muscle are the same kind of thing
- [ ] Medium: air and water with density, and a body that knows which it is in
- [ ] Buoyancy from displaced volume against the core's water level
- [ ] Aerodynamic and hydrodynamic coefficients per body, not a table lookup
- [ ] Character controller: capsule, gravity, step height, slope limit, ground snap
- [ ] Ragdoll transition from a driven body
- [ ] Sleeping and islanding, so a parked world costs nothing
- [ ] Deterministic solver ordering, because pace deciding the result is a bug (principle 7)
- [ ] Terrain collision against the drawn surface, not against a second heightfield
- [ ] Building collision — `Buildings` deliberately claims no occupancy today, because a cylinder cuts a terrace's neighbours
