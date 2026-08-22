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

---

**The decomposition plan (2026-08-22, move 2 after the src/sim fold).** Journey::Lay is one
function with seven tenants; they become systems over the store graph, each taking (Store&,
Entity) and columns, none owning another:

| tenant (Lay lines) | becomes | reads | writes |
|---|---|---|---|
| sources + tile fetch + OsmField | `RoadNet` service (exists as Wayfinding+RoadHarvest) | wire, tiles | the network |
| route planning | the NAV TOOL's `Plan` -- mind Uses nav, Assigned carries Between | network, vehicle limits from Column<Vehicle> | Column<Route> on the assignment |
| corridor fit + widths + elevations + grades | `CorridorSystem` | Route, GroundStream | Column<Corridor> (ReferenceLine + widths) on the assignment |
| rig stand-up | already `Clients::Stand(Vehicle)` | Column<Vehicle> | Column<Rig> on the body |
| speed profile | `PlanSystem` | Corridor, Rig envelope | Column<SpeedProfile> on the assignment |
| pilot + ride tick | `DriveSystem::Tick(dtS)` -- the mind acts on the seam | all of the above via handles | Body state, Ridden telemetry |
| the Sink claims | stay with the CASES -- systems publish numbers, cases judge them |  |  |

Order of execution: (a) Route+Corridor columns as public types; (b) CorridorSystem extracted
from Lay with Journey delegating; (c) DriveSystem from Ride; (d) Journey shrinks to an
assembly recipe -- read scenario, assemble through the door, hand tools, tick; (e) the driver
cases declare through Engine and the build enforcement of 1582 closes. Each step keeps Munich
and Kyoto green before the next begins.
