Type: feature
State: open
Area: core, world
Tags: scope, actor-chain
Supersedes: 1516, 1594

# The engine reflects the causal chain of an actor

**Benchmark** — Unreal: `AController` possesses a pawn and the causal chain is controller -> input -> movement component -> physics. RAGE: `CTask` trees drive a ped or vehicle. **Taking both** — Unreal for the seam (one interface serves a mind and a player) and RAGE for what happens behind it (a task tree decomposes the way the act does).

A car is GEOMETRY; it has FUNCTIONS; PHYSICS hangs off the functions; an INTELLIGENCE acts on
the functions; the intelligence can SEE in the world and can use NAVIGATION.

```
body (glTF parts) -> actuators (steer, drive, brake, lamps, walk, open) -> physics (forces at the contacts)
                                  ^
                    controller ---+--- perceives (bounded spatial queries)
                                  +--- asks (pathfinding: walk, drive, fly, rail)
```

The chain holds for EVERYTHING that moves, with or without a mind: a parked car is a body whose
seam nobody possesses, a door is a body with one actuator, and an NPC differs from the player
only in who possesses the seam.

The pilot below it is built and general — resection lives once, the base demands a CURVATURE in
1/m, and four modes convert it and derive their own limits (`walk` omega = v*kappa, `drive`
delta = atan(L*kappa), `fly` phi = atan(v^2*kappa/g), `rail` converts the lateral channel into
NOTHING and publishes the unbalanced lateral acceleration instead, which is the case that proves
the shape).

## What will be true

- [ ] A vehicle is geometry plus actuators; the physics binds to the ACTUATORS and never to the
      mind, and a mind acts only on the same seam a player's bindings actuate.
- [ ] **A demand becomes FORCES and never a pose** — today the modes publish a steering angle
      and a turn rate, and something downstream places the body.
- [ ] The mode a scenario declares reaches the pilot, so a kind carries how it moves; a fifth
      mode swims and the water's medium decides its limit.
- [ ] Perception is bounded spatial QUERY over bounds, ground and sight — declared channels
      (sight cone, hearing radius), published, never privileged state.
- [ ] Navigation is ONE service with modes, handed as a TOOL: two coordinates in, corridor out.
- [ ] Crowds and fauna are bodies with minds cast from the same store, and an external
      intelligence possesses a seam through the same `DrivenBy` relation the player uses.
