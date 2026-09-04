Type: feature
State: open
Area: actor, engine, world
Tags: architecture, physics, owner
Supersedes: 2118

# A body meets the ground and a wall, and a joint holds

**Benchmark** -- RAGE: `phInst` carries a `phBound` -- a SEPARATE, coarse collision body: the
terrain is a heightfield bound, a building is a handful of extruded convex prisms -- and
`phSimulator` resolves contacts and constraints each step. Unreal: `FBodyInstance` over Chaos,
`UBodySetup`'s simplified convex elements for collision, the landscape as a heightfield collider,
a constraint solver for joints. **Both agree**: a physics engine is contact plus constraint over a
collision representation that is NOT the render mesh, and the render mesh is never what a body
touches.

**Cited beside the two, and it decides the solver**: Jolt Physics (MIT, readable, ships in
Horizon Forbidden West) is DETERMINISTIC across platforms by design -- a fixed solver order, no
floating-point atomics, a broad phase rebuilt in a declared order -- which is the fourth
invariant's demand and what neither Chaos nor RAGE's `phSimulator` promise. Its contact solver is
sequential impulses with warm starting; XPBD (Muller 2016) and TGS are the two order-stable
alternatives. Taken: Jolt's SHAPE -- body, shape, constraint, a fixed-order islands solver --
over a heightfield bound for the ground (board:2115's height texture, read on the CPU) and
convex prisms for footprints.

## Where it stands, measured 2026-09-04

CLAUDE.md's first sentence defines the engine as an INTERACTIVE PHYSICS SIMULATION. The tree
holds:

```
  src/actor/body/Rigid.{h,cpp}       150 lines   a rigid body, a wrench, a semi-implicit step
  src/actor/body/Prismatic.{h,cpp}    75 lines   a prismatic joint's force -- NO CALLER
  Engine::State::Falls()                          gravity on every free body, every step
```

That is everything. No contact, no collision query, no constraint solver, no joint that holds two
bodies. A body declared by a scenario falls through the ground it was placed on. The driver that
once stood on this was removed (board:2117) and the wheel it drove is being taken out of the door
(board:2118); what is left is a step integrator and nothing for it to hit.

The collision representation already exists and is cheap: the ground is a height field the engine
owns (`sampleHeight`, `TriangleBvh::Under`), and every building carries a `Footprint` -- a ring
plus base, height and seat, which IS RAGE's extruded prism (board:2100 measured it at a fraction
of the render geometry).

## What will be true

- [ ] A body meets the GROUND: a contact against the height field, resolved by the step, so a
      declared body placed above the terrain comes to rest on it and a case measures its height
      settling to the sample within a bound
- [ ] A body meets a WALL: a contact against a footprint prism, so a case that drives a body into
      a building reports the contact -- and the case is written to FAIL against a bounding box
      alone
- [ ] A JOINT holds: `Prismatic` gets a caller and a case, or it goes
- [ ] The collision representation is named and separate from the render mesh: heightfield and
      prisms, never a wall triangle
- [ ] The step allocates nothing and reaches no block, which `NoFramePathCallReachesABlock`
      already refuses on the physics seed and is RED today
- [ ] Negative control: remove the ground contact and the settling case goes RED

## What will show I was wrong

A body at rest on the ground with a measured height that drifts. If the integrator cannot hold a
resting contact without jitter, the step is the wrong integrator for contact and the item says
so before a solver is written on top of it.

## Not in this item

Vehicles, tyres, wheels: a scenario assembles those from body, joint, drive and contact, and the
door's tyre model is board:2118's to remove first.
