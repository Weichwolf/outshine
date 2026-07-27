---
name: flightbox
description: FlightBox architecture and implementation knowledge — the JSBSim-backed F-16 simulator itself (core library, FDM adapter, avionics bus, mission runner, multi-unit, sensors, weapons/damage, pilot AI, renderer, tile streaming), plus its principles, conventions, build gates, project progress and open TODO. Load when working ON FlightBox's own code or architecture: adding or changing a system, a module, a client, the mission format, the pilot AI, the renderer; judging whether a change fits the architecture; or answering what is built, what is not, and what comes next. For the REAL F-16's documented behaviour (manuals, symbology definitions, procedures) load `f16-systems` instead — the two are complements.
---

# FlightBox Reference

The knowledge base lives in `<repo>/doc/flightbox/`. It is the authority on **what FlightBox is and
how it is built**. `CLAUDE.md` is deliberately only a session-start card and points here; if the two
ever disagree, `doc/flightbox/` is right and `CLAUDE.md` needs fixing.

Start at [`doc/flightbox/INDEX.md`](../../../doc/flightbox/INDEX.md).

## Read this first, always

| File | Why |
|---|---|
| `CLAUDE.md` (repo root) | The principles and the **anti-cheat guarantees**, condensed. A change that violates one of these is wrong even if it works. |
| `conventions.md` | Naming, structure, the no-printf rule, and the rule that **every number carries its provenance** (derived / measured / `[SET]`). |

## Then read what the task touches

| Task | Read |
|---|---|
| Anything at all — orientation, where does a file belong | `architecture.md` (process model, core-lib + three clients, directory map) |
| JSBSim seam, FDM instances, initial conditions, stores carriage, damage channels | `fdm.md` |
| Avionics bus, command bus, telemetry, logging, the two judges, mission-data types, damage model, ballistics, elevation | `core.md` |
| Mission runner, `.fbm`, actors, multi-unit, threading, spawn, determinism | `units-and-missions.md` + [`doc/mission-format.md`](../../../doc/mission-format.md) |
| Guidance, FBW, air data, nav, radar altimeter, warnings, display slot, airframe controls | `systems.md` |
| Datalink, radar, RWR, countermeasures — **and the perception boundary** | `sensors.md` |
| Pilot AI: phases, BFM, BVR intercept, attack, track estimation, tuning, tournaments | `pilot-ai.md` |
| Weapons, stores, gun, ballistics, damage resolution, system health, weapon modules | `weapons-and-damage.md` |
| The F-16 module and its overrides, HUD symbology implementation | `modules-f16.md` |
| Renderer, stages, HUD backend, camera, ECEF/reversed-Z | `rendering.md` |
| World, terrain, tile streaming, fb-tiles | `world-and-terrain.md` |
| Build targets, gates, the mission control loop, host specifics | `build-and-ops.md` |
| What is built / what is not / what comes next | `PROGRESS.md`, `TODO.md` |

## The two knowledge bases

| | `doc/f16/` (skill `f16-systems`) | `doc/flightbox/` (this skill) |
|---|---|---|
| Subject | the **real** F-16C, from manuals | **FlightBox's** implementation |
| Authority | Chuck's Guide, ED EA Guide — cited by page | the source tree itself |
| Status of its numbers | **design targets** | what the code actually does |
| Ground truth for flight behaviour | no — the vanilla JSBSim model is | no — the vanilla JSBSim model is |

Use `f16-systems` to learn what the jet does. Use this skill to learn how FlightBox does it and whether
a change belongs. When a FlightBox number is justified by a manual, the FlightBox doc says so and cites
the `doc/f16/` file — it does not restate the manual.

## Maintenance obligation

**This documentation is kept current, not written once.** It is part of the deliverable, exactly like
the code.

When a change lands, in the same round:

1. Update the subsystem file(s) whose described behaviour changed — including the derivations, because
   a number without its derivation is a defect here.
2. Add the round to `PROGRESS.md` (commit hash, what it built, what it measured).
3. Move whatever it closed out of `TODO.md`, and add whatever it opened — including honest negatives
   and rejected approaches, with their measurements. A measured failure is knowledge; deleting it means
   someone re-runs the experiment.
4. Only if a session-start fact changed (a principle, a build target, a host detail), touch `CLAUDE.md`
   — and keep it under 100 lines.

An `## Offene Punkte` section at the end of every subsystem file is where known gaps, contradictions
between comment and code, and unresolved questions live. Do not quietly resolve one away; either fix it
and say so, or leave it stated.

## Ground rules when applying this knowledge

- The vanilla JSBSim F-16 model is the ground truth for flight behaviour, above any guide and above any
  text in these files (`CLAUDE.md`, Prinzip 5).
- Measurements beat inspection. The mission control loop (`build-and-ops.md`) is how a claim about
  behaviour gets settled — a defect that was not measured is a hypothesis.
- The anti-cheat structure is load-bearing. If a task seems to require the pilot to read the unit
  registry, write JSBSim state, or repair its own systems, the task is wrong, not the structure.
