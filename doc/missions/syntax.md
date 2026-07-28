# Mission format — syntax, data model, tick order

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Syntax", "Datenmodell" and "Tick-Reihenfolge und Snapshot-Regel". Translated 1:1 from the German
original; no revision of content. Parser: `sim/src/core/FBMissionFile.h` (`FBParseMissionFile`) — a
pure text-in/`FBMission`-out function, no file I/O (the app does that).

The binding reading rules for the whole format are in [`INDEX.md`](INDEX.md); the actor/runner
machinery behind it is in [`runtime.md`](runtime.md).

---

## Spec

Zero-dependency, line-based text format for the mission orchestrator (`gpu_native --mission FILE`,
`fb-gym --mission FILE`).

A mission describes a **FLIGHT**: mission-wide data (name, optional runway, timeout) plus a **list of
actor blocks** (`unit <callsign>`). Every block is exactly one simulated unit (`units/FBSimUnit`)
with its own module, its own team, its own initial state and its own objectives. A single flight is
the special case "one block" — no second dialect, no special path in the code.

### Line syntax and the two scopes

One statement per line, `#` opens a comment to end of line, blank lines are ignored, leading
indentation is purely cosmetic. **Two scopes:**

- **mission-wide** — `name`, `runway`, `timeout`, `wx`, `time`. Must stand BEFORE the first `unit` block.
- **actor-scoped** — `module`, `team`, `flight`, `spawn`, `set`, `wp`, `land`, `objective`. Only
  INSIDE a `unit` block; a block runs until the next `unit` or end of file.

Both directions are hard parse errors (a `runway` line between two units would otherwise be silently
"mission-wide, just declared late"; a `spawn` line before the first `unit` would have no owner).

### Single jet (example: `sim/missions/payerne-takeoff-only.fbm`)

```
name payerne-takeoff-only-1
runway 46.84335 6.91523 441.0 228.0 2889     # lat lon elevM trueHdgDeg lengthM (mission-wide)
timeout 600                                  # sim seconds until TIMEOUT

unit viper                                   # block start: this unit's callsign
  module f16                                 # FBModuleRegistry key -> FBF16Module
  spawn threshold ground 228.0 0             # 'threshold' = the runway line above, 'ground' = on the gear
  wp 46.74293 6.75267 2500 350               # lat lon altM speedKt (enroute)
  wp 46.66467 6.62666 3000 350               # no 'land': SUCCESS = both waypoints reached
```

### Pair (example: `sim/missions/payerne-pair.fbm`)

```
name payerne-pair-1
runway 46.84335 6.91523 441.0 228.0 2889     # mission-wide: applies to EVERY unit (off-runway check)
timeout 600

unit lead
  module f16
  team friendly
  spawn 46.73800 6.72200 2500 48.0 300       # air start, 2500 m ASL, heading 048, 300 kt
  set gear up
  set fuel_pct 60
  wp 46.84335 6.91523 2900 320               # ITS OWN waypoints
  wp 46.95000 7.05000 3200 320

unit two
  module f16
  team friendly
  spawn 46.72600 6.73900 2500 48.0 300       # ~1.5 km south-east of lead
  set gear up
  set fuel_pct 60
  wp 46.81000 6.95000 2100 300               # its own, lower route
  wp 46.90000 7.12000 2400 300
```

### The keyword table

