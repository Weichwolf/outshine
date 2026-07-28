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

- **mission-wide** — `name`, `runway`, `timeout`, `wx`. Must stand BEFORE the first `unit` block.
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
| Actor  | `unit`    | callsign | block start. 1–24 characters from `[A-Za-z0-9_-]` (the callsign also names the telemetry file and the `unit=` log attribution), unique mission-wide. |
| Actor  | `module`  | rest of line | module name, resolved through `FBModuleRegistry` — determines both the `FBModule` and the JSBSim aircraft folder name (`sim/assets/aircraft/<module>`). Mandatory per block. |
| Actor  | `team`    | `friendly`\|`hostile`\|`neutral` | team (`FBUnitTeam`, `core/FBTeam.h`) — lands in the unit registry that sensors and weapons read. Optional, default `friendly`. |
| Actor  | `flight`  | name position | the FLIGHT this unit belongs to and the position it holds in it (`FBFlightId`, `core/FBFlight.h`) — identity beside the team, and read off the registry by the cooperative datalink for the same reason. Position 1 is the LEAD; 2..8 are wingmen (8 because a flight cannot be larger than the track list that carries it). Optional, at most once per block: a unit without it is in no flight, and every piece of formation behaviour is then a no-op. See [`../formation.md`](../formation.md). |
| Actor  | `spawn`   | `<lat lon \| threshold>` `<altM \| ground>` `hdgDeg` `speedKt` | mandatory, exactly once per block: this unit's declarative IC — position, altitude-OR-ground, heading, speed. `threshold` takes lat/lon from the mission-wide `runway` line (pure writing convenience, not a second position syntax). `ground` resolves the altitude from terrain plus landing-gear geometry; a numeric value is a LITERAL ASL altitude (an air start). Both cases run through the same single JSBSim IC application. |
| Actor  | `set`     | `key value...` | system state as mission data — the runner only parses the KV list and hands it, inside the spawn IC window, to `FBModule::ApplySetup(key, value)` of THIS unit; the MODULE interprets its own keys. An unknown key is a runtime FAIL (exit 1, `SET_REJECTED` event), not a parse error. The key sets live with their topics: [`sensors.md`](sensors.md), [`avionics.md`](avionics.md), [`weapons.md`](weapons.md), [`combat.md`](combat.md). |
| Actor  | `wp`      | lat lon altM speedKt | an `FBWaypoint` of type `Enroute`, in THIS unit's flight plan |
| Actor  | `land`    | — | an `FBWaypoint` of type `Land` AT the runway threshold (needs the mission-wide `runway` line) |
| Actor  | `objective` | `survive` \| `waypoints` \| `kill unit <callsign>` \| `kill team <team>` | COMBAT OBJECTIVE of this unit (`core/FBObjective.h`) — repeatable, see [`verdict.md`](verdict.md). `kill unit` must name a unit OF THIS MISSION (forward reference allowed, checked at end of file) and not itself; `objective waypoints` needs `wp`/`land` lines above it. A doubly declared objective is a parse error. |

`name`, `timeout` and at least one `unit` block are mandatory; per block `module` and `spawn` are
mandatory. `runway` and `wx` are optional (`runway` only needed for `spawn threshold`/`land`/the
off-runway check, `wx` defaults to `calm`).

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
(`HaveWeather`) + `Units` (list of `FBMissionUnit`). `FBMissionUnit` = `Id` (callsign) + `ModuleName`
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

## Gaps

| Gap | Detail |
|---|---|
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
