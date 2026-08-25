Type: feature
State: open
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

---

**Move 2 (b), the concrete cut (2026-08-22).** Journey::State is legible along the seams now:
streaming (Store/Sources/Pool/Ground), declaration (Declared/CarWidthM/Stood), CORRIDOR
PRODUCTS (Corridor, Fitted, Profile, RoadM/HalfWidthM/LaneHalfM/AsideM/FineAside/FineEdge,
FineM/SpanM/NarrowestLaneM/BudgetM/HoldWithinM), ride state (Rig/Body/NearM/LostM/Tally),
frame anchor. The cut: `src/sim/CorridorLay.{h,cpp}` gains `struct Corridor` carrying exactly
the corridor products, and `LayCorridor(route, ground, assetsDir tables, carWidthM, envelope,
say, out, error)` -- the middle ~500 lines of Lay move verbatim with their published numbers;
Journey::Lay shrinks to: read declaration, open streaming, harvest+weave+plan (the nav tool),
call LayCorridor, stand the rig, seed the ride. Then (c) DriveSystem from Ride over the same
Corridor product. Each step keeps the fast gate and the sporadic driver proof green.

---

**Move 2 (d), the decided shape (2026-08-22).** The driver cases enter through the one door
with NO new facade verb and NO second scenario file: read the F31 declaration, set `.Driven`
on the Scenario VALUE in code (building the declaration in C++ IS the C++ door -- the parity
law's whole point), `Engine::Declare` + `Assemble` size the pool exactly, and the handles come
from `Scene()`. Engine exposes its columns read-through (`Vehicles()`, `Drives()`) -- columns
are the data half of the graph, not verbs, so the door stays one door. Journey's orchestration
then takes (Store&, the assembled handles, Provision, wire) and consumes the assignment's
coordinates from the column instead of a Between parameter; the mind possesses the seam that
DriveTick actuates. Proven when Munich and Kyoto run through Engine with the same numbers.


---

**Move 2(e) plan (2026-08-22 evening, survey at 5b5cc642).** Journey's State is already two
clean halves: the GROUND COLUMN (ContentStore, SourceSet, TilePool, GroundStream -- owned
infrastructure, opened from Provision + a focus coordinate) and the DRIVE PRODUCT (Vehicle,
Rigged, Corridor, DriveState -- pure values). Consumers: the five driver cases and the unit
twin ONLY -- no library-internal caller. The slices:

- (e1) `World::GroundStack` under src/ground: owns store->sources->pool->stream, Open(cacheDir,
  assetsDir, focusLat, focusLon, wire, say) verbatim from Lay's opening block, Close in
  reverse; Journey delegates to it first (proof: fast gate + road edge).
- (e2) `Sim::DriveProduct {Car, Stood, Way, State}` and free
  `Sim::AssembleDrive(scene, cast, vehicles, driven, world, GroundStack&, wire?, say, product&)`
  -- Lay's body moves verbatim; Journey::Lay becomes a delegation shell.
- (e3) the class dies: the six consumers hold {GroundStack, DriveProduct} and call
  AssembleDrive + DriveTick directly; accessors resolve to product fields; the name Journey
  leaves the tree; CLAUDE.md's map replaces the amber Journey node with GroundStack (ground
  column) and AssembleDrive (sim system). Proof: headless three + stills; window stays the
  named-only long proof.

The say-narration stays with AssembleDrive as Sink calls -- it documents engine claims, not
case prose.
---

Reviewer note (2026-08-22, night round), so it is not lost when this closes: the decomposition
table's own row says "the Sink claims stay with the CASES -- systems publish numbers, cases
judge them". Move 2e slice 2 moved Lay's claims verbatim INTO the free function:
src/sim/DriveAssembly.cpp carries the cases' judgement texts, including content literals a
scenario-agnostic system may not spell -- "roads a 1.811 m car can fit down" (line ~183) and
"**AND THE DECLARED F31 STANDS UP AS A RIG**" (line ~232) name one car's width and one car's
model inside src/sim. The row remains unpaid: AssembleDrive publishes numbers, the driver
cases claim.
