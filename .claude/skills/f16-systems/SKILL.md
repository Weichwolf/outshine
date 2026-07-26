---
name: f16-systems
description: F-16C systems knowledge for FlightBox — complete rebuild spec distilled from the DCS Viper Guide plus researched engineering depth (FLCS control laws, TP-1538 aero provenance, F110 engine, HUD symbology, procedures). Load when working on flight-control laws, HUD fidelity, procedures, or judging F-16 behavior.
---

# F-16C Systems Reference

The knowledge base lives in `<repo>/doc/f16/` — 15 subsystem files, each with a
guide distillate (facts/tables/steps, page-cited) plus a `Technical depth (researched — for rebuild)`
section (engineering sources, confidence-flagged). Start at `INDEX.md`; coverage map in `PROGRESS.md`.

Read the files relevant to the task at hand:

| Task | Read |
|---|---|
| FBW / control laws / autopilot | `flight-controls-flcs.md` (FULL: signal flow, g/AoA blend, limiters, gains, actuators, sensors, FLCC) |
| FDM validation / envelope | `aerodynamics-performance.md` (FULL: TP-1538 provenance, limits, deep stall) |
| HUD implementation / judging | `hud-symbology.md` (FULL: every element + MIL-STD-1787 conventions), `cockpit-displays.md` |
| Engine modelling | `engine-fuel.md` (FULL: F110 ratings, spool dynamics, DEEC) |
| Approach / ILS / nav modes | `navigation-ils.md` (FULL: localizer/GS geometry, command steering, INS) |
| Takeoff/landing speeds & procedures | `procedures-takeoff-taxi.md`, `procedures-landing.md`, `procedures-startup.md` |
| Combat systems (out of current scope) | `radar-sensors.md`, `weapons.md`, `defence-rwr-cm.md`, `datalink-iff.md` (SHALLOW-marked) |

Ground rules when applying this knowledge: the operative artifact is the vanilla JSBSim F-16 model
(reference chain NASA TP-1538 → JSBSim → FlightBox, CLAUDE.md Prinzip 5); researched
real-jet values are DESIGN TARGETS, not defect criteria. Confidence flags in the files are honest —
do not present medium-confidence numbers as certainties.

## Reference implementation

**FlightGear F-16 by NikolaiVChr — https://github.com/NikolaiVChr/f16** — the one open-source
F-16 built on the SAME stack as FlightBox: a JSBSim FDM with a fully modelled FLCS (control laws,
limiters, gain schedules as JSBSim `<system>` XML), plus HUD symbology, avionics, and systems logic
(Nasal/XML). Consult it when designing FLCS/autopilot behavior, HUD element dynamics, or system
interactions — it shows how each concept maps onto JSBSim in practice. License is **GPL-2.0**: read
it to understand approaches and cross-check numbers; do NOT copy code/XML into FlightBox's
differently-licensed tree.
