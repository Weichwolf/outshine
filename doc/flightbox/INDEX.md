# FlightBox — knowledge base

What FlightBox is, what it must become, and what is actually built. This collection is the
**authority**; `CLAUDE.md` in the repository root is a session-start card and points here. If the two
disagree, this wins and `CLAUDE.md` is to be corrected.

Loaded task-wise through the skill **`flightbox`** (`.claude/skills/flightbox/SKILL.md`).

State: `33667ac`, restructured to the spec-driven form on 2026-07-27.

## How to read a file

Every topic file carries the same four sections:

| Section | What it is | Use it when |
|---|---|---|
| `## Spec` | the contract — what it must do, acceptance criteria, measurement anchors. Changes by decision, not by building. | you are about to change behaviour: change this first |
| `## State` | what is built, with commit and measurement. Honest, including "nothing". | you need to know what exists today |
| `## Gaps` | Spec − State, ordered by value, **including rejected approaches with their measurements** | you are looking for work, or about to retry something |
| `## Knowledge` | derivations, formulas, measured constants | you need the number and where it came from |

The working rule that binds a round to this shape is in [`conventions.md`](conventions.md).

Two exceptions, both deliberate: the **meta files** (this index, `vision.md`, `roadmap.md`,
`journal.md`, `conventions.md`) carry no Spec/State/Gaps — they *are* the direction, the order, the
history and the rules. And `world-and-terrain.md` is being rewritten in the weather round, so it keeps
its old shape and its old path until that lands.

**Language:** new material is English. Files whose body is still the German distillation carry a
`> Body still in German — translation pass pending` note under their title; the translation is
roadmap R10.

## Start here

| File | Content |
|---|---|
| [vision.md](vision.md) | what FlightBox is *for*: tactical air combat on real physics, the staggered scale, the two mission classes, and why anti-cheat is a game decision |
| [roadmap.md](roadmap.md) | the stages R1–R10, thin — each one points at the file whose Spec it must satisfy |
| [conventions.md](conventions.md) | naming, structure, the no-printf rule, **every number carries its provenance**, and the spec-first working rule |
| [architecture.md](architecture.md) | process model, core-lib plus three clients, directory map, the layering pattern, multi-unit in brief |
| [journal.md](journal.md) | the chronicle: one entry per finished round, plus the defect classes the control loop uncovered |

## The simulator — `sim/`

| File | Content |
|---|---|
| [sim/core.md](sim/core.md) | the avionics block bus with three-state validity, the command bus with acknowledgement and rejection catalogue, `FBLog`/`FBTelemetry`, the **two judges**, mission-data types, objectives, health register and damage model, ballistics, elevation hook, geodesy |
| [sim/fdm.md](sim/fdm.md) | the JSBSim adapter: the one-TU seam, instance capability, **IC lockdown**, ownership, carriage and damage channels through model-owned APIs, the full load sequence |
| [sim/units-and-missions.md](sim/units-and-missions.md) | `FBUnit`/`FBSimUnit`/`FBUnitRegistry`, the snapshot barrier, the four-step orchestrator, spawn, the multi-unit stages incl. thread pool and the honest scaling numbers, detonation and impact resolution |
| [sim/systems.md](sim/systems.md) | the generic slots: guidance (incl. the **full path-following derivation**), FBW inner loop, air data, radar altimeter as the reference case for `Invalid`, warnings, navigation, display slot, airframe controls |
| [sim/sensors.md](sim/sensors.md) | datalink, radar, RWR, countermeasures — and **the perception boundary**: who may see the registry, why a contact is anonymous, why IFF is two-valued |
| [sim/weapons-and-damage.md](sim/weapons-and-damage.md) | weapon-as-unit, SMS and gun, shared ballistics, the **three resolution boundaries**, the damage model from geometry to system consequence, and the coupling "failure → block invalid" |
| [sim/pilot-ai.md](sim/pilot-ai.md) | phase machine, attack, BFM with its own control law, the **datum** as the pilot's memory, BVR intercept, debriefing channels, variants and tournament, the mission control loop |

## Aircraft — `aircraft/`

| File | Content |
|---|---|
| [aircraft/f16.md](aircraft/f16.md) | the F-16 module: composition, cadence, command router, every override with its numbers and their provenance, HUD symbology implementation |
| [aircraft/mig29.md](aircraft/mig29.md) | the first opponent — **spec only, nothing built**. BVR scale, model per `doc/mig29/flight-model-spec.md`, module per the registry pattern, GCI-led doctrine. The first spec-first file. |
| [aircraft/stores.md](aircraft/stores.md) | the model side of mk82/aim120, the **MODEL-DELTAS discipline** and its gate, and the Mk-82 fidelity caveat |

## Rendering — `render/`

| File | Content |
|---|---|
| [render/renderer.md](render/renderer.md) | WebGPU, ECEF camera-relative, reversed-Z, the **pass topology as a contract**, the stage catalogue, atmosphere, terrain stage, camera and ground truth |
| [render/hud.md](render/hud.md) | the HUD backend: geometry buffer, WebGPU stage, the generic font system with coverage AA |
| [render/clouds.md](render/clouds.md) | the **cloud rebuild spec** (bounded-volumetric but simple, incl. the cirrus layer) against the existing six-stage chain it replaces |
| [render/units-visual.md](render/units-visual.md) | units, weapons and effects in the picture — spec written, **nothing built**: `FBUnitsStage`/`FBSpritesStage` are NoOp |

## Clients — `clients/`

| File | Content |
|---|---|
| [clients/clients.md](clients/clients.md) | gym (the reference path), native (the frame oracle), wasm (the browser) — contract, state and gaps per client |

## World

| File | Content |
|---|---|
| [world-and-terrain.md](world-and-terrain.md) | `FBWorld`, tile streaming and worker, elevation over tiles, the terrain library, `fb-tiles` from the client's side — **plus the `/wx` weather interface being written in the R2 round**. It will be split into `world/terrain.md` + `world/weather.md` once the weather round has landed; it stays at this path until then. |

## Operations

| File | Content |
|---|---|
| [build-and-ops.md](build-and-ops.md) | make targets, the **gates**, measurement discipline, the mission control loop, host specifics |

## Related collections

| Place | Subject | Relationship |
|---|---|---|
| `doc/f16/` (skill `f16-systems`) | the **real** F-16C from the manuals | design targets, not defect criteria. Where a FlightBox number comes from a manual, the file here cites that one — it does not restate it. |
| `doc/mig29/` | the **real** MiG-29 from the two DCS manuals | same relationship, being written (roadmap R3) |
| [`doc/mission-format.md`](../mission-format.md) | the `.fbm` format | reference; `sim/units-and-missions.md` points at it instead of duplicating it |
| `doc/clouds/` | cloud rendering studies | prior work, feeds the R5 rebuild |
