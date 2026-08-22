Type: feature
Area: clients
Tags: scope

**The engine reflects the causal chain of an actor**

**The owner's direction, 2026-08-22, verbatim in substance:** a car is GEOMETRY; it has
FUNCTIONS; PHYSICS hangs off the functions; an INTELLIGENCE acts on the functions; the
intelligence can SEE in the world and can use NAVIGATION. The engine must reflect this causal
chain -- and `Journey` does not: it is a specialised bundle (route + corridor + vehicle + ride
loop) that belongs deep inside `Sim`, not beside it as a second world door.

```
geometry (glTF part) -> functions (steer, drive, brake, lamps, ...) -> physics (contacts, wrench)
                                   ^
                     intelligence --+-- sees (queries the world)
                                    +-- navigates (asks the planner)
```

- [ ] a VEHICLE is declared as geometry + functions; the physics binds to the functions, never
      to the mind
- [ ] a MIND acts only on functions (the same ones a player's bindings actuate -- one seam,
      already proven by the handover)
- [ ] the mind's perception is world QUERIES (ground, corridor, sight lines), not privileged
      state
- [ ] navigation is a service the mind asks (two coordinates in, corridor out) -- today's
      `Journey::Lay`
- [ ] `Journey` folds into `Sim`: the drive becomes a Sim scenario, and the driver tool a
      declaration over it (board:1573's migration list is the same motion)

## Comments

Found the measured way the same hour: a second route (Kyoto-Osaka) refused because
`Journey::Lay` asserts MUNICH constants inside the engine path -- route-1 expectations (612 km
crow-line, 523 m elevation, Marienplatz walk) living where the causal chain says only mechanism
may live. board:1582 (clients include only the public interface) is the enforcement half.

---

Learned from the architecture adjudication (2026-08-22): the fold must DECOMPOSE, not attach.
Welding Journey onto today's Sim grows the god facade the component model exists to replace --
Sim carries ~50 getters and hand-wired subsystems (src/clients/Sim.h). The fold therefore reads:
Journey's parts (corridor, speed plan, rig, mind) become entities and columns in the scene
store, wired by the assembly API; Sim shrinks to the systems that advance that graph. Same
verdict names TilePool::Camera and World::Refine(Eye) as ground-layer violations of the layer
table -- the compositor (not yet standing) is where camera, frustum and LOD selection belong.
