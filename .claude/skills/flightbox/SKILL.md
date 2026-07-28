---
name: flightbox
description: The whole FlightBox knowledge base — the JSBSim-backed F-16 simulator itself (core library, FDM adapter, avionics bus, mission format and runner, multi-unit, sensors, weapons/damage, pilot AI, renderer, HUD, clouds, tile streaming, weather, clients) plus its vision, roadmap, conventions, build gates and the honest gap list per subsystem, AND the two aircraft reference bases: the real F-16C (DCS Viper Guide + ED Early Access Guide + researched FLCS/engine/HUD/weapons/RWR/radar/datalink depth + the pinned JSBSim flight model) and the real MiG-29A 9-12 (both DCS manuals: mechanical controls/ARU, RD-33, N019 with its quantified Doppler notch, KOLS IRST, SPO-15, GCI doctrine, R-27R/R-73, flight-model build spec). Load when working ON FlightBox's code or architecture, on the mission format, on the pilot AI or the renderer; when judging whether a change fits; when you need what the real F-16 or MiG-29 documentably does; or when answering what must be true, what is built, what is missing and what comes next.
---

# FlightBox Reference

The knowledge base lives in `<repo>/doc/`. It is the authority on **what FlightBox must be and how it
is built**, and it carries the two aircraft reference bases inside it. `CLAUDE.md` is deliberately only
a session-start card and points here; if the two ever disagree, `doc/` is right and `CLAUDE.md` needs
fixing.

Start at [`doc/INDEX.md`](../../../doc/INDEX.md).

## The map: `doc/` mirrors `sim/src/`

The layout is not a taxonomy of its own — it is the source tree. If you know where the code is, you
know where its file is.

| `sim/src/` | `doc/` |
|---|---|
| `core/` | `core.md` |
| `fdm/` | `fdm.md` |
| `systems/` | `systems.md` |
| `sensors/` | `sensors.md` |
| `weapons/` | `weapons.md` |
| `pilot/` | `pilot.md` |
| `missions/` + `units/` | `missions/` (10 files, entry `missions/INDEX.md`) |
| `modules/` | `modules/f16/`, `modules/mig29/`, `modules/stores.md` |
| `render/` | `render/` (+ `render/clouds-legacy/`, a closed archive) |
| `world/` | `world/terrain.md`, `world/weather.md` |
| `clients/` | `clients/clients.md` |
| *(cross-cutting, no source dir)* | `duels.md` (a PAIRING), `formation.md` (a FLIGHT), `air-defence-network.md` (a NET) — three subjects that cut through core/units/sensors/pilot/missions at once and are kept whole |
| *(no source dir)* | the meta files at the root: `INDEX.md`, `vision.md`, `roadmap.md`, `journal.md`, `conventions.md`, `architecture.md`, `build-and-ops.md` |

## The shape of every topic file

The collection is **spec-driven**. Each topic file carries the same four sections, and which one you
read depends on the question:

| Section | What it holds | Read it when |
|---|---|---|
| `## Spec` | the contract: what it must do, acceptance criteria, measurement anchors. Changes only by decision. | you are about to change behaviour — **change this first** |
| `## State` | what is built, with commit and measurement. Honest, including "nothing". | you need to know what exists today |
| `## Gaps` | Spec − State by value, **including rejected approaches with their measurements** | you are picking work, or about to retry something that already failed |
| `## Knowledge` | derivations, formulas, measured constants | you need a number and where it came from |

Meta files (`INDEX.md`, `vision.md`, `roadmap.md`, `journal.md`, `conventions.md`, the reference bases'
`INDEX.md`/`PROGRESS.md`, and `render/clouds-legacy/`) carry no schema — they are the direction, the
order, the history, the rules and a closed archive. Everything is English.

## Read this first, always

| File | Why |
|---|---|
| `CLAUDE.md` (repo root) | the principles and the **anti-cheat guarantees**, condensed. A change that violates one of these is wrong even if it works. |
| `vision.md` | what the thing is *for*, and the **staggered scale** — it decides how deep a subsystem is allowed to be |
| `conventions.md` | naming, structure, the no-printf rule, **every number carries its provenance**, and the spec-first working rule |

## Then read what the task touches

