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

### Single jet (example: `mods/f16/src/missions/payerne-takeoff-only.fbm`)

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

### Pair (example: `mods/f16/src/missions/payerne-pair.fbm`)

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
| Mission | `time`    | `<YYYY-MM-DDThh:mm:ssZ>` | the CLOCK of the mission: the UTC instant at `simT = 0`. Optional, at most once; absent = no clock at all (see below). Consumed by the ephemeris and by everything that depends on where the sun is; it changes no physics. |
| Mission | `zone`    | `<name> <lat> <lon> <radiusM> <altMinM> <altMaxM>` | a DECLARED CYLINDER of mission geometry — a belt, a floor, a corridor. Repeatable, names unique, `radius > 0` and `altMin < altMax` (both parse errors otherwise); altitudes are metres ASL, like `spawn` and `wp`. **Only the judge and the telemetry writer ever see one**: `core/FBZone.h` is `RESTRICTED` with an empty outside-includer list, so no module, no pilot and no sensor can name it. It produces `zone_<name>_in`/`zone_<name>_s` per JUDGED unit and `zone ENTER`/`EXIT` events, and it is what `objective avoid zone` is measured against. See [`../air-defence-network.md`](../air-defence-network.md) §4. |
| Mission | `net`     | `<name>` + an indented block | the DOCTRINE of one connected air defence. Block keywords, each at most once except `member`: `control <callsign>` (the node; must also be a `member`), `link wire \| radio <rangeM> [mast <m>]`, `period <s>`, `hold <cycles>`, `wcs free\|tight\|hold` (what the NODE transmits), `member <callsign> [sector <centreDeg> <halfDeg>] [autonomy free\|tight\|hold]`. A member naming no unit of this mission, a `control` that is not a member, a unit in two nets and an unknown block token are all parse errors. The block generates seven reserved `set` keys per member (`net_*`) — never author-written. **A member may be an AIRCRAFT since 2026-08-03** (`modules/FBAirNet.h`): on a fighter the net is the Link-16 terminal it already carries, `net_control` makes it a listener and `net_wcs` the node. Two keys are then REFUSED at spawn with a reason rather than ignored — `member ... sector` (a sector of responsibility belongs to a position in the ground) and `autonomy tight` (it needs target addressing this tree has none of). See [`../air-defence-network.md`](../air-defence-network.md) §8. |
| Actor  | `unit`    | callsign | block start. 1–24 characters from `[A-Za-z0-9_-]` (the callsign also names the telemetry file and the `unit=` log attribution), unique mission-wide. |
| Actor  | `module`  | rest of line | module name, resolved through `FBModuleRegistry` — determines both the `FBModule` and the JSBSim aircraft folder name (`mods/f16/src/aircraft/<module>`). Mandatory per block. |
| Actor  | `team`    | `friendly`\|`hostile`\|`neutral` | team (`FBUnitTeam`, `core/FBTeam.h`) — lands in the unit registry that sensors and weapons read. Optional, default `friendly`. |
| Actor  | `flight`  | name position | the FLIGHT this unit belongs to and the position it holds in it (`FBFlightId`, `core/FBFlight.h`) — identity beside the team, and read off the registry by the cooperative datalink for the same reason. Position 1 is the LEAD; 2..8 are wingmen (8 because a flight cannot be larger than the track list that carries it). Optional, at most once per block: a unit without it is in no flight, and every piece of formation behaviour is then a no-op. See [`../formation.md`](../formation.md). |
| Actor  | `spawn`   | `<lat lon \| threshold>` `<altM \| ground>` `hdgDeg` `speedKt` | mandatory, exactly once per block: this unit's declarative IC — position, altitude-OR-ground, heading, speed. `threshold` takes lat/lon from the mission-wide `runway` line (pure writing convenience, not a second position syntax). `ground` resolves the altitude from terrain plus landing-gear geometry; a numeric value is a LITERAL ASL altitude (an air start). Both cases run through the same single JSBSim IC application. |
| Actor  | `set`     | `key value...` | system state as mission data — the runner only parses the KV list and hands it, inside the spawn IC window, to `FBModule::ApplySetup(key, value)` of THIS unit; the MODULE interprets its own keys. An unknown key is a runtime FAIL (exit 1, `SET_REJECTED` event), not a parse error. The key sets live with their topics: [`sensors.md`](sensors.md), [`avionics.md`](avionics.md), [`weapons.md`](weapons.md), [`combat.md`](combat.md). |
| Actor  | `wp`      | lat lon altM speedKt | an `FBWaypoint` of type `Enroute`, in THIS unit's flight plan |
| Actor  | `land`    | — | an `FBWaypoint` of type `Land` AT the runway threshold (needs the mission-wide `runway` line) |
| Actor  | `objective` | `survive` \| `waypoints` \| `kill unit <callsign>` \| `kill team <team>` \| `identify unit <callsign> range <m> hold <s>` \| `protect unit\|team <x>` \| `no_fire` \| `deny release unit\|team <x>` \| `avoid zone <name> [exposure <s>]` \| `suppress unit\|team <x> [emitting <s>]`, each optionally followed by `until <s>` | COMBAT OBJECTIVE of this unit (`core/FBObjective.h`) — repeatable, see [`verdict.md`](verdict.md). Every unit-scoped target must name a unit OF THIS MISSION (forward reference allowed, checked at end of file) and not itself; `objective waypoints` needs `wp`/`land` lines above it. A doubly declared objective is a parse error. `avoid zone` must name a `zone` line of this mission and is fulfilled while the cumulative dwell stays at or below `exposure` (default 0); it is DEFERRED, like `survive`. The last four are round `C12`; their grammar is below. |

