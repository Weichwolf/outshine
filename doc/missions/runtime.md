# World entities and mission core — `units/`, `missions/FBMission*`, `modules/FBModule*`

**Sources of this file:** the comment banners of `sim/src/units/` (`FBUnit.h`, `FBSimUnit.h/.cpp`,
`FBUnitRegistry.h`), `sim/src/missions/FBMissionRunner.h/.cpp`, `sim/src/missions/FBMissionBoot.h`,
`sim/src/missions/FBTickPool.h/.cpp`, `sim/src/missions/FBModelRoots.h`, `sim/src/clients/FBLogSinks.h`,
`sim/src/modules/FBModule.h`, `sim/src/modules/FBModuleRegistry.h/.cpp`, plus CLAUDE.md for the
stage history and the measured numbers. The mission file format itself is in
[`doc/missions/INDEX.md`](INDEX.md) and is **not** repeated here — only referenced.

Subject: what a simulated unit IS, who owns it, who may see it, and the four steps with which the
orchestrator turns a text file into a run with a verdict.

---

## Spec

What a simulated unit **is**, who owns it, who may see it, and the four steps that turn a text file
into a run with a verdict.

| Contract | Acceptance / measurement anchor |
|---|---|
| An actor is ONE object owning its FDM and its module | `units/FBSimUnit`; the registry hands out borrowed `const FBUnit*` only |
| The mission runner is a pure orchestrator — four steps, no mission knowledge in code | `FBMissionRunner.cpp` includes no concrete module header; `module <name>` resolves through the registry |
| Spawn is declarative data, one IC application, ground or air | `FBMissionBoot.h::FBMissionSpawnActor`; no separate ground/air code paths |
| Cross-unit reads see the last COMPLETED tick | `PublishPose` barrier after all `Run()`s — tick order cannot influence a result |
| The verdict is per actor; the LOOP only combines | one `UNIT_RESULT` line per actor; an expected loss (a declared `kill` objective of another unit) does not decide the run |
| **There is exactly ONE simulation loop and no client writes one** | `missions/FBMissionSim`: `Tick()` private, `FBRunState` `[[nodiscard]]`, the unit tick surface (`Run`/`PublishPose`/`RunMonitors`/`Update*`/`FinalizeMission`) private with that one friend. Acceptance: `make -C sim verify-guards` (8 cases, 6 of them must FAIL to compile) and `verify-layers`, which prints the number of simulation-loop drivers — **1** |
| **A run ends when an actor is no longer ALIVE, asked of the damage register** | `FBSystemHealth::Destroyed()`, written only through `FBDamageModel::ApplyPhysicalKo`, the register's single friend. The criterion is physics (`core/FBFlightMonitor`'s model-derived checks), never a hit count; the RESULT line quotes the monitor's own reason |
| Determinism is independent of thread count | identical fingerprint (SHA-256 over all `telemetry*.csv` + normalised `events.log` + exit code) over `--threads 1..4` × 5 repetitions |
| A dropped store grows the actor list at exactly one place, at the END of the tick | `FBMissionSpawnStore`; nothing is allocated in the tick path |

## State

Built through stage 4 (threading) plus the store-spawn path; stages 5–8 are covered in
`sensors.md`, `pilot-ai.md` and `core.md`.

| Stage | What it built | Anchor |
|---|---|---|
| 1 | FDM instance-capable | `c1bc9de` |
| 2 | the actor is one object | `c08a168` |
| 3 | the flight is mission data — two jets fly | `2c03704` |
| 4 | thread per unit in the gym, lockstep barrier, bit-identical | `6d7ed5a` |
| — | four-step orchestrator, declarative spawn, mission monitor | `92fe8a4` |
| — | store spawn as a full unit (own FDM, own module, own telemetry) | `b62c769` |

Measured scaling, honestly: 2 units 1.29–1.41× at 2 threads, 4 units 1.49/1.53/1.77× at 2/3/4 threads
on an A18 Pro; the ceiling is the machine, not the barrier (two independent `fb-gym` processes scale
just as badly).

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `FBMissionRunner.h` | docstring omits `LOC` (shares exit code 2 with `Crash`) and still speaks of a single module; the detonation banner claims "what a hit DOES is deliberately not modelled" directly above the `ResolveBurst` call that does exactly that |

### Inventory (from the previous `Offene Punkte` section)