| Task | Read |
|---|---|
| Orientation — where does a file belong | `architecture.md` (process model, core-lib + three clients, directory map) |
| What comes next, in which order, and why | `roadmap.md`; history in `journal.md` |
| JSBSim seam, FDM instances, initial conditions, stores carriage, damage channels | `fdm.md` |
| Avionics bus, command bus, telemetry, logging, the two judges, mission-data types, damage model, ballistics, elevation | `core.md` |
| Guidance, FBW, air data, nav, radar altimeter, warnings, display slot, airframe controls | `systems.md` |
| Datalink, radar, RWR, countermeasures — **and the perception boundary** | `sensors.md` |
| Weapons, stores, gun, ballistics, hit resolution, system health, weapon modules | `weapons.md` |
| Pilot AI: phases, BFM, BVR intercept, attack, track estimation, tuning, tournaments | `pilot.md` |
| **Writing or changing a `.fbm` mission** | `missions/INDEX.md` (leading rules + exit codes), then the topic file: `syntax.md`, `verdict.md`, `sensors.md`, `avionics.md`, `weapons.md`, `combat.md`, `weather.md`, `output.md` |
| Mission runner, actors, multi-unit, threading, spawn, determinism | `missions/runtime.md` |
| **A scenario with an anchor** — the ten campaign specs, the aggregated cast, the shared gap catalogue `C0…C24`, the identification anti-cheat test, Bekaa as a yardstick | `campaigns/INDEX.md`, then the campaign file. **Spec only — nothing built** |
| **Ground threat — one emitting, shooting position** (catalogue rows, envelopes, the engagement machine, damage) | `modules/ground/module.md` + `modules/ground/catalogue.md` (`C1`, spec only) |
| **Ground threat — the NET above the positions**: cueing an antenna without inventing a track, layered belts declared as zones and judged, weapons control / sector responsibility / autonomy when the node dies, the bounded comms-jamming model, and what the attacking pilot may see | `air-defence-network.md` (`C22`/`C23`/`C24`, spec only) |
| **The campaign layer above a mission** — carry, aggregation, replay determinism | `missions/campaign.md` (`C0`, spec only) |
| The mission clock / time of day / sun position | `missions/syntax.md` §"The mission clock" (`C2`, spec only) + `clients/clients.md` §"Clock defaults per client" |
| Visual acquisition, seeing an aircraft with the eye, visual identification | `sensors.md` §9 (`C3`, spec only) + `missions/sensors.md` |
| The F-16 module and its overrides, HUD symbology implementation | `modules/f16/module.md` |
| An opponent aircraft, or the MiG-29 module | `modules/mig29/module.md` (spec only — nothing built) |
| Store models, MODEL-DELTAS discipline, model fidelity caveats | `modules/stores.md` |
| Renderer, stages, camera, ECEF/reversed-Z, pass topology | `render/renderer.md` |
| HUD backend, geometry buffer, font, coverage AA | `render/hud.md` |
| Clouds — the rebuild spec and what it replaces | `render/clouds.md`; the prior studies and their rejected approaches in `render/clouds-legacy/` |
| Drawing units, weapons, effects | `render/units-visual.md` (spec only — nothing built) |
| World, terrain, tile streaming, `fb-tiles`, elevation providers | `world/terrain.md` |
| The `/wx` weather data kind, FBWX format, GFS run, GRIB2 decoder | `world/weather.md` |
| gym / native / wasm: what each client must be, and what wasm still cannot do | `clients/clients.md` |
| Build targets, gates, the mission control loop, host specifics | `build-and-ops.md` |

## The module reference bases

Two collections inside `doc/modules/` document the **real aircraft**, not FlightBox. They are **design
targets, not defect criteria**: the pinned JSBSim model is the ground truth for flight behaviour
(CLAUDE.md principle 5). Where a FlightBox number is justified by a manual, the implementation file
cites the reference file — it does not restate it.

Their four headings mean something different from the implementation side: `## Spec` = what the real
jet documentably does (the bulk of every file), `## State` = what FlightBox implements of it in a few
honest lines with links into the implementation files, `## Gaps` = **source gaps** (unprocessed pages,
shallow research passes, unresolved discrepancies) *and* **implementation gaps**, `## Knowledge` = the
researched engineering depth with its sources.

### F-16C — `doc/modules/f16/` (19 files; `INDEX.md`, coverage ledger in `PROGRESS.md`)

Distilled from the DCS Viper Guide **plus** the official ED Early Access Guide, cite tags always
`Chuck p.NN` vs `ED EA Guide p.NN`. Where the two disagree, both values are kept and the conflict is
flagged — never silently resolved.