| Scope | Keyword | Fields | Meaning |
|---|---|---|---|
| Mission | `name`    | rest of line | mission name (telemetry/logs) |
| Mission | `runway`  | lat lon elevM trueHdgDeg lengthM | optional landing geometry (`FBRunway`) — needed for `spawn threshold`, `land`, and `FBMissionMonitor`'s off-runway-touchdown FAIL check; a pure air-start mission without landing intent needs no `runway` line. Mission-wide: all units share it. Width unused (0, default fallback in the monitor). |
| Mission | `timeout` | seconds (>0) | sim time until TIMEOUT if the mission does not end earlier. Applies to every unit. |
| Mission | `wx`      | `calm` \| `fixture <name\|path>` \| `wind <dirDegFROM> <speedKt>` | the ATMOSPHERE of the mission (`core/FBWeatherProvider`), optional, at most once, default `calm`. See [`weather.md`](weather.md). |
| Mission | `time`    | `<YYYY-MM-DDThh:mm:ssZ>` | **[SPEC, NOT BUILT — gap `C2`]** the CLOCK of the mission: the UTC instant at `simT = 0`. Optional, at most once; absent = no clock at all (see below). Consumed by the ephemeris and by everything that depends on where the sun is; it changes no physics. |
| Actor  | `unit`    | callsign | block start. 1–24 characters from `[A-Za-z0-9_-]` (the callsign also names the telemetry file and the `unit=` log attribution), unique mission-wide. |
| Actor  | `module`  | rest of line | module name, resolved through `FBModuleRegistry` — determines both the `FBModule` and the JSBSim aircraft folder name (`sim/assets/aircraft/<module>`). Mandatory per block. |
| Actor  | `team`    | `friendly`\|`hostile`\|`neutral` | team (`FBUnitTeam`, `core/FBTeam.h`) — lands in the unit registry that sensors and weapons read. Optional, default `friendly`. |
| Actor  | `flight`  | name position | the FLIGHT this unit belongs to and the position it holds in it (`FBFlightId`, `core/FBFlight.h`) — identity beside the team, and read off the registry by the cooperative datalink for the same reason. Position 1 is the LEAD; 2..8 are wingmen (8 because a flight cannot be larger than the track list that carries it). Optional, at most once per block: a unit without it is in no flight, and every piece of formation behaviour is then a no-op. See [`../formation.md`](../formation.md). |
| Actor  | `spawn`   | `<lat lon \| threshold>` `<altM \| ground>` `hdgDeg` `speedKt` | mandatory, exactly once per block: this unit's declarative IC — position, altitude-OR-ground, heading, speed. `threshold` takes lat/lon from the mission-wide `runway` line (pure writing convenience, not a second position syntax). `ground` resolves the altitude from terrain plus landing-gear geometry; a numeric value is a LITERAL ASL altitude (an air start). Both cases run through the same single JSBSim IC application. |
| Actor  | `set`     | `key value...` | system state as mission data — the runner only parses the KV list and hands it, inside the spawn IC window, to `FBModule::ApplySetup(key, value)` of THIS unit; the MODULE interprets its own keys. An unknown key is a runtime FAIL (exit 1, `SET_REJECTED` event), not a parse error. The key sets live with their topics: [`sensors.md`](sensors.md), [`avionics.md`](avionics.md), [`weapons.md`](weapons.md), [`combat.md`](combat.md). |
| Actor  | `wp`      | lat lon altM speedKt | an `FBWaypoint` of type `Enroute`, in THIS unit's flight plan |
| Actor  | `land`    | — | an `FBWaypoint` of type `Land` AT the runway threshold (needs the mission-wide `runway` line) |
| Actor  | `objective` | `survive` \| `waypoints` \| `kill unit <callsign>` \| `kill team <team>` | COMBAT OBJECTIVE of this unit (`core/FBObjective.h`) — repeatable, see [`verdict.md`](verdict.md). `kill unit` must name a unit OF THIS MISSION (forward reference allowed, checked at end of file) and not itself; `objective waypoints` needs `wp`/`land` lines above it. A doubly declared objective is a parse error. **Four further kinds are specified and not built (gap `C12`): `identify`, `protect`, `no_fire`, `deny release` — grammar below, semantics in [`verdict.md`](verdict.md).** |

`name`, `timeout` and at least one `unit` block are mandatory; per block `module` and `spawn` are
mandatory. `runway`, `wx` and `time` are optional (`runway` only needed for `spawn threshold`/`land`/the
off-runway check, `wx` defaults to `calm`, `time` defaults to *no clock*).

### The mission clock (`time`) — gap `C2`

**Status: specified here, nothing built.** Written as the contract a build round has to satisfy, per
[`../conventions.md`](../conventions.md)'s spec-first rule. The consumer side is
[`../sensors.md`](../sensors.md) §9 (visual acquisition) and the renderer's ephemeris; the per-client
default lives with the clients ([`../clients/clients.md`](../clients/clients.md)).

```
time 1999-03-24T22:00:00Z        # mission-wide, at most once, before the first `unit` block
```

#### The format, and why Zulu only

One token, **ISO-8601 with a mandatory trailing `Z`**. A mission declares **UTC and nothing else**;
there is no local-time spelling and no offset suffix. Four reasons, in decreasing weight:

1. **The consumers take UTC.** `SunPos`/`MoonPos` are functions of `(lat, lon, utc)`. A local
   declaration would have to be converted back, and the conversion needs either a tz database
   (FlightBox has none, and shipping one for a sun angle is absurd) or a `longitude/15` approximation —
   **an invented offset is a number without provenance**, which `conventions.md` forbids outright.
2. **A mission can span several zones.** W2's ingress is 1,600 km; "local" would be ambiguous *inside
   one run*, and the ambiguity would grow with exactly the missions that care about light.
3. **The tree's other absolute quantity is already Zulu.** The weather fixture is
   `wx-2026-07-27T00Z.wxb` and GFS cycles are Zulu. Two clocks in one scenario, one of them local, is a
   defect generator with no upside.
