# Journal — the chronicle of the rounds

**What this file is:** one line per finished round, in the order they happened — commit, what it
built, what it measured. It is history, not a plan and not a contract.

- **What each area must do** → the `## Spec` section of its topic file ([`INDEX.md`](INDEX.md)).
- **What is built right now** → the `## State` section of that same file.
- **What is missing, and what was tried and rejected** → its `## Gaps` section.
- **What comes next, in order** → [`roadmap.md`](roadmap.md).

Every round adds a line here; nothing here is ever rewritten to look better. Rejected approaches are
kept — in the Gaps of the file they belong to, with their measurements.

State of the entries below: commit `793e1fe` + the model-root/delta round (2026-07-27).

## Maturity per area

| Area | State | Doc |
|---|---|---|
| FDM adapter | **finished** — instanceable, IC-sealed, damage and stores channels | [fdm.md](sim/fdm.md) |
| Core / avionics bus | **finished** — typed blocks with three-state validity, command bus with acknowledgement | [core.md](sim/core.md) |
| Mission orchestrator | **finished** — four steps, no mission knowledge in the code | [units-and-missions.md](sim/units-and-missions.md) |
| Multi-unit | **finished** — formation as mission data, thread per unit in the gym, deterministic | [units-and-missions.md](sim/units-and-missions.md) |
| Sensors | **built** — datalink, radar, RWR, countermeasures. Without terrain masking. | [sensors.md](sim/sensors.md) |
| Weapons | **built** — AIM-120, Mk-82, M61A1, ground targets, damage model | [weapons-and-damage.md](sim/weapons-and-damage.md) |
| Pilot AI | **in progress** — takeoff/route/landing, BFM, BVR intercept, air-to-ground all fly; refinement ongoing | [pilot-ai.md](sim/pilot-ai.md) |
| Renderer | **built** — stage split complete. Units and weapons still invisible. | [rendering.md](render/renderer.md) |
| HUD | **built** — generic default HUD + full F-16 symbology, coverage AA | [modules-f16.md](aircraft/f16.md) |
| Cockpit displays | **not started** — the values are on the bus, the presentation is missing | [clients/clients.md](clients/clients.md) |
| HOTAS | **not started** — deliberately last, it is only a mapping | [clients/clients.md](clients/clients.md) |

## Chronology

### Foundation (24–25 Jul)

| Commit | Section |
|---|---|
| `59f08c8` | module architecture runtime-polymorphic, nine system slots with NoOp defaults |
| `c9206eb`…`2099cb0` | renderer stage split in four slices — at the end zero inline shaders in `FBRenderer.cpp` |
| `4cb92e8` | HUD stopgap → generic default HUD in the displays slot |
| `2f3c277`, `8997eec`, `6f160af` | HUD font: coverage AA instead of alpha test, split into generic font system / MAX7456 hook, 16×16 glyphs from B612 Mono, the same AA technique for all strokes |
| `6802a6d`, `d31b1a9` | F-16 main HUD with the real combiner aperture, legibility for 720p |

### Pilot AI and the control loop (26 Jul)

| Commit | Section |
|---|---|
| `681c5f8` | pilot-AI framework: `FBPilot`, units, airframe controls |
| `65d334c` | mission runner + telemetry — **the control loop itself**, the prerequisite for everything that follows |
| `e49d335` | phase 1: takeoff flies |
| `e4d7c26` | telemetry/log architecture: declarative sources, central bus, `FBLog` |
| `705c90a` | lib/client split: core lib, `fb-gym`, elevation hook, baked Swiss DEM |
| `28e74e5` | `FBFlightMonitor` — incorruptible physics K.O., model-derived |
| `92fe8a4` | mission orchestrator down to four steps, declarative spawn, `FBMissionMonitor` |
| `8cd3a74` | phase 3: landing — `payerne-full` flies fully autonomously |
| `bf4ee62` | **hardening**: silent wrong values, aborts, client divergence — see "Defect classes found" |

### Multi-unit (26 Jul)

| Stage | Commit | What it built |
|---|---|---|
| 1 | `c1bc9de` | FDM instanceable — `FBFdm` as an object, no global instance |
| 2 | `c08a168` | the actor is ONE object (`units/FBSimUnit`) |
| 3 | `2c03704` | the formation is mission data — two jets fly |
| 4 | `6d7ed5a` | thread per unit in the gym, lockstep barrier, bit-identical |
| 5 | `9190e7c` | datalink — units see each other through a system |
| 6 | `4049a7b` | FCR radar with ACM modes, anonymous contacts, IFF |
| 7 | `b375bef` | BFM manoeuvre AI — flies on radar contacts alone |
| 8 | `071ea2b` | avionics data model: output blocks with validity + command bus |

### Knowledge base (26 Jul)