| Task | Read |
|---|---|
| **What the aircraft ACTUALLY does (principle-5 ground truth) / judging a measured number / porting to a new airframe** | `flight-model.md` — the JSBSim model tree itself, not a PDF: geometry/mass/gear (incl. the fact that JSBSim declares **no** breaking load — the monitor derives its own), F100 thrust tables + spool law + the **throttle 0.5 = MIL, 1.0 = max AB** mapping, all aero functions/tables with breakpoint grids (**α only −10…+45°, so no deep stall in this model**), the FLCS as XML with every gain converted to "full stick = X", §7.11 the model-vs-real-FLCS deviation table, the measured envelope (corner 380 KCAS / 5.625 g / 16.22 °/s; roll saturates ~186 °/s), §9 twelve accepted model properties that must not be "fixed", §11 the transferable checklist for the next airframe. **Schema inverted here:** §1–§10 are its State, §12 its Gaps, §11 its Knowledge. |
| FBW / control laws / autopilot | `flight-controls-flcs.md` — signal flow, g/AoA blend, limiters, gains, actuators, sensors, FLCC; + ED autopilot addendum with a flagged Chuck/ED bank-limit discrepancy |
| FDM validation / envelope | `aerodynamics-performance.md` — TP-1538 provenance, limits, deep stall |
| HUD implementation / judging | `hud-symbology.md` — every element + exact scale numbers + MIL-STD-1787 conventions + the **"what the pilot actually sees"** instrumentation checklist; `cockpit-displays.md` — ICP/UFC/DED (every DED page + the propose/commit/reject edit protocol) and MFD |
| Avionics command model | `controls-commands.md` — the pilot command list as trigger→precondition→effect→feedback→failure rows, the DED propose/commit/reject cycle as the reference command pattern, the rejection/precondition taxonomy, the HOTAS-vs-DED latency-class split. **The pilot AI must drive avionics through this same vocabulary a human uses — no state shortcuts.** |
| Engine modelling | `engine-fuel.md` — F110 ratings, spool dynamics, DEEC |
| Approach / ILS / nav modes | `navigation-ils.md` — INS alignment, nav-solution blending, TACAN, a full ILS chapter incl. glideslope-intercept and descent-rate tables usable as guidance inputs; ⚠️ 2 flagged discrepancies |
| Takeoff/landing speeds and procedures | `procedures-takeoff-taxi.md`, `procedures-landing.md` (an **11–13° AoA range**, not flat 11°; ⚠️ break-G discrepancy), `procedures-startup.md` |
| Weapon-system logic | `weapons.md` — SPI/cursor mechanism, station/carriage data, gun EEGS funnel/dispersion, AIM-9/AIM-120 DLZ + guidance phases, CCIP/CCRP/DTOS/LGB/JDAM/HARM/Maverick release computation, munition specs, confidence-tiered ballistics/fuzing numbers |
| FCR mode logic | `radar-sensors.md` — CRM/ACM taxonomy + exact scan geometry, TWS/SAM/STT/DTT transitions, NCTR gates, TGP facts |
| RWR / CMDS logic | `defence-rwr-cm.md` — RWR blind-spot geometry, CMDS mode×CMS×ECM state machine, per-program chaff/flare parameter schema |
| Datalink (TNDL) / IFF | `datalink-iff.md` — TNDL full (TDMA/STN/3-channel/System Track File); the IFF procedure section is still a flagged gap |
| HOTAS mapping | `hotas.md` |
| Aerial refuelling | `air-refueling.md` |

**Reference implementation** (read, never copy): the FlightGear F-16 by NikolaiVChr,
https://github.com/NikolaiVChr/f16 — the one open-source F-16 on the same stack (JSBSim FDM with a
fully modelled FLCS as `<system>` XML, plus HUD/avionics logic). **GPL-2.0**: consult it to understand
approaches and cross-check numbers; do not copy code or XML into FlightBox's differently-licensed tree.

### MiG-29A 9-12 — `doc/modules/mig29/` (12 files; `INDEX.md`, ledger in `PROGRESS.md`)

Built to the F-16 base's template; `modules/f16/flight-model.md` §11 is the checklist
`flight-model-spec.md` follows. **Coverage is honestly PARTIAL** — both DCS manuals are fully
distilled, but they are far thinner than the F-16 corpus. Page citations are printed pages everywhere
(DCS-FM printed = PDF − 6; DCS-EA no offset). **Nothing is implemented**: every State section says so.

