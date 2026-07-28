# FlightBox — knowledge base

What FlightBox is, what it must become, and what is actually built. This collection is the
**authority**; `CLAUDE.md` in the repository root is a session-start card and points here. If the two
disagree, this wins and `CLAUDE.md` is to be corrected.

Loaded task-wise through the skill **`flightbox`** (`.claude/skills/flightbox/SKILL.md`) — the one
skill for the whole collection, including the two module reference bases.

State: restructured to the spec-driven form on 2026-07-27; **`doc/` is a 1:1 mirror of `sim/src/`**
since the Phase-3 mirror rebuild on 2026-07-28.

## The mirror

The directory layout is not a taxonomy of its own — it is `sim/src/`, one file or one directory per
source directory. If you know where the code is, you know where its file is.

| `sim/src/` | `doc/` |
|---|---|
| `core/` | [core.md](core.md) |
| `fdm/` | [fdm.md](fdm.md) |
| `systems/` | [systems.md](systems.md) |
| `sensors/` | [sensors.md](sensors.md) |
| `weapons/` | [weapons.md](weapons.md) |
| `pilot/` | [pilot.md](pilot.md) |
| *(missions, not a source dir)* | [duels.md](duels.md) — the asymmetric duel campaign |
| *(cross-cutting, not a source dir)* | [formation.md](formation.md) — the flight as a fighting unit |
| *(cross-cutting, not a source dir)* | [air-defence-network.md](air-defence-network.md) — the net above the single ground position |
| *(cross-cutting, not a source dir)* | [air-to-ground.md](air-to-ground.md) — the air-to-ground half: beating that defence down |
| `missions/` + `units/` | [missions/](missions/INDEX.md) |
| `modules/` | [modules/](modules/f16/INDEX.md) |
| `render/` | [render/](render/renderer.md) |
| `world/` | [world/](world/terrain.md) |
| `clients/` | [clients/](clients/clients.md) |
| *(no source dir)* | the meta files at the root: this index, `vision.md`, `roadmap.md`, `journal.md`, `conventions.md`, `architecture.md`, `build-and-ops.md` |

## How to read a file

Every topic file carries the same four sections:

| Section | What it is | Use it when |
|---|---|---|
| `## Spec` | the contract — what it must do, acceptance criteria, measurement anchors. Changes by decision, not by building. | you are about to change behaviour: change this first |
| `## State` | what is built, with commit and measurement. Honest, including "nothing". | you need to know what exists today |
| `## Gaps` | Spec − State, ordered by value, **including rejected approaches with their measurements** | you are looking for work, or about to retry something |
| `## Knowledge` | derivations, formulas, measured constants | you need the number and where it came from |

The working rule that binds a round to this shape is in [`conventions.md`](conventions.md).

Four files are deliberately outside the mirror because their subject is: [`duels.md`](duels.md) (a
PAIRING), [`formation.md`](formation.md) (a FLIGHT — it cuts through core, units, sensors, pilot and
missions at once, and belongs whole in one place rather than in fifths),
[`air-defence-network.md`](air-defence-network.md) (a NET — the same cut, on the ground side, and the
sequel to `modules/ground/` rather than a part of it) and
[`air-to-ground.md`](air-to-ground.md) (an ENGAGEMENT of the one by the other — a weapon family, a
sensor function, a verdict kind and a pilot cue, which live in five layers and belong in one argument).

Exceptions, all deliberate: the **meta files** (this index, `vision.md`, `roadmap.md`, `journal.md`,
`conventions.md`) carry no Spec/State/Gaps — they *are* the direction, the order, the history and the
rules. The same holds for the `INDEX.md`/`PROGRESS.md` of the module reference bases, and for
`render/clouds-legacy/`, which is a closed archive of prior studies.

**Language:** English throughout. The last German bodies (`mission-format.md`, `world-and-terrain.md`)
were translated in the Phase-3 split; no German prose remains in `doc/`.

## Start here

| File | Content |
|---|---|
| [vision.md](vision.md) | what FlightBox is *for*: tactical air combat on real physics, the staggered scale, the two mission classes, and why anti-cheat is a game decision |
| [roadmap.md](roadmap.md) | the stages R1–R10, thin — each one points at the file whose Spec it must satisfy |
| [conventions.md](conventions.md) | naming, structure, the no-printf rule, **every number carries its provenance**, and the spec-first working rule |
| [architecture.md](architecture.md) | process model, core-lib plus three clients, directory map, the layering pattern, multi-unit in brief |
| [journal.md](journal.md) | the chronicle: one entry per finished round, plus the defect classes the control loop uncovered |
| [build-and-ops.md](build-and-ops.md) | make targets, the **gates**, measurement discipline, the mission control loop, host specifics |