`2dd1142`, `e22f228`, `c4e96e7` — the official ED documentation distilled into `doc/f16/`.
`weapons.md` and `defence-rwr-cm.md` from SHALLOW to FULL; `controls-commands.md` new, as the template
for the command blocks.

### Weapons, damage, tactics (27 Jul)

| Commit | Section |
|---|---|
| `b62c769` | weapons foundation: the weapon is a unit of its own with its own FDM |
| `5c68fc5` | AIM-120 with seeker, guidance and datalink support |
| `439f53a` | RWR and countermeasures — who notices being seen |
| `1ecd433` | intercept AI: BVR tactics — guide, shoot, support, defend |
| `6d84647` | damage model: hits become system failures, failures become invalidity |
| `82df2e2` | combat objectives and evolutionary tournaments |
| `a1a8fbf` | M61A1 cannon: derived ballistics, EEGS funnel, kinetic damage |
| `1eeff72` | air-to-ground: ground targets without an FDM, CCIP/CCRP from one integration |

### Refinement of the AI (27 Jul, ongoing)

| Commit | Section |
|---|---|
| `cac7b62` | pilot memory: the datum instead of the last measurement point; gun tracking with a rate term; roll-rate controller |
| `9673e00` | guidance holds a track where a track is declared — cross-track error and waypoint capture |

### One model root and the delta rule (27 Jul)