`name`, `timeout` and at least one `unit` block are mandatory; per block `module` and `spawn` are
mandatory. `runway`, `wx`, `time`, `zone` and `net` are optional (`runway` only needed for `spawn threshold`/`land`/the
off-runway check, `wx` defaults to `calm`, `time` defaults to *no clock*).

### The mission clock (`time`)

**Status: built** (round `C2`). The consumer side is [`../sensors.md`](../sensors.md) §9 (visual
acquisition, still `C3`) and the renderer's ephemeris; the per-client default lives with the clients
([`../clients/clients.md`](../clients/clients.md)).

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
Zurich and in a container. `[SET]` bounds, declared as `kEphemerisMinYear`/`kEphemerisMaxYear` next to
the formulae they bound (`core/FBEphemeris.h`) so the range and its reason sit in one place. The
conversion is `core/FBCivilTime.h` — `FBParseIsoUtc`/`FBFormatIsoUtc` over `FBDaysFromCivil`, the same
calendar in both directions, so a logged instant re-reads as the instant that was declared.

#### The default is *no clock*, and that is what makes the 84 missions byte-identical

| Design | Consequence |
|---|---|
| default = a fixed epoch constant | every existing frame proof moves, the browser's live sky is pinned to 1970, and `gpu_native --utc`'s "0 = wall clock" sentinel changes meaning |
| default = the client's current behaviour, expressed as a value | same problem one level down: the gym has no ephemeris at all today, so there is no value to express |
| **default = `HaveTime = false`, no channel touched** | **built.** Nothing is computed, nothing is published, nothing is logged — the same mechanism by which `wx calm` is byte-identical to the tree before the weather hook existed ("the wind is only written on change", [`weather.md`](weather.md)) |

**How that was checked, concretely.** `mods/f16/src/missions/` held **84** `.fbm` files before the round. The
acceptance was the regression gate of [`../build-and-ops.md`](../build-and-ops.md) at full strength,
run against the PRE-ROUND `fb-gym` binary kept aside for the purpose:

| Check | Requirement | Measured |
|---|---|---|
| Telemetry | all `telemetry*.csv` of all 84 missions **byte-identical** to the pre-round tree. C2 adds **no column** — a per-run constant does not belong in a 10 Hz time series | **259/259 files byte-identical** (84 missions, the multi-unit ones contributing one CSV per actor), at `--threads` 1, 2 and 4 |
| Events | all `events.log` byte-identical modulo `wallS`/`speedup`. The `mission CLOCK` line is emitted **only when a clock is declared**, so a mission without the line produces no line | **84/84 identical** modulo `wallS`, `speedup` and the absolute `--out` path |
| Determinism | unchanged single fingerprint over `--threads 1/2/4` | identical exit-code set and identical telemetry across all three, the new clock mission included |
| Frame proofs | `gpu_native` without `--mission` is untouched | `--utc 922312800` over Payerne, SVS and EVS: **PNG byte-identical** to the pre-round binary |

The one thing a clock DOES change is measurable in the same place: `clock-night-payerne.fbm` and
`payerne-airstart.fbm` fly the identical spawn and route, and their 2 167 telemetry rows differ in
**exactly one column, `blk_env`** (1 versus 0). The clock is a stamp, not an input.

#### Who consumes it

| Consumer | What it takes | State |
|---|---|---|
| Ephemeris → `FBEnvironmentBlock` (sun/moon el/az, moon phase) | the instant + the unit's position | **built.** `core/FBEphemeris.h`, fed from the mission clock in all three clients; the `blk_env` telemetry column is 1 exactly when a clock was declared |
| Cloud drift advection (`FBRenderer::SetSkyClock`) | the instant | **built.** Follows the declared clock where there is one, the client's own where there is not |
| **Visual acquisition** (`C3`, [`../sensors.md`](../sensors.md) §9) | sun elevation for the day factor, the sun's direction for the glare term | **specified, not built** — but the channel it will read now exists in `fb-gym` |
| Terrain/cloud lighting, night lighting, an infrared background term | the instant | future; named so the clock is not re-invented for each |

**The wiring, and the one structural consequence.** The clock is a mission-wide datum that the OWNER
samples and pushes down — the identical shape ground elevation and the cloud sky already have
(`FBSimUnit::UpdateSky` → `FBModule::SetCloudSky`). So: `FBSimUnit::UpdateSolar` → `FBModule::SetSolar`
with a small value block, computed per unit per decision tick from `(lat, lon, T0 + simT)`. **No system
below the owner ever holds a clock**, exactly as no sensor queries the world.

That forced one move, **done in this round**: the ephemeris was `render/FBEphemeris.h`, and `core/`,
`sensors/` and `systems/` may not include `render/` (`verify-layers`). The pure functions now live in
`core/FBEphemeris.h` (namespace `FlightBox`, `FBSunPos`/`FBMoonPos`, `double` Unix seconds instead of
`time_t`), and the clients include them from below. That it was a relocation and not a rewrite is
MEASURED, not claimed: the screenshot venue's PNGs at a pinned `--utc 922312800` are byte-identical
before and after the move, in SVS and in EVS.

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

**Status: built.** The semantics, the check order and the conservation argument are
in [`verdict.md`](verdict.md); here only what the parser accepts.

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

### The declared span (`E17`) — grammar only

```
objective <any kind> [<its own tokens>] until <seconds>
```

One optional trailing modifier on EVERY kind, stripped from the tail before the kind word is read, so no
kind's own optional token (`exposure`, `emitting`, `hold`) has to know about it. `<seconds>` must be
greater than zero and **strictly less than the mission `timeout`** — a span that cannot close would be
the end-of-run rule wearing a new word, and the parser says so with both numbers. Two objectives that
differ only in their span are two objectives, not a duplicate. Absent, the field is `+infinity` and the
objective is judged exactly as every objective written before it. Semantics: [`verdict.md`](verdict.md),
"The seventh thing in the vocabulary".
| `unit`/`team` discriminator mandatory, as for `kill` | for the same stated reason: a callsign may legally be spelled `hostile` |
| Named units are resolved against the WHOLE cast at end of file | same rule as `kill unit`; an objective naming nobody is a parse error, never a silently unmeetable objective |
| A unit may not `identify`, `protect` or `deny release` **itself** | `protect` on oneself is `survive` and has a spelling already; `identify`/`deny` on oneself are meaningless |
| `range` > 0, `hold` ≥ 0, both mandatory on `identify` | there is no default box — it is `[SET]` per mission and must be visible in the file (W5 §Knowledge 2) |
| Exact duplicates are a parse error | unchanged rule; two `identify` lines on the same unit with different numbers are NOT duplicates and are legal (a coarse pass and a close pass). Measured: a file with both `range 2000 hold 30` and `range 500 hold 5` runs, latches the coarse one and TIMEOUTs on the close one |
| `no_fire` takes no argument | it is about this unit only. Note that a trailing token is IGNORED rather than refused, exactly as it is on `survive` — the parser's laxness here is format-wide and older than this round ([`verdict.md`](verdict.md) Gaps) |