## The simulator

| File | Content |
|---|---|
| [core.md](core.md) | the avionics block bus with three-state validity, the command bus with acknowledgement and rejection catalogue, `FBLog`/`FBTelemetry`, the **two judges**, mission-data types, objectives, health register and damage model, ballistics, elevation hook, geodesy |
| [fdm.md](fdm.md) | the JSBSim adapter: the one-TU seam, instance capability, **IC lockdown**, ownership, carriage and damage channels through model-owned APIs, the full load sequence |
| [systems.md](systems.md) | the generic slots: guidance (incl. the **full path-following derivation**), FBW inner loop, air data, radar altimeter as the reference case for `Invalid`, warnings, navigation, display slot, airframe controls |
| [sensors.md](sensors.md) | datalink, radar, RWR, countermeasures — and **the perception boundary**: who may see the registry, why a contact is anonymous, why IFF is two-valued |
| [weapons.md](weapons.md) | weapon-as-unit, SMS and gun, shared ballistics, the **three resolution boundaries**, the damage model from geometry to system consequence, and the coupling "failure → block invalid" |
| [pilot.md](pilot.md) | phase machine, attack, BFM with its own control law, the **datum** as the pilot's memory, BVR intercept, debriefing channels, variants and tournament, the mission control loop |
| [duels.md](duels.md) | the **asymmetric measurement campaign** — `missions/duel-*.fbm`, the geometry × outcome table, the four asymmetries with their numbers, the EMCON timeline, the mixed tournament |
| [formation.md](formation.md) | the **flight**: roles as mission data, the wingman's station on a moving point, target sorting from the shared picture against the briefed contract, the cover rule and why it is free for one weapon and unavailable for the other |
| [air-defence-network.md](air-defence-network.md) | the **connected air defence** (`C22`/`C23`/`C24`, spec only): the cue that aims a fire unit's antenna and can never create a track, the belt declared as zones and read out of the verdict, weapons control and sector responsibility, what a defence becomes when its node is killed or jammed, and the bounded comms-jamming model. **The net adds no seventh registry reader** |
| [air-to-ground.md](air-to-ground.md) | the **air-to-ground half** — `C8` (built, minus the rocket pod), `C26` and `C27` closed, `C25` still open: the anti-radiation weapon whose seeker IS the warning receiver and whose memory is a RATE and never a point, so a crew that goes dark still escapes — MEASURED at 85.0 % / 88.1 % of the flight against a predicted 61 % / 76 %, with the deviation's three causes named; five more stores each with the one quantity that decides it; **suppressed against destroyed** as a mechanism plus one objective kind; and the pre-existing defect that made a bunker and a falling bomb radiate a fighter radar |
| [campaigns/](campaigns/INDEX.md) | the **ten scenario specifications** — five flown by the F-16, five by the MiG-29, ten missions each, every anchor cited and tiered; plus the aggregated cast list, the capability gaps ordered by blocking degree, the identification task as an anti-cheat test and Bekaa as a measurable yardstick. **Spec only, nothing built** |

## Missions — `missions/`

The `.fbm` format and the runtime that consumes it. Entry point: [missions/INDEX.md](missions/INDEX.md)
(the leading rules and the exit codes).

| File | Content |
|---|---|
| [missions/syntax.md](missions/syntax.md) | line syntax, the two scopes, the keyword table, parse errors versus runtime FAIL, the data model, tick order and snapshot rule |
| [missions/verdict.md](missions/verdict.md) | the two judges, the combination rule, waypoint reach, combat objectives, the expected-loss rule, landing standstill |
| [missions/sensors.md](missions/sensors.md) | the `set` keys and rules for datalink, FCR/IFF, RWR and countermeasures |
| [missions/avionics.md](missions/avionics.md) | three-state block validity, the command bus outcomes, and the `brief_*` lines |
| [missions/weapons.md](missions/weapons.md) | load-out, release, gun, store life cycle, guided round, ground targets, air-to-ground and its measured error budget |
| [missions/combat.md](missions/combat.md) | `set task bfm`/`intercept`, the engagement state machine, the `bfm_*`/`eng_*` columns, the `pilot_*` variants and the tournament |
| [missions/weather.md](missions/weather.md) | the `wx` line, the three providers, the precedence rule, the measured crosswind and release cases |
| [missions/output.md](missions/output.md) | the files per run, damage events, unit attribution, `UNIT_RESULT`, the example-mission catalogue |
| [missions/runtime.md](missions/runtime.md) | `FBUnit`/`FBSimUnit`/`FBUnitRegistry`, the snapshot barrier, the four-step orchestrator, multi-unit incl. thread pool and scaling numbers, detonation and impact resolution |
| [missions/campaign.md](missions/campaign.md) | the **campaign layer** (`C0`, built): `.fbc`, the three carried facts, the overlay that only deletes, the campaign fingerprint and the standalone replay of every step |