**What it built.** All flown JSBSim models now live under `sim/assets/aircraft` — `f16` (incl.
`Systems/` and the two referenced engine XMLs, which moved into the model directory as `f16/engine/`:
JSBSim's own per-aircraft layout, which its loaders search first), `mk82`, and the `aim120` that was
already there. `FBModelRoots` has ONE root, `FBModule::FdmModelVendored()` and `FBStoreSpec::Vendored`
have been dropped without replacement, `FBFdm`'s engine/Systems probing (`stat` + parent truncation)
has become two unconditional paths, and the WASM build embeds one root instead of five individual
paths.

**Why.** Principle 1 has moved from "never patched" to the **delta rule**: the pinned submodule is the
base, the copy flies, and every deviation is a named, evidenced entry in `sim/assets/MODEL-DELTAS.md` —
a better mission result is explicitly not evidence. The gate is `make -C sim verify-models`
(`sim/tools/verify_models.py`): canonical unified diff per file, character by character against the
diff block of the entry. Deliberately no `patch`/`git apply` — an application with fuzz could swallow a
deviation.

**Measured.**

| Check | Result |
|---|---|
| Regression, 50 missions | **121/121 telemetry files byte-identical**, all 50 exit codes equal, `events.log` identical except for output path and wall clock |
| `verify-models` | green (4 upstream-covered paths, 0 deltas, 1 FlightBox-own model) |
| Negative test | one changed byte in `f16.xml` → rc=1 with the missing block; likewise a declared but absent delta, a non-matching diff, and an undeclared model directory |
| Harnesses | all seven rc=0; corner speed unchanged at 380 KCAS / 16.2214 °/s |
| Determinism | 5 missions × `--threads 1/2/4` × 2 repetitions = one signature each |
| WASM | builds; JSBSim loads `f16` from the embedded `/fb/aircraft` and trims (`trimConverged=1`) |
| Frame | `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs, terrain + HUD |

## Defect classes found

What the control loop brought to light that an inspection would not have found. The list is both a
warning and a test pattern.

| Class | Concrete case |
|---|---|
| **Silent wrong values** | `ApplySetup` returned 0.0 for unparsable text and reported success. An HTML error page from the `/elev` endpoint was cached as a sea-level elevation — a whole 216 s mission flew over sea level and reported SUCCESS. |
| **Missing divergence check** | 16 injected NaN cases all ran through with `tripped=0`. |
| **Unguarded calls** | unchecked JSBSim calls → `std::terminate`, exit 134. |
| **Missing header dependencies** | The Makefile had no `-MMD -MP`: stale objects, phantom measurements. Proven by a deliberate header change that altered a telemetry hash and was then reverted. |
| **Architecture leak** | `FBFlightMonitor` knew about runways. Physics K.O. and mission verdict were separated. |
| **Module specifics in generic code** | F-16 references in `FBFlightMonitor`; limits are now derived entirely from the model. |
| **Non-determinism through ordering** | Log line position depended on the scheduler. Solved via merge order instead of locks. |
| **Two copies of the same data** | `sim/web/missions/*.fbm` was a hand-kept copy in the old format — the WASM app stayed black. Now a build copy. |
| **Aliasing through tick rates** | The seeker looked at 20 Hz at poses published at 10 Hz: 446 m/s measured instead of 654 m/s. Solved via a dwell window instead of two single measurements. |
| **Zombie state** | A detonated missile kept radiating for 74 s after its detonation. `Retire()` now clears the signature. |
| **Wrong controlled variable** | A pure P controller against a ramp (the gun solution against a turning opponent) parks at ramp rate × time constant. A point controller against a track has a steady-state cross-track offset. Both are a matter of controller type, not tuning. |
| **Stale documentation in a data file** | Two mission headers still documented "ends in a timeout" after both runs had become kills. The header carries the reading rule and must be maintained with it. |

### Documentation: the spec-driven restructuring (27.07.)

**What it built.** `doc/flightbox/` moved from "one file per subsystem plus a central TODO" to a
spec-driven shape: every topic file now carries `## Spec` / `## State` / `## Gaps` / `## Knowledge`,
grouped into `sim/`, `aircraft/`, `render/`, `clients/`. New: `vision.md` (the direction),
`roadmap.md` (R1–R10, thin, pointing at the Spec each stage must satisfy), `aircraft/mig29.md` and
`render/units-visual.md` (both spec-only, nothing built), `aircraft/stores.md`,
`clients/clients.md`. `PROGRESS.md` became this file; `TODO.md` dissolved into the Gaps sections of
the files it belonged to, plus `roadmap.md`/`vision.md`. `render/rendering.md` split into
`renderer.md` + `hud.md` + `clouds.md` (whose Spec is the owner-approved rebuild, including the
cirrus layer) + `units-visual.md`.

**Rule change.** The maintenance obligation is now spec-first: change the Spec, build until State
meets it, then update State/Gaps and add a line here (`conventions.md`). There is no second list of
open work any more.

**Not done.** Existing bodies stay German for now (each file says so); the translation wave plus the
schema alignment of `doc/f16/` and `doc/mig29/` is roadmap R10. `world-and-terrain.md` stays at its
old path until the `/wx` round lands, then splits into `world/terrain.md` + `world/weather.md`.

### 2026-07-27 — /wx: worldwide weather on the tile server (`24ac1fc`)

New `/wx` endpoint on fb-tiles: NOAA GFS 0.25°, decoded by an own 330-line GRIB2 reader (wgrib2 is
not packaged in Debian trixie; ecCodes serves as the test oracle — max error 0.5 quantisation steps
over all 20 fields × 259,920 points), delivered as ONE packed 8.3 MB blob per run ("one run is one
atmosphere" — split blobs could straddle a cycle boundary). Byte-identical deterministic builds
across two compilers; the fixture in `tiles/testdata/` doubles as the gym dataset. Poisoned-cache
lesson applied: NOMADS failure writes nothing, ever.

### 2026-07-27 — weather in the simulation (`43b82b5`)

`core/FBWeatherProvider` (calm / constant-wind instrument / FBWX blob from file or memory),
`FBFdm::SetWindNedMs` → `FGWinds` (derivation in the header; only the owner writes, only on change),
`wx` mission declaration (mission always wins; defaults gym/native calm, **browser live**).
Measured: crosswind drift 3.3078° vs 3.2765° derived (0.95 %); uncorrected CCRP in 25 kt crosswind
shifts 12.8 m — far below wind×TOF (127 m) because a 227 kg bomb barely couples laterally in a 10 s
fall; GFS fixture wind recovered from the flown trajectory to 0.12 m/s. All 50 pre-existing missions
byte-identical. Found and open: guidance cannot close a steerpoint inside its drift-widened turning
circle at 18 m/s crosswind (permanent 59° orbit); the 10 m wind anchors at 10 m ASL, not AGL.

### 2026-07-27 — R10: English throughout, schema everywhere (this commit)

The four-part wave: (a) the seven big `sim/` bodies translated (~8,300 lines, zero content loss,
anchors fixed); (b) the rest of `doc/flightbox/` plus legacy markers on the twelve old cloud
studies; (c) `doc/f16/` on the Spec/State/Gaps/Knowledge schema — producing the first **coverage
map** FlightBox-vs-real-jet (near-full: command bus, HUD symbology, RWR/CMDS; nothing: startup,
displays, HOTAS, refueling; and the surfaced fact that the model flies an F100-PW-229 while the
doc describes the F110); (d) `doc/mig29/` on the schema plus the citation reconciliation — where
the task premise ("uniformly PDF pages") proved wrong: the files were internally mixed, so all 131
DCS-FM citations were scored individually against the extracted PDF text (88 converted, 43 already
printed, re-grep proof 125 printed / 0 PDF). Provenance tags (`[MESS]`/`[ABL]`/`[MODELL]`) stay
German deliberately — they appear identically in code and three doc trees; renaming is only sane as
a coordinated sweep. Remaining German: `world-and-terrain.md` (splits into `world/` in phase 3 of
the mirror refactor) and the pre-refactor `sim/src` paths inside the seven translated files (also
phase 3).