The refusals have three fixtures in `missions/negative/`: `objective-self-protect.fbm` (a unit naming
itself), `objective-identify-unknown.fbm` (a target nobody in the cast answers to) and
`objective-identify-nobox.fbm` (`range 0`). Each states its required exit code and message in its own
header; the table is in that directory's `README.md`.

### Parse errors versus runtime FAIL

Parse errors (`FBParseMissionFile` returns `false` plus a message in `*err`) are: unknown keyword, an
actor line without an open block, a mission-wide line after the first block, a duplicate or
non-file-safe callsign, two `spawn` lines in one block, `spawn threshold`/`land` without `runway`, a
`set` without a value, a `team` outside the three values, a missing mandatory field — and, for
`flight`: a second `flight` line in one block, a position outside 1..8, a name that is not
`[A-Za-z0-9_-]` or is 16 characters or longer (it travels in a fixed PPLI field), two units at the same
(name, position), and — checked at end of file, like a `kill unit` forward reference — **a declared
flight with no unit at position 1**. Every piece of formation behaviour is defined against the lead,
so a flight without one is not a flight. For `objective`: an unknown kind word, a missing or non-`unit`/
`team` scope word, an `identify` without `range`/`hold` or with `range <= 0` / `hold < 0`, a `deny`
without `release`, a unit-scoped target naming ITSELF, an exact duplicate — and, at end of file, a
unit-scoped target naming nobody in the cast.

A `module` that `FBModuleRegistry` does not know, or an unknown `set` key, is a runtime FAIL of the
runner (not of the parser).

**Consistency validation at set-up:** a physically contradictory declaration is a FAIL as soon as the
runner has resolved the elevation (not already in the parser, which knows no geodata) — today: an
explicit `spawn` altitude below the resolved ground. `v=0` in the air is by contrast NOT an error
(legal, the unit then simply falls — `FBFlightMonitor` judges that like any other flight state).

### Data model

`FBMission` = `Name` + optional `FBRunway` (`HaveRunway`) + `TimeoutS` + `FBWeatherSpec`
(`HaveWeather`) + `UtcT0S` + `HaveTime` + `Units` (list of `FBMissionUnit`). `FBMissionUnit` = `Id` (callsign) + `ModuleName`
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
comes from the injected `FBElevationProvider` (`--elev tiles|const|baked`) — the file elevation of the
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
| `time` line | **built** (`C2`). Parser + `core/FBCivilTime.h`; precedence and the flag collision in `missions/FBClockBoot.h`; pushed per actor per decision tick (`FBSimUnit::UpdateSolar` → `FBModule::SetSolar` → `FBEnvironmentBlock`) by the runner and by the WASM frame loop. Reference mission `missions/clock-night-payerne.fbm`, the two refusal fixtures in `missions/negative/` |
| The declared span, `until <s>` (`E17`) | **built.** Stripped from the tail before the kind word is read, so every kind takes it and none of them knows about it; `> 0` and `< timeout` are parse errors otherwise, with both numbers in the message. Two spans of the same kind are two objectives. Negative fixture: `missions/negative/objective-until-past-timeout.fbm` |
| The four new objective kinds (`C12`) | **built.** Parser + `FBObjectiveScope` in `core/FBObjective.h`; unit-scoped targets resolved against the whole cast at end of file, three refusal fixtures in `missions/negative/`; eight reference missions in `mods/f16/src/missions/` ([`verdict.md`](verdict.md)) |

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
