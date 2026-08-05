# FlightBox Core (`sim/src/core/`)

**Source**: the source files themselves — `sim/src/core/` (53 files) + `sim/src/math/FBMat4.h`, state
commit `9673e00` (2026-07-27) — plus `CLAUDE.md`'s `core/` paragraph in the `sim/src/` directory tree.
The long comment banners of the source files carry the DERIVATIONS; they are taken over here in full,
every number with its justification or its marking as a setting. Where code and CLAUDE.md diverge, that
is recorded under [Gaps](#gaps) — not silently resolved.

**What this layer is responsible for**: the VALUE TYPES of the simulation (what a store, a contact, a
waypoint, a command IS), the shared PRIMITIVES (geodesy, atmosphere, ballistics, units), the two
OBSERVATION CHANNELS (log, telemetry), the AVIONICS BUS (state + command) and the three states no
module may write: the two incorruptible judges and the health register.

**What it is NOT responsible for**: behaviour. No control law, no sensor, no pilot, no renderer, no
JSBSim seam. `core/` does not know that an F-16 exists, it knows no `FGFDMExec`, and it owns no unit.
It is used by `systems/`, `modules/`, `units/`, `fdm/`, `missions/`, `clients/` — and uses none of them.

---

## Spec

`core/` is the value layer of the simulation: what a store, a contact, a waypoint, a command **is**,
the shared primitives (geodesy, atmosphere, ballistics, units), the two observation channels, the
avionics bus (state + command) and the three states no module may write. It carries **no behaviour** —
no control law, no sensor, no pilot, no renderer, no JSBSim seam.

| Contract | Acceptance / measurement anchor |
|---|---|
| `core/` never points at `systems/` or `modules/` | include graph; the `core-lib` target builds without either |
| I/O-free, not format-free | no `FILE*`/`fstream`; `snprintf` into a local buffer is allowed (`../conventions.md`) |
| The avionics bus is a set of typed output blocks, each with **exactly one** writer and a three-state validity head (`Invalid` / `Valid` / `Held`) | a block whose writer is unpowered or destroyed reads `Invalid`, never a stale number; the `blk_*` telemetry columns make validity observable per tick |
| Avionics is operated through a command path with acknowledgement, two latency classes and a closed rejection catalogue | a command into a destroyed box acks `rejected/system_failed`; every issue/ack/reject shows up in `events.log` |
| The two judges belong to the client, never to the module | `grep -rn 'FBFlightMonitor\|FBMissionMonitor' sim/src/systems sim/src/modules` stays empty |
| The health register is monotone and writable by exactly one class | every mutator private, single `friend FBDamageModel` — enforced by the compiler, not by convention |
| Damage resolution is deterministic | same geometry → same damage picture, thread-independent (measured) |
| Every number carries its provenance | derived / measured / `[SET]` — see `../conventions.md` |

## State

Built and in service; 53 files plus `math/FBMat4.h`.

| Piece | Status | Anchor |
|---|---|---|
| Avionics block bus (17 blocks) + command bus | built | `071ea2b` |
| `FBCommandBus::AckOf(seq)` — the answer to ONE posted entry | built | a fixed ring of the last 16 completions. It exists because a poster that must know whether ITS entry landed cannot read `LastAck()`: between `Post()` and the box's answer sit four seconds and every other switch throw in them. First consumer: a tactical order whose act is a typed entry ([`player-layer.md`](player-layer.md) §12 M7) |
| `FBForcePicture` — WHAT ONE FACTION HAS COLLECTED | built | the tactical map's only data product, built from PUBLISHED BLOCKS ONLY (one `Ingest(FBState, pose)` per contributor; six sources: own position, PPLI, the members' `FBNetReport`s, the controller feed, own echoes, the eye and the warning receiver). It names no registry — `verify-layers` still prints **six** readers. APP-6 affiliation, and **HOSTILE is never derived**: a PPLI is FRIEND, an echo with a valid Mode 4 reply is FRIEND, everything else is UNKNOWN |
| `FBTacticalOrder` — what a commander may say to a unit's AI | built | seven kinds, four refusal reasons, no identity of anything it points at: an order to attack names a PLACE, because a place is what the commander's own picture holds. Consumed by `pilot/FBPilot`; there is no path from it to a state write |
| `FBLog` / `FBTelemetry`, thread-local context for the gym parallel path | built | `e4d7c26`, `6d7ed5a` |
| `FBFlightMonitor` (physics KO) | built | `28e74e5` |
| `FBMissionMonitor` (mission verdict) | built | `92fe8a4` |
| Objectives, roster, team-capable verdict | built | `82df2e2` |
| `FBSystemHealth` + `FBDamageModel` | built | `6d84647` |
| Gun catalogue, gun ballistics, projectile pool | built | `a1a8fbf` |
| Free-fall ballistics (the shared CCIP/CCRP primitive) | built | `1eeff72` |
| Elevation providers: constant, runway plateau, baked Swiss DEM | built (the tiles provider lives in `world/` and is **not** part of the core lib) | `705c90a` |
| Calendar (`FBCivilTime.h`: days-from-civil, strict ISO-8601-Zulu parse/format) + sun/moon ephemeris (`FBEphemeris.h`) | built. The ephemeris moved DOWN from `render/` in the C2 round: `core/`/`sensors/` may not include `render/`, and visual acquisition needs the sun. Pure functions, no state — the move is proven pixel-exact (`--utc 922312800`, SVS + EVS PNGs byte-identical) | this round |

Everything below under Knowledge is the distilled per-file detail, every constant with its provenance.

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `core/FBFlightMonitor.h` | banner locates the off-runway verdict in `FBMissionRunner.cpp`; it has lived in `core/FBMissionMonitor::Tick` since the mission monitor exists |
| `core/FBStateBusTelemetry.cpp` | banner counts "the two blocks added afterwards (Rwr, Cmds)" — there are three, `blk_gun` follows the same rule |
| `core/FBDamageModel` | `kMaxZones = 5` is coupled to `FBDamageZone` but **not** compiler-checked; a new zone disappears silently in `AddKinetic`'s range check |
| `core/FBAvionicsBlocks.h` | `FBStoresBlock::Arm` defaults to `Arm`, `FBGunBlock::Arm` to `Sim` — asymmetry without a source, and the armed side is the less conservative one |
| `core/FBFlightPlan` | `FBWaypointType` declares four types, the parser produces two |
| `math/FBMat4.h` | breaks the tree's coding convention (pre-pivot inheritance) |

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| `GroundElevPatch` declared, unimplemented | no terrain following, no CFIT prediction. Parked here because the provider hook is a `core/` type; it moves to the world-side file when `world/` is split. |

### Inventory (from the previous `Offene Punkte` section)

Gaps, inconsistencies and questions found — none of it written away or glossed over.

1. **CLAUDE.md names the block list incompletely.** The `core/` paragraph lists "Platform, Env,
   AirData, RadarAlt, Nav, Cruise, FireControl, Ufc, Stores, Airframe, Warnings, Radar, Datalink,
   Bfm" (14). `FBState.h` carries **17** today: additionally `Gun`, `Rwr`, `Cmds`. The code is the
   truth; CLAUDE.md is out of date at this point.

2. **The banner of `FBStateBusTelemetry.cpp` does not name the gun block.** It justifies the frozen
   column list with "the two blocks that were added afterwards (Rwr, Cmds)"; in fact there are
   **three** — `FBGunSystem::DeclareTelemetry` declares `blk_gun` with the same justification as its
   own first column. Everything is factually right, only the enumeration in the comment is incomplete.

3. **The banner of `FBFlightMonitor.h` locates the off-runway verdict wrongly.** It writes that this
   verdict belongs to "the caller who knows the mission (`FBMissionRunner.cpp` scores it as FAIL, a
   separate, still incorruptible, runner-owned check; see its own banner)". The check meanwhile lives
   in `core/FBMissionMonitor::Tick` (`OnRunway`, 50 m/30 m margin). The comment dates from before the
   mission monitor.

4. **`FBStoresBlock::Arm` and `FBGunBlock::Arm` have different struct defaults** — `FBArmState::Arm`
   resp. `FBArmState::Sim`. Both are written at runtime by their respective system, so the difference
   is presumably without consequence; whether the asymmetry is intended is said by no source. A
   zeroed/uninitialised published stores block reads as ARMED, which is the less conservative of the
   two defaults.

5. **`FBFlightPlan` declares four waypoint types, the parser produces two.** `Takeoff` and `Approach`
   are declared in `FBWaypointType`, but `FBParseMissionFile` produces exclusively `Enroute` (`wp`)
   and `Land` (`land`). Not an error — only an as yet unused part of the type; whoever reads the enum
   should know that.

6. **`FBNavBlock::MagVarDeg` is a placeholder (always 0).** Marked as such in the block comment. Any
   magnetic bearing that were derived from it anywhere is a true bearing today.