4. **Local time is a presentation concern.** A briefing that wants "0430 local" derives it from the same
   instant plus a longitude — derived, not declared.

The cost is real and is paid rather than hidden: a mission author writing a night mission over
Batajnica must convert once. It is paid back at `MISSION_START`, where the runner logs the **computed
sun elevation at the primary actor's spawn point** — so the file's intent ("this is night") is
checkable in the output without the author doing spherical trigonometry.

**Parse rules:** exactly the ISO shape above, `Z` mandatory, seconds mandatory, no fractional seconds,
year 1901…2099. Anything else is a parse error, not a best-effort read. The conversion to Unix seconds
is a **calendar computation in `core/`** (days-from-civil), never `mktime`/`timegm`: those read the
host time zone or the host's leap-second view, and the same file would then mean a different sky in
Zurich and in a container. `[SET]` bounds; the range is the one over which the NOAA approximation the
ephemeris ports is stated to hold.

#### The default is *no clock*, and that is what makes the 84 missions byte-identical

| Design | Consequence |
|---|---|
| default = a fixed epoch constant | every existing frame proof moves, the browser's live sky is pinned to 1970, and `gpu_native --utc`'s "0 = wall clock" sentinel changes meaning |
| default = the client's current behaviour, expressed as a value | same problem one level down: the gym has no ephemeris at all today, so there is no value to express |
| **default = `HaveTime = false`, no channel touched** | **recommended.** Nothing is computed, nothing is published, nothing is logged — the same mechanism by which `wx calm` is byte-identical to the tree before the weather hook existed ("the wind is only written on change", [`weather.md`](weather.md)) |

**How that is checked, concretely.** `sim/missions/` holds **84** `.fbm` files today. The acceptance of
the C2 round is the regression gate of [`../build-and-ops.md`](../build-and-ops.md) applied at full
strength:

| Check | Requirement |
|---|---|
| Telemetry | all `telemetry*.csv` of all 84 missions **byte-identical** to the pre-round tree. C2 adds **no column** — a per-run constant does not belong in a 10 Hz time series, and a column would break this line for a value nobody reads |
| Events | all `events.log` byte-identical modulo `wallS`/`speedup`. The `mission CLOCK` line is emitted **only when a clock is declared**, so a mission without the line produces no line |
| Determinism | unchanged single fingerprint over `--threads 1/2/4` × repetitions |
| Frame proofs | `gpu_native` without `--mission` is untouched (no mission file, hence no `time` line, hence today's `--utc` path verbatim) |

#### Who consumes it

| Consumer | What it takes | State |
|---|---|---|
| Ephemeris → `FBEnvironmentBlock` (sun/moon el/az, moon phase) | the instant + the unit's position | exists (`FBEphemeris.h`), fed from a client-side clock today |
| Cloud drift advection (`FBRenderer::SetSkyClock`) | the instant | exists, fed from the same client-side clock |
| **Visual acquisition** (`C3`, [`../sensors.md`](../sensors.md) §9) | sun elevation for the day factor, the sun's direction for the glare term | **specified, not built — and it is why the clock must reach `fb-gym`, which has no ephemeris at all today** |
| Terrain/cloud lighting, night lighting, an infrared background term | the instant | future; named so the clock is not re-invented for each |

**The wiring, and the one structural consequence.** The clock is a mission-wide datum that the OWNER
samples and pushes down — the identical shape ground elevation and the cloud sky already have
(`FBSimUnit::UpdateSky` → `FBModule::SetCloudSky`). So: `FBSimUnit::UpdateSolar` → `FBModule::SetSolar`
with a small value block, computed per unit per decision tick from `(lat, lon, T0 + simT)`. **No system
below the owner ever holds a clock**, exactly as no sensor queries the world.

That forces one move: the ephemeris is `render/FBEphemeris.h` today, and `core/`, `sensors/` and
`systems/` may not include `render/` (`verify-layers`). The pure functions therefore move **down** to
`core/FBEphemeris.h` and `render/` includes them from below. They are already dependency-free, so this
is a relocation, not a rewrite — but it is a layer change and it belongs in the C2 round rather than
being discovered inside the C3 round.

#### What it explicitly does NOT do

- **It changes no physics.** JSBSim's own time base (`sim-time-sec`, the integration, every substep) is
  untouched; nothing is written into any FDM property. The clock is a *stamp*, not an input.
- **It is not a schedule.** It does not delay a spawn, arm a trigger or start a mission late. A staggered
  scramble is still spawn data.
- **There is no time acceleration and no pause semantics.** Sim seconds are seconds: `utc(t) = T0 + t`.
  The clock ADVANCES with sim time, because a 40-minute ingress at sunrise must see the sun move; a
  frozen clock would be a second, cheaper lie and would cost nothing less (`SunPos` is a pure function
  already called per frame in the browser).
- **It is not weather.** `wx fixture` carries a GFS *analysis step* and has no time axis
  ([`weather.md`](weather.md) Gaps); declaring a `time` does not select a different atmosphere and must
  not be read as doing so. The two lines are independent, and a mission whose `time` and whose fixture
  cycle disagree is a mission-authoring problem the runner does not police.

### The four new objective kinds (`C12`) — grammar only

**Status: specified, nothing built.** The semantics, the check order and the conservation argument are
in [`verdict.md`](verdict.md); here only what the parser must accept.

```
objective identify unit <callsign> range <metres> hold <seconds>
objective protect  unit <callsign>
objective protect  team <friendly|hostile|neutral>
objective no_fire
objective deny release unit <callsign>
objective deny release team <friendly|hostile|neutral>
```

| Rule | Detail |
|---|---|
| Every new keyword is a NEW token | no existing spelling changes meaning; `objective kill unit X` parses exactly as it does today |
| `unit`/`team` discriminator mandatory, as for `kill` | for the same stated reason: a callsign may legally be spelled `hostile` |
| Named units are resolved against the WHOLE cast at end of file | same rule as `kill unit`; an objective naming nobody is a parse error, never a silently unmeetable objective |
| A unit may not `identify`, `protect` or `deny release` **itself** | `protect` on oneself is `survive` and has a spelling already; `identify`/`deny` on oneself are meaningless |
| `range` > 0, `hold` ≥ 0, both mandatory on `identify` | there is no default box — it is `[SET]` per mission and must be visible in the file (W5 §Knowledge 2) |
| Exact duplicates are a parse error | unchanged rule; two `identify` lines on the same unit with different numbers are NOT duplicates and are legal (a coarse pass and a close pass) |
| `no_fire` takes no argument | it is about this unit only |

### Parse errors versus runtime FAIL

Parse errors (`FBParseMissionFile` returns `false` plus a message in `*err`) are: unknown keyword, an
actor line without an open block, a mission-wide line after the first block, a duplicate or
non-file-safe callsign, two `spawn` lines in one block, `spawn threshold`/`land` without `runway`, a
`set` without a value, a `team` outside the three values, a missing mandatory field — and, for
`flight`: a second `flight` line in one block, a position outside 1..8, a name that is not
`[A-Za-z0-9_-]` or is 16 characters or longer (it travels in a fixed PPLI field), two units at the same
(name, position), and — checked at end of file, like a `kill unit` forward reference — **a declared
flight with no unit at position 1**. Every piece of formation behaviour is defined against the lead,
so a flight without one is not a flight.

A `module` that `FBModuleRegistry` does not know, or an unknown `set` key, is a runtime FAIL of the
runner (not of the parser).

**Consistency validation at set-up:** a physically contradictory declaration is a FAIL as soon as the
runner has resolved the elevation (not already in the parser, which knows no geodata) — today: an
explicit `spawn` altitude below the resolved ground. `v=0` in the air is by contrast NOT an error
(legal, the unit then simply falls — `FBFlightMonitor` judges that like any other flight state).

### Data model

`FBMission` = `Name` + optional `FBRunway` (`HaveRunway`) + `TimeoutS` + `FBWeatherSpec`
(`HaveWeather`) + [`C2`: `UtcT0S` + `HaveTime`] + `Units` (list of `FBMissionUnit`). `FBMissionUnit` = `Id` (callsign) + `ModuleName`
+ `Team` + `FBFlightId` (`Flight`, `Position` 0 = undeclared) + `FBSpawn` (`HaveSpawn`) + `SetKV` (file order) + `FBFlightPlan` (the `wp`/`land` lines in
file order) + `Objectives` (the `objective` lines, `std::vector<FBObjective>`). A landing objective is
not a flag of its own: the flight plan then ends on an `FBWaypointType::Land` waypoint, and that is
exactly how the monitor recognises the standstill rule ([`verdict.md`](verdict.md)).

The orchestrator (`missions/FBMissionRunner.cpp`, shared by `fb-gym` and `gpu_native --mission`)
spawns ONE actor per block (`FBMissionBoot.h::FBMissionSpawnActor`: `ModuleName` through
`FBModuleRegistry::Create` into a `std::unique_ptr<FBModule>`, one IC, plan/runway/`set` on top of it)
and from then on holds EVERYTHING only through the generic `FBModule` accessors (`Autopilot()`/
`FlightControl()`/`PilotSystem()`/`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/
`Telemetry()`/`ApplySetup()`) — the runner never names a concrete module type and contains no
mission-specific code (no runway-threshold spawn, no waypoint advance — both sit in the boot resp. in
`systems/FBNavSystem::AdvanceWaypoint`, the module itself). The ground elevation for a `ground` spawn
comes from the injected `FBElevationProvider` (`--elev tiles|const|swiss`) — the file elevation of the
`runway` line is only documentation/fallback.

### Tick order and the snapshot rule

All actors run in ONE thread, in **file order** (index 0 = primary actor: its telemetry keeps the
canonical file name, its eyes are the camera). Per tick:

1. resolve ground elevation per actor,
2. `Run()` per actor (module → guidance → FLCS → JSBSim substeps),
3. **barrier**: `PublishPose()` per actor,
4. feed both monitors per actor,
5. sample telemetry per actor.

Step 3 is the **snapshot discipline**: `FBUnit::GetPose()` — what the unit registry shows to other
units (sensors, weapons) — ALWAYS delivers the pose of the last COMPLETED tick, never a half
integrated one. With that, the tick ORDER cannot influence the result; the parallelisation (one thread
per unit, lockstep barrier) is then a pure parallelisation, not a rebuild. The WASM frame loop runs
the same order.

## State

| Item | State |
|---|---|
| Parser | `core/FBMissionFile.h/.cpp`, pure text→`FBMission`; every parse error listed above is produced |
| Registered module names | `f16`, `mk82`, `aim120`, `target_soft`, `target_hard` (the registration files under `sim/src/modules/*/`) |
| Spawn | one IC application for ground and air (`missions/FBMissionBoot.h::FBMissionSpawnActor`) |
| Tick order | as specified; the gym parallelises only the STEP phase and is bit-identical over `--threads 1..4` |
| Consistency validation | today exactly one rule: explicit spawn altitude below resolved ground |
| `time` line (`C2`) | **not built.** Specified above; no clock reaches the gym at all today |
| The four new objective kinds (`C12`) | **not built.** Grammar above, semantics in [`verdict.md`](verdict.md) |

## Gaps

| Gap | Detail |
|---|---|
| **`C2` — no mission clock** | 30+ missions across the ten campaign specs are night missions and not one can say so ([`../campaigns/INDEX.md`](../campaigns/INDEX.md)). The renderer already owns the ephemeris; the gym owns nothing. Contract above |
| **`C12` — the objective vocabulary is four kinds** | five campaigns cannot state what they measured. Grammar above, contract in [`verdict.md`](verdict.md) |
| `FBWaypointType` declares four values, the parser produces two | `Takeoff`, `Enroute`, `Approach`, `Land` are declared in `core/FBFlightPlan.h`; `wp` always produces `Enroute` and `land` always `Land`. There is no syntax for `Takeoff` or `Approach`. |
| Runway width is unused | the `runway` line carries no width; the monitor falls back to a default footprint margin |
| Only one runway per mission | `FBRunwayPlateauElevation` can already hold several (start + a future `dest_runway`), the format declares only one |
| `module` and `set` keys are validated at runtime, not at parse time | a typo in a module name or a `set` key costs a full spawn before it fails |
| No mission-data layer for the picture mode | SVS (OSM) versus EVS (photo) is a client switch (TAB / `--albedo`), a `.fbm` cannot declare it — see [`../world/terrain.md`](../world/terrain.md) |

## Knowledge

- **Why a single flight is not a second dialect.** `FBMission` is mission-wide data plus a LIST of
  `FBMissionUnit`. One block is the degenerate list, so no code path exists that only a single-unit
  mission takes. The same holds for telemetry: one CSV per unit means N=1 needs no special case (the
  lines stay byte-identical).
- **Why `threshold` is only writing convenience.** It copies lat/lon from the mission-wide `runway`
  line into the same `FBSpawn` fields a numeric pair would fill. There is no second position syntax
  and no second IC path.
- **Why the ground/air distinction is data and not code.** `spawn` carries position, altitude-OR-
  ground, heading, speed; `ground` resolves through the elevation provider plus the gear geometry of
  the model. Both cases end in the same single JSBSim IC application, so a ground start cannot drift
  away from an air start through a divergent code path.
- **Why step 3 is a barrier and not a per-unit publish.** If a unit published its pose inside its own
  `Run()`, a later unit in the same tick would read a NEWER pose than an earlier one, and the file
  order of the mission would become a physical input. The barrier makes `GetPose()` the last completed
  tick for everybody.