| Task | Read |
|---|---|
| JSBSim model build (the main event) | `flight-model-spec.md` — three-column rows (documented+tier / derivation path / open+`[SET]`), RD-33 `<turbine_engine>` spec, declared F/A-18 HARV high-α analogy, envelope-anchor table (= the future gym acceptance test), 10-step build order with promotion gates. Deliberately thin frame; the rows are themselves a Spec+Gaps hybrid. |
| Flight controls / pitch response | `flight-controls.md` — mechanical runs, ARU-29-2 gearing schedule, SOS-3M stick pusher at 26° AoA. **No FBW — `fcs/fbw-override` has no counterpart; the gearing schedule IS the model.** |
| Engines / fuel | `engines-fuel.md` — two RD-33, tanks and feed order; spool times and EGT limits are the worst gap for a throttle loop |
| Radar / IRST / helmet sight | `radar-sensors.md` — N019 modes and the **quantified Doppler notch** (closure > 81 kts beyond 8 nm, > 27 kts inside; 6 s inertial coast), KOLS IRST 13.5–5.4 nm with laser range, Shchel-3UM ±60°. The IRST has **no IFF**. |
| RWR / countermeasures | `defence-rwr-cm.md` — SPO-15 as a physics simulation with eleven analogue failure modes |
| Navigation / procedures | `navigation.md`, `procedures.md` — waypoints advance **manually** (a pilot action with latency, belongs on the command bus), brake chute |
| GCI doctrine (the AI's guidance model) | `datalink-gci.md` — voice BRAA → pilot enters expected range/relative altitude → radar computes scan elevation. The counter-design to Link-16. |
| Weapons | `weapons.md` — R-27R **SARH support obligation** (~26 s illumination vs the AIM-120's 5–15 s, crank ceiling 67°, Support/Defend mutually exclusive), R-73 + helmet cueing, R-60M, GSh-301 built row-for-row against the M61A1 table, DLZ mapped onto `Raero/Rtr/Rmin` |
| Cockpit / HUD | `cockpit-displays.md` — HUD FOV is a **24° circle** (not the F-16's rectangle); symbol geometry is unsourced, do not invent it |

Six cross-manual conflicts are registered (SPO-15 threat letters, waypoint sequencing, tachometer
100 %, radar-alt units, N019 gimbals, 7 g vs 9 g) — both values kept, same rule as the F-16 base. The
one acquisition that would upgrade ~a dozen gaps: **GAF T.O. 1F-MIG29-1**; both files are written so
its arrival is an edit, not a rewrite.

## The working rule (binding)

A round that intends to change behaviour:

1. **Changes the Spec of its topic file first.** If it cannot state what the contract becomes, it is
   not ready to start. A Spec change is a decision.
2. **Builds until State meets Spec** — measured against that Spec's own anchors. Measurements beat
   inspection; the mission control loop (`build-and-ops.md`) is how a claim about behaviour is settled.
3. **Updates State and Gaps, and adds one line to `journal.md`** (commit, what it built, what it
   measured).
4. **Leaves rejected approaches in Gaps, with their measurements.** A measured failure is knowledge;
   deleting it means someone re-runs the experiment.

There is no second list of open work — no `TODO.md`, no per-file "open points" tail. Gaps is the one
place. `CLAUDE.md` is touched only when a session-start fact changed, and stays under 100 lines.

## Ground rules when applying this knowledge

- The vanilla JSBSim model is the ground truth for flight behaviour, above any guide and above any text
  in these files (`CLAUDE.md`, principle 5). Researched real-jet values are **design targets, not defect
  criteria** — and the flown model is exactly named: pinned state plus the evidenced delta list in
  `sim/assets/MODEL-DELTAS.md`.
- **Read `Spec` for the target and `State` for what exists — never assume a documented system is built.**
  Coverage is uneven on purpose: `controls-commands.md`, `hud-symbology.md` and `flight-model.md` are
  near-fully reflected in the code, while `air-refueling.md`, `navigation-ils.md` and
  `cockpit-displays.md` have essentially no implementation, and the whole MiG-29 base has none.
- The scale is staggered on purpose (`vision.md`): the F-16 exact, sensors and envelopes believable,
  enemy aircraft their envelope and no more. Do not "fix" something the spec says is out of scale.
- The anti-cheat structure is load-bearing — for the game, not only for the engineering. If a task seems
  to require the pilot to read the unit registry, write JSBSim state, or repair its own systems, the
  task is wrong, not the structure.
- Confidence flags and flagged cross-manual discrepancies are honest — do not present a
  medium-confidence number as a certainty and never silently resolve one value away.
- Do not invent a number. Derived, measured, or `[SET]` and named as such — otherwise it does not go in.
