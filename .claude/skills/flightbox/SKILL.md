---
name: flightbox
description: FlightBox architecture and implementation knowledge — the JSBSim-backed F-16 simulator itself (core library, FDM adapter, avionics bus, mission runner, multi-unit, sensors, weapons/damage, pilot AI, renderer, HUD, clouds, tile streaming, clients), plus its vision, roadmap, conventions, build gates and the honest gap list per subsystem. Load when working ON FlightBox's own code or architecture: adding or changing a system, a module, a client, the mission format, the pilot AI, the renderer; judging whether a change fits the architecture; or answering what must be true, what is built, what is missing and what comes next. For the REAL F-16's documented behaviour (manuals, symbology definitions, procedures) load `f16-systems` instead — the two are complements.
---

# FlightBox Reference

The knowledge base lives in `<repo>/doc/flightbox/`. It is the authority on **what FlightBox must be
and how it is built**. `CLAUDE.md` is deliberately only a session-start card and points here; if the
two ever disagree, `doc/flightbox/` is right and `CLAUDE.md` needs fixing.

Start at [`doc/flightbox/INDEX.md`](../../../doc/flightbox/INDEX.md).

## The shape of every topic file

The collection is **spec-driven**. Each topic file carries the same four sections, and which one you
read depends on the question:

| Section | What it holds | Read it when |
|---|---|---|
| `## Spec` | the contract: what it must do, acceptance criteria, measurement anchors. Changes only by decision. | you are about to change behaviour — **change this first** |
| `## State` | what is built, with commit and measurement. Honest, including "nothing". | you need to know what exists today |
| `## Gaps` | Spec − State by value, **including rejected approaches with their measurements** | you are picking work, or about to retry something that already failed |
| `## Knowledge` | derivations, formulas, measured constants | you need a number and where it came from |

Bodies still carrying the distilled German text say so under their title (translation is roadmap R10);
all new material is English.

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
| JSBSim seam, FDM instances, initial conditions, stores carriage, damage channels | `sim/fdm.md` |
| Avionics bus, command bus, telemetry, logging, the two judges, mission-data types, damage model, ballistics, elevation | `sim/core.md` |
| Mission runner, `.fbm`, actors, multi-unit, threading, spawn, determinism | `sim/units-and-missions.md` + [`doc/mission-format.md`](../../../doc/mission-format.md) |
| Guidance, FBW, air data, nav, radar altimeter, warnings, display slot, airframe controls | `sim/systems.md` |
| Datalink, radar, RWR, countermeasures — **and the perception boundary** | `sim/sensors.md` |
| Pilot AI: phases, BFM, BVR intercept, attack, track estimation, tuning, tournaments | `sim/pilot-ai.md` |
| Weapons, stores, gun, ballistics, hit resolution, system health, weapon modules | `sim/weapons-and-damage.md` |
| The F-16 module and its overrides, HUD symbology implementation | `aircraft/f16.md` |
| An opponent aircraft, or anything MiG-29 | `aircraft/mig29.md` (spec only — nothing built) |
| Store models, MODEL-DELTAS discipline, model fidelity caveats | `aircraft/stores.md` |
| Renderer, stages, camera, ECEF/reversed-Z, pass topology | `render/renderer.md` |
| HUD backend, geometry buffer, font, coverage AA | `render/hud.md` |
| Clouds — the rebuild spec and what it replaces | `render/clouds.md` |
| Drawing units, weapons, effects | `render/units-visual.md` (spec only — nothing built) |
| gym / native / wasm: what each client must be, and what wasm still cannot do | `clients/clients.md` |
| World, terrain, tile streaming, fb-tiles, weather interface | `world-and-terrain.md` |
| Build targets, gates, the mission control loop, host specifics | `build-and-ops.md` |

## The two knowledge bases

| | `doc/f16/` (skill `f16-systems`) | `doc/flightbox/` (this skill) |
|---|---|---|
| Subject | the **real** F-16C, from manuals | **FlightBox's** contract and implementation |
| Authority | Chuck's Guide, ED EA Guide — cited by page | the source tree itself, plus owner decisions |
| Status of its numbers | **design targets** | what the code actually does |
| Ground truth for flight behaviour | no — the vanilla JSBSim model is | no — the vanilla JSBSim model is |

Use `f16-systems` to learn what the jet does. Use this skill to learn what FlightBox must do, how it
does it, and whether a change belongs. When a FlightBox number is justified by a manual, the FlightBox
doc says so and cites the `doc/f16/` file — it does not restate the manual. `doc/mig29/` is the same
kind of source for the MiG-29.

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

- The vanilla JSBSim F-16 model is the ground truth for flight behaviour, above any guide and above any
  text in these files (`CLAUDE.md`, principle 5).
- The scale is staggered on purpose (`vision.md`): the F-16 exact, sensors and envelopes believable,
  enemy aircraft their envelope and no more. Do not "fix" something the spec says is out of scale.
- The anti-cheat structure is load-bearing — for the game, not only for the engineering. If a task
  seems to require the pilot to read the unit registry, write JSBSim state, or repair its own systems,
  the task is wrong, not the structure.
- Do not invent a number. Derived, measured, or `[SET]` and named as such — otherwise it does not go in.