## Modules — `modules/`

One directory per airframe: the **module** file (what FlightBox implements) beside its **reference
base** (what the real aircraft documentably does).

| Place | Content |
|---|---|
| [modules/f16/module.md](modules/f16/module.md) | the F-16 module: composition, cadence, command router, every override with its numbers and their provenance, HUD symbology implementation |
| [modules/f16/INDEX.md](modules/f16/INDEX.md) | the **real F-16C** reference base — 19 files distilled from the DCS Viper Guide and the ED EA Guide plus researched engineering depth, incl. `flight-model.md` (the pinned JSBSim model itself) |
| [modules/mig29/module.md](modules/mig29/module.md) | the first opponent — **spec only, nothing built**. BVR scale, model per `flight-model-spec.md`, module per the registry pattern, GCI-led doctrine. |
| [modules/mig29/INDEX.md](modules/mig29/INDEX.md) | the **real MiG-29A (9-12)** reference base — 12 files from the two DCS manuals plus research |
| [modules/ground/](modules/ground/INDEX.md) | **`C1` — the ground unit that emits and shoots. Specified, nothing built.** The line between a module and a unit; one data-driven class with nine sourced catalogue rows (`p18` `sa2` `sa3` `sa6` `sa8` `zsu23` `zu23` `sa7` `sa18`); how it sees without widening the registry gate; and the rest of the campaign cast at four quantities per type |
| [modules/air/](modules/air/INDEX.md) | **`C7` — the catalogue aircraft, the third level below the module. Specified, nothing built.** One data-driven class with eighteen sourced rows (`e3` `e2c` `f15c` `mig21` `mig23` `mig25` `mig17` `su7` `su22` `su27` `mirf1` `f5e` `kc135` `tu95` `an26` `ef111` `mi8` `ah64`); the two-part test that decides whether a row flies on a **generated** JSBSim deck or moves kinematically; the recipe that builds those decks from eight published anchors with a closed-form drag inversion; **five pilot tiers instead of one pilot**; the early-warning row and the one interface price it costs; and the attribution test that separates *"he lost as a MiG-21"* from *"he lost as a coarse deck"* |
| [modules/stores.md](modules/stores.md) | the model side of mk82/aim120, the **MODEL-DELTAS discipline** and its gate, and the Mk-82 fidelity caveat |

## Rendering — `render/`

| File | Content |
|---|---|
| [render/renderer.md](render/renderer.md) | WebGPU, ECEF camera-relative, reversed-Z, the **pass topology as a contract**, the stage catalogue, atmosphere, terrain stage, camera and ground truth |
| [render/hud.md](render/hud.md) | the HUD backend: geometry buffer, WebGPU stage, the generic font system with coverage AA |
| [render/clouds.md](render/clouds.md) | the **cloud rebuild spec** (bounded-volumetric but simple, incl. the cirrus layer) against the chain it replaces |
| [render/units-visual.md](render/units-visual.md) | units, weapons and effects in the picture — spec written, **nothing built**: `FBUnitsStage`/`FBSpritesStage` are NoOp |
| [render/clouds-legacy/](render/clouds-legacy/INDEX.md) | the twelve cloud studies that preceded the rebuild — a closed archive, kept for its measurements and its rejected approaches |

## World — `world/`

| File | Content |
|---|---|
| [world/terrain.md](world/terrain.md) | `FBWorld`, tile streaming and worker, elevation over tiles, the terrain library, `fb-tiles` from the client's side |
| [world/weather.md](world/weather.md) | the `/wx` data kind: the FBWX format, the GFS run determination, the GRIB2 decoder and its independent verification, the operating figures |

## Clients — `clients/`

| File | Content |
|---|---|
| [clients/clients.md](clients/clients.md) | gym (the reference path), native (the frame oracle), wasm (the browser) — contract, state and gaps per client |

## Not part of the mirror

| Place | Subject |
|---|---|
| `doc/*.pdf` | the four DCS manuals the two reference bases are distilled from — gitignored, cited by page |
| `doc/webgl-webgpu-report.txt` | the target GPU capability survey |