7. **`FBEnvironmentBlock` is not an avionics block**, but is treated like one (own validity head, own
   writer = the client). Expressly justified as such in the comment ("not avionics in the airframe
   sense, but shared per-frame state with exactly the same producer/consumer question"). For a reader
   who reads the bus as an aircraft-system bus, that is a surprise, noted here.

8. **`kMaxStoreStations = 12` against nine F-16 pylons.** The block reserves twelve station slots; the
   F-16 declares nine (`modules/f16/FBF16Sms`). No contradiction (capacity ≥ demand), but the three
   surplus slots are unoccupied and permanently carry 0.

9. **`FBDamageZone` has five values including `None`, `FBSystemHealth::kMaxZones` is 5.** The coupling
   is named in the comment (`kMaxZones = 5 /* core/FBDamageModel's FBDamageZone, including None */`),
   but it is NOT compiler-checked: a `static_assert` against the enum size is missing, and
   `core/FBSystemHealth.h` deliberately does not include `FBDamageModel.h` (the dependency runs the
   other way round). A new zone would silently point past the end of the `Kinetic_` array — caught only
   by the range check in `AddKinetic`, which then DISCARDS the contribution.

10. **`FBGunProjectiles` never resolves against terrain.** Explained and justified in the banner
    (air-to-air is what the pool is for), recorded here only as a known limit: there is no strafing
    footprint on the ground.

11. **`FBDamageModel::ApplyKinetic` sums, `Apply` does not** — deliberately and with justification
    (stream vs. event). A consequence stated nowhere: **a warhead burst never profits from kinetic
    energy taken earlier, and vice versa.** The two effects share the register (the system states) but
    not the energy bookkeeping — `Kinetic_` is purely kinetic. Whether an airframe that already has 50
    gun hits should react more sensitively to a fragment burst is an open modelling question.

12. **`FBLog::Unit_` is limited to 32 characters, callsigns to 24** — that fits, but the coupling is
    not fixed as a `static_assert`; `snprintf` would silently truncate.

13. **`math/FBMat4.h` does not follow the coding convention of the tree** (free `static` C functions, no
    `namespace FlightBox`, no `FB` prefix on the functions). It is the oldest file of this collection
    and evidently a pre-pivot inheritance. Not a defect, but a style break that a future reader might
    otherwise take for an intention.

14. **The enumeration "53 files, ~4,800 lines" of the task statement is correct** (`ls | wc -l` = 53;
    `wc -l` over `core/` + `math/` = 4,927 including `FBMat4.h`).

15. **Not in `core/` and therefore only mentioned here**: `FBTilesElevation` (the fourth elevation
    provider) lies in `world/` and is NOT part of the core lib — `fb-gym` does not link it. Whoever
    wants to read the provider list in full has to go there.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 0. The rule that defines `core/`

| Rule | Evidenced by |
|---|---|
| `core/` NEVER includes `systems/`, `modules/`, `units/`, `fdm/`, `render/`, `world/`, `missions/`, `clients/` | `grep -rn '#include' sim/src/core` finds exclusively `"FB*.h"` from `core/` itself and standard headers (verified) |
| `core/` is I/O-free — no `FILE*`, no `fstream`, no `printf` | two named exceptions, see below |
| Formatting is allowed | `snprintf` into a local buffer, everywhere (`FBLog.cpp`, `FBTelemetry.cpp`) |
| Events run through `FBLog`, periodic state through `FBTelemetryBus` | no scattered `printf` in `core/systems/modules/render/world/fdm/units` |

**The I/O exceptions, both justified:**

| File | What it does | Why it is not a violation |
|---|---|---|
| `core/FBLog.cpp`, `core/FBTelemetry.cpp` | `<cstdio>` for `snprintf` — pure formatting into local buffers | no file handle, no stream; the SINKS live in `clients/` (`FBLogSinks.*`, `FBTelemetrySinks.*`) |
| `core/FBBakedDemElevation.cpp` | `fopen`/`fread` of ONE static data asset at construction | the same category in which JSBSim loads its own model XML — an asset load, no streaming, no network. Expressly justified as such in the header: "core/, not world/". |

#### File inventory by subject

| Subject | Files | Section |
|---|---|---|
| Avionics bus (state) | `FBState.h`, `FBAvionicsBlocks.h`, `FBBlockStatus.h`, `FBStateBusTelemetry.h/.cpp` | [1](#1-the-avionics-bus) |
| Avionics bus (command) | `FBAvionicsCommand.h`, `FBCommandBus.h/.cpp` | [2](#2-the-command-side) |
| Observation channels | `FBLog.h/.cpp`, `FBTelemetry.h/.cpp` | [3](#3-the-two-channels) |
| Judges | `FBFlightMonitor.h/.cpp`, `FBMissionMonitor.h/.cpp` | [4](#4-the-two-judges) |
| Mission data | `FBMissionFile.h/.cpp`, `FBFlightPlan.h`, `FBRunway.h`, `FBSpawn.h`, `FBObjective.h`, `FBTeam.h`, `FBMode.h`, `FBMasterMode.h`, `FBArmState.h` | [5](#5-mission-data-as-types) |
| Damage | `FBSystemHealth.h/.cpp`, `FBDamageModel.h/.cpp` | [6](#6-damage) |
| Weapons + ballistics | `FBStore.h`, `FBBallistics.h/.cpp`, `FBGun.h`, `FBGunBallistics.h/.cpp`, `FBGunProjectiles.h/.cpp`, `FBWeaponUplink.h` | [7](#7-weapon-value-types-and-ballistics) |
| Sensor/EW value types | `FBRadarContact.h`, `FBDatalinkTrack.h`, `FBEmitter.h`, `FBRwrThreat.h`, `FBCountermeasure.h` | [8](#8-sensor-and-ew-value-types) |
| Elevation hook | `FBElevationProvider.h`, `FBConstantElevation.h`, `FBRunwayPlateauElevation.h/.cpp`, `FBBakedDemElevation.h/.cpp` | [9](#9-the-elevation-hook) |
| Base types/mathematics | `FBGeodesy.h`, `FBAtmosphere.h`, `FBUnits.h`, `math/FBMat4.h` | [10](#10-geodesy-atmosphere-units-mathematics) |

---

### 1. The avionics bus

`core/FBState.h` is the ONE shared per-frame state. It is **not a flat bundle of fields** but a set of
typed OUTPUT BLOCKS (`core/FBAvionicsBlocks.h`), each with a validity head (`core/FBBlockStatus.h`).

**Why blocks** (`FBState.h` banner): the flat version could not express "this value does not apply
right now" — and with damage/failure that is exactly the first thing a display and an AI pilot need to
know. It also had no compiler-checkable answer to "who wrote this field": a maintenance check found
**ten dead fields and four that were read but never written**.

**The ONE rule**: every block has EXACTLY ONE writer (named in the block comment) and any number of
readers. No reader writes, no block has two writers.

**`NowS`** is the bus's time reference: the module stamps it once per `Run()` from its sim clock,
BEFORE it ticks any slot; every block-head timestamp comes from it. One clock for the whole bus is what
makes "how old is this block" answerable without every system keeping its own now.

**What is taken over and what is not** (`FBAvionicsBlocks.h` banner, model MIL-STD-1553): taken over is
the SEMANTICS — defined data groups, one producer, a validity flag. NOT taken over: bus addressing,
word packing, message scheduling. The transport is a typed structure by reference in ONE address space;
inventing remote terminal addresses for it would be cargo cult.

#### 1.1 `FBBlockStatus` — the three-state validity

`core/FBBlockStatus.h`. Three states, not two, and the third one is EVIDENCED instead of invented.

| State | Meaning | The consumer does |
|---|---|---|
| `Invalid` (0) | The numbers mean nothing. Never written, or the source system is off/failed. | declutter/dash the display |
| `Valid` (1) | Written by its one writer at time `StampS`, current there. | read normally |
| `Held` (2) | The writer has DELIBERATELY stopped updating. The fields still carry the last good values, `StampS` still the time of the last REAL update. | keep showing the last good value, mind the age |

**The derivation of `Held`**: DCS/ED model several computed CRUS page fields as FREEZE-AT-LAST-VALUE as
soon as the gear is down — they stop updating, they do not go blank
(`doc/modules/f16/controls-commands.md`, "The DED's propose -> commit/reject protocol"). A failed system and a
deliberately frozen one are different facts, and a consumer reacts differently to them (clear the cue
vs. leave the last good number standing).

**`StampS` does NOT move on `Hold()`.** Precisely that makes "how old is this held number" an
answerable question.

| Method | Contract |
|---|---|
| `Publish(nowS)` | `StampS = nowS`, status → `Valid` |
| `Hold()` | `Valid` → `Held`; a block never published has nothing to hold and stays `Invalid` |
| `Invalidate()` | status → `Invalid` |
| `Readable()` | `Status != Invalid` — the usual question: may I read the numbers at all? `Valid` AND `Held` say yes |
| `AgeS(nowS)` | `nowS - StampS` |

#### 1.1a What the head does NOT answer: **is there a BOX**

A head is a statement about a **product** — "is there a picture". It is not a statement about the
**equipment**, and the two come apart in exactly one place: a box that is powered, healthy and
deliberately producing nothing. A silent radar is the case that matters, because the silence is a
DECISION and every decision has to be reversible.

Each of those blocks therefore carries a small set of fields that are the box's own switch state rather
than its output, written by the system on every `Run()` **before** it decides whether a product exists:

| Block | Control readback (valid whenever the readback's own presence bit is set) | Product (valid under `H`) |
|---|---|---|
| `Radar` | `Powered`, `Radiating`, `ModeOrdinal`, `IffTransponder`, `ScanAzHalfDeg` | `ContactCount`, `LockIndex`, `Contacts[]` |
| `Rwr` | `Powered` | threats |
| `Datalink` / `NetLink` | `Powered`, `Transmitting` | tracks |
| `Irst` | `Powered`, `Searching`, `ModeOrdinal`, `LaserArmed` | contacts |
| `Cmds` | `Mode`, program, quantities | `Dispensing`, `ActiveClouds` |

**The rule, and it is a rule because breaking it cost a whole tournament round.** A consumer that wants
to KNOW something asks `H`. A consumer that wants to CHANGE something — a hand on a knob — asks the
readback. `pilot/FBPilot` asked `Radar.H.Readable()` before commanding an FCR mode, so the one
transition that empties the head (going silent) also removed the only path back out of it: the F-16 in
`sat-02-picture-split` switched its radar off once at t = 57.5 s and stayed off for the remaining 462 s,
while its own EMCON decision said "silent" for 10 of 5,200 ticks. `Radar` had no `Powered` bit to ask —
its two nearest siblings did — and that absence is what made the substitution look reasonable.
[`pilot.md`](pilot.md) §7.6b, [`doctrine-evolution.md`](doctrine-evolution.md) `X-6`.

`FBRadarBlock::SetAbsent()` exists for the other half of the same distinction: the module branch that
does not reach `FBRadarSystem::Run` at all (the set is shot away) must clear the readback as well as the
head, or a dead box goes on advertising the switch position it died in.

#### 1.2 The blocks and their writers

`FBState` carries **17** blocks today. Order = declaration order in `FBState.h`.

| Block | Writer (the ONE) | Content |
|---|---|---|
| `FBPlatformBlock Platform` | the owner of the FDM state: the module publishes from the `st` it gets per `Run()`; the client re-publishes the same structure with the live pose of the frame (`units/FBSimUnit::HudState`) | roll/pitch/yaw, `AltM` (ASL geodetic), ENU offset from the sim origin, GS/TAS/VS, home distance/bearing (relative to the nose, −180…180), `FBMode Mode` (the REAL, confirmed guidance mode) |
| `FBEnvironmentBlock Env` | the client (ephemerides + live weather), never a module system | cloud cover total + 3 layers (0..1), cloud base AGL (0 = unknown), sun/moon el/az, moon phase (illuminated fraction) |
| `FBAirDataBlock AirData` | `systems/FBAirDataSystem` | CAS, Mach, `GLoad` + running `GLoadPeak` since boot, `TrackDeg` (ground track true 0..360), `FpaDeg` (flight path angle, + = climbing) |
| `FBRadarAltBlock RadarAlt` | `systems/FBRadarAltimeter` | `AglFt` |
| `FBNavBlock Nav` | `systems/FBNavSystem` | steerpoint bearing/elevation angle/distance (nm)/elevation (ft ASL), bullseye bearing (FROM the bullseye TO the aircraft)/distance, `MagVarDeg` (placeholder 0) |
| `FBCruiseBlock Cruise` | `systems/FBNavSystem` (one source system may publish more than one message) | `SteerTtgS` |
| `FBFireControlBlock FireControl` | `modules/f16/FBF16FireControl` | four products, see [1.3](#13-the-firecontrol-block--four-products-under-one-head) |
| `FBUfcBlock Ufc` | `modules/f16/FBF16Ufc` | `AlowFt`, `BingoLbs` + `BingoEffectiveLbs`, `SteerNum` |
| `FBStoresBlock Stores` | `weapons/FBStoresSystem` (the F-16 fills the slot with `modules/f16/FBF16Sms`, which contributes ONLY pylon geometry) | `FBArmState Arm`, station count, selected station (1-based, −1 = none), `Station[12]` = `FBStoreKind` ordinal per station (0 = empty), loaded count/weight, `ReleasedCount` |
| `FBGunBlock Gun` | `weapons/FBGunSystem` (F-16: `modules/f16/FBF16Gun`) | `FBArmState Arm`, `Kind` (FBGunKind ordinal, 0 = no gun), rounds remaining, rounds fired, `Firing`, `Ready` |
| `FBAirframeBlock Airframe` | `systems/FBAirframeControls` (via the module that owns the FDM handle) | `GearPosition` 0..1 kinematically delayed, `WeightOnWheels`, `SpeedbrakeNorm`, `FuelLbs`/`FuelPct`, `EngineRunning` |
| `FBWarningBlock Warnings` | `systems/FBWarningSystem` | `Active` and `Inhibited` bitmask |
| `FBRadarBlock Radar` | `sensors/FBRadarSystem`; the module's own no-set branch calls `SetAbsent()` (§1.1a) | `Powered`, `Radiating`, `ModeOrdinal` (a module-owned label, no logic), contact count, `LockIndex` (−1 = no lock), `IffTransponder`, `FBRadarContact Contacts[8]` |
| `FBRwrBlock Rwr` | `sensors/FBRwrSystem` | `Powered`, threat count, `PriorityIndex` (−1 = none), `MissileLaunch`, `Activity`, `HiddenSearch`, `FBRwrThreat Threats[8]` |
| `FBCmdsBlock Cmds` | `sensors/FBCountermeasureSystem` | `FBCmdsMode`, `FBCmdsStatus`, selected program, chaff/flare remaining, `ChaffLow`/`FlareLow` ("LO" lamp), `Dispensing`, counts expended, `ActiveClouds` |
| `FBDatalinkBlock Datalink` | `sensors/FBDatalinkSystem` | `Powered`, `Transmitting`, track count, `FBDatalinkTrack Tracks[8]` |
| `FBBfmBlock Bfm` | `pilot/FBBfmTrack` (published onto the bus by the module after the pilot decision tick) | `Locked`, range, az/el (body-referenced = ATA), closure, aspect (AT THE TARGET: 0 = we sit on his tail, 180 = head-on), HCA, estimated ENU offset + estimated target velocity vector |
| `FBIrstBlock Irst` | `sensors/FBIrstSystem` | `Powered`, `Searching`, `ModeOrdinal`, contact count, `LockIndex` (−1 = none), `LaserArmed`, `CloudMaskedCount`, `FBIrstContact Contacts[8]` |
| `FBVisualBlock Visual` | `sensors/FBVisualSystem` | `Powered`, contact count, `CloudMaskedCount`, `GlareFactor` (1 = no glare), `FBVisualContact Contacts[8]`. **The one block whose writer READS another** — `FBEnvironmentBlock`'s sun angles decide whether an eye works at all; documented, one-way, and named in the block's own comment |
| `FBMfdBlock Mfd` | `systems/FBMfdSystem` | the cockpit's OWN readout: `Pages[8]` (the module's fixed catalogue, ordinal -> role), `Available` (the bit mask the current loadout and the block heads leave standing), `Bay[3]` (which ordinal each display carries, −1 = dark), `LastSelectPage`/`LastSelectS`. It exists so that WHICH page a display shows is itself a published reading rather than renderer-side state — the same rule every other symbol on the screen obeys. **Deliberately absent from `FBStateBusTelemetry`'s column list**: appending there would move a column in every telemetry.csv ever measured |

**Where `Held` is the NORMAL case, not the exception:**

| Block | Why |
|---|---|
| `Radar` | Between two antenna frames the geometry stands still, only the contact age runs — exactly freeze-at-last-value. The head says `Held` until the next completed sweep publishes anew. |
| `Datalink` | The same from the other side: the net refreshes once per cycle, in between the picture stands. |
| `Rwr` | A threat whose emission is no longer heard is carried a little longer before it drops; as long as nothing new is received, the picture stands and only the age runs. |
| `Cruise` | Gear down → the computed CRUS fields freeze while bearing/distance keep running. The origin of the third state. |
| `Bfm` | The head carries the three states of the fusion literally: `Invalid` = never seen, `Valid` = fresh or inside the credible extrapolation window, `Held` = beyond it, where the estimate falls back to the last MEASURED position and is no longer something to pull lead on. **`StampS` is the LOOK** on which the estimate stands, not the publication time. |

**The reference case for `Invalid`** is `RadarAlt`: the CARA is a powered box, and
`doc/modules/f16/controls-commands.md` §6.4 names the consequence verbatim — the ALOW warning fires only with a
powered and transmitting radar altimeter, however willingly the DED accepted the threshold. Unpowered,
the box publishes no 0 ft; it invalidates its block, and every consumer has to say what it does
without it.

**Documented fusion (not accidental coupling)** — the three places where a system reads a foreign block
in order to derive its own output are named in the respective block comment: fire control reads Nav +
Platform; Warnings reads RadarAlt, UFC and Airframe; the cruise freeze is driven by the gear signal of
the Airframe block.

#### 1.3 The FireControl block — four products under one head

All four share the head, because all four are output of THE SAME box and all become invalid together
when the sources they fuse do. Each additionally carries its own "is there a SOLUTION" bit — a fire
control without a target still publishes its block, and that is a different fact from an unpublished
block. **The bus is SI** (metres/seconds); only displays convert to nm.

**(1) Ranging**: `SteerSlantNm`, `RangeProvider` (the letter beside the number, F-16: `'B'` =
baro/steerpoint elevation).

**(2) The air-to-air launch zone (DLZ)** — `doc/modules/f16/weapons.md` §2.5:

| Field | Meaning |
|---|---|
| `DlzValid` | a locked target AND a selected weapon that has a launch zone |
| `TargetRangeM` | slant range of the locked target — what the limits bracket |
| `ClosureMs` | + = closing |
| `RaeroM` | kinematic maximum range against a NON-manoeuvring target |
| `RtrM` | turn-and-run: a hit even if the target turns away at the moment of launch |
| `RminM` | the smallest range that still allows arming + terminal homing |
| `TimeToActiveS` | predicted seconds from launch to the seeker going active |
| `TimeToImpactS` | predicted total time of flight; < 0 = no intercept from here |
| `InZone` | `Rmin <= range <= Raero` — what the SMS launch interlock reads |

**(3) The gun solution (EEGS)**: The EEGS funnel is a SIGHT — its walls are the known wingspan of the
target, drawn at the range for which the gun is correctly leading. A target that fills the funnel is at
the range for which the lead was computed. What this geometry says about a SHOT is three numbers:

| Field | Meaning |
|---|---|
| `GunValid` | there is a solution |
| `GunRangeM` | to the predicted INTERCEPT POINT, not to the target now |
| `GunTofS` | projectile time of flight to it |
| `GunAimErrorDeg` | angle between the demanded barrel direction and the nose |
| `GunLeadAzDeg`/`GunLeadElDeg` | the demanded barrel direction, body-referenced (+ = right/up) |
| `GunSpreadM` | sigma of the dispersion pattern at the intercept point |
| `GunSpanMr` | angular span of the target there, milliradians |
| `GunFunnelTopMr` | its span at the MINIMUM range of the funnel (the narrow end) |
| `GunFunnelBottomMr` | …and at the maximum range |
| `GunTolDeg` | the funnel's own aiming tolerance at this range |
| `GunInRange` | within the range window of the funnel |
| `GunInFunnel` | in range AND the nose within the tolerance of the lead solution |

The guide's out-of-range test ("target smaller than the funnel bottom") is thereby literally
`GunSpanMr < GunFunnelBottomMr`.

**(4) The air-to-ground release solution (CCIP/CCRP)**: ONE integration (`core/FBBallistics.h`), two
questions. The IMPACT POINT is the CCIP pipper; the three ERRORS are that point measured against the
designated aim point along the current ground track, which is what the CCRP solution cue counts down. A
consumer chooses the mode by WHICH numbers it reads — it does not demand that the box be in a mode.

| Field | Meaning |
|---|---|
| `AgValid` | there is a solution |
| `AgImpactLatDeg`/`AgImpactLonDeg` | **the only geodetic `double`s on this bus**: at 1e-5° a `float` is a metre, and the metre is the quantity in which the whole solution is measured |
| `AgImpactElevM` | the plane against which it was solved (that of the ranging source), m ASL |
| `AgTofS` | fall time if released now |
| `AgRangeM` | horizontal aircraft → impact point: the bomb's own throw range |
| `AgAlongErrM` | + = the weapon falls TOO SHORT; 0 = release now |
| `AgCrossErrM` | + = it falls RIGHT of the target — the steering line error |
| `AgMissM` | both together: distance of the pipper from the aim point |
| `AgTimeToReleaseS` | `AgAlongErrM` at the current ground speed; <= 0 = the cue has passed |
| `AgArmMarginS` | fall time remaining AFTER the fuze arming delay has elapsed; < 0 = a dud |
| `AgInRange` | both halves of the guide's release condition as the one bit on which the decision is made: the aim point is still AHEAD of the current impact point (the release moment has not passed) AND a release now would arm before arrival |

#### 1.4 The UFC block carries TWO bingo numbers

`doc/modules/f16/controls-commands.md` §6.8: the DED field shows what the pilot TYPED; the warning fires at the
system limit. Displays read `BingoLbs`, the warning system `BingoEffectiveLbs` — merging the two would
have made the documented clamp invisible.

#### 1.5 The warning block is a bitmask

`FBWarningBit`: `FBWarnAlow` (1<<0, below the CARA ALOW threshold), `FBWarnBingo` (1<<1, fuel at or
below the confirmed BNGO threshold), `FBWarnGearUnsafe` (1<<2, on the wheels without down-and-locked).
A bitmask, so that ONE block carries the whole annunciator panel without growing a field per lamp.

**`Inhibited` is not decoration**: conditions whose SOURCE BLOCK is `Invalid` cannot be evaluated — and
that is a different fact from "does not warn".

#### 1.6 `FBStateBusTelemetry` — validity as a measurable time series

`core/FBStateBusTelemetry.h/.cpp`. A telemetry source named `"blk"` which emits, per block, its
`FBBlockStatus` ordinal (0/1/2).

**Why**: the three-state head is only worth something if it is CHECKABLE — and the values themselves
cannot show it, because a `Held` block carries the same numbers as a `Valid` one. Without this source,
"the datalink picture is frozen between two net cycles" and "the radar altimeter is dead" would both
look like ordinary data in a CSV. With it, both are ONE column.

**The column list is frozen — as a RULE, not as an omission.** It declares 14 names (`blk_platform`,
`blk_env`, `blk_airdata`, `blk_radalt`, `blk_nav`, `blk_cruise`, `blk_firecontrol`, `blk_ufc`,
`blk_stores`, `blk_airframe`, `blk_warn`, `blk_radar`, `blk_datalink`, `blk_bfm`). This source sits in
the MIDDLE of every `telemetry.csv` ever measured; one more name would shift every column to the right
of it. Blocks added later therefore report their own validity as the FIRST column of their own
telemetry source, which the bus registers at the END:

| Block | Where its validity stands |
|---|---|
| `Rwr` | `blk_rwr`, first column of `sensors/FBRwrSystem`'s source `"rwr"` |
| `Cmds` | `blk_cmds`, first column of `sensors/FBCountermeasureSystem`'s source `"cm"` |
| `Gun` | `blk_gun`, first column of `weapons/FBGunSystem`'s source `"gun"` |
| `Irst` | `blk_irst`, first column of `sensors/FBIrstSystem`'s source `"irst"` |
| `Visual` | `blk_vis`, first column of `sensors/FBVisualSystem`'s source `"vis"` |

A block whose head is not in this list is not unobservable; it is observed one column further to the
right.

---

### 2. The command side

`core/FBAvionicsCommand.h` is the VOCABULARY, `core/FBCommandBus.h/.cpp` the WAY. The form is taken
from the documented propose→commit/reject protocol of the DED (`doc/modules/f16/controls-commands.md`,
"The DED's propose -> commit/reject protocol"): a command is `{target, proposed value}`, the
acknowledgement is `{committed, reason}`. Nothing in `FBAvionicsCommand.h` executes anything — the
OWNING system does that at its own rate and answers.

**Why a command path at all, when the AI could call the setter directly** (banner): because a setter
call is not a pilot action. It cannot be rejected, it cannot cost time, it cannot fail to happen while
both hands are flying, and it leaves no trace. All four are real properties of the cockpit, and all
four are reintroduced by this model — an AI that flies the jet through the same vocabulary as a human
is an AI whose advantage is decision quality, not access.

#### 2.1 The targets

`FBCommandTarget` — ordinals are telemetry-visible: **append, never reorder**. Class and group are
derived from `FBCommandClassOf`/`FBCommandGroupOf` (`FBCommandBus.cpp`), not stored on the target.

| Target | Log/telemetry name | Class | Group | Value semantics |
|---|---|---|---|---|
| `RadarMode` | `radar_mode` | HOTAS | Sensors | mode ordinal |
| `RadarRangeNm` | `radar_range_nm` | **DED** | Sensors | nm (typed) |
| `RadarSlewAz` | `radar_slew_az` | HOTAS | Sensors | deg |
| `RadarSlewEl` | `radar_slew_el` | HOTAS | Sensors | deg |
| `IffTransponder` | `iff_xpdr` | HOTAS | Sensors | 0/1 |
| `IffInterrogator` | `iff_interrogator` | HOTAS | Sensors | 0/1 |
| `DatalinkPower` | `datalink_power` | HOTAS | Comms | 0/1 |
| `DatalinkTransmit` | `datalink_xmt` | HOTAS | Comms | 0/1 (EMCON) |
| `DatalinkFilter` | `datalink_filter` | HOTAS | Comms | filter ordinal |
| `DatalinkRangeNm` | `datalink_range_nm` | **DED** | Comms | nm |
| `MasterMode` | `master_mode` | HOTAS | Avionics | `FBMasterMode` ordinal |
| `MasterArm` | `master_arm` | HOTAS | Avionics | `FBArmState` ordinal |
| `AlowFt` | `alow_ft` | **DED** | Avionics | ft |
| `BingoLbs` | `bingo_lbs` | **DED** | Avionics | lbs |
| `SteerpointNum` | `steerpoint` | **DED** | Avionics | number |
| `WeaponSelect` | `weapon_select` | HOTAS | Avionics | selection ordinal |
| `Designate` | `designate` | HOTAS | Avionics | published track number; **0 = break lock** |
| `StationSelect` | `station_select` | HOTAS | Stores | station number |
| `WeaponRelease` | `weapon_release` | HOTAS | Stores | pickle |
| `CmDispense` | `cm_dispense` | HOTAS | Defensive | **0 = the program selected by the PRGM knob, 1..6 = a program directly** |
| `CmConsent` | `cm_consent` | HOTAS | Defensive | CMS aft/right: SEMI/AUTO consent |
| `CmdsMode` | `cmds_mode` | **DED** | Defensive | `FBCmdsMode` ordinal |
| `GunTrigger` | `gun_trigger` | HOTAS | Stores | **duration of the trigger squeeze in seconds** |
| `IrstMode` / `IrstDesignate` / `IrstLaser` | `irst_*` | HOTAS | Sensors | the passive optical head; the laser is the one ACTIVE thing it does |
| `RadarEmission` | `radar_emission` | **DED** | Sensors | a radar with an emission switch SEPARATE from power (the MiG-29's PUR-31); `FBRadarEmission` ordinal |
| `MfdPageSelect` | `mfd_page` | HOTAS | Avionics | **the MODULE's own page ordinal** (`core/FBMfdPage.h` names the roles, the module names the catalogue) |

**Why `MfdPageSelect` carries a page and not a display.** Three bays, one target: the command names
WHICH PICTURE the pilot wants, and the cockpit's placement rule (`core/FBMfdPage.h`,
`kMfdAttentionBay`) decides where it lands. Packing a bay index into the same scalar would be a second
field pretending to be a value — `Designate` carries a track number, `CmDispense` a program, and each
of those is one quantity. A second target for "which bay" is a later decision with its own acceptance,
booked as gap D1 in `doc/modules/f16/cockpit-displays.md`.

**Why it is HOTAS and not DED.** A page button is one press on the display's own bezel, not a typed
field edit — and the classification is load-bearing rather than cosmetic: DED class is refused above
`kDedMaxG`, so a DED-classed page select would go dark in exactly the fight a watcher wants to read.

**Why the value is a MODULE ordinal.** The same reason `RadarMode`'s is. A generic page enum in the
command would let a jet accept a page it does not have; here the module publishes its catalogue
(`FBMfdBlock`), the pilot looks the ordinal up in it, and a page outside the catalogue is answered
`Rejected/OutOfContext`.

**Why `CmdsMode` is DED and `CmDispense`/`CmConsent` are not**: the CMDS mode knob sits on the left
auxiliary console, not on the stick — a hand off the throttle and the head down
(`doc/modules/f16/defence-rwr-cm.md` §2.2). `CmDispense`/`CmConsent` are the CMS switch and stay HOTAS: the
whole point of a countermeasure is that it can be thrown IN THE MIDDLE of a manoeuvre.

**Why the trigger duration is the VALUE** (not a stream of press/release commands): a command models
ONE action, and an action on the trigger is a burst of declared length. A press/release pair could
never carry the bus's 0.5 s floor between two actions on the same switch.

**Why the stores group contains the gun**: a gun is not an SMS, but it is armament and is answered at
the same slot rate — one group for "the things that make this aircraft lethal", so that a trigger acts
at the rate of the box that fires.

#### 2.2 The two latency classes

`FBCommandClass` — `doc/modules/f16/controls-commands.md` §5 is the only quantitative timing material in the
sources.

| Class | What it is | Properties |
|---|---|---|
| `Hotas` | a press or a switch throw | sub-second, usable IN THE MIDDLE of a manoeuvre; the avionics itself uses 0.5 s and 1.0 s hold times to distinguish two commands on the same switch |
| `Ded` | select field → type → ENTR | several seconds, head DOWN, hands off — the class of things a pilot does BETWEEN manoeuvre segments, never during one |

Modelling both as one class would allow an AI to type a steerpoint at 7 g — and that is exactly what
this split forbids.

#### 2.3 Outcomes

`FBCommandOutcome` — four outcomes, because the sources document four distinguishable endings:

| Outcome | Meaning |
|---|---|
| `Pending` | the limbo between `Post()` and the answer of the owning system |
| `Accepted` | confirmed, effect live |
| `Clamped` | confirmed, the field shows what was typed, but a system limit governs the EFFECT (§6.8, the BNGO limit). **Not a rejection: ENTR was successful.** |
| `Inhibited` | confirmed, the effect is blocked by something else (§6.4, ALOW without a powered radar altimeter) |
| `Rejected` | not confirmed; `Reason` says why |

#### 2.4 The complete rejection/reason catalogue

`FBCommandReason`. The **first eight** are the eight documented rejection/precondition patterns from
`doc/modules/f16/controls-commands.md` §6, one to one and in its order. The **last four** are FlightBox's OWN
and are marked as such, because the sources document none of them.

| Reason | Log name | Origin | Meaning |
|---|---|---|---|
| `None` | `none` | — | — |
| `PilotReject` | `pilot_reject` | §6.1 | RCL/RTN — the pilot has changed his mind |
| `HardwarePrecedence` | `hardware_precedence` | §6.2 | a physical switch position locks the software path out |
| `SequencePrecondition` | `sequence_precondition` | §6.3 | a state machine ordering (a roll AP mode needs a pitch mode first) |
| `EffectPrecondition` | `effect_precondition` | §6.4 | accepted, but the EFFECT needs something else (ALOW/radalt) |
| `OutOfContext` | `out_of_context` | §6.5 | the silent no-op: valid command, wrong SOI/wrong mode |
| `NotImplemented` | `not_implemented` | §6.6 | the box exists in the jet, but not (yet) in this simulator |
| `SoftFailure` | `soft_failure` | §6.7 | successful, but with corrupt state (DTE MPD upload with CMDS not in STBY) |
| `ValueClamped` | `value_clamped` | §6.8 | accepted, a system limit governs |
| `OutOfRange` | `out_of_range` | **FlightBox** | §6 closes with the statement that the sources document NO range validation policy ("a FlightBox command-block model will need to invent its own range-validation policy"). FlightBox REJECTS and says so instead of silently clamping; the one documented clamp (BNGO) is `Clamped`, so silence is never an outcome. |
| `ChannelBusy` | `channel_busy` | **FlightBox** | the answer of the latency model: the pilot's hands/head are already occupied with an unfinished command of the same class. DERIVED from §5's press-duration ceiling, claimed by no guide. |
| `Depleted` | `depleted` | **FlightBox** | the box is willing and the command valid, but the magazine behind it is empty. Separate from `OutOfContext`, because an empty dispenser is a fact about the AIRCRAFT which the pilot has to hear differently — and it is the one rejection a defensive system produces IN THE MIDDLE of being shot at. |
| `SystemFailed` | `system_failed` | **FlightBox** | the addressed box is GONE — shot away, not switched off (`core/FBSystemHealth`). Its own reason, because it is neither a context error nor a satisfiable precondition: nothing about the aircraft's configuration brings this command back, and a cockpit that answered a destroyed radar with "wrong mode" would send its pilot looking for a switch that no longer does anything. |

#### 2.5 The value types

```
FBAvionicsCommand { Seq, Target, Value(double), IssuedS, DueS }
FBCommandAck      { Seq, Target, Value, Outcome, Reason, CompletedS }  // Committed() = not Rejected/Pending
```

`Value` carries EVERY payload as a `double` — enum selections travel as their own ordinal, booleans as
0/1. A queue with a variant payload would need a tag and a `switch` at every hop, for no gain: the
TARGET already says how the number is to be read. The acknowledgement is SEPARATE from the command, so
that a consumer can log/telemeter the pair without the queue entry having to survive.

#### 2.6 `FBCommandBus` — what the bus itself enforces

Owned by the MODULE (like the state bus it mirrors). The pilot POSTS, the module hands every due
command to the system that owns it — at THAT system's rate — and that system COMPLETES it. Fixed
capacity, no allocation, no ownership.

**The three rules before any system sees a command:**

| Rule | Mechanics | Rejection reason |
|---|---|---|
| **LATENCY** | a command is not consumable before `IssuedS + LatencyS(Target)`. Nothing an AI does arrives faster than a hand can move. | — (it waits) |
| **OCCUPANCY** | ONE DED entry at a time (a pilot has one head); the same HOTAS switch cannot be operated twice within a press-duration window | `ChannelBusy` |
| **MANOEUVRE LOCK** | a DED entry is head-down, hands-off work; above `kDedMaxG` the pilot flies the jet instead of typing | `SequencePrecondition` |

Everything else — does this value make sense, is this box even installed — is the answer of the OWNING
system, because that is where the knowledge sits.

**The constants, with their derivation:**

| Constant | Value | Derivation |
|---|---|---|
| `kHotasLatencyS` | **0.5 s** | the documented short/long press discriminator (§5): the avionics itself uses 0.5 s to distinguish two commands on one switch — so that is the floor for the effect of a HOTAS action and for the reuse of the same switch. |
| `kDedLatencyS` | **4.0 s** | **FlightBox's own number, derived instead of quoted.** §5 says a DED field entry is "realistically several seconds per field (select field, type digits, press ENTR)" and names no number. Four seconds is the middle of "several" and — the point — an order of magnitude above the HOTAS class, which is the property the model needs. |
| `kDedMaxG` | **1.5 g** | **FlightBox's own number**: no guide names a g limit for data entry. 1.5 g is a shade above level flight — enough that a gentle cruise turn does not lock the DED, low enough that anything recognisable as manoeuvring does. |
| `kTriggerLatencyS` | **0.1 s** | **The one HOTAS action whose latency is NOT a press duration.** `kHotasLatencyS` is the number with which the avionics distinguishes a short from a long press on a MODE switch — it is how long the SYSTEM waits before deciding what the pilot meant. Applied to the trigger it would mean that projectiles leave the barrel half a second after the finger closes. They do not: the delay between press and first round is the gun's spin-up, and that is modelled where it belongs (`core/FBGun.h`'s `SpoolUpS`, 0.3 s). What remains is the FINGER, and 0.1 s is the human actuation time. **It counts**: at a fighter's tracking rates the aiming solution moves about a degree in half a second, which at gun range is five metres of miss — the difference between a burst that hits and one that does not (measured in both directions). |
| `kMaxPending` | **8** | fixed queue capacity; an overflow is rejected with `ChannelBusy` |
| `kTargetSlots` | **32** | a flat array "last completion per target ordinal", O(1) and allocation-free; a `static_assert` forces expansion when new targets are added |

The SPACING between two trigger actuations stays `kHotasLatencyS` like on any other switch: a finger
can pull quickly, but not twice in the same instant.

**The API:**

| Method | Contract |
|---|---|
| `Post(target, value, nowS)` | the pilot's one verb. Returns the acknowledgement as it stands NOW: `Pending` if the command entered the queue, or a final `Rejected` if the bus itself rejected it |
| `TakeDue(group, nowS, out)` | the module side: hand out the next due command of this group. Order-preserving removal; `false` if nothing is due |
| `Complete(cmd, outcome, reason, nowS)` | the answer of the owning system. At the same time it sets the window "this switch has just been operated" |
| `SetLoadFactor(g)` | the manoeuvre state that the DED lock reads — published by the module from the AirData block |

**It is at the same time the RECORDER of the command stream** — `FBTelemetrySource "cmd"` and `FBLog`
source `cmd`:

- Log events: `CMD_ISSUE` (seq/target/value/class/dueS), `CMD_ACK`
  (seq/target/value/outcome/reason/latencyS), `CMD_REJECT` (seq/target/value/reason).
- Telemetry columns: `cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`,
  `cmd_pending`, `cmd_last`, `cmd_last_outcome`, `cmd_last_reason`.

With that, a gym run shows WHAT the AI operated long before there is a display to watch. `Clamped` and
`Inhibited` additionally count as `Accepted` (they ARE confirmed).

---

### 3. The two channels

The separation is sharp and mutually referenced in both banners:
**log = discrete events, telemetry = periodically sampled state.**

#### 3.1 `FBLog` — discrete, greppable events

`core/FBLog.h/.cpp`. A STATIC FACADE, not an owned object: logging is cross-cutting infrastructure that
every layer needs (systems/render/world/fdm), and threading an `FBLog&` through every `Run()` signature
would touch the whole call graph without gaining behaviour. A call site stays a one-liner:

```cpp
FBLog::Warn("pilot", "sink_rate_high", {{"vs", -12.3}});
```

**I/O-free**: emission happens only if an `FBLogSink` is injected (`FBLog::SetSink`); without a sink it
costs a pointer comparison and no formatting. The concrete sinks (stdout/file/fan-out, plus
`FBBufferedLogSink`) live in `clients/FBLogSinks.h` — the one place where raw stdio is allowed.

| Level | `FBLogLevel::Debug / Info / Warn / Error` |
|---|---|
| Default level | `Debug` — every migrated call site previously printed unconditionally, and the WASM browser console is supposed to look unchanged; whoever wants a quieter channel (the mission runner's `events.log`) raises the level explicitly. |

**`FBLogField`** is a `key=val` field: numeric overloads format compactly (`%g`), `int` as a decimal,
`bool` as `0`/`1`, strings unchanged — the sink quotes a value containing spaces (mirroring the old
`events.log` convention `reason="..."`).

**Threading** — the decisive split for `fb-gym --threads N`:

| What | Storage class | Why |
|---|---|---|
| `Sink_`, `Level_` | process-wide static | CONFIGURATION, set once at boot |
| `TimeS_`, `Unit_[32]`, `ThreadSink_` | `thread_local` | CONTEXT: a thread computing unit `two` IS in a different context from one computing `lead`. The alternative (a context object through every `Run()` signature) is exactly what this facade avoids. |

Single-threaded clients (native, wasm) see identical behaviour: one thread, one context.

**Unit attribution**: `FBLog::SetUnit(label)` — if it is set, every line carries `unit=<callsign>` as
its FIRST field (a script splits on the first field, a human sees whose line it is without scanning).
If it is empty, NOTHING is added: the lines of a single unit are the mission's lines and need no
attribution — that also keeps them byte-identical to every regression baseline from before the
multi-unit era. `Unit_` is a fixed 32-byte buffer: it changes per actor per tick and must never
allocate.

**`FBLog::SetThreadSink(sink)`** redirects the output of THIS thread. The mission runner points every
worker at the buffer OF THE UNIT it is computing, never at the shared `events.log` — a worker writing
straight through would make the line order a function of the scheduler. The buffers are drained at the
tick barrier in unit order. **Level and "is anybody listening at all" stay the question of the PROCESS
sink** — a capture buffer is a redirection of an already accepted line, not a second switch.

**Two RAII scopes**, so that no state leaks past a unit:

| Class | Effect |
|---|---|
| `FBLogUnitScope(label)` | sets the attribution and clears it in the destructor — no label can leak onto the lines of the next unit or onto the mission-wide lines between the loops |
| `FBLogThreadSinkScope(sink)` | the same discipline for the capture buffer — a worker returning without clearing would keep writing into it on the next tick, possibly into the buffer of another unit |

#### 3.2 `FBTelemetry` — a time series with a schema

`core/FBTelemetry.h/.cpp`. Classes DECLARE themselves as a source; the emission is CENTRAL.

| Type | Role |
|---|---|
| `FBTelemetryChannel` | `{Name, Unit}` |
| `FBTelemetrySchema` | ordered channel list, `Add(name, unit="")` |
| `FBTelemetryRow` | field buffer; `Push(double)` formats with `%.6f`, `Push(int)`/`Push(bool)`/`Push(string)` |
| `FBTelemetrySource` | interface: `TelemetryName()`, `DeclareTelemetry(schema)` (ONCE, at `Bus::Start()`), `SampleTelemetry(row)` (ONCE per `Bus::Tick()`) |
| `FBTelemetrySink` | interface: `Header(columns)`, `Row(fields)` — concrete implementation (`FBCsvTelemetrySink`) in `clients/` |
| `FBTelemetryBus` | the ONE emitter: `Register(src)` (a BORROWED pointer, the bus never owns a system), `SetSink`, `Start()`, `Tick(simTimeS)` |

**The rule that holds it all together**: a row arises by CONCATENATION — every source pushes exactly as
many fields as it declared channels, in the same order. **Declaration = registration = column order**,
no string-indexed lookup at sample time.

`Start()` first creates the channel `t` (unit `s`), then every source in registration order, and pushes
the header out. `Tick()` starts itself if necessary, pushes the sim time and samples every source into
one row. **A null sink makes `Tick()` a cheap no-op** — the WASM boot leaves it unset.

**The append rule** (CLAUDE.md + the `FBSystemHealth.h` banner): new sources are ALWAYS appended at the
end (`units/FBSimUnit::StartTelemetry`), so that no column ever measured loses its position. That is
why `FBSystemHealthTelemetry` is its own source instead of further columns on an existing one, and why
Rwr/Cmds/Gun carry their block validity themselves (see [1.6](#16-fbstatebustelemetry--validity-as-a-measurable-time-series)).

**The `core/`-owned telemetry sources:**

| Source | Name | Columns |
|---|---|---|
| `FBCommandBus` | `cmd` | `cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`, `cmd_pending`, `cmd_last`, `cmd_last_outcome`, `cmd_last_reason` |
| `FBStateBusTelemetry` | `blk` | 14 × `blk_*` (see 1.6) |
| `FBSystemHealthTelemetry` | `dmg` | `dmg_hits`, `dmg_failed` (bitmask over `FBSystemId`), `dmg_degraded`, `dmg_effective` |

---

### 4. The two judges

Two instances, two QUESTIONS, never mixed into one. Both belong to the CLIENT/runner, both are fed with
a write-protected per-tick sample, both are invisible to `systems/` and `modules/`.
`grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems src/modules` stays without a hit —
the module IS OBSERVED, it does not see the judges.

| | `core/FBFlightMonitor` | `core/FBMissionMonitor` |
|---|---|---|
| Question | "has the AIRFRAME survived" | "has the MISSION succeeded" |
| Source of truth | the pinned JSBSim model itself (attitude, rates, gear position, strut force, ground reaction contact flags, static weight) | the MISSION FILE itself (its own immutable plan/objective/runway copy) + observed position + observed roster |
| Knows NOTHING about | runways, missions, "where a landing should have taken place" | physics/crashes |
| Verdicts | `FBKoReason` | `FBMissionVerdict` (`Success`/`Fail`/`Timeout`) |
| Latching | yes: `Tick()` returns `true` on exactly the ONE tick on which it trips; every later call is a no-op with `false` | likewise |
| Self-log | `FBLog::Error("monitor", "KO", …)` with all measured values | `FBLog::Info("mission", "RESULT", {result, reason})`, plus `WP_REACHED` |

Both are fed by EVERY client that runs a sim loop — `missions/FBMissionRunner.cpp` (fb-gym / `gpu_native
--mission`) just as much as the WASM frame loop (`clients/FBAppWasm.cpp`). ONE definition each, no second
parallel test.

#### 4.1 `FBFlightMonitor` — the physical knockout

`core/FBFlightMonitor.h/.cpp`. **Completely model-derived and airframe-agnostic by construction**: the
class knows no module/aircraft type and no module-declared numbers, only generic FDM/ground-reaction
quantities that every JSBSim model provides. **There is no module-side declaration channel for KO
thresholds at all** — a module cannot argue itself a milder verdict.

**`FBFlightMonitorSample`** — deliberately NOT `fb_fdm_state` (core/ does not depend on fdm/). The
caller fills this narrow view (`FBBuildFlightMonitorSample`, `missions/FBMissionBoot.h`):

| Field | Meaning |
|---|---|
| `LatDeg`, `LonDeg`, `ElevM` | position, altitude m ASL |
| `GroundAslM` | THE SAME per-tick sample the caller has already pulled from its injected `FBElevationProvider` and fed into JSBSim's ground elevation — passed as a `double`, so that the monitor stays a flat value consumer |
| `RollDeg`, `PitchDeg` | attitude |
| `PDegS`, `QDegS`, `RDegS` | body rates, °/s |
| `VsMs` | vertical speed, + = climbing |
| `TasMs` | true airspeed, m/s — **exclusively** to derive the flight path angle, never as an aero/AoA surrogate |
| `GearPosNorm` | 0=up..1=down, the model-owned delayed gear position |
| `GearForceLbs` | peak strut compression force of the wheels in this tick (`FBFdm::GetMaxGearForceLbs`) |
| `WeightLbs` | the model-owned current static weight (`FBFdm::GetWeightLbs`) |
| `AnyWow` | any BOGEY (wheel) contact is compressing |
| `StructureContact` | any NON-wheeled ground reaction contact point is compressing (a declared STRUCTURE point: wingtip/tail/intake/radome of today's F-16 — whatever an `aircraft.xml` declares beyond its landing gear) |
| `FdmFault` | the integrator itself has given up (JSBSim threw e.g. a `FloatingPointException` out of a table lookup) — a plain `bool`, so that the monitor stays fdm-decoupled |

**The check order in `Tick(s, simTimeS)`** — the order is part of the derivation:

| # | Check | Condition | Trigger | Why at this place |
|---|---|---|---|---|
| 0 | `NumericalDivergence` | `FdmFault` OR the first non-finite field of the RAW inputs | 1 tick | **BEFORE everything else**: every other check is a COMPARISON, and IEEE-754 makes every comparison against NaN false. Without this check a diverged FDM sailed past all of them and the run ended as an unexplained TIMEOUT. Checked on the RAW inputs (not on `aglM` or the derived FPA), so that the divergence is caught at its point of entry. |
| 1 | `CfitPenetration` | `aglM < kPenetrationMarginM` (−3.0 m) | 1 tick | before the confirmation group, because its own 3 m margin already absorbs a single-tick terrain jump; a real hole-through-the-mesh should not wait for a window |
| 2 | `StructureContact` | `StructureContact` for `kContactConfirmS` (0.2 s) | confirmed | a binary signal, see below |
| 3 | `GearUpContact` | `AnyWow && GearPosNorm < kGearDownThreshold` (0.5) for 0.2 s | confirmed | a binary signal |
| 4 | `HardLanding` | `AnyWow && WeightLbs > 0 && GearForceLbs > kHardLandingForceFactor * WeightLbs` (3.0×) | 1 tick | checked on EVERY tick with a compressed bogey, not only on the touchdown edge, so that the actual force peak is caught wherever in the compression cycle it lies |
| 5 | `AttitudeContact` | (`AnyWow` OR `StructureContact`) AND (`|Roll| > 15°` OR `Pitch > 15°`) | 1 tick | a geometry risk (tailstrike/structural strike) even with a benign gear load |
| 6 | `Loc` (departure) | airborne AND `sqrt(p²+q²+r²) > kLocRateDegS` (60 °/s), sustained `kLocSustainS` (3 s) | sustained | purely behavioural |
| 6b | `Loc` (stall/mush) | airborne, `TasMs > kMinTasMs` (15 m/s), `|Pitch − FPA| > kNoseFlightpathMismatchDeg` (30°), sustained `kStallSustainS` (4 s) | sustained | purely kinematic |

**The thresholds and their derivation** (all in `FBFlightMonitor.cpp`; "regression" means: a
pre-existing `FBMissionRunner` constant, moved here verbatim):

| Constant | Value | Derivation |
|---|---|---|
| `kPenetrationMarginM` | **−3.0 m** | gear/belly is INSIDE the terrain mesh, not a gentle landing. Regression: `FBMissionRunner`'s pre-existing `aglM < -3.0`, unchanged. |
| `kGearDownThreshold` | **0.5** | JSBSim's own `gear/gear-pos-norm` (0=up..1=down, the delayed kinematic transit of every retractable gear model). Any WOW contact with substantially retracted gear is a belly/gear-up landing, wherever it happens — a purely physical fact, and **0.5 is simply the generic midpoint of this normalised [0,1] range**. |
| `kHardLandingForceFactor` | **3.0** | From the model's OWN landing gear physics, not from a declared sink rate: JSBSim's spring/damper reaction (`GetMaxGearForceLbs`) IS the touchdown load actually simulated; comparing it against the model-owned static weight (`GetWeightLbs`) needs exactly ONE core-owned, airframe-agnostic bound — a load factor beyond which no landing gear of this CLASS is designed. Generic landing gear design practice sizes a limit/hard landing at a low single-digit "g" at the CG; **concentrated on ONE strut** (the check takes the PEAK, not a sum), a cleanly flown landing routinely exceeds 1× total weight on one main gear without being remotely hard. 3.0× lies comfortably above this normal transient and far below a structural-failure-grade impact — conservative in the direction of NOT triggering on a good landing. **Empirically verified**: the reference take-off run never exceeds a small fraction of 1× weight per strut; a deliberately excessive touchdown profile exceeds the bound by a wide margin. |
| `kMaxContactPitchDeg` / `kMaxContactRollDeg` | **15° / 15°** | extreme attitude at ground contact — a geometry risk, generic for any airframe with finite ground clearance; it SUPPLEMENTS (does not replace) the model-driven structure contact check for an airframe whose `aircraft.xml` declares no STRUCTURE points. A conservative, class-generic bound, not tuned to one airframe. |
| `kLocRateDegS` / `kLocSustainS` | **60 °/s / 3.0 s** | Purely BEHAVIOURAL, not an aero-model-specific quantity (**no AoA number** — that means something different per aircraft class and is not even meaningful for some): a sustained, multi-axis body rate magnitude of this size, held for several seconds in the air, is a spin/tumble for any fixed-wing airframe, not a coordinated manoeuvre. Generic engineering judgement, not a per-airframe number. |
| `kMinTasMs` | **15 m/s** | excludes the near-zero-airspeed settling process. Regression: `FBMissionRunner`'s old `cas > 15.0` gate (m/s). |
| `kNoseFlightpathMismatchDeg` / `kStallSustainS` | **30° / 4.0 s** | Stall/mush/deep-stall signature, purely KINEMATIC and deliberately NOT the model-owned `alpha-deg` output (not an aerodynamic quantity, no per-airframe stall AoA; the vanilla model exposes no declared alpha limit from which one could be derived anyway): from attitude + velocity vector alone — where the nose POINTS (`PitchDeg`) against where the aircraft actually GOES (FPA from `VsMs`/`TasMs`, `atan2(VsMs, sqrt(TasMs²−VsMs²))`). A large, sustained difference is flight that is attached/conventional for NO fixed-wing airframe. A fast, shallow, deliberate dive (near-level attitude, high sink rate out of pure speed) keeps this difference small — **empirically verified against a dive test profile** — a stalled/mushing aircraft does not. |
| `kContactConfirmS` | **0.2 s** | **Applied only to the two BINARY, threshold-crossing signals** (`StructureContact` and the `AnyWow` component of `GearUpContact`: JSBSim's WOW flag, a hard true/false about which side of the surface a contact point sits on) which a single-tick terrain jump can flip for exactly one sample: the caller pushes a FRESH terrain elevation into the FDM once per tick (a discrete update, not a continuously tracked surface), and on a live DEM with real longitudinal slope this jump can briefly read as a contact. **Empirically confirmed**: a live-data run without this window falsely triggered `STRUCTURE_CONTACT` on an ordinary take-off run. Deliberately NOT applied to `HardLanding`/`AttitudeContact` — those are smoothly varying PHYSICAL quantities from the aircraft's own continuous dynamics, and (measured) a real hard-landing force peak decays even within a similarly short window; "sustaining" it would make the check miss exactly the impacts for which it exists. 0.2 s is a few ticks at any rate in this codebase (10 Hz mission decision, 60+ Hz WASM frame loop) — long enough to discard a single-sample artefact, far too short to matter for a real sustained contact (seconds, not a fraction of one). |

**`FBKoReason` strings** (as they stand in the `events.log`): `NONE`, `NUMERICAL_DIVERGENCE`,
`STRUCTURE_CONTACT`, `CFIT`, `GEAR_UP_CONTACT`, `HARD_LANDING`, `ATTITUDE_CONTACT`, `LOC`.

**The `KO` log entry** carries everything measured, so that the caller does not have to recompute
anything: `reason`, `detail`, `lat`, `lon`, `aglM`, `vsMs`, `roll`, `pitch`, `p`, `q`, `r`, `gearPos`,
`gearForceLbs`, `weightLbs`.

#### 4.2 `FBMissionMonitor` — the mission verdict

`core/FBMissionMonitor.h/.cpp`. Constructed from the MISSION FILE:
`FBMissionMonitor(plan, objectives, runway, haveRunway, timeoutS, wpCaptureM = 500.0)` — everything
COPIED, never the living, module-mutated exemplar. `wpCaptureM` matches the guidance-side capture
radius (`FBNavSystem::AdvanceWaypoint`, likewise 500 m by default), so that mission verdict and flown
path agree about "reached".

**`FBMissionMonitorSample`** — deliberately narrow:

| Field | Meaning |
|---|---|
| `LatDeg`, `LonDeg` | observed position |
| `AnyWow` | is any gear currently carrying weight |
| `GroundSpeedKt` | the standstill-on-the-runway SUCCESS gate |
| `CombatIneffective` | **the shoot-down as a MISSION fact** (`FBSystemHealth::CombatEffective` negated). It belongs here and not in the physics judge, for exactly the reason that separates the two: the physics judge asks whether the airframe survived, and a jet whose engine has just been shot out is still flying and has survived nothing. Whether its SORTIE is over is a mission question. **The unit is not stopped, frozen or declared dead by it** — it keeps being integrated until the physics judge has its own say. |
| `ReleasedWeapon` | **this unit's own release register**, monotone over the run and filled by the owner from the queue it drains itself — what `no_fire` is judged against. Structurally the twin of the line above: the monitor has no identity and so cannot look itself up in the roster. |
| `Roster` | `FBMissionRoster` — the other units as observed: id, faction, the one bit of their own health register, plus (round `C12`) their release bit and their planar range from THIS unit. Empty for a mission without combat objectives. |

**The check order in `Tick()`:**

1. **Shoot-down** (`CombatIneffective`) — FIRST, because everything below it presupposes an aircraft
   that could still get somewhere. WHOSE failure that is depends on the declaration:

   | Declaration | Consequence |
   |---|---|
   | no `objective` line | `FAIL "combat ineffective (weapon damage)"` — the old reading, unchanged |
   | `objective survive` | `FAIL "combat ineffective (survive objective lost)"` |
   | only `objective kill …` | **nothing** — this unit was not tasked with coming home, its own loss ends nothing. A simultaneous exchange is thereby a trade instead of a mutual failure: a mission design decision of the FILE, not of this class. |

2. **The two kinds that are lost the instant they are broken** (round `C12`), both reading a MONOTONE
   register so that the conclusion is safe to latch: `no_fire` against the sample's own release bit →
   `FAIL "weapon released (no_fire objective lost)"`, and `protect` against the roster →
   `FAIL "protected unit lost: <id>"`, naming the one that went.
3. **Ground contact away from the assigned runway** → `FAIL "touchdown off the assigned runway"`. A
   landing that the pure physics judge accepted (survivable) but which took place in the wrong place
   has missed the mission objective.
4. **Waypoint progress** against its own copy of the plan, purely from the observed position.
5. **SUCCESS needs BOTH halves**: plan complete AND every objective met AND no DEFERRED objective open
   (`survive`, `protect`, `no_fire`, `deny release` — none of them is monotone until the run is over).
6. **Timeout** (`simTimeS >= TimeoutS_`) → calls `Finalize()`.

Before all of it, and before any verdict: the `identify` dwell is accumulated from the roster's ranges
over the elapsed tick (`NoteIdentify`, `Dwell_` index-parallel to the objectives). It is bookkeeping,
not a verdict — and because a flown pass cannot be un-flown, it latches on FULFILMENT, which is what
makes `identify` the one new kind that can conclude a run early.

**`OnRunway` geometry** (unchanged geometry of the predecessor crash gate): project (lat,lon) onto the
longitudinal/lateral axis of the runway (centreline from the threshold on `TrueHeadingDeg`,
`FBTrackProjectM`) — on the runway exactly when within its length (± `marginAlongM` before/after) and
half width (± `marginAcrossM`). **`WidthM <= 1`** (the mission format leaves it unset) falls back to a
generous **60 m** generic runway width.

| Call | Margins |
|---|---|
| Off-runway FAIL gate | 50 m along, 30 m across |
| Land waypoint SUCCESS gate | 0 m along, 15 m across |

**`kStillstandKt` = 2.0 kt** — a taxi speed threshold, not a full standstill: a rolling-out aircraft
still has a few knots for several ticks, and the mission objective ("landed and stopped") is fulfilled
well before the last knot.

**Waypoint capture — two rules:**

| Type | Rule |
|---|---|
| `Enroute` etc. | (a) **capture**: `FBPlanarDistM(pos, wp) <= WpCaptureM_`; the reference is the AIRCRAFT (its latitude scales the longitude) — the historical convention here. (b) **passed**: OR THE AIRCRAFT IS BEYOND IT. A capture circle asks "has it arrived"; for a fix at which the aircraft CANNOT arrive (one inside its own turn radius), the honest question is "did it get there", and the answer is the axis of the leg: beyond the perpendicular through the fix, the fix is behind. **Only from the second waypoint on**, because only then is there a declared inbound course against which "behind" can be measured. |
| `Land` (always the LAST) | is captured and does NOT advance. It demands that the aircraft actually COMES TO A STOP ON the runway (`AnyWow` AND `GroundSpeedKt < 2.0` AND `OnRunway(0, 15)`) — an approach that only grazes the threshold at flying speed is not a landing. SUCCESS is an independent condition here, never via an `ActiveIdx_` that falls off the end of the plan. |

The "passed" rule is THE SAME geometric rule with which `systems/FBNavSystem` sequences the GUIDANCE —
and deliberately a SECOND, independent formulation of it instead of a call over there: this class
judges from its own private copy of the plan and the observed position alone, and that must not be
traded away for five shared lines.

**`PlanJudged_` — when the flight plan is part of the verdict at all** (decided in the constructor):

```
PlanJudged_ = Objectives_.empty() || HasObjective(Waypoints);
```

Without `objective` lines the plan is the WHOLE verdict (the format's original verdict, unchanged).
WITH them, the block is the complete statement of what this unit has to achieve — so the plan is only
judged if it says so (`objective waypoints`): the `wp` line of a BVR mission is a briefed vector for
the guidance, not a place the fighter has to arrive at, and reading it as an objective is exactly what
would make a decided engagement run into a timeout.

**`Finalize(s, simTimeS)` — the end of the run, when it was not this unit's own clock.** `survive` is
the FIRST objective that CANNOT be met early (round `C12` adds `protect`, `no_fire` and `deny release`
to the same class, for the same reason and through the same gate) — "still combat-effective" is only true when there is no
run left in which one could be shot down (a missile still in the air when its shooter died is exactly
why there is no shortcut here). A unit with `survive` therefore deliberately stays undecided while the
engagement runs, and is asked here. Idempotent and latching like `Tick`.

**`ObjectivesMet` deliberately skips two kinds**: `Survive` (answered only in `Finalize`) and
`Waypoints` (answered by `PlanJudged_`/`PlanDone_`). If either were let into `FBObjectiveMet`, it would
be permanently unmet — a unit declaring `kill` AND `waypoints` (the documented way to have the flight
plan judged and still have a combat objective) could then never reach SUCCESS. `Identify` is answered
from `Dwell_` and `NoFire` from the sample; the other three go to `FBObjectiveMet` against the roster.

**The SUCCESS wording** is word-for-word backwards compatible: without objectives exactly the plan's
sentence, byte for byte (these strings are in every measured `events.log`) — `"all waypoints reached"`
resp. `"stopped on the runway"`. With objectives, `", objectives met"` is appended, and in `Finalize`
additionally `", survived"`. An actor with objectives and without waypoints has only those to report
(`"objectives met"`).

#### 4.3 Why there are two

A gentle touchdown with the gear down on a field is no crash for the AIRCRAFT — only for the scoring of
a mission. Conversely, "combat ineffective" is a verdict about the SORTIE, never a freeze and never a
case for the physics judge. The separation is what keeps both verdicts honest: the physics judge cannot
be influenced by mission design, and the mission judge cannot be overruled by physics.

---

### 5. Mission data as types

#### 5.1 `FBMissionFile` — the `.fbm` parser

`core/FBMissionFile.h/.cpp`. **A pure string-in/struct-out function**, no file I/O — the app reads the
file and hands the text in; that is how `core/` stays platform-neutral. Format reference:
`doc/missions/syntax.md`.

```cpp
bool FBParseMissionFile(const std::string &text, FBMission &out, std::string *err = nullptr);
```

**Structure:**

```
FBMission { Name, FBRunway Runway, HaveRunway, TimeoutS, vector<FBMissionUnit> Units }
FBMissionUnit { Id, ModuleName, FBUnitTeam Team, FBSpawn Spawn, HaveSpawn,
                FBFlightPlan Plan, vector<FBObjective> Objectives,
                vector<pair<string,string>> SetKV }
```

One block = one `units/FBSimUnit`; N blocks = a formation. That is why nothing here is a scalar "the
module"/"the spawn" any more. A single flight is the special case "one block", not a second dialect.

**Two scopes, one file** — both directions are hard errors:

| Scope | Keywords | Rule |
|---|---|---|
| mission-wide | `name`, `runway`, `timeout` | only BEFORE the first `unit` block. A `runway` line between two units would otherwise silently read as "mission-wide, but declared late". |
| actor-related | `module`, `team`, `spawn`, `wp`, `land`, `objective`, `set` | only INSIDE a block; without a preceding `unit` line there is no owner |

**The line grammar:**

| Line | Syntax | Note |
|---|---|---|
| `unit` | `unit <callsign>` | exactly one callsign; a duplicate is an error |
| `name` | `name <free text to end of line>` | mandatory |
| `runway` | `runway <lat> <lon> <elevM> <hdgDeg> <lengthM>` | `WidthM` stays 0 → the 60 m fallback rule applies |
| `timeout` | `timeout <positive seconds>` | mandatory; `0` or negative = parse error |
| `module` | `module <name>` | mandatory per block; resolved through `FBModuleRegistry` |
| `team` | `team friendly\|hostile\|neutral` | missing = `friendly` |
| `spawn` | `spawn <lat\|threshold> <lon> <altM\|ground> <hdgDeg> <speedKt>` | mandatory per block, only ONCE. `threshold` reuses the runway position (a ground-start convenience, not a second position syntax) and needs a `runway` line; `ground` resolves the altitude from terrain + gear clearance at spawn (`missions/FBMissionBoot.h`), never a separate code path |
| `wp` | `wp <lat> <lon> <altM> <speedKt>` | `FBWaypointType::Enroute` |
| `land` | `land` | needs a `runway` line; produces an `FBWaypointType::Land` at threshold position/elevation, speed 0 |
| `objective` | `objective survive` \| `objective waypoints` \| `objective kill unit <callsign>` \| `objective kill team <faction>` | see below |
| `set` | `set <key> <value…>` | RAW KV data; **the parser NEVER interprets a key**, only the module does (`FBModule::ApplySetup`). An unknown key is a runtime FAIL of the runner, not a parse error. |

Comments: `#` to end of line. Empty lines are ignored.

**The callsign rule** (`CallsignOk`): 1–24 characters from `[A-Za-z0-9_-]`. Justification: a callsign
also becomes a FILENAME (`outDir/telemetry_<id>.csv`) and a log field value — the accepted alphabet is
the intersection of "safe in both": no separators, no quoting, no spaces.

**The objective special rules:**

- `kill` demands an explicit `unit`/`team` discriminator instead of guessing what follows the word: a
  callsign MAY be called "hostile" (the parser alphabet does not forbid it), and a mission whose kill
  objective silently changes meaning because somebody named a jet after a faction is not a format
  anybody should have to debug.
- `objective waypoints` needs `wp`/`land` lines ABOVE it.
- A unit cannot have itself as a `kill unit` objective.
- An exact duplicate (same kind + same target) is an error.

**The whole-file checks at the end** (errors without a line number, because there is no line to point
at): `name` missing, `timeout` missing, no `unit` block, a block without `module`, a block without
`spawn` — and: **a `kill unit` objective is resolved against the WHOLE cast**, not only against the
blocks seen so far (an objective may name a unit declared further down; the two sides of a duel would
otherwise be an ordering puzzle). An objective that does not exist is a mission that can never be won —
hence a parse error instead of a silent, never fulfilled objective.

Error messages carry `"line N: …"`; `out` is fully valid only on `true`.

#### 5.2 `FBFlightPlan` / `FBWaypoint`

`core/FBFlightPlan.h`. A plain ordered waypoint chain. **Only structure, no procedure logic** —
SIDs/holdings/approach sequencing are the phase machine of `FBPilot`, not this container. Lives in
`core/` so that `pilot/FBPilot` and a future mission setup UI can share it without either owning the
other.

```
FBWaypointType { Takeoff, Enroute, Approach, Land }
FBWaypoint { LatDeg, LonDeg, AltM (m ASL), SpeedKt (CAS), Type }
FBFlightPlan: AddWaypoint / Clear / Size / Empty / At(i) / ActiveIndex / SetActiveIndex / ActiveWaypoint
```

`ActiveWaypoint()` returns `nullptr` if the index is out of range. The parser today produces only
`Enroute` and `Land`.

#### 5.3 `FBRunway`

`core/FBRunway.h`. The landing-relevant geometry of ONE runway: `ThresholdLatDeg`, `ThresholdLonDeg`,
`ThresholdElevM`, `TrueHeadingDeg` (course of the extended centreline), `LengthM`, `WidthM`. A value
type in `core/` (like `FBFlightPlan`), so that the approach/landing phases of `FBPilot` and a future
airfield database share the same shape.

#### 5.4 `FBSpawn`

`core/FBSpawn.h`. The declarative initial condition of a unit — **pure data, no modes/phases**:

| Field | Meaning |
|---|---|
| `LatDeg`, `LonDeg` | position |
| `Ground` | `true`: the `ground` keyword — sit on the gear at the resolved terrain elevation. `false`: `AltM` is a literal ASL altitude (air start) |
| `AltM` | literal target altitude m ASL — meaningful only with `!Ground` |
| `HeadingDeg`, `SpeedKt` | heading, speed |

**There is no separate ground/air code path here beyond this one `bool`.** Runner/boot turn it into
EXACTLY ONE JSBSim IC application (`FBFdmBoot::Spawn` applies position + attitude + velocity together —
`missions/FBMissionBoot.h`'s `FBMissionSpawnActor`) plus the module's initial `FBPilot` phase.

#### 5.5 `FBObjective` — combat objectives + the roster

`core/FBObjective.h`.

**Why the format needed it** (`doc/missions/syntax.md`, "Verdict"): previously a weapon hit could only
produce the FAILURE of the one who was hit, because no unit could declare that this failure was its own
OBJECTIVE. A mission whose hostile unit was shot down therefore ended as FAIL — the verdict was
team-blind. An objective is what makes the same observed fact readable from two sides: the loser's FAIL
and the shooter's SUCCESS are the same shot.

| `FBObjectiveKind` | `.fbm` spelling | Meaning |
|---|---|---|
| `Survive` | `survive` | stay combat-effective until the end of the run |
| `KillUnit` | `kill unit <callsign>` | render a named unit combat-ineffective |
| `KillTeam` | `kill team <faction>` | every unit of a faction |
| `Waypoints` | `waypoints` | reach every `wp`/`land` line of one's own flight plan — what a unit WITHOUT objectives is implicitly judged on; declared explicitly it is the way to KEEP that when declaring other objectives |
| `Identify` | `identify unit <callsign> range <m> hold <s>` | hold a planar range ≤ `<m>` to a named unit for a cumulative `<s>` — the flown pass, not a sensor event (`doc/missions/verdict.md`, round `C12`) |
| `Protect` | `protect unit <callsign>` / `protect team <t>` | the named unit(s) are still combat-effective at the END of the run; any one of them lost ⇒ immediate FAIL of the DECLARING unit |
| `NoFire` | `no_fire` | this unit released no store and fired no burst for the whole run |
| `DenyRelease` | `deny release unit <callsign>` / `deny release team <t>` | the named unit(s) released nothing for the whole run |

| `AvoidZone` | `avoid zone <name> [exposure <s>]` | cumulative dwell inside a DECLARED cylinder stays at or below the budget |
| `Suppress` | `suppress unit\|team <x> [emitting <s>]` | the named unit's cumulative radiating time stays at or below the allowance |

**And one field that is not a kind: the DECLARED SPAN** (`double UntilS`, default `+infinity`;
`doc/missions/verdict.md`, "The seventh thing in the vocabulary"). Any kind may be written
`… until <s>`, and then its state is FROZEN at that sim time — read once through the same `StateOf` the
judge already computes, at an instant the MISSION named instead of one the run's events chose. It exists
because a cumulative objective read at an event-chosen instant amplifies chaos into a bit: [MESS, `E17`]
`sat-03`'s belt dwell is 175.2 s at t = 317.9 s in both the truncated and the full run and reads `met` in
one and `unmet` in the other. `FBObjectiveCovers` does NOT read it — covering is a property of the
DECLARATION, so a windowed `kill` still makes its target's loss expected for the whole run.

`Protect`/`DenyRelease` offer both target scopes without becoming two kinds each: the discriminator is
`FBObjectiveScope { Unit, Team }` on the objective. `KillUnit`/`KillTeam` predate it and keep carrying it
in the KIND — one mechanism where the spelling is older than the enum, one where it is not, and the
addressing question itself is asked in exactly one place (`FBObjectiveNames`).

**The observation types — deliberately minimal:**

```cpp
struct FBUnitObservation { const char *Id; FBUnitTeam Team; bool CombatEffective;
                           bool ReleasedWeapon; double RangeM; };
struct FBMissionRoster   { const FBUnitObservation *Units; int Count; };
```

`FBUnitObservation` carries **no position, no self-declaration, no module handle** — only who it is,
whose side it is on, whether it can still fight (`FBSystemHealth::CombatEffective`), whether it has ever
let anything go, and how far away it is. `Id` BORROWS the unit's name for the tick. `FBMissionRoster` is
a borrowed VIEW onto the caller's per-tick buffer (a container instead of a view would allocate per
judged unit per tick — the client fills ONE reused vector, nothing allocates in the tick path).

The two `C12` fields are of exactly the same kind as the health bit, and both are filled by the OWNER
from registers the owner holds itself:

| Field | Filled from | Default |
|---|---|---|
| `ReleasedWeapon` | `FBSimUnit::NoteWeaponRelease()`, called by the runner at the ONE place it drains `Stores().TakeRelease()` / `Guns().TakeBurst()` (the growth phase). **Monotone** — set-only, never cleared, so a violation cannot be un-observed | `false` |
| `RangeM` | the planar distance between two PUBLISHED poses, aimed FROM the judged unit right before its monitor runs. Per judged unit, hence rewritten per unit per tick — and only computed at all when some unit in the mission declares an `identify` | `+inf` |

Both defaults mean "nothing happened", and only the `C12` kinds read them: a mission that declares none
of them reaches none of the branches, which is what makes the round byte-identical over the whole
pre-round mission tree.

`no_fire` asks about the DECLARING unit, and the monitor has no id with which to find itself in the
roster (it deliberately has no identity at all). The same bit therefore also travels on
`FBMissionMonitorSample`, beside `CombatIneffective`, filled in the same line of
`FBSimUnit::BuildMissionSample`.

**The three predicates:**

| Function | Contract |
|---|---|
| `FBObjectiveNames(o, id, team)` | Pure ADDRESSING: is that unit in this objective's target set? No verdict at all, which is what lets `kill` and `protect` share it while meaning opposite things. `Survive`/`Waypoints`/`NoFire` name nobody — they are about the declaring unit. |
| `FBObjectiveCovers(o, id, team)` | Did somebody declare this unit's LOSS as their objective? Read by the runner's combination rule to decide whether a loss was expected and therefore stops deciding the run (`missions/FBMissionRunner.cpp`). **Only `KillUnit`/`KillTeam` cover.** `Protect` naming a unit is the exact opposite declaration, and letting it cover would make the protected unit's death the expected outcome — the inversion `doc/missions/verdict.md` calls the load-bearing rule and measures as an exit code (`missions/objective-covers-none.fbm`). The `switch` is exhaustive, so a future kind that forgets its case does not compile. |
| `FBObjectiveMet(o, roster)` | The roster-decidable kinds — `KillUnit`/`KillTeam` (every named unit combat-ineffective), `Protect` (every named one still effective), `DenyRelease` (none of them released) — always with at least one named unit, so an objective against a faction to which nobody in this mission belongs is **never** met instead of trivially true, because a mission that misspells its enemy should not pass. `Survive`/`Waypoints`/`NoFire`/`Identify` are NOT decided here: the first two by construction, `NoFire` because it reads the sample rather than the roster, `Identify` because it needs a dwell no roster can carry. |

`FBObjectiveStr(o)` returns the `.fbm` spelling — for logs and parser error messages.

**It stays an OBSERVATION, not an ASSERTION**: an objective is evaluated by `FBMissionMonitor` against
the roster that the client fills from the health registers that belong to IT — exactly as it fills the
monitor's position sample. A module can no more declare its opponent dead than declare itself landed.

#### 5.6 `FBTeam`, `FBMode`, `FBMasterMode`, `FBArmState`

| Type | File | Values | Why in `core/` |
|---|---|---|---|
| `FBUnitTeam` | `FBTeam.h` | `Friendly`, `Hostile`, `Neutral` | it is BOTH: world entity identity (`units/FBUnit`) and mission DATA (a `.fbm` `team` line). Parking it in `units/` would make `core/` depend on `units/` just to name a faction; duplicating the enum would give the mission file and the world two notions of "hostile". |
| `FBMode` | `FBMode.h` | `Manual`, `Direct`, `Course` | the LIVE autopilot state that every layer reads (FBState/HUD, telemetry, the guidance systems themselves) |
| `FBMasterMode` | `FBMasterMode.h` | `Nav`, `AirToAir`, `AirToGround`, `Dogfight` | the AUTHORITY lies with the module, not globally; the enum is shared because input, display and weapon systems all take it as a parameter |
| `FBArmState` | `FBArmState.h` | `Sim`, `Arm` | the ARM/SIM line of the HUD; `FBState` carries it, several layers need the same enum |

`FBUnitTeamStr` / `FBUnitTeamFromString` define the only accepted `.fbm` spellings (lower case, exactly
the `FBUnitTeamStr` strings) — the parser is strict everywhere else, so it is strict here too.

---

### 6. Damage

Three separate things in a clear division of roles: the STATE (`FBSystemHealth`), the RESOLUTION
(`FBDamageModel`) and the ZONE DATA (module data, e.g. `modules/f16/FBF16Damage` — not in `core/`).

#### 6.1 `FBSystemHealth` — the health register

`core/FBSystemHealth.h/.cpp`. ONE register per `units/FBSimUnit`, belonging to the CLIENT, fed
exclusively by a core-owned verdict and **read, never written**, by the module.

**The write gate is the TYPE, not a convention.** Every mutator is `private`, and there is exactly ONE
`friend`: `core/FBDamageModel`, which produces a state change only as the result of a resolved weapon
burst. So nowhere does an API exist — neither on a const nor on a non-const handle — with which a
system, a pilot or a module could damage or repair itself (or anybody else).
`grep -rn FBSystemHealth src/systems src/modules` finds only reads, **and can find nothing else,
because nothing else compiles**.

**MONOTONE by decision**: a state never improves. There is no repair in flight, and a monotone register
is what makes the damage picture of a run a function of the bursts received alone — no question of
order, no healing race between two observers.

**`Destroyed()` — is this unit still there.** The ONE question a simulation loop asks
(`missions/FBMissionSim`), and it lives here because every unit kind has a health register while only
an aircraft has a stall: a tank, a ship or a helicopter inherits the rule by being damaged rather than
by getting a monitor of its own. Written through the register's single friend as before — the third
entry point, `FBDamageModel::ApplyPhysicalKo`, which carries no geometry and no number: it RECORDS what
`core/FBFlightMonitor` measured (ground contact, structure contact, ground penetration below
`kPenetrationMarginM = −3 m`, integration divergence — every threshold model-derived and stated in
§4.1). No hit points, no life energy, no "destroyed after N hits" anywhere in this chain.

It is deliberately NOT folded into `CombatEffective()`: that judges whether a SORTIE can still be
flown, is read by the mission judge's `CombatIneffective` sample and by the expected-loss rule, and
making a crash imply it would change what a duel scores. Two questions, two bits.
**Stated boundary:** today only the flight monitor sets `Destroyed`. A ground target reduced to
`Failed(Structure)` is combat-ineffective and NOT destroyed in this sense, which is why its loss still
does not end a run — the same behaviour as before this bit existed. A damage-side criterion for
non-flying units is a round of its own, with its own sources.

**`FBSystemId`** — the addressable inventory. Deliberately the module SLOT set plus the three physical
things a hit can knock out which are not avionics boxes (engine, flight controls, structure), because
their consequence is exactly what JSBSim can carry itself. **Append only**: the ordinal is
telemetry-visible (the `dmg_*` bitmasks).

| Ordinal | Id | String | Meaning |
|---|---|---|---|
| 0 | `Engine` | `engine` | propulsion: thrust |
| 1 | `FlightControls` | `flight_controls` | FLCS/hydraulics: control authority |
| 2 | `Structure` | `structure` | airframe: drag |
| 3 | `AirData` | `air_data` | ADC + probes |
| 4 | `RadarAlt` | `radar_alt` | radar altimeter (CARA) |
| 5 | `Nav` | `nav` | INS/navigation |
| 6 | `Radar` | `radar` | the active air-to-air set |
| 7 | `FireControl` | `fire_control` | the launch envelope of the fire control computer |
| 8 | `Stores` | `stores` | SMS: racks and wiring |
| 9 | `Datalink` | `datalink` | the net terminal |
| 10 | `Rwr` | `rwr` | the warning receiver |
| 11 | `Countermeasures` | `countermeasures` | the dispenser |
| 12 | `Gun` | `gun` | the gun: drum, feed, barrels — APPENDED per the enum's own rule |

**`FBHealthState`** and what it means to the consumer (the coupling that gives this file its point):

| State | Meaning |
|---|---|
| `Intact` (0) | the system runs and publishes its output block normally |
| `Degraded` (1) | it keeps running and publishing, with reduced performance **where a reduced performance is DERIVABLE** (radar range, engine ceiling, FLCS authority). Where not, a system has no degraded behaviour, and its layout entry simply never produces one |
| `Failed` (2) | the system does not run and does not publish. Its block goes `Invalid`, and everything else follows by itself from what the avionics bus does anyway: the HUD dashes, the warning set reports the condition as INHIBITED instead of absent, and the pilot refuses the actions whose data basis is gone. **None of that is reimplemented here; that it happens for free is the reason the bus carries validity heads at all.** |

**The read API**: `State(id)`, `Ok(id)`, `Degraded(id)`, `Failed(id)`, `Working(id)` (= not `Failed` —
the gate a module asks before ticking a slot), `Damaged()`, `FailedMask()`, `DegradedMask()`, `Hits()`.

**`CombatEffective()` — the mission question and a stated modelling decision:**

```cpp
bool CombatEffective() const {
  return !Failed(Engine) && !Failed(FlightControls) && !Failed(Structure);
}
```

An aircraft is combat-ineffective as soon as the AIRFRAME can no longer finish the sortie. **Avionics
losses, however total, are expressly NOT part of it**: a jet with a dead radar and dead racks is out of
the fight but still flying — and what this predicate feeds (`core/FBMissionMonitor`) judges the SORTIE,
not the engagement. **The unit is not "dead" when this goes false**: it keeps flying exactly as long as
the physics allows.

**The private mutators (for `FBDamageModel` only):**

| Method | Contract |
|---|---|
| `Worsen(id, s)` | monotone: `if (s <= cur) return false`. Keeps the bitmasks consistent (first clear both bits, then set the right one). Returns `true` if this call really changed something. |
| `NoteHit()` | hit counter |
| `AddKinetic(zone, fluxJm2)` | **the one piece of damage state that belongs to no system**: how much areal energy a ZONE of this airframe has cumulatively taken from PROJECTILES. Returns the new sum. It exists because a gun is a continuous stream which this simulator necessarily cuts into per-tick bundles (`core/FBGun.h`) — judging every bundle on its own would make the damage a function of the TICK RATE, which is exactly what principle 4 forbids. **A warhead has no equivalent and does not use it: a burst is ONE event, and the energy of an event is what it is.** `kMaxZones = 5` (the `FBDamageZone` values including `None`). |

**`FBSystemHealthTelemetry`** — its own source (`"dmg"`), registered LAST by the unit, per the append
rule: `dmg_hits`, `dmg_failed` (bitmask), `dmg_degraded`, `dmg_effective`.

#### 6.2 `FBDamageModel` — the resolution

`core/FBDamageModel.h/.cpp`. The ONE writer of `FBSystemHealth`, belonging to the client. A module
resolves its own damage as little as it judges its own crash.

**It is a MODEL, and it says so.** Nothing here is a measurement. What is OBSERVED and checkable is the
INPUT: the burst geometry (the runner's own closest-approach computation on the published poses), the
closure rate and the warhead mass from the store catalogue. MODELLED is the step from these three
numbers to a system state, and it is built from the two things that actually are physics — isotropic
fragment spread and kinetic energy — plus a threshold per system, and that is a setting.

##### The energy chain, in three steps each with its named assumption

| Step | Formula | Assumption |
|---|---|---|
| 1. FRAGMENT MASS | `m_frag = kCaseFraction · WarheadKg` | `kCaseFraction = 0.5` **[SET]** — the usual order of magnitude for a fragmentation case; `doc/modules/f16/weapons.md` §4.7 lists warhead internals as a genuine gap, so this is a declared setting and not a citation |
| 2. AREAL DENSITY | `ρ_A = m_frag / (4π r²)` [kg/m²] | the fragments spread ISOTROPICALLY. That is the ONE geometric assumption of the model — a real warhead sprays into a focused band, which would make the result depend on the angle of the burst to the missile axis, and nothing here claims to know that band |
| 3. SPECIFIC ENERGY | `v_eff = sqrt(v_eject² + v_closure²)`, `flux = ½ · ρ_A · v_eff²` [J/m²] | every fragment arrives with the VECTOR SUM of its ejection velocity (`kFragSpeedMs = 1800 m/s` **[SET]**, radial) and the closure of the two aircraft. For a radially symmetric spray pattern the mean magnitude of this sum is exactly `sqrt(v_eject² + v_closure²)` — **deliberately not `v_eject + v_closure`**, which would hold only for the fragments thrown straight ahead |

**The result is a 1/r² law in the energy: twice the miss distance = a quarter of the arrival.** That,
and not any single threshold, is what makes the model behave sensibly at ranges at which nobody has
calibrated it.

**The range floor**: `r = max(rangeM, 0.5 m)` — not a physical statement but a protection: the 1/r² law
diverges at zero, and a burst INSIDE the airframe is no more instructive than one against its skin.
0.5 m is about half the width of a fighter fuselage, hence the closest a burst can be to the axis and
still be outside the aircraft.

The function is PUBLIC so that a report, a harness or a log line can reproduce the exact number behind
a damage verdict instead of believing it:

```cpp
double FBFragmentFluxJm2(double warheadKg, double rangeM, double closureMs);
```

##### The zones

An aircraft is not a point: WHERE the burst sits relative to the AIRFRAME AXIS decides which systems
are near it. The LAYOUT (module data) cuts the airframe along its own longitudinal axis into zones and
names which systems sit in each; this file computes PER ZONE the distance from the burst to THAT zone's
axis segment and the flux there.

**Every zone is evaluated, not only the nearest**: fragments go everywhere, they merely arrive thinner
further out — and letting the 1/r² law say that is more honest than partitioning the airframe and
giving one partition everything. **The airframe is modelled as that axis segment and nothing more** —
no cross-section, no shielding, no fragment count.

`ZoneRangeM(b, z)`: the longitudinal coordinate is CLAMPED into the zone's segment, the lateral/
vertical offsets are carried unchanged. A burst abeam the middle of a zone is thereby as close as its
lateral error; one ahead of the nose additionally has to reach back along the axis.

```
FBDamageZone { None=0, Nose, Forward, Center, Aft }   // names generic; WHERE they sit is module data
FBZoneSystem { FBSystemId Id; double DegradeJm2; double FailJm2; }
FBDamageZoneSpec { FBDamageZone Zone; double AftM, FwdM; const FBZoneSystem *Systems; int SystemCount; }
```

`AftM`/`FwdM` are metres from the CG, **+ = forward**, hence `AftM < FwdM`. A system without derivable
degraded behaviour simply declares `DegradeJm2 == FailJm2` and therefore never has one. Everything is
plain arrays with counters — the whole layout is a compile-time table that a module hands out by const
reference; nothing allocates.

##### `FBDamageLayout` — plus the geometry that only a projectile stream needs

| Field | Meaning |
|---|---|
| `Zones`/`ZoneCount` | the zone table |
| `FrontalAreaM2` | presented area from ahead/astern |
| `LateralAreaM2` | presented area from the side/from above |
| `FrontalExtentM` | how far the airframe REACHES in this view: half the wingspan from astern |
| `LateralExtentM` | half the length from the side |
| `PlanExtentM` | ...and the third orthogonal view, from directly above or below. It exists because an EYE measures the largest DIMENSION of a silhouette rather than its area (`sensors/FBVisualSystem`, [`sensors.md`](sensors.md) §9.4), and the plan view's largest dimension is a separate fact from the side view's. **It lives here and not in a table of its own: the gun and the eye look at the same aeroplane, and two presented-geometry tables would be two truths about one airframe.** Undeclared = the lateral figure, i.e. exactly a two-view layout's behaviour. For both current airframes the two coincide (each is longer than it is wide) |

A warhead sprays isotropically, and the airframe's cross-section never appears in its arithmetic. A
burst is a narrow pattern that either lands on the aircraft or does not — there the PRESENTED AREA
decides how much arrives. **Two numbers instead of one**, because the difference is a factor of three
on every fighter and the interpolation is free. **Two SCALES** (area + extent), because the area says
how much material is there and the extent how far out it is spread — a fighter has a lot of wingspan
and little material. A module that declares neither (the default, and every released store) presents
nothing and takes no gun damage — which is right: a bomb in free fall is not something anybody shoots
at.

```cpp
double FBPresentedAreaM2(layout, fwd, right, down);   // |cos| · frontal + |sin| · lateral
double FBPresentedExtentM(layout, fwd, right, down);  // the same interpolation
```

The simplest interpolation that is exact at both ends — and **no claim about the shape in between**.
The direction vector is in the TARGET's BODY FRAME and need not be normalised.

##### The two inputs

```cpp
struct FBBurst        { FwdM, RightM, DownM; ClosureMs; WarheadKg; };      // warhead
struct FBKineticBurst { FwdM; FluxJm2; SpreadM; Rounds; ImpactSpeedMs; };  // projectile stream
```

**Why two types instead of a flag**: the two weapon effects are known through DIFFERENT things. A
warhead is known through its MASS, and the model derives from it what energy reaches an area. A burst
is known through the ENERGY DENSITY that the owner of the simulation has already computed from hit
count, impact velocity and dispersion (`core/FBGunBallistics.h`'s `FBGunFluxJm2`) — a number this file
may not and could not re-derive, because it never sees a projectile.

**What they SHARE, and why that is legitimate**: the TARGET. Both express what arrives as J/m² of areal
energy at a place on the airframe, and both are judged against the same per-system thresholds, so that
ONE damage register answers for both without a second, uncalibrated set of numbers. **That is a stated
modelling decision and not a physical claim**: 20 mm impacts and warhead fragments do not damage
structure through the same mechanism, and expressing both as areal energy is this simulator's shared
currency, not the statement that they are equivalent.

##### The two resolutions compared

| | `Apply` (warhead) | `ApplyKinetic` (projectile stream) |
|---|---|---|
| Reaches | EVERY zone (isotropic spray pattern), with 1/r² falloff per zone | only the zones the FOOTPRINT overlaps: `[FwdM − half, FwdM + half]` with `half = max(SpreadM, 0.5 m)` |
| Flux | recomputed per zone (`FBFragmentFluxJm2`) | supplied by the caller |
| Summation | **NONE** — a detonation is ONE event, its energy is what it is | **YES**, cumulative per zone (`FBSystemHealth::AddKinetic`): fifty projectiles in five bundles do the damage of fifty projectiles; otherwise the damage would hang on the tick rate |
| `res.RangeM` | distance burst → structure of the peak zone | **0.0** — a hit, not a stand-off burst: there is no range |
| The 0.5 m floor | a range floor against the 1/r² divergence | a footprint floor, so that a point-blank burst (pattern centimetres wide) lands on the zone it went through instead of on a mathematical point between two of them |

Common to both: `NoteHit()` once per burst; threshold logic `want = Failed` if
`FailJm2 > 0 && flux >= FailJm2`, otherwise `Degraded` if `DegradeJm2 > 0 && flux >= DegradeJm2`;
`Worsen` decides whether anything really changed; a layout without zones (the default of every module
that declared none — e.g. a released store) takes no damage and returns an empty result.

```cpp
struct FBDamageResult {
  FBDamageZone Zone;      // the zone with the highest flux
  double RangeM;          // burst -> structure of that zone
  double PeakFluxJm2;
  uint32_t NewlyFailed, NewlyDegraded;   // bitmasks: what THIS burst changed
  bool WasEffective, NowEffective;
  bool Changed() const;
};
```

**DETERMINISM IS STRUCTURAL**: no random number anywhere in this file, no time dependence, no hidden
state. Same geometry, same warhead, same closure → always the same masks (measured thread-independent).

##### The physical consequences — the constants with derivation

All of them run through JSBSim (`units/FBSimUnit::ApplyDamageToAirframe` → `fdm/FBFdm`), never through
a second, parallel flight model.

| Constant | Value | Derivation |
|---|---|---|
| `kAuthorityDegraded` | **0.5** | half the commanded control deflections. **[SET, but the one number with a structural reason]**: the F-16 has TWO independent hydraulic systems driving its actuators — losing one is the natural meaning of "degraded". |
| `kAuthorityFailed` | **0.0** | no authority: the controls no longer answer, the aircraft flies on whatever trim and stability is left — exactly the departure that JSBSim then integrates by itself |
| `kThrottleLimitDegraded` | **0.6** | afterburner gone: the throttle cannot be commanded beyond military power. 0.6 is where the AB gate lies in the F-16 model's own `throttle-cmd-norm` convention **[DERIVED]**. Failed = fuel cutoff, i.e. JSBSim's own engine-out — no thrust term is invented here. |
| `kDamageDragFt2Degraded` / `kDamageDragFt2Failed` | **1.5 / 6.0 ft²** | battle damage is holes and torn skin: additional drag, applied as a DRAG AREA through the same `<external_reactions>` mechanism as the carriage drag (`fdm/FBFdm::SetDamageDrag`), **through the CG**, so that no pitching moment is claimed that nobody could evidence. **[SET]** — for orientation: the zero-lift drag area of a clean F-16 is on the order of 4 ft², so a degraded airframe is "noticeably dirty" and a failed one "flies with a hole in it". |
| `kRadarRangeDegraded` | **0.70710678…** | **[DERIVED]** via the radar equation: a degraded set has half the antenna aperture, `R⁴ ~ Pt·G²` with `G ~ A`, hence `R ~ sqrt(A)` — half the aperture = 1/√2 of the range |

---

### 7. Weapon value types and ballistics

#### 7.1 `FBStore.h` — the store catalogue

`core/FBStore.h`. **Every number in it either comes from the store's OWN pinned JSBSim model or is
derived from it by a named formula** — nothing about a weapon is invented here, because a released
store flies as its own FDM instance of exactly that model, and the carriage figures have to describe
the same object.

**Why a catalogue and not a class**: a store has NO behaviour on the pylon — it is mass, drag and a
model name. Its behaviour IS the JSBSim model it becomes at the moment of release
(`modules/stores/FBStoreModule` resp. `modules/missile/`). What remains on the aircraft is a value
type. It lives in `core/` for the same reason as `FBRunway`/`FBSpawn`: the mission parser, the module's
SMS and the app-side spawn path all name it, and none of them may include the others.

```
FBStoreKind { None = 0, Mk82, Aim120 }   // append; None must stay 0 so that a zeroed block reads "empty"
```

**`FBStoreSpec`:**

| Field | Meaning |
|---|---|
| `Kind`, `Key` | ordinal; mission file/registry name |
| `FdmModel` | JSBSim model directory under the ONE model root (`mods/f16/src/aircraft`) |
| `MassLbs` | carriage mass |
| `DragAreaFt2` | CdA: carriage drag = this × qbar (lbf) |
| `MaxFlightS` | lifetime cap after release |
| `Guided` | `true`: flown by `modules/missile` (seeker + guidance law); `false`: by `modules/stores` (integrate and fall) |
| `RequiresLock` | the SMS refuses the launch without a fire control solution |
| `FuzeRadiusM` | proximity fuze: passing a unit closer than this is a hit. **0 = no proximity fuze at all** (a bomb hits what it lands on) |
| `WarheadKg` | explosive + case mass — the ONE store-side input to the damage model. 0 = an inert round that hurts nothing |
| `Perf` | `FBWeaponPerf`, see below |

##### `FBWeaponPerf` — the performance table of the FIRE CONTROL COMPUTER

The coarse table on which a launch envelope or a bomb fall computation runs
(`modules/f16/FBF16FireControl`). **Deliberately a separate, simplified copy of what the weapon's
JSBSim model does**: a real FCC integrates a stored table, not the weapon's actual aerodynamics, and the
difference between the two is a real property of every DLZ ever flown and every CCIP pipper ever laid
on a target. The intercept mission measures it for the guided case (predicted time of flight against
the flown one), the attack missions for the unguided one — instead of hiding it by feeding the
computation the same numbers with which the weapon flies.

| Field | Meaning |
|---|---|
| `BoostThrustN`, `BoostS`, `SustainThrustN`, `SustainS` | the motor as the FCC knows it |
| `LaunchMassKg`, `BurnoutMassKg` | masses |
| `DragCoefA` | supersonic axial force coefficient on `RefAreaM2` |
| `RefAreaM2` | reference area |
| `MinSpeedMs` | below this the round can no longer fly an intercept |
| `ActivationRangeM` | range at which the seeker is switched on (the DLZ's "Radar Activation Range" cue, `weapons.md` §2.5) |
| `SeekerRangeM` | what the seeker can actually acquire at |
| `ArmingS` | separation + arming. On a guided round it also covers motor ignition and sets `Rmin`; on a bomb it is the fall time the fuze needs — the pull-up anticipation cue's own number. One quantity, one field. |

**An UNGUIDED store uses the same table** and only the four entries a falling body has:
`LaunchMassKg`, `DragCoefA`, `RefAreaM2`, `ArmingS`. The motor fields stay zero because it has no
motor, the seeker fields because it has no seeker — the table is not "the guided block", it is what the
computer knows about the round.

##### `kMk82` — Mk-82, 500 lb free-fall bomb

`doc/modules/f16/weapons.md` §3.

| Number | Value | Derivation |
|---|---|---|
| `MassLbs` | **500.0** | the model's own `<emptywt>` (`mk82.xml`: 500 LBS). One object, one mass — the number the carrier loses on release is the same one the released FDM instance then flies with |
| `DragAreaFt2` | **0.366** | the model's own zero-lift drag at carriage Mach, expressed as an AREA so that it can be multiplied by the CARRIER's dynamic pressure: `mk82.xml`'s CDmin table gives Cd = 0.144 at M 0.8 over `<wingarea>` 2.54 ft² → **CdA = 0.366 ft²**. Deliberately the store's own coefficient and NO "drag index" from a loading manual: a T1/T2 source for that does not exist (§4.5 marks station/loading numbers as T4, cross-check only), and interference/pylon drag is a real effect nobody here can quantify — so the carriage drag is exactly the store's own drag, without an invented installation factor |
| `MaxFlightS` | **300.0** | leak protection, not physics: a released store that after this time has neither hit anything nor diverged is retired, so that a run does not accumulate zombie actors. Fall times of this class are tens of seconds (§4.2), so 300 s never truncates a real trajectory |
| `Guided`/`RequiresLock`/`FuzeRadiusM` | `false`/`false`/**0.0** | a bomb has no proximity fuze; nothing resolves an Mk-82 burst against an aircraft. What READS it is the ground burst at the impact point (`missions/FBMissionRunner.cpp`), through the same `core/FBDamageModel` as a warhead beside a jet |
| `WarheadKg` | **87.0** (192 lb Tritonal) | **[T3, the standard fill of the Mk-82]** |
| `Perf.LaunchMassKg` | **226.796** | = the model's own 500 lb, one object one mass |
| `Perf.RefAreaM2` | **0.235974** | = the model's own `<wingarea>` (2.54 ft²), the area to which its whole drag table refers |
| `Perf.DragCoefA` | **0.142** | **[DERIVED, and deliberately coarse]**: `mk82.xml`'s CDmin table runs from 0.140 at M 0.2 to 0.144 at M 0.8 and then rises steeply transonically; the computer carries ONE subsonic number — that IS a stored table — and the resulting prediction error against the model's Mach-dependent drag is exactly what the CCIP/CCRP missions MEASURE instead of tuning away |
| `Perf.ArmingS` | **2.0** | **[SET]**: no quotable arming delay exists for the Mk-82's standard fuzes (§4.7 marks fuze internals as a gap, §4.2's PUAC text gives the CONCEPT without a number); 2 s is the order of magnitude of a nose fuze arming vane and is what the pull-up anticipation cue is computed from |

##### `kAim120` — AIM-120 AMRAAM

`doc/modules/f16/weapons.md` §2.5, §3, §4.4. The FIRST guided round: `mods/f16/src/aircraft/aim120` — the only
model in the root WITHOUT an upstream counterpart, because the pinned submodule has no AMRAAM. Module:
`modules/missile`.

| Number | Value | Derivation |
|---|---|---|
| `MassLbs` | **335.0** | launch weight **[T3]** — the same number that structure + propellant in `aim120.xml` sum to, so the pylon loses what the released FDM then flies |
| `DragAreaFt2` | **0.115** | **[DERIVED]**: the model's own subsonic CA (0.43 at carriage Mach 0.8) over its 0.2672 ft² reference area. The same rule as with the Mk-82 — the store's own drag, no invented installation factor |
| `MaxFlightS` | **120.0** | **[SET]** leak protection far beyond any credible engagement (a 40 nm shot arrives in under 90 s, see the DLZ integration), never a cut-off on a live trajectory |
| `FuzeRadiusM` | **10.0** | **[SET]**. The AMRAAM's active radar proximity fuze and the lethal radius of its WDU-41/B fragmentation warhead are not published with any precision (§4.7 lists exactly this class of number as a genuine gap). 10 m is the conservative reading of a 50 lb fragmentation warhead against a fighter: close enough that it is a HIT and not a claim, small enough that a guidance law which only arrives "roughly" scores none |
| `WarheadKg` | **20.5** (45 lb, WDU-41/B) | **[T3 — the published figure is consistently "about 40–50 lb", and §4.7 marks warhead internals as a genuine gap]**. It is the ONE weapon-side number the damage model reads; everything else about a hit comes from the measured geometry of the burst |
| `Perf.BoostThrustN`/`BoostS` | **24020 N / 3.0 s** | from `engine/WPU-6.xml` |
| `Perf.SustainThrustN`/`SustainS` | **6228 N / 7.7 s** | from `engine/WPU-6.xml` |
| `Perf.LaunchMassKg`/`BurnoutMassKg` | **152.0 / 99.8** | from the model |
| `Perf.DragCoefA`/`RefAreaM2` | **0.55 / 0.02482** | from `aim120.xml`'s CA at Mach 3 |
| `Perf.MinSpeedMs` | **340.0** | **[SET]** roughly Mach 1 at altitude: below that the round has neither the closure nor the dynamic pressure for an intercept — which ends the DLZ integration |
| `Perf.ActivationRangeM` | **18520 m (10 nm)** | **[SET]** — the DLZ's own "Radar Activation Range" cue differs per engagement and has no published constant (§4.4 says so expressly); 10 nm is the doctrinal order of magnitude and is where this simulator switches the seeker on |
| `Perf.SeekerRangeM` | **14816 m (8 nm)** | **[SET]** likewise unpublished; **deliberately SHORTER than the activation range**, so that the seeker is already looking when the target comes into its acquisition range and the handover is an ACQUISITION event, never a timer |
| `Perf.ArmingS` | **1.5** | **[SET]** separation (0.5 s to motor ignition, see `FBFdm`'s throttle slew) plus fuze arming delay; it is what sets `Rmin` |

**Catalogue access**: `kStoreCatalogue[]`, `FBFindStore(key)` (mission file/registry name),
`FBStoreSpecOf(kind)`.

#### 7.2 Release value types

**`FBDeliveryMode { Ccip = 0, Ccrp }`** — append only: the ordinal is the mission-visible
`set attack_mode` value and a telemetry column. `FBDeliveryModeStr` → `"ccip"`/`"ccrp"`.

**`FBReleaseSolution` — what an unguided release was aimed with**, the fire control's answer at the
moment the pickle was accepted, carried out of the aircraft with the weapon. The exact counterpart to
`FBWeaponTargetState` on a guided launch, and it exists for the same reason: **the prediction must
leave the jet WITH the weapon**, so that the owner of the simulation can lay it beside the then
measured impact and quantify the error. Nothing in it controls anything — a bomb has no guidance; it is
a RECORD.

| Field | Meaning |
|---|---|
| `Valid`, `Mode` | valid? solved in which mode? |
| `ImpactLatDeg`/`ImpactLonDeg`/`ImpactElevM` | where the computer said it would land — and the plane against which it solved |
| `TofS` | time of flight |
| `AimLatDeg`/`AimLonDeg` | what was aimed at (the designated point) |
| `AimMissM` | predicted impact → aim point, at the moment of release |
| `ArmMarginS` | < 0 = released below the arming margin (a dud) |
| `StampS` | **WHEN the computer produced it.** A release is answered by the SMS in the module's stores command group, which is serviced BEFORE the fire control's own tick in the same sensor sweep — so the solution a round is stamped with is necessarily that of the PREVIOUS sweep. This delay is a real property of the bus order and worth tens of metres at fighter speed; it is therefore RECORDED instead of hidden, and any measurement made from this structure can say how much of its error is simply the age of the number |

**`FBStoreRelease` — a released store as the SMS hands it over:**

| Field | Meaning |
|---|---|
| `Station`, `Kind`, `MassLbs`, `SimTimeS` | which station let go of what and when |
| `OffFwdM`/`OffRightM`/`OffDownM` | where this station sits relative to the carrier's CG (body axes, metres). The offset travels WITH the release, because the SMS is the only thing that knows its own pylon geometry, and the app-side spawn — the only code allowed to create an FDM (`fdm/FBFdmBoot.h`) — has to place the new unit at the PYLON, not at the carrier's centre of gravity |
| `LauncherId`, `Target` (`FBWeaponTargetState`) | **the launch programming of a guided round**: who is shooting, and what the shooter's fire control had made of the target at the moment of launch. A missile leaves the rail already knowing where to start looking — that is what makes an inertial midcourse possible at all — and it knows whose uplink to listen to afterwards for corrections. Both null/invalid for an unguided store |
| `Solution` (`FBReleaseSolution`) | the unguided half of the same idea. Invalid for a guided round, which is aimed by its seeker and not by a table |

#### 7.3 `FBBallistics` — where an unguided store lands

`core/FBBallistics.h/.cpp`. **The ONE arithmetic behind BOTH air-to-ground delivery procedures**
(`doc/modules/f16/weapons.md` §2.5), so that the two cannot drift apart: it is the same forward integration,
asked two different questions.

| Mode | Question | Function |
|---|---|---|
| CCIP | "if I release now, where does it hit?" | `FBSolveImpactPoint` |
| CCRP | "given that point down there, when must I release?" | the same prediction, projected onto the current ground track (`FBSolveAim`) |

**What is integrated — and why deliberately NOT what the bomb then flies**: at the moment of leaving
the pylon the round becomes its own JSBSim instance (`modules/stores/FBStoreModule`), with the full
aerodynamics of the vendored model — Mach-dependent drag, lift at the alpha it trims to, pitch damping.
A real fire control computer has none of that: it carries a stored ballistic table (mass, one drag
coefficient, one reference area) and integrates a point mass. That is exactly what this does too, from
`core/FBStore.h`'s `FBWeaponPerf` — THE SAME table structure on which the launch envelope of the guided
rounds already runs, for the same stated reason.

**The model, complete, with every assumption on the surface:**

```
a = -g·u_up  -  (0.5 · ρ(h) · v² · Cd · S / m) · v̂
```

Gravity plus axial drag along the (negative) velocity vector. **The density is ISA at the CURRENT
altitude of the falling round** (`core/FBAtmosphere.h`), re-evaluated at EVERY step — a bomb released
from 4 km falls through a third of the atmosphere on its way down, and a single-density approach (which
the launch envelope can afford over the flat band of an engagement) would be wrong here by more than
the measured effect is large.

**NOT modelled** — each of these a stated omission OF THE COMPUTER, not of the simulation: lift (the
round is a point mass that never develops alpha), wind (there is none in this simulator), the Coriolis
term, the Mach dependence of Cd.

**The impact plane is PASSED IN, never looked up**: this file knows no terrain. The caller supplies the
elevation against which it solves — with the F-16 fire control the same elevation provider sample that
the radar altimeter and the mission's ground truth already use. A flat plane at that altitude is
exactly what a jet with a barometric/steerpoint-elevation ranging solution has (`'B'`, the same
provider letter that the slant range carries).

**Numerics:**

| Parameter | Value | Derivation |
|---|---|---|
| `kStepS` | **0.05 s** | an Mk-82 released at 450 kt from 4 km falls ~30 s, hence 600 steps of a six-term update — cheap enough for the 10 Hz fire control slot and fine enough that the step error lies far below the modelling error that the whole prediction is meant to expose (**measured: halving it shifts the impact point by well under a metre**) |
| `kMaxTofS` | **120 s** | leak protection, not physics: nothing this file integrates falls for two minutes |
| Integrator | **Heun** (predictor + corrector on the same acceleration law) | the drag term is quadratic in the velocity, and plain Euler distorts the range over a long fall by several metres at this step size — an error of the same order of magnitude as the measured effect, hence not one to accept for free |
| Impact interpolation | linear within the step that crosses the plane | the whole prediction is a sub-metre statement, and quantising the impact point onto a 0.05 s grid would throw away ~15 m of range at release speed |

`k = 0.5 · DragCoefA · RefAreaM2 / LaunchMassKg` (i.e. `drag/(ρ·v·v)`).

**`FBReleaseState`**: `LatDeg`, `LonDeg`, `AltM`, `VelE`, `VelN`, `VelU` — the pylon position and the
velocity vector inherited from the carrier (ENU m/s, geodetic degrees, m ASL).

**`FBImpactPrediction`**: `Valid`, `LatDeg`/`LonDeg` (**geodetic, hence `double`**: a `float` carries
~1e-5° = one metre, and the metre is the measured quantity), `ElevM` (the caller's plane), `TofS`,
`RangeM`, `BearingDeg` (true, 0..360), `ImpactSpeedMs` (what it arrives with — the closure with which a
ground burst is resolved), `ArmMarginS`.

`Valid == false` for a table that cannot be integrated (no mass, no reference area) or for a release
already at/below the impact plane.

**`ArmMarginS` = `TofS − ArmingS`** — the pull-up anticipation cue (`weapons.md` §2.5: the altitude
margin a release needs "for the fuze to arm"), expressed as the margin the computation actually
produces: how much fall is left AFTER the arming delay has elapsed. Negative = a release from here
arrives unarmed (the guide's dud case). The real jet draws it as a screen position running towards the
FPM; a margin in seconds is the same fact in the form in which a decision is made, and it needs no
second integration. A store without a declared delay leaves it equal to the whole fall time.

**`FBSolveAim` — the same prediction, measured against a target**: BOTH points (the predicted impact
and the designated aim point) are projected onto the aircraft's CURRENT ground track
(`FBTrackProjectM`) — that is the axis on which a release cue lives: the round can only be shifted
along it by WAITING, and across it only by TURNING. One projection answers both modes; **there is
deliberately no second geometry for the second mode**.

| `FBAimSolution` | Meaning | Who reads it |
|---|---|---|
| `AlongErrM` | + = the round would fall TOO SHORT; 0 = release now | CCRP |
| `CrossErrM` | + = it would fall RIGHT of the target (steering line error) | both |
| `MissM` | both combined — distance of the CCIP pipper from the target | CCIP ("am I on it") |
| `TimeToGoS` | `AlongErrM` at the current ground speed; < 0 = the release point has passed | CCRP countdown |

**A release cue needs a COURSE**: both errors are projections onto the direction of motion, and an
aircraft that is not moving has none — a stationary (or not yet stepped) state therefore yields NO
solution instead of a zero solution (`groundSpeedMs <= 1.0`). That is the difference between "release
now" and "no answer".

#### 7.4 `FBGun.h` — the gun catalogue

`core/FBGun.h`. The sibling of `FBStore.h`, and deliberately a SEPARATE file, because the two weapons
differ in their KIND: a store is an object that hangs on a pylon and becomes its own JSBSim unit at the
moment of release; a gun is a fixed installation that never leaves the aircraft and whose product is a
stream of projectiles, far too numerous to be units at all (6,000 rd/min against a 0.1 s tick is **ten
projectiles per tick per firing aircraft**).

**THE ONE MODELLING DECISION**: A BURST IS A BALLISTIC BUNDLE. Every projectile that one tick of
trigger pressure produces shares ONE start point, ONE start velocity and ONE integration
(`core/FBGunProjectiles`); what makes it a burst instead of a single shot is that it carries a COUNT
and a DISPERSION ANGLE and that both enter the hit resolution as a DENSITY, never as a position.
**Nothing is claimed about the position of an individual projectile, because nothing here knows it.**

**What is physics and what is modelling** (the whole point of the split — no number below can be
mistaken for a measurement):

| Category | Content |
|---|---|
| **PHYSICS** (integrated, not tabulated) | the bundle's trajectory. Muzzle velocity adds to the aircraft's own velocity vector, gravity acts, quadratic drag decelerates against the ISA density at its own altitude (`core/FBGunBallistics.h`). Time of flight, drop and impact velocity are therefore COMPUTED, and the fire control's lead solution is a solve against the same trajectory instead of a lookup |
| **MODELLING** | (a) that a bundle stands for N projectiles, (b) that the projectiles in it lie as a circular normal distribution about its axis, (c) that a hit is an EXPECTED projectile count and an areal energy density instead of a set of individual impacts. (b) is FITTED to the sources' one dispersion statement (see `kM61A1`); (c) is what makes the model deterministic — **there is no random number anywhere in the gun path** |
| **NEITHER, and declared absent** | barrel wear, shot-to-shot velocity dispersion, tracer/HEI/API mix (§3 lists six ammunition types; the drum here is ONE homogeneous projectile) and the mass of the ammunition itself |

##### `kM61A1` — the M61A1 Vulcan

`doc/modules/f16/weapons.md` §2.5, §3, §4.1. Source and confidence per number:

| Field | Value | Derivation |
|---|---|---|
| `MuzzleVelMs` | **1030** | 3,380 ft/s for standard projectiles **[T4, §4.1 — "consistent across sources, no T1/T2 found"]**. PGU-28/B is 20 m/s faster; ONE projectile type is modelled |
| `RoundsPerMin` | **6000** | **[ED number, §2.5 and §3 agree]** |
| `Capacity` | **510** | **[ED §3's drum figure.** §2.5 of the same guide says 512; the two differ by two rounds, and §3 is the specification table, so §3 wins — **the discrepancy is noted instead of averaged]** |
| `SpoolUpS` | **0.3 s** | **[T4, §4.1 — marked there as needing T1/T2.** It is modelled, because leaving it out would silently claim full rate from the first moment, which is the LARGER error: at 6,000 rd/min that is ~15 rounds per actuation] |
| `RoundMassKg` | **0.100** | **[SET — and this is the one number that §4.1 expressly does not certify: "do not treat the ED dispersion footnote's projectile mass as authoritative spec data".** FlightBox needs a mass to turn a hit into energy, and uses the ~100 g class as a declared SETTING. **Every kinetic damage number in this simulator is linear in it**, which is why it is named here and nowhere else] |
| `RoundDiaM` | **0.020** | 20×102 mm **[ED §3]** — the calibre, hence the drag reference area |
| `DragCoef` | **0.30** | **[SET]** for a spin-stabilised supersonic projectile. Not a citation, but checkable rather than free: with the mass and calibre above it yields a time of flight to 1,000 m of ~1.3 s (`make -C sim test-gun` prints it), which is the order of magnitude every published 20 mm firing table shows |
| `DispersionSigmaRad` | **2.2295e-3** | **[DERIVED** from the guides' one dispersion specification, §2.5's MIL-DTL-45500/1A citation: "80 % of a 75-round burst within an 8.0 in circle at 1,000 in", hence **80 % within a 4 mil RADIUS**. For a circular normal pattern `P(r<R) = 1 − exp(−R²/2σ²)`, hence **σ = 4 mil / sqrt(2·ln 5) = 2.2295 mil**. The fit is checkable against the SECOND number of the same citation, which was not used for fitting: it predicts 97.3 % inside the 12 mil circle (6 mil radius) which the guide calls "100 %". A uniform distribution on a disc — the obvious alternative — would have put only 44 % inside the 8 mil circle and is therefore **excluded by the source, not by taste**] |
| `MaxBurstS` | **1.0** | **[SET]** — the longest trigger squeeze the gun honours in one command, i.e. 100 rounds. A trigger command is ONE pilot action (`core/FBAvionicsCommand.h`), so it needs a duration; this is the upper bound, not a doctrine |

**DELIBERATELY ABSENT — the MASS of the ammunition.** 510 rounds are on the order of 110 lb, and firing
them off would shift the aircraft's weight and centre of gravity. It is NOT modelled, because the empty
weight of the vanilla `f16.xml` cannot be decomposed (principle 1: the model is read-only and its mass
breakdown is its own) — adding a drum as a point mass would be as likely to double-count as to correct.
The omission is under half a per cent of the take-off weight and is DECLARED instead of hidden.

**`FBGunBurst` — a bundle as the gun hands it over:** `LauncherId`, `Kind`, `Rounds`,
`LatDeg`/`LonDeg`/`AltM`, `VelE`/`VelN`/`VelU`, `SimTimeS`. **The velocity is already the SUM** of
aircraft and muzzle velocity along the barrel axis (that sum is physics, and the gun knows both
halves), so that the receiver integrates a plain projectile and has to know nothing about the firing
aircraft.

**The boundary**: the gun system produces burst records and stops there — exactly as the SMS produces
release records and stops there. What a projectile does to another unit is resolved by the CLIENT on
the published poses, never by the system that fired.

#### 7.5 `FBGunBallistics` — the shared ballistic primitives

`core/FBGunBallistics.h/.cpp`. Pure functions on values, no state, no allocation — exactly what lets
the THREE consumers that MUST agree use literally the same arithmetic instead of three copies of it:

| Consumer | Role |
|---|---|
| `modules/f16/FBF16FireControl` | computes the EEGS aiming solution BEFORE the shot |
| `core/FBGunProjectiles` | flies the projectiles AFTERWARDS |
| `test/weapons/FBTestGun` | checks both against the numbers of `doc/modules/f16/weapons.md` itself |

**Why this is NOT a cheat here**, although with the missile it is avoided for exactly that reason
(`FBWeaponPerf` is deliberately a coarse SEPARATE copy of the missile aerodynamics): the flight of an
AMRAAM is that of a guided airframe — its own JSBSim model, its own autopilot, its own energy
management — and no table predicts it exactly. **A 20 mm projectile is an unguided lump on a ballistic
arc, and the F-16's FCC solves exactly that arc.** Modelling a discrepancy between the two would mean
INVENTING an error, not measuring one.

**The trajectory model** (physics, with its one named simplification): a point mass under gravity and
quadratic drag, `dv/dt = −k·v²` along the velocity, with `k = 0.5·ρ·Cd·A/m` against the ISA density at
firing altitude.

**THE SIMPLIFICATION**: drag acts on the SPEED (magnitude) and gravity on the vertical component
**separately**, instead of on the vector sum. Over the whole usable lifetime of a projectile (under
2 s, under 2 km) the drop is metres against a path of kilometres, so the angle between the two is small
and the decoupling is worth a few centimetres. **What it buys is a CLOSED FORM** — and with it an
exact, iteration-free inverse:

| Function | Formula |
|---|---|
| `FBGunRetardation(spec, rho)` | `k = 0.5·ρ·Cd·(π/4·d²)/m` [1/m] |
| `FBGunSpeedAfter(k, v0, t)` | `v(t) = v0 / (1 + k·v0·t)` |
| `FBGunPathAfter(k, v0, t)` | `s(t) = ln(1 + k·v0·t) / k` |
| `FBGunTimeToPath(k, v0, s)` | `t(s) = (exp(k·s) − 1) / (k·v0)` — the exact inverse. **Guard**: `k·s > 20` → `−1`, because `exp(k·s)` overflows long before any range at which a gun is used, and a nonsense input must not propagate an infinity into a pose |

That is what lets the lead solve below converge in a handful of fixed steps — **without a search and
without per-frame allocation**.

##### The hit/energy model

**`FBGunFluxJm2(rounds, spec, impactSpeedMs, missM, sigmaM, targetAreaM2, extentM)`** [J/m²] — the same
currency in which the fragment flux of `core/FBDamageModel` is expressed, so that ONE damage register
answers for both weapon effects without a second set of thresholds.

**The model in one line**: the projectiles are a circular normal pattern of width `σ` about the bundle
axis, the TARGET is a disc of its presented area, and the expected hit count is the OVERLAP of the two.
Writing the target disc as its own equivalent normal distribution (`σ_t² = A/(2π)`, the width whose
central density equals a disc of area A), this overlap becomes a closed form:

```
hits = N · A/(A + 2π σ²) · exp( −d² / (2·(σ² + A/(2π))) )
```

with `d` = miss distance from the target centre. Every projectile carries `E = ½·m·v_rel²` of kinetic
energy IN THE TARGET'S FRAME OF REFERENCE, and the flux is those hits spread over the SMALLER of the
two areas — the pattern (`2π σ²`) or the target.

**Every limiting case falls out of the same formula:**

| Case | Result |
|---|---|
| Pattern much larger than the target | the target catches `N·A/(2πσ²)` projectiles, and the flux falls off as `1/range²`, because σ grows linearly with range. **That is why a gun is a short-range weapon — DERIVED here instead of imposed by a range limit** |
| Pattern much smaller than the target | every projectile hits, over an area `2πσ²`, and the flux saturates at what a point-blank burst does |
| Burst a few metres off | the target's own EXTENT still catches a part — the term a point-target model gets wrong: an aircraft is metres wide, and a pattern centred on its wingtip puts projectiles into the wing |

**`FBGunExpectedHits(rounds, missM, sigmaM, targetAreaM2, extentM)`** — the expected hit count for the
record (the flux does not need it, but a hit report saying "0.4 projectiles" is more honest than one
saying "a hit").

**TWO SCALES, because an aircraft has two.** `targetAreaM2` is how much MATERIAL it presents; `extentM`
is how far that material reaches from its centre (half the wingspan from astern, half the length from
the side). A single disc of the presented area cannot express both: it is right for a burst on the
fuselage and says "nothing at all" for one four metres out, where a real F-16 still has wings. So the
expected hits are the **LARGER of two readings of the same pattern**:

| Reading | Model | Where right |
|---|---|---|
| COMPACT | the material as ONE disc of area A (`σ_t² = A/(2π)`) | exact when the burst is on the centre — where a lethal burst lies |
| EXTENT | the same material spread thinly over the whole silhouette disc of radius `extentM`, so that a projectile within it hits something with probability `A/(π·extent²)` (`σ_t² = extent²/2`) | right for the wing |

Neither of the two is a measurement, and both are DECLARED. What the pair buys: **a burst does not go
from lethal to literally nothing over one metre of aiming error.** `extentM == 0` switches the second
reading off entirely (a target without a declared extent counts as compact). A bundle can never land
more projectiles than it holds (`hits = min(hits, rounds)`).

##### The lead solution

**`FBGunSolveLead(spec, altM, ownVel…, rel…, tgtVel…) → FBGunAim`**

A fixed-point solve of ONE equation: the path length `s(t)` of the projectile must equal the distance to
the place where the target will be at `t`, in a frame of reference in which the projectile starts at the
aircraft. Written out, the displacement to be bridged is:

```
D(t) = rel + v_target·t + up·(½·g·t²)
```

— the target's current offset, its motion during the time of flight, and the drop OVER which the
projectile has to be aimed in order to compensate for it. Given a direction, `t` follows exactly from
`FBGunTimeToPath`; given `t`, the direction follows from `D(t)`. **Six passes** settle this to well
below one metre, at every range at which the gun is used.

**The last step is the one a naive lead computation gets wrong**: the projectile leaves the barrel with
the AIRCRAFT'S VELOCITY in addition to the muzzle velocity — so the direction in which the projectile
FLIES is NOT the direction in which the barrel POINTS. That is solvable in closed form: decompose the
own velocity into components along and across the demanded direction of flight; the barrel has to be
offset across by exactly as much as makes the muzzle velocity cancel the across component:

```
μ    = sqrt(v_muzzle² − |v_own_across|²)
bore = (μ·flightdir − v_own_across) / |…|
v0   = v_own_along + μ
```

**This offset is the reason why the projectiles of a hard turning fighter go where its nose is not —
and it is the physical origin of the shape of the EEGS funnel.**

Everything is ENU metres/metres per second relative to the position of the firing aircraft.

| `FBGunAim` | Meaning |
|---|---|
| `Valid` | `false` = there is no solution: the target runs faster than the projectile can catch up (`v0 <= 1`), or the aircraft crosses so fast that the muzzle velocity cannot cancel it (`cross² >= v_muzzle²` — cannot happen with 1,030 m/s and an aircraft, **but is checked instead of assumed**) |
| `TofS` | projectile time of flight to the intercept point |
| `RangeM` | distance to that point |
| `BoreE`/`BoreN`/`BoreU` | unit vector along which the gun has to point |
| `SpreadM` | sigma of the pattern there (`DispersionSigmaRad × path length`) |
| `ImpactSpeedMs` | projectile speed RELATIVE to the target on arrival (`v(t) − v_target,along`, clamped to ≥ 0) — the energy is the one the target SEES, so that a head-on burst arrives harder than a tail chase, without that having to be said separately anywhere |

`kGravityMs2 = 9.80665`.

#### 7.6 `FBGunProjectiles` — the projectiles in the air

`core/FBGunProjectiles.h/.cpp`. A fixed pool of ballistic BUNDLES, belonging to the CLIENT, stepped by
it and read by it in order to resolve what a burst hit — the structural sibling of `core/FBDamageModel`
in every way that matters:

- a module can neither reach it nor construct one, **so no aircraft flies its own projectiles and none
  can decide what they did**;
- nothing in it is random, time-dependent or hidden: the same burst from the same geometry flies the
  same trajectory — which makes a gun engagement reproducible across thread counts;
- **it allocates nothing.** The pool is a plain array; a bundle that cannot be taken up is COUNTED
  (`DroppedCount()`) instead of being silently lost, because a pool that quietly ate a burst would break
  the arithmetic of a magazine.

**Why a bundle is not a `units/FBUnit`**: a released store BECOMES a unit, because it is ONE object with
its own airframe, its own FDM and its own verdict. One tick of gun fire is ten projectiles, from one
aircraft, every 0.1 s — a sustained engagement would produce thousands of them, each with a JSBSim
instance, a telemetry file and a monitor. What the projectiles physically ARE does not justify that:
unguided lumps without systems and without decisions, of which only a single question is asked — where
are they and what did they hit. So they live here, as arithmetic.

| Constant | Value | Derivation |
|---|---|---|
| `kMaxBundles` | **64** | enough for four continuously firing aircraft over the whole lifetime of a bundle (10 ticks), with reserve; one actuation produces one bundle per tick |
| `kMaxAgeS` | **3.0 s** | lifetime cap |
| `kMaxPathM` | **3000 m** | ditto; both far beyond the ranges at which the gun is used (`doc/modules/f16/weapons.md` §2.5 puts the funnel's own limit at 3,000 ft) |

**`Bundle`** carries BOTH the previous AND the current position, because a hit is a closest-approach
computation over the SEGMENT of the tick: a projectile covers ~100 m per 0.1 s tick, so a per-tick
distance test would miss almost everything. The same reason for which the proximity fuze works on
segments — the caller uses the same helper.

Fields: `Live`, `LauncherId`, `Spec`, `Rounds`, `LatDeg`/`LonDeg`/`AltM`,
`PrevLatDeg`/`PrevLonDeg`/`PrevAltM`, `VelE`/`VelN`/`VelU`, `PathM` (path length covered — **the lever
arm of the dispersion pattern**), `AgeS`, `FiredS`.

**`Step(dt)`**: drag on the SPEED (closed form), gravity on the vertical, and a TRAPEZOIDAL position
update on the mean of the two velocities — **second order in dt, which counts at 0.1 s ticks and
~1,000 m/s**. The density is re-evaluated at the bundle's current altitude. A bundle below 1 m/s is
retired.

**`Launch(burst)`** returns `false` if the pool was full (and counts `Dropped_`). **`Retire(index)`** is
the caller's verdict: this bundle has been resolved against a target and is spent — **a bundle can hit
once**, the projectiles it stood for went into the target.

**What this class deliberately does NOT model**: projectiles are NOT tracked down to the ground, and
there is no ballistic terrain impact. Air-to-air gunnery is what this pool is for, and claiming a
strafing footprint that nothing here computes would be worse than the declared absence.

#### 7.7 `FBWeaponUplink` — the guidance link value types

`core/FBWeaponUplink.h`. What a launching aircraft transmits to a missile it is supporting, and what
that missile was programmed with on the rail.

**Why it is a RADIATED signature and not a function call**: the AMRAAM's initial guidance is "datalink
command from the launching aircraft … transitions to onboard active radar terminal homing"
(`doc/modules/f16/weapons.md` §2.5, §4.4). This uplink is a TRANSMISSION: the shooter radiates it, and it stops
the moment the shooter stops supporting. So it is published in the shooter's `FBUnitSignature` — beside
the datalink XMT switch and the IFF transponder, under the same snapshot contract — and the missile
READS it through its own comms slot (`modules/missile/FBMissileUplink`), exactly as a receiver reads any
other emission. **Nothing hands the missile a pointer to the shooter, and nothing hands either of them
the truth.**

**What travels in it is an ESTIMATE, not a position.** `FBWeaponTargetState` is what the SHOOTER'S
RADAR made of the target: a position derived from range/bearing/elevation at its own nose, a velocity
differentiated from successive looks, and the **SIM TIME OF THE LOOK** on which it stands. The missile
therefore flies on data as old, as noisy and as wrong as the shooter's sensor picture — the whole point
of the lost-lock case: if the uplink stops, the last of it is all the missile has.

```
FBWeaponTargetState { Valid; LatDeg, LonDeg, AltM; VelE, VelN, VelU (ENU m/s); StampS }
FBWeaponUplink      { Active; LauncherId; FBWeaponTargetState Target }
```

**No identity**: the shooter's radar does not know whom it is looking at either
(`core/FBRadarContact.h`), so the missile cannot know it. `Active` goes false the instant the fire
control loses the supported track, and the missile then has nothing left to receive — the tactically
decisive moment, and the reason why this is a PUBLISHED STATE and not a stream of messages that nobody
could observe. `LauncherId`: **a missile listens only to its own shooter.**

---

### 8. Sensor and EW value types

The four contact/threat types are deliberately constructed as OPPOSITES. What they do NOT carry is in
each case the model:

| Type | What it is | Carries | Deliberately does NOT carry |
|---|---|---|---|
| `FBDatalinkTrack` | a MESSAGE | callsign, team, reported position/vector — identity is free | freshness (it is always "the last message that arrived") |
| `FBRadarContact` | an ECHO | geometry: range, bearing, angle off the nose, closure | **unit id, callsign, team** |
| `FBRwrThreat` | a DIRECTION | relative bearing, mode, estimated emitter type, lethality | **range**, certain identity |
| `FBEmitterSignature` | a RADIATION | mode, kind, the body-fixed beam window, the range gate | identity, transmit power, frequency |

#### 8.1 `FBRadarContact` + `FBIffReply`

`core/FBRadarContact.h`. A return as an ACTIVE radar reports it, and the deliberate counter-design to
`FBDatalinkTrack`. **There is no callsign field, no team field and no unit id here, and that absence IS
the model, not an omission** (`doc/modules/f16/radar-sensors.md`: the FCR processes returns by range/Doppler;
identification is a separate box).

**The one legitimate identity channel is IFF** (`doc/modules/f16/datalink-iff.md`, AN/APX-113): the
interrogator challenges the contact, and a valid Mode 4 reply PROVES FRIENDLY. Everything else stays
UNKNOWN — an enemy and a friend with a dead transponder produce the same `NoReply`. **That is why this
enum has no value "hostile" at all:**

```
FBIffReply { NotInterrogated, NoReply, Friendly }
```

`NoReply` is NOT "hostile": it is the ABSENCE OF PROOF, and the two must never be conflated. Everything
above the sensors that wants to shoot has to live with that — exactly like the pilot of the real jet.

| Field | Meaning |
|---|---|
| `TrackNum` | the radar's OWN file number (1..), assigned in acquisition order and reused after a drop. It exists so that a display or a pilot can follow the same echo across frames without the sensor having to hand out what it does not know — who that is. **Never a unit id.** |
| `RangeM` | SLANT range at the last look (`LookAgeS` ago) |
| `BearingDeg` | true bearing own → contact, 0..360 |
| `ElevAngleDeg` | elevation above the local horizontal (+ = up) — the WORLD-referenced partner of `BearingDeg`, so that a consumer can place the echo in space without having to rotate a look-old body vector back through a now-current attitude. Still pure geometry: it names a direction, never an identity |
| `AzDeg` | azimuth AT THE NOSE, −180..180 (+ = right), body-referenced |
| `ElDeg` | elevation above the boresight plane (+ = up), body-referenced |
| `ClosureMs` | range rate, + = closing |
| `LookAgeS` | sim seconds since the beam last really hit it; > 0 = coasting |
| `Coasting` | held on the last look, not seen in this scan frame |
| `Iff` | `FBIffReply` |

**`kMaxRadarContacts = 8`** — fixed capacity, no heap: `FBState` carries the list inline, building a
picture allocates nothing. Eight matches `kMaxDatalinkTracks` and comfortably exceeds the relevance of
the APG-68's ten TWS track files for the close fights this simulator flies.

#### 8.2 `FBDatalinkTrack`

`core/FBDatalinkTrack.h`. A contact as a COOPERATIVE datalink reports it (MIDS/Link-16, DCS' TNDL —
`doc/modules/f16/datalink-iff.md`). **Not a sensor return**: the sender radiates its own INS/GPS position and
its own identity, so callsign and team come free, and the accuracy is the navigation accuracy OF THE
SENDER, not of the receiver. What a receiver adds is only WHEN it heard it — `ReportTimeS` and the
`AgeS` derived from it, which is why a track is never "live": it is the last message that arrived.

| Field | Meaning |
|---|---|
| `UnitId` | the sender's unit id — the track's identity key |
| `Callsign[25]` | the sender's own name, NUL-terminated (`kDatalinkCallsignLen = 25`: `.fbm` callsigns are 1..24 characters + NUL) |
| `Team` | `FBUnitTeam` |
| `LatDeg`/`LonDeg`/`AltM` | as REPORTED (the sender's own position fix) |
| `HeadingDeg`/`SpeedMs` | the reported velocity vector, in polar form |
| `RangeM`/`BearingDeg` | computed ON THE RECEIVER SIDE: own position → reported position |
| `ReportTimeS` | sim time of the message on which this track still stands |
| `AgeS` | `now − ReportTimeS`; 0 only in the tick in which it arrived |

**`kMaxDatalinkTracks = 8`** — fixed capacity, no heap. Eight is a four-ship plus its package — enough
for this simulator's missions, and the number that bounds the per-frame `FBState` copy of the HUD path.

#### 8.3 `FBEmitter` — what a radar puts into the air

`core/FBEmitter.h`. The third emission in `units/FBUnit`'s `FBUnitSignature` (after the datalink
transmitter and the IFF transponder) and **the first that has a DIRECTION** — the whole reason this
file exists.

**A RADAR DOES NOT RADIATE IN ALL DIRECTIONS.** It puts its energy into a beam, and where that points
decides who hears it — a signature that were only "radar on/off" would model an omniscient warning
receiver, and that is exactly what the real box is not according to `doc/modules/f16/defence-rwr-cm.md` §2.1
("a geometry-gated emission detector, not a ground-truth threat oracle"). What travels here is
therefore the geometry of the beam, **BODY-REFERENCED to the radiating aircraft**: the receiver already
has its published pose (`FBUnit::GetPose`), so it can rotate into its frame with THE SAME
transformation that the emitter's antenna uses for itself (`core/FBGeodesy.h`'s `FBEnuToBodyLos`) and
ask the one question that matters: **am I in that beam?**

**The three signals, and why they are not one** — the difference is tactical, not cosmetic:

| `FBEmitterMode` | Beam window | Meaning |
|---|---|---|
| `Search` | the WHOLE search volume — the antenna sweeps a volume, so the beam crosses everything in it once per frame | **information**: somebody is looking, nobody has found you |
| `Track` | a narrow cone on the tracked target — single target track collapses the pattern onto ONE target: all the power, a pencil beam, continuous; only that target hears it | **warning**: he has you |
| `Guidance` | the same beam | a tracking radar that is at the same time supporting a missile in flight (the shooter's midcourse uplink, `core/FBWeaponUplink.h`) — `doc/modules/f16/defence-rwr-cm.md` §1's flashing circle, hence the MISSILE LAUNCH light |
| `None` | — | this unit does not radiate at all |

A missile's own seeker is **not a fourth MODE but a different KIND of emitter** (`FBEmitterKind`),
because what makes it a threat is what sits behind the antenna, not how it scans.

```
FBEmitterKind { Unknown = 0, AirborneFireControl, MissileSeeker }
```

`Kind` is WHAT radiates, as the emitter itself knows it. A receiver only ever ESTIMATES that (it hears
a waveform, not a nameplate) — which is why `sensors/FBRwrSystem` keeps its own estimated copy instead
of passing this field through.

**`FBEmitterSignature`**: `Mode`, `Kind`, `AzCenterDeg`/`AzHalfDeg`, `ElCenterDeg`/`ElHalfDeg` (centre +
half width of the window, body-referenced), `RangeM`.

**NO IDENTITY, NO POWER FIGURE, NO FREQUENCY** — deliberately the same asceticism that keeps
`core/FBRadarContact.h` on the other side of the fence: the signature says WHAT is radiated and from
where, never WHO does it. **`RangeM` is the emitter's own acquisition gate: the ONE number that stands
in for transmit power here**, because a set that can acquire a fighter at 40 nm puts out about ten
times the energy of one capped at 10 nm. **How far it is HEARD is the receiver's business** (a one-way
path against the emitter's two-way path — `sensors/FBRwrSystem::kBeamRangeFactor`), not the emitter's.

#### 8.4 `FBRwrThreat`

`core/FBRwrThreat.h`. An emitter as a WARNING RECEIVER reports it — the deliberate counter-design to
BOTH of the above. A datalink track is a message and carries a callsign; a radar contact is an echo and
carries range; an RWR threat is neither — it is a DIRECTION from which a signal arrives, plus what the
receiver made of it.

**THE TWO ABSENCES ARE THE MODEL:**

1. **NO RANGE.** An RWR measures bearing and received POWER; it cannot measure range, because it never
   transmitted anything whose return it could time. `doc/modules/f16/defence-rwr-cm.md` §2.1 is explicit: the
   distance of the symbol from the scope centre is RELATIVE LETHALITY, not physical range. So this
   structure carries a lethality number and no metres, **and nothing downstream can accidentally fly a
   range solution out of a warning receiver.**
2. **NO CERTAINTY ABOUT WHO.** `Kind` is what the receiver ESTIMATED from the signal, not what the
   emitter published — the same relationship that `FBRadarContact::Iff` has to the truth. Today the
   estimate is perfect (the threat library is one entry deep); the field exists so that on the day it no
   longer is, no consumer has to change.

**`FBRwrThreatMode { Search = 0, Track, Missile }`** — the TACTICAL CONTENT
(`doc/modules/f16/defence-rwr-cm.md` §1's symbol table, one to one): a plain symbol for a searching set, a boxed
one for a tracking one, a flashing one for one guiding a missile at you — information, warning, threat.
**THE ORDER IS THE PRIORITY ORDER**: a higher ordinal beats a lower one on the display. A missile's
seeker illuminating you is the third case, however it happens to be scanning.

| Field | Meaning |
|---|---|
| `Id` | the receiver's OWN symbol number (1..), in acquisition order — **never a unit id**, exactly the role of `FBRadarContact::TrackNum`, for exactly the same anti-cheat reason |
| `BearingDeg` | RELATIVE to one's own nose, −180..180 (+ = right): the TWA is a relative-bearing display with one's own nose at the top (§1) |
| `ElDeg` | elevation at which the signal arrives, body-referenced (+ = up) — NOT displayed on the real, purely azimuthal scope, but it is what the antenna coverage limit is decided on, so it is published instead of hidden |
| `LethalityNorm` | 0..1, the radial position on the scope: 1 = centre (most lethal) |
| `SignalNorm` | received power, 0..1 of what this receiver can hear at all — **the ONE hint of proximity an RWR really has** |
| `AgeS` | since the last acquisition; > 0 = held, the emission stopped or the beam moved away and the symbol has not dropped yet |
| `Mode`, `Kind` | see above; `Kind` is ESTIMATED |
| `New` | within the new-threat tone window |

**`kMaxRwrThreats = 8`** — the ALR-56M's OPEN display shows 16 and PRIORITY 5
(`doc/modules/f16/defence-rwr-cm.md` §2.1); those are **DISPLAY caps over the detected set**, this here is the
size of the detection table itself and matches `kMaxRadarContacts`/`kMaxDatalinkTracks`.

#### 8.5 `FBCountermeasure` — programs and chaff clouds

`core/FBCountermeasure.h`.

**THE PROGRAM SCHEME IS THAT OF THE AN/ALE-47** (`doc/modules/f16/defence-rwr-cm.md` §2.2, "CMDS CHAFF/FLARE DED
pages"), field by field and range by range: per countermeasure TYPE a burst quantity (cartridges in one
burst, 0–99), a burst interval in the sense of the cartridge spacing (0.020–10.000 s), a salvo quantity
(salvoes in the program, 0–99) and a salvo interval (0.50–150.00 s).

```
FBCmProgramType { int BurstQty; double BurstIntervalS; int SalvoQty; double SalvoIntervalS; }
FBCmProgram { FBCmProgramType Chaff, Flare; }
```

| Field | DED name | Range |
|---|---|---|
| `BurstQty` | BQ | 0..99 cartridges per burst (**0 = this type is not in this program**) |
| `BurstIntervalS` | BI | 0.020..10.000 s between cartridges |
| `SalvoQty` | SQ | 0..99 salvoes (**0 = type not in the program**) |
| `SalvoIntervalS` | SI | 0.50..150.00 s between salvoes |

**Zeroing a type's burst quantity or salvo quantity removes it from the program** — that is how a
chaff-only or flare-only program is expressed: a rule of the real DED page, reproduced here instead of
being replaced by a "type" flag. A program is thereby mission/loadout data, not behaviour.
`Present()`, `Valid()` (checks all four ranges), `Cartridges() = BurstQty · SalvoQty`.

**`FBCmType { Chaff = 0, Flare }`** — only the two consumables this airframe really carries: §2.2
records that the OTHER1/OTHER2 stations exist on the panel and have NO function.

**`FBCmdsMode { Off = 0, Stby, Man, Semi, Auto, Byp }`** — the mode knob (§2.2's state machine table).
Telemetry-visible ordinals: append, never reorder. `FBCmdsModeStr`/`FBCmdsModeFromString` (the latter
without `<cstring>`, by character comparison).

**`FBCmdsStatus { NoGo = 0, Go, DispenseReady }`** — the panel's 3-state status indication (§2.2):
powered-but-failed, ready, and ready-and-awaiting-consent (the SEMI "counter" prompt).

**THE CLOUD IS WHAT MAKES THE PROGRAM MEANINGFUL AT ALL.** An ejected chaff cartridge blooms into a
cloud of resonant dipoles which within about a second has essentially lost the aircraft's whole speed
and hangs in the air mass. These two facts — **a large radar return and NO velocity of its own** — are
the entire physics a radar sees, and both are in this structure: the cloud's position is where it was
dispensed, and it does not move (FlightBox has no wind field, so "stationary in the air mass" =
"stationary").

```cpp
struct FBChaffCloud { bool Active; double LatDeg, LonDeg, AltM; double BloomS; };
```

**The ageing curve and why it has its shape [SET]**: a cartridge is a packed bundle at ejection and only
becomes a useful reflector after blooming; afterwards it keeps growing and thinning until it is no
longer dense enough to compete with an aircraft's return. Hence: nothing before `kChaffBloomS`, full
strength at bloom, then a linear decay to zero at `kChaffLifeS`.

| Constant | Value | Status |
|---|---|---|
| `kChaffBloomS` | **0.3 s** | **[SET]** — blooming is fast (the cartridge is designed to open in the slipstream) |
| `kChaffLifeS` | **8.0 s** | **[SET]** — the usable lifetime is of the order of ten seconds before the cloud is too thin to hold a seeker |
| `kMaxChaffClouds` | **8** | the freshest eight cartridges are published in the dispensing unit's emission signature. Eight covers every program whose salvo spacing stays within a cloud lifetime; older ones are the dispersed ones and are the right ones to lose |

**The sources document dispensing PARAMETERS, never bloom or persistence times** — these two numbers
are FlightBox's own, and they are the two knobs that decide how long a salvo protects; that is why they
stand here as named constants and not in the radar that reads them.

```cpp
inline double FBChaffRcsNorm(double ageS);   // 0..1 relative to its own maximum
```

A free function, because BOTH sides need THE SAME curve: the dispenser, to know when a cloud has
stopped counting, and the radar, to weigh two clouds against each other.

---

### 9. The elevation hook

`core/FBElevationProvider.h`. The ONE seam through which every core consumer of ground elevation goes —
mission ground spawn, AGL/radar altitude, crash detection — so that "where is the ground" is an
INJECTED dependency and not a hard-wired fb-tiles wire.

```cpp
class FBElevationProvider {
  virtual double GroundElevM(double latDeg, double lonDeg) const = 0;
  virtual bool GroundElevPatch(latMin, lonMin, latMax, lonMax, cols, rows, double *out) const;
};
```

**`GroundElevPatch`** is the area query for future terrain-aware guidance: it fills `out` row by row
(`cols`×`rows`, **row 0 = southern edge, column 0 = western edge**). The default implementation simply
loops over `GroundElevM` — correct for every provider; an implementation may override it as soon as a
real batch path (e.g. ONE DEM tile decode over the whole patch) is worth the code. It returns `false`
exactly when `out` is null or the grid is degenerate (`cols<2` or `rows<2`); a single unresolved sample
writes the sentinel and does NOT make the patch fail.

**The sentinel:**

| Symbol | Value | Meaning |
|---|---|---|
| `kFBElevationUnresolved` | **−1e9** | "not resolved yet" — matches the existing convention of `fb_stream_ground` (`FBTerrainLoader.h`), so that `FBTilesElevation` is a pure pass-through |
| `FBElevationResolved(m)` | `m > -1e8` | the ONE "is this sample usable" check. Every caller used to write `sample > -1e8` by hand — the same magic threshold in the runner, in the browser loop and in the boot path; a named predicate keeps it one rule |

**All implementations are SYNCHRONOUS from the caller's point of view**: a client that is itself
asynchronous (WASM) polls `GroundElevM` until it stops returning the sentinel — exactly as the callers
of `fb_stream_ground` already do.

#### The four implementations

| Class | Location | Gym flag | Behaviour |
|---|---|---|---|
| `FBConstantElevation` | `core/` | — | a fixed elevation everywhere |
| `FBRunwayPlateauElevation` | `core/` | `--elev const` | runway plateaus + smoothstep falloff |
| `FBBakedDemElevation` | `core/` | `--elev baked` (`swiss` still accepted) | the MOD's baked raster, named by `mod.json`'s `"dem"`, bilinear |
| `FBTilesElevation` | **`world/`** (not part of the core lib) | `--elev tiles` | a thin pass-through onto the live fb-tiles DEM |

##### `FBConstantElevation`

`core/FBConstantElevation.h`. A fixed ground elevation, settable at construction (`SetElevM`
afterwards). The gym client sets it automatically to the threshold elevation of the mission's runway
(`FBRunway::ThresholdElevM`), so that a ground mission runs with NO elevation data at all
(principle-4-friendly: deterministic, no network). Default 0 m = sea level, like every other "no data
yet" fallback of this codebase.

##### `FBRunwayPlateauElevation`

`core/FBRunwayPlateauElevation.h/.cpp`. The gym's DEM-free provider. **Why not simply a constant**: a
mission can have SEVERAL runways at DIFFERENT elevations (today one; phase 3's `dest_runway` adds a
second), so a single flat value for take-off + landing in the same run is wrong.

| Zone | Answer |
|---|---|
| within the runway footprint (length × width) + `kPlateauMarginM` = **5,000 m** | that runway's own `ThresholdElevM` |
| up to `kFalloffM` = **10,000 m** beyond it | smoothstep (`1 − (3t² − 2t³)`) down to the flat base |
| beyond that | `BaseElevM` (default 0 m) |

The falloff exists so that cruise still reads a plausible (if approximate) AGL instead of a hard edge at
the footprint boundary.

**Overlapping plateaus follow ONLY the NEAREST runway** — the simplest continuous choice. A hard change
between two plateaus of different elevation is only possible where their footprints come close enough to
overlap, which real airfields never do: **document it, do not over-engineer for a case that cannot occur
with today's single-runway missions.**

`FootprintDistM` is the distance-to-rectangle variant of the same along/across projection
(`FBTrackProjectM`) that the mission monitor's off-runway gate uses — with the same **60 m** fallback at
`WidthM <= 1`.

##### `FBBakedDemElevation`

`core/FBBakedDemElevation.h/.cpp`. Loads a small baked island raster ONCE and answers `GroundElevM` by
**bilinear interpolation**; **0 m outside the raster's bounding box** (the "island" contract: the asset
covers only Switzerland, everything else reads sea level). On a load/format error `Ok()` is false and
`GroundElevM` always returns 0 — it DEGRADES to the flat sea-level fallback instead of crashing a
mission boot.

**The numbers of the Swiss raster** (`sim/tools/bake_dem.py --region swiss`, not a build target — run only on a
change):

| Parameter | Value |
|---|---|
| File | `mods/f16/src/data/swiss-dem-90m.bin` |
| Bounding box | **5.96–10.49 °E / 45.82–47.81 °N** |
| Target resolution | **90 m** (`TARGET_M`) at the mean latitude of the box |
| Raster size | `cols = round(Δlon · 111320 · cos(lat_mid) / 90) + 1`, `rows = round(Δlat · 111320 / 90) + 1` (~3900 × 2450) |
| File size | **18,888,520 bytes** (~18.9 MB) |
| Source | Terrarium tiles, zoom **11** (~52–53 m/px here, finer than the 90 m output), bbox + 1 tile of margin (~580 tiles), resampled bilinearly in-process onto the 90 m grid |
| Edge treatment | the outer **15,000 m** (`EDGE_BLEND_M`) of the box smoothstep down to 0 m — avoiding a hard cliff at the bbox edge where real terrain is non-zero (e.g. the Alps on the southern edge) |
| Outside the box | 0 m |

**The asset layout** (little-endian; **all fields read at fixed byte offsets, NEVER as a cast onto a C
structure**, so that padding/alignment can never desynchronise the reader from the writer):

| Offset | Type | Field |
|---|---|---|
| 0 | `char[8]` | magic `"FBDEM01\0"` |
| 8 | `uint32` | `cols` |
| 12 | `uint32` | `rows` |
| 16 | `double` | `lonMin` |
| 24 | `double` | `latMin` |
| 32 | `double` | `lonMax` |
| 40 | `double` | `latMax` |
| 48 | `float` | `scaleM` (int16 sample × `scaleM` = metres) |
| 52 | `uint32` | reserved (0) |
| 56 | `int16[rows·cols]` | row by row, **row 0 = `latMin` (south), column 0 = `lonMin` (west)** |

`kHeaderBytes = 56`. A grid with `cols < 2` or `rows < 2`, or a file that is too short, is rejected.

**Gym default**: `baked` if `mod.json` names a `"dem"` and it is on disk, otherwise `const` — a bare `fb-gym --mission FILE`
always runs, with or without a network.

---

### 10. Geodesy, atmosphere, units, mathematics

#### 10.1 `FBGeodesy` — the ONE planar ENU geodesy

`core/FBGeodesy.h`, header-only, no translation unit.

**Why this file exists**: the same five-line block `dlat*111320, dlon*111320*cos(lat)` stood in SIX
places (`core/FBMissionMonitor`, `core/FBRunwayPlateauElevation`, `pilot/FBPilot`,
`systems/FBNavSystem`, `systems/FBAutopilot`, `clients/FBAppWasm`) — **and only some of them wrapped the
longitude difference into [−180,180]**. The unwrapped copies read a ~360° delta across the antimeridian,
hence **~38,000 km of distance to a point one metre further on**: at 180° of longitude the mission
monitor's waypoint capture, the runway plateau elevation and the HUD's home distance were simply wrong.
The wrapping is now part of the primitive, not something every caller has to remember.

**CONVENTION**: the reference point comes FIRST and owns the cosine. `FBEnuOffsetM(ref, p)` returns the
offset of `p` FROM `ref`, with the longitude scaling at the REFERENCE latitude — one rule, so that a
bearing and a distance computed by two different subsystems agree. Whoever only needs a distance may
pass either of the two points as the reference (the offsets differ only in sign, the magnitude is
identical).

**SCOPE**: deliberately planar/small-angle, matching what every call site did anyway — steerpoints,
runway axes and waypoint captures lie tens of nautical miles away, not intercontinentally. Real geodetic
mathematics belongs to whatever needs it, not to the callers of this file.

| Function | Contract |
|---|---|
| `FBGeoToEcef(lat, lon, alt, out[3])` | **WGS84 geodetic → ECEF (m). The one function here that is NOT small-angle** — the exact ellipsoid conversion on which the renderer's camera-relative ECEF world stands (`a = 6378137.0`, `e² = 6.69437999014e-3`). It stood character-identically in both app entry points before it moved here |
| `FBEnuAxesEcef(lat, lon, E, N, U)` | the local ENU axes in ECEF — the rotation with which every ECEF vector/camera conversion begins (`render/FBCamera.h`'s `FBCameraBasisEcef`) |
| `FBWrap180(deg)` | angle difference in [−180,180]. The LOOP form (not `fmod`) is the one every existing call site used; it is exact for the one-or-two-revolution deltas that actually occur |
| `FBEnuOffsetM(refLat, refLon, lat, lon, eastM, northM)` | planar offset: `north = Δlat · kMPerDeg`, `east = FBWrap180(Δlon) · kMPerDeg · cos(refLat)` |
| `FBPlanarDistM(...)` | horizontal distance (unsigned) |
| `FBBearingDeg(ref, p)` | true bearing 0..360 (`atan2(e, n)`) |
| `FBEnuToBodyLos(roll, pitch, yaw, e, n, u, azDeg, elDeg)` | **line of sight → body frame**: ENU in, body-referenced azimuth/elevation out (+az = right of the nose, +el = above the boresight plane) — the standard NED→body Euler sequence `Rx(roll)·Ry(pitch)·Rz(yaw)` applied to the offset, hence **WHAT THE ANTENNA SEES** instead of what a map would show. The one caller of the forward direction is `sensors/FBRadarSystem::RelativeLos`, the one caller of the inverse is the BFM control of `pilot/FBPilot` — **they MUST agree exactly, otherwise the pilot would steer at a point that the radar reports elsewhere** |
| `FBBodyLosToEnu(...)` | the exact inverse, unit length |
| `FBBodyVecToEnu(roll, pitch, yaw, fwd, right, down, e, n, u)` | a BODY vector (+forward/+right/+down, any unit) into local ENU. **Built on `FBBodyLosToEnu` instead of on a second copy of the Euler sequence** — two diverging spellings of the same rotation are exactly the class of error against which this file exists. The one caller is the store release geometry (`missions/FBMissionBoot.h`): a pylon offset and the rotational velocity at that pylon are both body vectors that have to end up in the world frame |
| `FBEnuToBodyVec(...)` | the exact inverse of that, built on `FBEnuToBodyLos`. The one caller is the damage resolution (`missions/FBMissionRunner.cpp`): a detonation happens at a point in the world, and what decides which systems it destroyed is where that point sits along the TARGET's airframe axis |
| `FBTrackProjectM(refLat, refLon, courseDeg, lat, lon, alongM, acrossM)` | **along/across projection** onto the line through the reference on the true course: +along down the course, +across to the right. The runway axis primitive that the mission monitor's on-runway gate, the plateau provider's footprint, `FBPilot`'s centreline control and `FBAutopilot`'s localizer all need — **one definition, so that "on the line" means the same for the pilot who flies it and the monitor who judges it** |

#### 10.2 `FBAtmosphere` — ISA

`core/FBAtmosphere.h`, header-only. **For the two consumers that have to compute about air in which
they are not currently flying** — everything that flies has JSBSim's own atmosphere behind it
(`aero/qbar-psf` etc.):

1. the launch envelope integration of `modules/f16/FBF16FireControl`, which predicts the flight of a
   weapon before that weapon exists;
2. the gain schedule of `modules/missile/FBMissileGuidance`, which needs the dynamic pressure acting on
   its own airframe from the pose it is handed (`fb_fdm_state` carries no qbar).

One definition instead of two private copies of the same four constants.

| Function | Model |
|---|---|
| `FBIsaDensity(altM)` | troposphere up to **11,000 m** with the standard temperature lapse rate **6.5 K/km**: `T = 288.15 − 0.0065·h` (clamped to ≥ 1 K), `ρ = 1.225·(T/288.15)^4.2561`. Above it isothermal: `ρ = 0.36391·exp(−(h−11000)/6341.62)` |
| `FBDynamicPressure(tasMs, altM)` | `q = ½·ρ(h)·v²` [Pa] |

**No wind, no weather, no non-standard day**: the consumers above are a stored fire control table and a
gain schedule, and neither would be improved by a fidelity that the rest of the engagement does not
have.

#### 10.3 `FBUnits` — the ONE definition of every conversion factor

`core/FBUnits.h`, header-only, `constexpr`.

**Why this file exists**: the same numbers were re-declared privately file by file — `kMPerDeg` six
times, `kMsToKt` five times, π six times, `kR2D` four times — **and one of them had DRIFTED**:
knots→m/s stood as `0.51444444444` in `missions/FBMissionBoot.h` (the spawn IC) and as `0.5144444444` in
`modules/f16/FBF16Module.cpp` (the commanded target speed). So the speed a mission DECLARED and the one
the pilot COMMANDED were converted with different precision — exactly the class of error that multiplies
as soon as several units fly at the same time, and which no reader can see from inside one file.

| Constant | Value | Status |
|---|---|---|
| `kPi` | 3.14159265358979323846 | |
| `kDeg2Rad` | `kPi/180` | |
| `kRad2Deg` | 57.29577951308232 | |
| `kMPerDeg` | **111320.0** | metres per degree of latitude, spherical approximation. The planar ENU convention of this codebase (see `FBGeodesy.h`): valid for the tens-of-nautical-miles scales over which FlightBox actually measures, not for intercontinental geodesy |
| `kFtToM` | **0.3048** | **exact**, by definition of the international foot |
| `kMToFt` | `1/kFtToM` | |
| `kNmToM` | **1852.0** | **exact**, by definition of the nautical mile |
| `kMToNm` | `1/kNmToM` | |
| `kKtToMs` | `kNmToM/3600` | **exact**: 1 kt = 1 nm/h |
| `kMsToKt` | **1.9438444924406** | **a historical 14-digit literal, deliberately kept**: it is consumed by the telemetry columns and by the ground speed gate of `FBMissionMonitor`, and every digit already agreed bit-exactly — re-deriving it as `3600/1852` would shift measured numbers for no gain |

**Values are EXACT definitions where one exists** — writing the ratio instead of a truncated decimal is
both more accurate and self-documenting.

#### 10.4 `math/FBMat4` — renderer mathematics

`sim/src/math/FBMat4.h`. The only content of `math/`. **Column-major, OpenGL convention**: element
`m[c*4+r]` is column c, row r; multiplication with a column vector `v` gives `m*v` (matching
`glUniformMatrix4fv(..., GL_FALSE, m)`).

**Why it was extracted from `world3d.h`**: it is the one part of the renderer that needs NO GL context —
pure float mathematics, hence directly assertable instead of being judged on pixels. **A wrong sign here
does not crash**; it silently mirrors the world or turns the camera inside out — exactly the class of
error a unit test catches and a pair of eyes does not.

Style note: the file does NOT follow the `FB` class convention of the rest of the tree — it exposes free
`static` C-style functions (`m_identity`, `m_mul`, `m_persp`, `m_lookat`, `v_norm`, `v_cross`), without
`namespace FlightBox`.

| Function | Contract |
|---|---|
| `m_identity(m)` | identity matrix |
| `m_mul(o, a, b)` | `o = a·b` (via an intermediate buffer, hence aliasing-safe) |
| `m_persp(m, fovy, asp, zn, zf)` | **REVERSED-Z perspective**: near maps to NDC z=+1 (window depth 1.0), far to −1 (0.0) — the standard `zn`↔`zf` swap in the z row. Together with `glClearDepthf(0)` + `GL_GEQUAL` and the 32-bit float depth buffer, the projection's 1/z curve cancels the distribution of the float mantissa and delivers **nearly uniform precision over 0.01 m…240 km**, where plain depth z-fights distant terrain into shimmering. x/y (`m[0]`, `m[5]`) and w (`m[11]`) are unchanged, so screen projection, the HUD's manual projection and the frustum extraction stay untouched |
| `m_lookat(m, eye, ctr, up)` | world → view: camera at `eye`, looking at `ctr`, with approximately `up` as up |
| `v_norm(v)` | normalises (a no-op below length 1e-6) |
| `v_cross(o, a, b)` | cross product |