- **Done (comment round):** the four outdated banner statements in `FBUnit.h` ("planned per-unit
  threading"), `FBUnitRegistry.h` ("today the datalink, tomorrow the radar"), `FBSimUnit.h`
  (`GetSignature` "today its datalink transmitter") and `FBModule.h` (`AttachFdm`, "`units/FBUnit`
  later") went away together with the banners themselves; likewise `FBSimUnit.h`'s counting
  contradiction "exactly the six places" where a missing airframe counts.
- **`FBMissionRunner.h`'s docstring is incomplete:** "Returns 0/1/2/3 = Success/Fail/Crash/Timeout" does
  not name `LOC`, which shares code 2 with `Crash` (correctly explained in the `FBMissionResult` banner).
  Likewise the same docstring still describes `FBRunMission` as "Ground-spawns `missionPath`'s module" —
  singular, although it has long been a cast.
- **The detonation banner is out of date:** "What a hit DOES is deliberately not modelled yet — this is
  the event, not a damage verdict" stands immediately before the `ResolveBurst` call that does exactly
  that.
- **`FBUnitRegistry` has no `Unregister`.** A retired unit stays registered (it only falls silent). Every
  sensor therefore has to deal with silent entries itself; whether each one does was not checked in this
  round.
- **No cross-check of the WASM loop.** `clients/FBAppWasm.cpp` demonstrably calls `PublishPose`,
  `RunMonitors`, `PrimeState` and `FBWorld::SetUnits`, but whether it mirrors the runner's phase order
  exactly (in particular elevation before STEP, roster construction) was not compared line by line.
- **`FirstFlightKo`/`ExpectedLoss`/`FirstJudged` are O(N²) over the actor list** and run in the loop
  condition, hence several times per tick. Irrelevant at today's cast sizes (< 10), but not documented as
  a limit.
- **Stage 7 is missing from the history.** CLAUDE.md names stages 1–6 and 8; a 7 is mentioned nowhere.
  Whether it was skipped or absorbed does not follow from any source.
- **`--elev` defaults, tournament runner, `.fbm` syntax** are deliberately not repeated here — they are in
  [`doc/missions/INDEX.md`](INDEX.md) resp. belong in the neighbouring files of this
  knowledge base (elevation hook, pilot variants).


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. Files

| File | Role |
|---|---|
| `units/FBUnit.h` | Base interface of every world entity: identity, pose, emission signature, `Run`. Plus `FBUnitKind`, `FBUnitPose`, `FBUnitSignature`. |
| `units/FBSimUnit.h/.cpp` | ONE simulated unit as ONE object (airframe + module + state + telemetry + health register + both judges). Plus `FBActorList`. |
| `units/FBUnitRegistry.h` | "Who exists" — list of borrowed `const FBUnit*`, in declaration order. |
| `missions/FBMissionSim.h/.cpp` | **THE SIMULATION LOOP — one tick body, one end rule, every client.** Owns the phase order, the judges' combined verdict and the run state; `Tick()` is PRIVATE, `Advance(budgetS)`/`RunToConclusion()` are the only ways in, and `FBRunState` is `[[nodiscard]]`. Also `FBMissionResult`, `FBMissionTickHook` and `FBActorStepper`/`FBActorStep` (the ONE phase a client may place on its own threads). On the core-lib list. |
| `missions/FBMissionRunner.h/.cpp` | The HEADLESS orchestrator (`FBRunMission`): files, threads, campaign carry, exit code — everything a headless run is and a browser is not. It drives `FBMissionSim` and writes the report. |
| `missions/FBMissionBoot.h` | `FBMissionSpawnActor` (actor from a mission block) and `FBMissionSpawnStore` (actor from the carrier state). Header-only. |
| `missions/FBOrdnance.h/.cpp` | Everything a released store and a fired burst DO once they have left the jet — the gun pool, the store tracks, closest approach, the fuze, the impact and the four damage resolutions. **The actor list's one growth point.** Three calls per tick in this order: `Resolve` → `Launch` → `SnapPoses`, so a round is never resolved in the tick it was created in. On the core-lib list, because the browser frame loop drives the identical object. |
| `missions/FBTickPool.h/.cpp` | The GYM-ONLY lockstep worker pool of the STEP phase. |
| `missions/FBModelRoots.h` | The two JSBSim model roots of a client. |
| `modules/FBModule.h` | The module interface: wiring, generic system accessors, `ApplySetup`. |
| `modules/FBModuleRegistry.h/.cpp` | Name → factory. |

---

### 2. `FBUnit` — the base interface

Every world entity, controllable or not. Pilot, sensors and weapons query units **through this
interface**, never via the concrete type — today ownship + AI jets + stores + ground targets, tomorrow
more, same shape.

**Identity** (`Id`, `Name`, `Kind`, `Team`) is set ONCE at construction and is immutable.
`Name` is the callsign, i.e. the `unit <id>` token of the `.fbm`.

**Pose** is read fresh on every query: a unit is a VIEW onto the truth of its owner, never a duplicated
copy that can drift apart.

| `FBUnitPose` field | Unit |
|---|---|
| `LatDeg`, `LonDeg`, `ElevM` | geodetic, m ASL |
| `RollDeg`, `PitchDeg`, `YawDeg` | deg |
| `SpeedMs` | TAS/ground speed, as the unit type defines it |
| `HeadingDeg` | ground track, true, 0..360 |

#### `FBUnitKind` — and what exactly the distinction does

| Kind | What stays the same | What is DIFFERENT (and why it belongs to the OWNER) |
|---|---|---|
| `Aircraft` | — | the normal case |
| `Weapon` | own FDM instance, own module, the same monitors, own telemetry file | (1) its physical knockout is a **detonation**, so it does not end the run (`FirstFlightKo` skips it, `ActorResultStr` calls it `IMPACT`); (2) air-to-air sensors do not look for weapons |
| `Ground` | roster, health register, damage model, own telemetry file — a full unit | (1) **no flight dynamics**, so it is never shown to the physics monitor (it does not fly, there is nothing to conclude); (2) air-to-air sensors do not look for it |

Precisely on `Ground` hangs the ONE exception in `FBSimUnit`: the airframe is optional (§3).

#### `FBUnitSignature` — what foreign sensors may perceive

The part of the system state that another sensor may **legitimately** notice. Published at the same
barrier as the pose (§4) — no receiver ever reads half a tick.

| Field | What it is | Who reads it |
|---|---|---|
| `DatalinkXmt` | MIDS terminal powered AND transmitting (XMT ON) | the others' `sensors/FBDatalinkSystem` |
| `Uplink` (`FBWeaponUplink`) | the midcourse guidance-link transmission to a self-launched weapon | `modules/missile/FBMissileUplink` |
| `IffXpdr` | AN/APX-113 answers Mode 4 | the interrogator in `FBRadarSystem` |
| `Radar` (`FBEmitterSignature`) | **the beam**: mode, emitter type, body-fixed window, range gate as a power measure — including WHERE it points | `sensors/FBRwrSystem` |
| `Chaff[kMaxChaffClouds]` | the dispensed clouds | the opposing radar (Doppler notch) |

Two explicit design decisions in the header:

- Chaff hangs on the **dispensing unit** instead of being a unit of its own. Stated consequence: a cloud
  can only decoy a radar that is looking at the aircraft that dispensed it — never one that is tracking
  somebody else nearby.
- A cloud is not a transmission but a REFLECTION — it is nevertheless published here, because it is the
  same kind of fact: what foreign sensors may perceive at this unit.

`Run(dt, units, world)` is the default no-op; `units` is the cast as simulated SENSORS may see it (every
entry a snapshot of the last completed tick, **including this unit itself**), `world` the terrain side
alongside. Both borrowed, both may be `nullptr` in a client without them.

---

### 3. `FBSimUnit` — a simulated unit, whole

Everything that was previously scattered locals in the mission runner and a set of file-scope statics in
the browser client is ONE object with ONE owner here. It IS an `FBUnit`.

#### What it owns — and the declaration order

```
std::unique_ptr<FBFdm>    Fdm_;        // owned
std::unique_ptr<FBModule> Module_;     // owned
fb_fdm_state              St_;
FBUnitPose                Pose_;       // published
FBUnitSignature           Sig_;        // published
std::string               LogLabel_;
double                    GroundAslM_;
FBSystemHealth            Health_;
FBFdmTelemetrySource      FdmSrc_;     // borrows Fdm_/St_/GroundAslM_
FBStateBusTelemetry       BusSrc_;     // borrows the module's bus
FBSystemHealthTelemetry   HealthSrc_;  // borrows Health_
FBTelemetryBus            Bus_;
FBFlightMonitor           Flight_;
std::unique_ptr<FBMissionMonitor> Mission_;   // missing when the unit has no objectives
```

**Why this order:** `Fdm_` stands BEFORE `Module_`, so that the airframe outlives the module that only
borrows it (destruction runs backwards). Likewise `St_`, `GroundAslM_` and `Health_` stand BEFORE the
telemetry sources that hold references to them.

Everything the unit hands out is borrowed (`const&`/`*`). The telemetry **sink** stays with the client:
file I/O belongs to the client, `core/` stays I/O-free.

#### The optional airframe

The ONE thing about a unit that is not universal. Universal are: identity, faction, published pose,
health register, both judges, a telemetry trace — whether somebody flies it or not.

A static ground target (`modules/ground/FBGroundModule`) has no flight dynamics and therefore no `FBFdm`.
The alternative would have been to give a bunker an invented JSBSim model and integrate it at 100 Hz in
order to reproduce the position at which it was spawned. So the pointer may be null — in exactly those
places where the behaviour really depends on the airframe:

| Place | Behaviour without an airframe |
|---|---|
| `Run`/`PrimeState` (FDM step) | no step; the spawn pose IS the whole truth |
| `UpdateGroundAsl` (push to JSBSim) | dropped |
| `RunMonitors` (physics judge) | the unit is **never shown** to it |
| `BuildMissionSample` (`AnyWow`) | `true` — a unit without an airframe is by definition on the ground |
| `ApplyDamageToAirframe` | dropped; "destroyed" means for it: destroyed in the register and nowhere else |
| `FBFdmTelemetrySource` (`fuelLbs`, `gearLoadFactor`) | 0, column set unchanged |

#### Anti-cheat, not weakened by the bundling

An `FBSimUnit` can only be built from an **already spawned** `FBFdm`, and that exists only via
`fdm/FBFdmBoot` (app/-only). So nothing under `systems/` or `modules/` can build a unit, reach a judge or
re-place an airframe. Evidence:
`grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems src/modules` is empty and stays so.

The module never sees the judges: they are fed **here**, from observed FDM truth, and the only thing a
trigger does to the airframe is the engine cutoff which the app has always applied.

#### `TakeBurst` / `TakeKineticBurst`

The whole chain of consequence in one call, in this order and nowhere else:

1. `core/FBDamageModel::Apply` (resp. `ApplyKinetic`) decides what the geometry did to which system —
   the module contributes exclusively `DamageLayout()`, i.e. WHERE its systems sit;
2. `ApplyDamageToAirframe()` pushes the result straight into the airframe.

| Register state | Physics (via `fdm/FBFdm`) |
|---|---|
| Engine `Failed` | `Controls().EngineCutoff()` (through the same control path a pilot uses) + `SetThrottleLimit(0)` |
| Engine `Degraded` | `SetThrottleLimit(kThrottleLimitDegraded = 0.6)` |
| FlightControls `Failed`/`Degraded` | `SetControlAuthority(0.0 / 0.5)` |
| Structure `Failed`/`Degraded` | `SetDamageDrag(6.0 / 1.5 ft²)` |

Idempotent and only called when the register has changed — no per-frame work. **Nothing is marked
"dead":** the unit keeps being ticked and keeps being judged by the same two monitors. From the next step
on, JSBSim integrates the aircraft it now is.

The register (`core/FBSystemHealth`) belongs to the unit for the same reason as the two judges: it is a
fact ABOUT the unit which its own module may read but never write. The module gets a `const&` at
construction (`FBModule::AttachHealth`).

#### `Retire()` — the end of a unit

```cpp
void Retire() { Active_ = false; Sig_ = FBUnitSignature{}; }
```

- **No deletion from the actor list.** That would shift every later index and make the tick order of a
  run depend on when a bomb happened to hit. The object lives on, because the registry borrows raw
  pointers and everything already written (telemetry file, lines in `events.log`) has to stay valid. The
  trajectory ends, the record does not.
- **Retiring SILENCES the unit** in the same act. The seeker of a detonated round was still radiating in
  the last snapshot — and because that snapshot is frozen from then on, everything with a warning
  receiver would have kept hearing a missile that no longer exists (**measured: the shooter's own RWR
  reported a live seeker two minutes after the detonation**). The pose stays — that is where the unit
  ended up.

#### `CheckEnvelope()` — generic envelope diagnostics

Latched per unit, not per run. Pure `FBLog::Warn` output, no verdict.

| Warning | Trigger | Reset |
|---|---|---|
| `stall` | `cas > 15 m/s` AND `alphaDeg > 25` | `cas <= 15` or `alphaDeg < 20` |
| `overspeed` | `mach > 1.2` | `mach < 1.1` |
| `sink` | `AGL < 150 m` AND `vy < −15 m/s` | `vy > −5` |

The `cas` gate exists because AoA is numerically undefined at nearly zero airspeed — otherwise there
would be a settling warning at spawn.

#### Telemetry registration — the append rule

`StartTelemetry(sink)` registers the sources in a **fixed column order**. The rule in the code, repeated
in six places: **a new source appends columns at the END, it never shifts old ones.** Every column ever
measured keeps its position; regression baselines and analysis scripts read by position.

Order (from `FBSimUnit.cpp`): `fdm` → AirData → Pilot → FlightControl → Controls → Datalink → Radar
→ `PilotSystem().BfmTrack()` → WarningSystem → Commands → `FBStateBusTelemetry` → Stores → Rwr →
Countermeasures → `PilotSystem().Engagement()` → HealthSrc (`dmg_*`) → Guns.

A `nullptr` sink leaves the bus a cheap no-op (the browser case).

#### Other contracts

| Method | Contract |
|---|---|
| `UpdateGroundAsl(sampleM)` | An unresolved sample keeps the last good value (`FBElevationResolved`). **ONE number** reaches both JSBSim's contact ground and the module's HUD/radar-altitude path — the two cannot disagree about the ground. For `FBUnitKind::Weapon`, JSBSim is instead given `FBFdm::kNoGroundElevM = −100 000 m` (§8) — **renamed and moved 2026-07-29** out of two anonymous namespaces (`units/FBSimUnit.cpp`, `clients/FBTestMissileAirframe.cpp`) into the one class that hands the number to JSBSim; and it is now also applied **at the initial condition** through `FBFdmSpawn::TerrainElevM` ([`../fdm.md`](../fdm.md) §6 step 1b). |
| `HudState()` | The module's bus with THIS frame's pose folded in — the module publishes the Platform block at its own rate, the client re-publishes it at the frame rate, so that the conformal symbology is drawn against the pose actually rendered. Same block, same writer role — no second copy of the truth. |
| `PrimeState()` | Boot-only: one FDM step + `PublishPose`, so that the first frame does not read an empty pose. |
| `SetLogAttribution(bool)` | Sets the `unit=` label — once at boot, never per tick. Empty with exactly one actor. |
| `Displays() const` | Read-only view onto the Displays slot for the renderer, so that no caller has to `const_cast` (`FBModule`'s accessors are deliberately non-const: systems are COMMANDED through them). |

---

### 4. `FBUnitRegistry` and the snapshot discipline

#### Why the registry lies in the core lib

It was a member vector of `world/FBWorld` — i.e. on the **renderer** side of the lib/client split.
`fb-gym` does not link `world/` at all and therefore handed every module a null world: a simulated
sensor could **never** have seen another unit in the only client that really runs the mission loop. The
cast is simulation state (it exists with or without a camera), so it lives in `units/`, the client owns
exactly ONE, and `FBWorld` only **borrows** it (`SetUnits`/`Units()`) for the drawing side.

#### Contract

| Property | Rule |
|---|---|
| Ownership | borrowed, never owned — the client owns the `FBActorList` |
| Order | registration order = mission declaration order, never changes during a run (growth only at the end, §8) |
| Element type | `const FBUnit *` — read-only **by construction**: a system can observe identity and the PUBLISHED pose/signature of the last completed tick, nothing else. It cannot tick, control or write another unit. |
| Who may hold it | only a simulated SENSOR, and that is why it travels down the module's system cycle as a `Run()` ARGUMENT instead of being a member everyone can reach. Today six files in `systems/`+`modules/`: `FBDatalinkSystem.cpp`, `FBRadarSystem.cpp`, `FBRwrSystem.cpp`, `FBIrstSystem.cpp`, `FBVisualSystem.cpp`, `modules/missile/FBMissileUplink.cpp`. |

#### The snapshot discipline

**`PublishPose()` is the barrier.** The client FIRST ticks every unit and THEN, in a second loop, makes
the new poses + signatures visible together.

```
for every unit:  UpdateGroundAsl            (sequential)
for every unit:  Run(dt, &registry, world)  (STEP phase, possibly parallel)
for every unit:  PublishPose()              (the barrier)
… only then: monitors, telemetry, hook
```

From that follows the contract of `GetPose()`/`GetSignature()`: **always the state of the last COMPLETED
tick**, never a half-integrated one. No unit can see a neighbour that has already stepped in this tick —
so **tick order cannot influence a result**. That is exactly why the parallelisation of the STEP phase
(§9) is a pure parallelisation and not a redesign.

The constructor of `FBSimUnit` calls `PublishPose()` immediately: the declarative spawn is already a valid
pose, so nobody ever reads an empty one.

Both renderer clients follow the same discipline: `clients/FBAppWasm.cpp` holds its own `FBActorList`, calls
`PublishPose` for all, then `RunMonitors` — the same two judges, no second parallel check.

---

### 5. The mission runner as a pure orchestrator

`FBRunMission(missionPath, timeoutOverride, outDir, models, elevation, hook, threads, clientClockOverride,
carry)` — shared by `fb-gym` and `gpu_native --mission`. **Exactly four steps**, no mission specifics in
the code. The last argument is the campaign layer's only seam (`const FBMissionCarry *`,
[campaign.md](campaign.md)): null for every ordinary run, and when set it applies the carried state to
the parsed mission BETWEEN step 1 and step 2 and reads the run's own outcome off the same actors step 4
judges. It adds no step and no phase.

| Step | What happens |
|---|---|
| **1 — load mission** | Read the file, `FBParseMissionFile` (pure text→`FBMission` function), resolve the timeout (`timeoutOverride > 0` beats the file value), log `MISSION_START`. |
| **2 — set up the world with its actors** | Per `unit` block: resolve the elevation at the spawn point, check consistency, call `FBMissionSpawnActor`, append it to the `FBActorList`. Then: reserve capacities, fill the `FBUnitRegistry`, open a telemetry file per actor, `hook->OnMissionStart`. |
| **3 — run the actors** | `FBMissionSim::RunToConclusion()`. The runner contributes ONE thing to the tick: a thread pool for the STEP phase (`FBActorStepper`). The loop, its `dt = kSimTickS` (`missions/FBSimTick.h`, 0.1 s, 10 Hz decision rate) and the rule that ends it are the SIMULATION's, not this file's — see §5a. |
| **4 — validate the world** | The monitors decided long ago and `FBMissionSim` already combined them; here the combination is REPORTED: `UNIT_RESULT`/`RESULT`/`SUMMARY` and the process exit code. |

---

### 5a. `FBMissionSim` — the loop nobody writes twice

A client does not step a mission. It says **"run until you stop, or until you owe somebody a picture"**
and is handed back whether the run is still going:

| Member | Contract |
|---|---|
| `Advance(budgetS)` | turns the client's elapsed wall time into a whole number of `kSimTickS` ticks, carries the remainder, and returns `FBRunState::Running` or `Concluded`. The browser's frame budget IS this argument. |
| `RunToConclusion()` | the headless form of the same sentence; returns the `FBMissionResult`, because that is all a finished run has left to say. |
| `Tick()` | **private.** There is no way for a client to take one step without the end rule being asked. |
| `FBRunState` | `[[nodiscard]]` on the TYPE — a client that drops it does not compile (`make -C sim verify-guards`, case `advance_discarded`). |
| `FBActorStep` | a pass-key: the only thing a client's STEP phase may do is `step(i)`, and only `FBMissionSim` can construct one. |

**Why the cut exists, measured.** Before it, `clients/FBAppWasm.cpp` had a second loop (`SimTick()`)
which called `RunMonitors` and DISCARDED the result. It had no `FirstFlightKo`, no verdict check, no
timeout — so the flight monitor logged its `monitor KO` line and the browser went on integrating a
wrecked F-16 for as long as the tab was open. The rule was in the runner's `while` head, i.e. in a
place a second client could not inherit it. It is now in the one object both drive, exactly as
`FBOrdnance` already is for what leaves the jet.

#### The end rule, and why it is not an aircraft's

The loop asks **"is this actor still alive"** — `FBSystemHealth::Destroyed()`, the damage register —
and not "did the flight monitor trip". Every unit kind has health; only an aircraft has a stall. A
physical K.O. therefore travels `FBFlightMonitor` → `FBDamageModel::ApplyPhysicalKo` →
`FBSystemHealth`, through the register's ONE writer, and a future tank or ship gets the rule for free
by being damaged rather than by being added to a list of monitors. The criterion stays pure physics
(contact, structure, penetration, divergence — all model-derived in `core/FBFlightMonitor`); the
register only records it, and the RESULT line still quotes the monitor's own `Reason()`/`Detail()`,
because the observer is what explains WHY.

The run ends when an AIRCRAFT is destroyed. A weapon's destruction is its impact and a ground
position's is somebody's objective — neither ends the run the cast is flying.

#### What it does NOT know

- **No concrete module type.** `FBMissionRunner.cpp`/`FBAppGym.cpp` never include a concrete module
  header; everything runs through `FBModule`'s generic accessors.
- **No mission semantics.** Waypoint sequencing is actor behaviour (`FBNavSystem::AdvanceWaypoint`,
  called by the module itself), not runner bookkeeping. The verdict is passed by `FBFlightMonitor`/
  `FBMissionMonitor`, not by this file.
- **No `set` keys.** The runner passes the raw KV list through; the MODULE interprets its own keys.
- **No renderer.** Ground truth comes from an injected `FBElevationProvider`, not from a hard-wired
  fb-tiles wire — which is why this file has no renderer/world/Dawn dependency and is part of the core
  lib that `fb-gym` links against. Whoever wants MORE than headless telemetry supplies an
  `FBMissionTickHook`, whose interface is deliberately free of GPU types; `FBAppNative.cpp` implements the
  concrete hook with `FBRenderer`/`FBWorld` in ITS own translation unit.

#### Module resolution: `FBModuleRegistry`

- `FBRegisterBuiltinModules()` calls four family-wise entry points: `FBRegisterF16Module`,
  `FBRegisterStoreModules`, `FBRegisterMissileModules`, `FBRegisterGroundModules`. Each is defined in its
  OWN family — the only files that may name a concrete module type.
- The map is a **function-local static** (Meyers singleton), **not** a namespace-scope global: filled
  explicitly at a known point instead of via static initialisation order across translation units — which
  avoids both the SIOF problem and the trap "an unreferenced `.o` in a static archive is never linked",
  which self-registration would run into here.
- Idempotent (re-registration overwrites the entry), so it can safely be called once per run.
- `Create(name)` → `std::unique_ptr<FBModule>` or `nullptr`. The registry name is a **pure key**: WHICH
  JSBSim model belongs to it is said by the module (`FdmModelName`); where it lies has not been a question
  since the single model root.

#### `FBModule` — what the orchestrator reaches through the base

| Category | Methods |
|---|---|
| Wiring (once, before the first `Run`) | `AttachFdm(FBFdm&)`, `SetUnitIdentity(id, team)`, `AttachHealth(const FBSystemHealth&)` |
| Self-declaration | `FdmModelName()`, `UnitKind()`, `DamageLayout()` |
| Tick | `Run(fb_fdm_state&, dt, const FBUnitRegistry*, const FBWorld*)` |
| System slots | `Autopilot`, `FlightControl`, `PilotSystem`, `Controls`, `Displays`, `AirDataSystem`, `NavSystem`, `WarningSystem`, `RadarAltimeter`, `Commands`, `Datalink`, `Radar`, `Rwr`, `Countermeasures`, `Stores`, `Guns`, `Telemetry` |
| Diagnostics | `LastGuidance()`, `LastSubsteps()` |
| Mission data | `FlightPlan()`, `SetRunway()`, `SetGroundAsl()`, `ProgramRelease()`, `ApplySetup(key,value)` |

**`ApplySetup` is the module-owned interpretation.** The runner/boot only parses the flat KV list; the
module knows its keys. Return `false` = **unknown key** (or an unparsable value / one outside the band),
which the caller turns into a mission **FAIL** (exit 1) — never a silent no-op. The module has already
logged the reason (only it knows its keys); the boot additionally logs `SET_REJECTED` and voids the spawn.

`UnitKind()` is Aircraft by default; `Ground` declares at the same time that there is **no airframe at
all** — which the empty `FdmModelName()` says at the one place the spawn path reads. **`Weapon` is NOT
set here:** a store is a kind because it was RELEASED, and that is a statement of the release path
(`missions/FBMissionBoot.h`), not of the module.

#### `UNIT_RESULT` and the `unit=` attribution

**Attribution rule (decided at exactly one place, `FBMissionBoot.h`):** with exactly one actor the lines
stay unattributed — they are the mission's. From two actors on, EVERY actor-related line (including the
module-internal ones) carries `unit=<callsign>` as its first field. A released store is always attributed
(it never flies alone — at least the jet that dropped it is there).

**`UNIT_RESULT` is only emitted with > 1 actor** — with a single one the `RESULT` line IS its verdict, and
a breakdown would only repeat it (the same rule as for the attribution). Fields: `result`, `reason`,
`team`, `decisive`, `lat`/`lon`/`altM`, `telemetry` (path of the trace).

`result` per actor (`ActorResultStr`):

| Kind / situation | Result |
|---|---|
| `Weapon` | `IMPACT` if the physics judge triggered, otherwise `IN_FLIGHT` |
| `Ground` | `INTACT` / `DESTROYED` (the only thing that ever happens to it) |
| Aircraft, physics knockout without a prior shoot-down | `LOC` or `CRASH` |
| Aircraft with a mission monitor | its verdict |
| Aircraft without objectives | `NONE` |

**`ShotDownFirst`:** a jet that was shot down and then flew into the terrain has TWO true verdicts, and
the useful one is the first — the shoot-down explains the crash, the crash explains nothing. So the
physical judge steps back when the mission judge of this unit has already concluded AND it is combat-
ineffective (the only constellation in which this pair occurs). An undamaged wreck (CFIT, departure)
still reports itself as such.

#### How N verdicts become one

The loop condition is the combination itself:

```
while (!FirstFlightKo && !FirstDecidingFailure && !AllJudgedConcluded && simT < timeoutS)
```

| Helper | Rule |
|---|---|
| `FirstFlightKo` | A physical knockout of ANY **Aircraft** actor ends the run — the conservative reading: no wreck keeps integrating in the background. `Weapon`/`Ground` are skipped. Returns the UNIT, not a `bool`, because WHOSE knockout it was decides the `RESULT` line. |
| `ExpectedLoss` | The loss of an actor is **EXPECTED** if it was the declared objective of another: (a) this unit is combat-ineffective AND (b) some other actor with a monitor declares an `objective` that covers it (`FBObjectiveCovers(o, name, team)`). Two observed facts and one declaration — **no team heuristic, no notion of a "player side"**, and for a mission without an `objective` line nothing at all happens (old missions combine exactly as before). |
| `FirstDecidingFailure` | The first judged actor whose monitor has concluded and is NOT `Success` and whose loss was not expected. |
| `AllJudgedConcluded` | All judged actors have an answer, whatever it is. Without objectives: the same moment as "all successful", because there every failure is decisive and the loop stops earlier. |
| `FirstJudged` | Whose verdict the combined `RESULT` line quotes when nothing decided the run: the first judged actor whose loss was NOT expected (quoting the loser of a decided duel would be exactly the team blindness that `objective` removes). If ALL judged actors were lost that way (mutual shoot-down), the first one speaks after all — nobody came home, and the record should say so instead of inventing a winner. |

**Why the rule exists:** a duel has a winner and a loser, not FAIL twice. Before the objectives, the
loser's FAIL was the only verdict in the run and became the run's — a mission whose HOSTILE unit was shot
down reported FAIL, and the shooter's success was invisible. An expected loss is still reported as that
actor's own FAIL in its `UNIT_RESULT` line; what it no longer does is decide the run.

**After the loop (step 4), in this order and the order is the whole point:**

1. if the knockout is an expected loss it is taken out of the scoring (`ko = nullptr`) and every
   monitor gets `FinalizeMission` — a `survive` objective can only be answered here, because the run
   to be survived is now over, and these verdicts are part of the combination below;
2. the combination itself (`ko` / `failed` / `judged` → `result`, the table below);
3. **`FinalizeMission` for every judge still open, whatever ended the run.** Without step 3 a run
   stopped by an unexpected knockout — or by somebody else's decisive failure — left the other
   monitors unconcluded, so they published no `mission OBJECTIVE` line at all and a consumer had to
   read "never judged" as "nothing met". That paid a doctrine for keeping the OPPONENT airborne
   ([`../doctrine-evolution.md`](../doctrine-evolution.md) X-1, closed 2026-07-30). It changes **when
   a judge finishes, never when the RUN ends**: `FirstFlightKo` is untouched to the tick, and because
   step 3 runs AFTER step 2, a monitor closing there cannot move `ko`, `failed`, `judged` or `result`.
   [MESS] all 251 `sim/missions/*.fbm`: 0 telemetry values moved, 0 exit codes moved, 27 `events.log`
   gained lines.

**The one line that is not an addition, and it is a rule that already existed.** A unit that had been
shot combat-ineffective and then hit the ground had TWO true verdicts and `ShotDownFirst` says the
mission judge speaks — but its precondition is `Concluded()`, and before step 3 such a unit's judge
never concluded, so the physical judge spoke by default. With the judge finishing, the documented rule
applies where it always should have: `net-belt-high`, `o1-08-belt-netted` and `o3-10-october-six` are
the three files in the tree where it does, and their `RESULT`/exit codes are unchanged. A HEALTHY
airframe that departs still reports `LOC`/`CRASH` — `CombatEffective()` is true, so `ShotDownFirst` is
false ([MESS] `w4-10-allied-force`, `kamig4` `LOC "stall/mush"` before and after).

| Priority of the combined `RESULT` | Source |
|---|---|
| 1. `ko` (unexpected knockout) | `FBFlightMonitor::Reason()` → `LOC` or `CRASH`, detail from the judge |
| 2. `failed` (first deciding mission defeat) | its `FBMissionMonitor` |
| 3. `judged` (otherwise) | its `FBMissionMonitor` |
| 4. nobody carried objectives | `TIMEOUT`, "sim time exceeded the mission timeout" |

`deciding` (knockout or first deciding defeat) supplies the `unit=` label of the `RESULT` line and the
`decisive=` field of the `UNIT_RESULT`s. On a clean success nobody decided alone, so it stays empty.

**Exit codes:** `SUCCESS 0`, `FAIL 1`, `CRASH 2`, `LOC 2`, `TIMEOUT 3`. `LOC` shares code 2 with
`CRASH` — both are `FBFlightMonitor` terminations, distinguished by the `result` field of the `RESULT`
line, not by the exit code; a caller that only branches on `exit != 0` (the documented contract) sees no
difference.

`SUMMARY` measures `wallS`/`speedup` over `steady_clock`, **not** `clock()`: the latter sums CPU time
across several threads and would have reported a FASTER run as slower.

#### Log sink lifetime

`FBLogSinkScope` is declared LAST, hence destroyed FIRST: `FBLog`'s sink pointer is gone before the sinks
and the `FILE*` behind them disappear — on EVERY return, not just the successful one. A second mission in
the same process (the planned pilot tournaments) would otherwise log through a dangling, long-closed sink.

---

### 6. The spawn of a mission actor

`FBMissionBoot.h::FBMissionSpawnActor(models, mission, unitIdx, groundAsl, timeoutS, err)` — header-only,
generic over `FBModule`, itself naming **no concrete module type**. It is at the same time the app-side
half of the IC gate: it includes `fdm/FBFdmBoot.h`, the only way to an `FBFdm` — and because an
`FBSimUnit` can only be built FROM a spawned airframe, this header is also the only producer of a
complete actor.

**The special case first: a module WITHOUT an airframe** (empty `FdmModelName()`). Deliberately an early
return instead of a branch threaded through the IC — the two have nothing in common: everything from
there on exists in order to bring a JSBSim instance into a state, and this unit has none. Only two things
are checked: the spawn must be `ground`, and `set` lines are not permitted. Everything after that stays
shared (unit object, identity, health register, telemetry, mission monitor).

**The aircraft path, in order:**

| Step | Content |
|---|---|
| 1 | `FBModuleRegistry::Create(block.ModuleName)` — unknown ⇒ `nullptr` + reason |
| 2 | Fill `FBFdmSpawn`: `ModelsRoot = models.Aircraft`, `Aircraft = module->FdmModelName()`, position, `GroundElevM`, `HeightOffsetM = Ground ? −1 : (AltM − groundAsl)`, `SpeedMs = SpeedKt·kt→m/s`, heading |
| 3 | `FBFdmBoot::Spawn(ic)` — **the ONE IC application**, ground OR air, no second code path |
| 4 | `SetGroundElevM(groundAsl)`, `module->AttachFdm(*fdm)` — BEFORE any `Controls()`/`ApplySetup` that reaches the airframe |
| 5 | Mission data onto the module: runway (if present), `FlightPlan() = block.Plan` |
| 6 | Initial state: `Autopilot().SetManual(0,0,0,0)` (idle stick — an airframe with a real FLCS like the F-16 holds the wings level by itself, so the preflight hold of a ground spawn has something stable to sit on), `Controls().SetGear(true)`, both wheel brakes at 1 (the real basic setting; a mission's `set gear up` overrides it for the air start) |
| 7 | `PilotSystem().SetPhase(Ground ? Preflight : Route)` — ground gets the WOW-gated hold/take-off-roll mechanics, air is already established |
| 8 | every `set <key> <value>` line via `module->ApplySetup` — rejection ⇒ spawn void ⇒ mission FAIL |
| 9 | Build `FBSimUnit` (Id = `unitIdx + 1`, callsign, `module->UnitKind()` read **before** the move: argument evaluation order is unspecified) |
| 10 | Mission monitor **iff** the block has objectives — waypoints OR `objective` lines. Built from the mission FILE (plan/objectives/runway) plus the RESOLVED timeout, never from the module's living, mutated copy. Without objectives: no monitor, so the actor never appears in the mission verdict. |
| 11 | `SetLogAttribution(mission.Units.size() > 1)` |

The runner checks two things **before** the call: the elevation must be resolved, and an explicit air
spawn below the terrain (`AltM < groundAsl − 1 m`) is a genuine contradiction, not a legal special case —
the 1 m margin absorbs rounding of the elevation source, not real penetration.

#### `FBMissionSpawnStore` — the same four-step spawn from the carrier state

The second producer of a complete unit and **structurally the same steps**: resolve the module through
the registry (the store's catalogue key IS its registry key), apply ONE declarative IC, attach the
airframe, wrap it in an `FBSimUnit`. It differs in exactly two things, and both are properties of what a
released store IS: the IC comes from the **carrier state** instead of from a mission file, and the unit
is `FBUnitKind::Weapon`.

**The separation state, completely:**

| Quantity | Derivation |
|---|---|
| **Position** | Carrier position + station offset, rotated out of body axes with the carrier attitude (`FBBodyVecToEnu`). A store leaves the pylon, not the CG. Metres → degrees via `kMPerDeg` resp. `kMPerDeg·cos(lat)`. |
| **Attitude** | That of the carrier, unchanged. Nothing tips it off the carrier; whatever the airframe was doing at that moment, the store keeps doing. **Unless the release declares a RAIL** (`FBStoreRelease::HaveRail`): then pitch is `RailPitchDeg` and roll is 0 — a launcher aims, a pylon does not. |
| **Ground** | `ic.TerrainElevM = FBFdm::kNoGroundElevM`, i.e. the "a weapon has no ground" rule applied **at the initial condition** and not one call later (2026-07-29). `SetGroundElevM` afterwards is the same number, so the rule holds for the IC and for every later tick — [`weapons.md`](../weapons.md) §1. |
| **Motor** | `ic.MotorRunning = rel.HaveRail` (2026-07-29). A rail launch separates *because* the motor pushed the round off the rail, so it is born burning; an air launch drops clear and lights afterwards, which is the throttle slew. `HaveRail` is what tells the two apart, and it is false for every air-launched store. |
| **Velocity** | Carrier velocity **at this station**, i.e. CG velocity + **ω × r**. The rotational component counts at the moment a release happens in a roll; leaving it out would be a silent simplification instead of a modelling decision. Computed in body axes (`p,q,r` deg/s → rad/s), then the same rotation into ENU. |
| **Ejector impulse** | **deliberately NONE.** A real pylon pushes the store down at a few ft/s, and `doc/modules/f16/weapons.md` has no quotable number for it (§4.5's station data are T4 at best). So the store separates with the carrier motion and nothing else. If a source turns up, it is ONE additional body-fixed velocity term, here, at this one place. |

`HeightOffsetM` is raised to at least 0.5 m (the IC needs a positive offset; below that the store hits
immediately anyway). `ic.Ballistic = true` switches off trim and engine start in the adapter (→
[`fdm.md`](../fdm.md) §6) — **engine start only when `MotorRunning` is false**, the condition there being
`!Ballistic || MotorRunning`. `module->ProgramRelease(rel)` is the generic launch programming: a bomb ignores
it, a guided round takes shooter id and target estimate from it — this file names no weapon type. Logged
as `stores SEPARATION` with the full state vector.

#### The tick semantics of newly appended units

**The actor list grows at exactly one place**: at the END of the tick in which the release/shot was
commanded — the new unit is computed only in the NEXT one.

**Why that is not convenience but determinism:** the STEP phase runs one job per actor index (§9); an
actor appearing in the MIDDLE of the phase would make the run result depend on WHEN in the phase it
appeared. Appending at the barrier keeps the whole tick a single snapshot, exactly like every pose.

**The order is deterministic all the way down:** actors are drained in list order, the release queue of
each actor is FIFO, and every new unit is appended in that order — so list, tick order and unit ids are
identical in a 1-thread and an N-thread run.

**No allocation in the tick path:** the capacity is pre-reserved — first for the declared units, then
exactly (`maxActors = actors + Σ LoadedCount()`) as soon as every module has applied its loadout: ONE
further unit per occupied station and no more, because a store can be released once. Everything
index-parallel (log buffers, `PrevPose`, roster) is dimensioned to THIS ceiling, so that a store appearing
mid-run resizes no buffer that a worker thread is currently referencing.

---

### 7. The tick, phase by phase

From the loop in `FBMissionRunner.cpp` (`dt = kSimTickS = 0.1 s`; the browser's `SimTick()` runs the
same phases in the same order for the same constant):

| # | Phase | Parallel? | Why |
|---|---|---|---|
| 1 | Elevation per active actor | **no** | The provider is the ONE shared object of the client (`FBTilesElevation` drives the tile streamer); one point query per tick is far too cheap to even raise the question. |
| 2 | **STEP** (`Module::Run` + own FDM substeps) | **yes** (`FBTickPool`) | The indices are independent (§9). |
| 3 | Drain log buffers in actor order | no | Line position must not hang on the scheduler. |
| 4 | `PublishPose` | no | **IS the barrier.** |
| 5 | `simT += dt`, build the roster | no | Roster = callsign, faction, the one bit from the health register; weapons stay out (a round in the air is nobody's objective). |
| 6 | `CheckEnvelope` + `RunMonitors` | no | So that the verdict which ends a run, and the lines it emits, are read in ACTOR order, never in completion order. |
| 7 | Telemetry sampling | no | Decision. |
| 8 | Projectiles fly + resolve | no | They belong to the client, not to a module. |
| 9 | Stores: fuze, impact, expiry | no | ditto |
| 10 | `hook->OnTick` | no | the renderer of the native oracle, single-threaded by decision |
| 11 | **Growth**: drain bursts + releases, append new actors | no | §6 |
| 12 | Memorise `PrevPose` of all actors | no | Last act, AFTER the growth, so that a unit which appeared in this tick also has an entry from now on. |

---

### 8. What else the runner resolves (not in the task list, but in the file)

These things stand in the orchestrator because they happen between TWO units and have to be resolved on
the **truth** (the published poses) — the same boundary as with the two judges: a weapon that scores
itself on its own estimate would be the purest form of cheating.

- **`ClosestApproach` (CPA).** Why not a distance check per tick: the tick is 0.1 s, a head-on closure
  can exceed 1,500 m/s, so successive samples are **150 m** apart — a pure per-tick check against a 10 m
  fuze radius would miss almost every real hit. So the minimum over the SEGMENT, standard CPA on
  `p(t) = p0 + t(p1−p0)`, `t ∈ [0,1]`. The straight-line assumption within one tick is worth about **one
  metre** of curvature at 20 g — stated in the banner instead of hidden. `FracT` makes the event time
  sub-tick.
- **The arming delay** (`Perf.ArmingS`) is what keeps a launch from detonating on its own carrier: a round
  that leaves the pylon 3 m beside the jet does not count as a hit on it.
- **`ResolveBurst`.** The CPA vector is rotated into the body frame of the TARGET (`FBEnuToBodyVec`, with
  the published attitude) and handed to `core/FBDamageModel` — via the unit that owns the register. The
  WEAPON supplies a number (explosive mass), the target's MODULE a table (where its systems sit), neither
  of them decides anything.
- **`ResolveGunHit`.** Structurally identical; what differs is what arrives: a warhead is a mass from
  which the model derives an energy — a burst is a NUMBER of projectiles in a pattern, so the energy
  density is computed here (miss distance, pattern width `σ = DispersionSigmaRad · PathM` with a minimum
  of 0.05 m, relative velocity, presented area). Gates: `kMinReportedHits = 0.1` (a tenth of a projectile
  is comfortably below one hit and comfortably above the noise of the continuous density function),
  `kGunHitReachM = 8 m` (half the wingspan of a fighter, ADDED to 3σ — not a hit radius, but the point
  beyond which the density model can only deliver zero), `kGunNearMissM = 200 m` (from when a near miss is
  worth a line at all). The launcher is exempt from being shot at (`LauncherId`).
- **`GroundCrossing` — the sub-tick impact.** The judge runs at 0.1 s, so a store, by the time it is
  observed as "penetrated", is already up to one tick below the surface: **measured on a Mk-82 at
  216 m/s: 14 m depth, hence ~20 m of horizontal travel beyond the true impact point** — a fifth of the
  entire release error the attack missions are supposed to measure, and a pure sampling artefact.
  Reconstruction: depth / sink rate = `backS`, position projected back linearly (the curvature of the arc
  over ~0.1 s is worth centimetres). That is only possible because a weapon **deliberately gets no ground
  to collide with** (`FBFdm::kNoGroundElevM`) and therefore flies ballistically to the last.
  **Deliberately NOT** interpolated between the last two published poses: by the time the physics judge
  concludes, the previous pose is already below the surface too, so there is no bracketing pair.
- **Why a weapon gets no ground:** JSBSim's ground reactions model a RESTING object — the two STRUCTURE
  contacts of the Mk-82 model are a spring with 10,000 lbf/ft and 200,000 lbf/ft/s damping. At 150 m/s
  that is a stiff ODE which **diverges within a single step** (measured: the integration blows up on the
  contact step, leaving no impact state to report). A store does not bounce — it detonates.
- **`ResolveGroundBurst`.** The unguided counterpart, the same 1/r² fragment physics. The proximity gate
  is DERIVED instead of chosen: the lowest threshold that the target's own layout declares is the least
  energy that can do anything to it at all. Against **aircraft** a ground burst is expressly NOT resolved
  — the fragment geometry against an airframe does not exist, and an invented radius would be a number
  passing itself off as physics.
- **`stores DELIVERY`.** The fire control's prediction travels along on the round
  (`FBStoreTrack::Solution`) and is emitted next to the measured impact: `predErrM` = what the COMPUTER
  had wrong (coarse stored table against the aerodynamics of the model — the error the CCIP/CCRP setup is
  meant to expose), `aimErrM` = what the DELIVERY had wrong, plus longitudinal/lateral error in the
  round's approach direction and the time-of-flight difference.

---

### 9. Multi-unit — what each stage built

| Stage | Content | Proof |
|---|---|---|
| **1** | The `fdm/` adapter is instance-capable: `FBFdm` is one object per airframe, no global instance, no static mutable globals. | `make -C sim test-fdm` → `build/fb-test-two-fdm` (two diverging airframes + a third reproducing the first bit for bit) |
| **2** | The actor is ONE object: `units/FBSimUnit`. | — |
| **3** | The flight is MISSION DATA: `.fbm` carries a list of `unit` blocks; every client holds an `FBActorList`; every unit has its own `FBFdm`/`FBModule`/monitors/telemetry file; the mission verdict is passed PER UNIT. The **snapshot discipline stands from here on**, although nobody read cross-unit yet. | — |
| **4** | **Thread per unit, but ONLY in the gym** (`fb-gym --threads N`, default 1). Parallelises EXACTLY one phase: the STEP. | fingerprint comparison, see below |
| **5** | Units see each other — but only through one system: the cooperative datalink. | — |
| **6** | The FCR radar as the active sensor alongside. | — |
| **8** | The avionics data model (`FBState` as a typed block bus, command/acknowledgement path). | — |

#### Stage 4 in detail — pool and barrier

`missions/FBTickPool` is a C++17 in-house build (`std::barrier` is C++20) and **GYM-ONLY by decision**: native
and wasm stay single-threaded in the sim loop (real time needs no parallel physics, and the browser is
spared the pthreads/SharedArrayBuffer build). The header is included **exclusively** by
`missions/FBMissionRunner.cpp`, is NOT part of the core lib and never reaches the WASM build.

| Element | Behaviour |
|---|---|
| Threads | N−1 workers, created ONCE for the run (at 10 Hz over thousands of ticks, one thread per tick would be pure spawn overhead), parked on a condition variable |
| `RunTick(job, count)` | calls `job.RunIndex(0..count−1)` **exactly once per index**, distributed over the workers PLUS the calling thread |
| Schedule | **dynamic**: an atomic counter (`Next_.fetch_add`) — whoever is free takes the next index. Intended for uneven load (a unit on the ground next to one in cruise). |
| Barrier | the RETURN of `RunTick` IS it (`Done_.wait(Busy_ == 0)`) |
| `--threads 1` | creates **no** thread at all; `RunTick` degenerates into an inline loop — the sequential reference path, structurally the same code instead of a second one |
| `Generation_` | counted up per tick: the wake-up predicate, immune to spurious wakeups; also incremented in the destructor, so that a parked worker sees the change and not just `Stop_` |
| Calling thread | works along (one thread fewer to wake, and it cannot idle at the barrier) |
| Sizing | clamped by the runner to the cast size — more threads than actors is idling, not speed |
| Declaration order | The pool is declared LAST, hence destroyed FIRST: its threads are joined while buffers and job are still alive |
| Logging about the pool | **nothing.** How many threads stepped the cast is a property of the client, not an event of the mission — a line about it would be the ONLY difference between a sequential and a parallel `events.log`. |

#### What stays sequential — and the respective reason

| Stays sequential | Reason |
|---|---|
| Loading/spawning the models | JSBSim's static `Element::convert` unit table is MUTATED with `operator[]` while parsing XML (→ [`fdm.md`](../fdm.md) §3) |
| Elevation sampling | The provider is the ONE shared object of the client (`FBTilesElevation` drives the tile streamer); and one point query per tick is too cheap to raise the question |
| `PublishPose` | That IS the barrier |
| Both monitors + envelope checks | The verdict that ends a run, and the lines about it, must be read in actor order, never in completion order |
| Telemetry sampling | Decision; the bus is per unit anyway |
| `FBMissionTickHook` | The renderer of the native oracle, single-threaded by decision |
| Growth of the actor list | §6 |

#### Log and telemetry without loss of determinism

- **Telemetry** has long been per unit (own bus, own file) and is sampled in the SEQUENTIAL phase — so no
  problem at all.
- **`FBLog`** keeps its static facade (cross-cutting infrastructure; the alternative would have been to
  thread a context object through every `Run()` signature — exactly what the facade avoids), but its
  **CONTEXT is `thread_local`**: `TimeS_`, `Unit_[32]` and a `ThreadSink_`. The **CONFIGURATION**
  (`Sink_`, `Level_`) stays process-wide — it is boot configuration.
- **No worker ever writes directly into a shared sink.** `FBActorStepJob::RunIndex` installs, via RAII
  (`FBLogThreadSinkScope`), the `FBBufferedLogSink` **of the unit** this thread is computing, plus an
  `FBLogUnitScope` with its callsign, and stamps the simulation time (`FBLog::SetTime`) — the worker
  learns the tick from the job, because the clock is thread-local.
- **At the barrier** the runner drains the buffers **in unit order** into the real sink
  (`for (auto &l : actorLogs) l.Drain(logSink)`). With that, not even the position of a line hangs on the
  scheduler. `FBBufferedLogSink` copies `tag`/`event` as `std::string` (a buffered line outlives the
  `Emit()` call, and only string literals would have survived that by accident) and keeps its capacity
  while draining — a steady-state tick without log output allocates nothing.
- The job passes `world = nullptr` through, exactly as before.

#### Determinism proofs (from CLAUDE.md)

- `payerne-pair`, `payerne-pair-fail`, `payerne-four`, `payerne-mixed` deliver, over `--threads 1..4` and
  **5 repetitions** each, ONE single fingerprint: SHA-256 of all `telemetry*.csv` + normalised
  `events.log` + exit code, including the `decisive=` attribution.
- The 7 single missions × `const`/`swiss` are **byte-identical** with the default to the state before
  stage 4.

#### Scaling, honestly

| Measurement | Value |
|---|---|
| Cost of one F-16 step | ~95–100 µs, practically **phase-independent** (ground roll vs. cruise ≤ 7 % difference) — a mission can therefore hardly generate uneven load across flight phases |
| 2 units, 2 threads | **1.29–1.41x** |
| 4 units, 2 / 3 / 4 threads | **1.49x / 1.53x / 1.77x** |
| Machine | Apple A18 Pro, 2 P + 4 E cores |
| Two INDEPENDENT `fb-gym` processes | 0.42 s alone → **0.58 s each**, i.e. 1.45x aggregate — they scale just as badly |
| Spin-before-park variant of the barrier (built, measured, discarded) | 1.41x vs. 1.41x at two threads; 1.72x vs. 1.68x at four — within the run-to-run spread |

**The ceiling is the MACHINE, not the barrier.** Threading pays off from ~4 units on real performance
cores; below that it is a factor < 1.5.

---

### 10. Telemetry at N > 1

| Rule | Content |
|---|---|
| One file per unit | The primary actor (index 0) keeps the canonical name `telemetry.csv`; every further one gets `telemetry_<callsign>.csv` |
| Callsign safety | The parser restricts the callsign to `[A-Za-z0-9_-]`, so that it is file-safe |
| Fixed column count | per file; new sources append at the end (§3) |
| A store gets its file when it appears | If opening fails it still flies — only its trace is missing (`StartTelemetry(nullptr)`) |

**Why no wide row with prefix columns:** the column set of an actor follows ITS module. A shared row
would either force all modules into one schema or make the header depend on the cast of the mission. The
file-per-unit needs no special case at N=1 — the rows stay byte-identical.

---

### 11. `FBModelRoots` — the ONE model root

**Why one.** Everything FlightBox flies lies under `sim/assets/aircraft` — a self-contained directory per
model (`.xml` plus its own `engine/` and `Systems/` subdirectories, JSBSim's own per-aircraft layout).
Today `f16`, `mk82` and `aim120`.

The pinned submodule is **no longer a load path but the base**: the upstream state against which
`make -C sim verify-models` diffs every copy (see [architecture.md](../architecture.md)'s delta rule,
delta rule). The earlier two-root split ("from the submodule, because read-only" versus "with us, because
the submodule does not have it") no longer holds as soon as a model may be corrected: loaded from the
submodule it cannot carry a correction, loaded from a copy of unknown provenance it is no reference. With
that, `FBModule::FdmModelVendored()` also fell away without replacement — a distinction without an
effect.

| Client | Model root |
|---|---|
| native / gym (relative to `sim/`) | `assets/aircraft` |
| WASM (embedded FS, see `--embed-file` in the `wasm` target) | `/fb/aircraft` |

`FBNativeModelRoots()` is the ONE definition of the native/gym root for every client that runs from
`sim/` (both apps and every test harness) — instead of string literals that can drift apart.

**Lies in `missions/`**, because only `missions/` and `clients/` boot an airframe (the IC gate): nothing under `systems/` or
`modules/` reaches a model path, no more than an initial condition.
