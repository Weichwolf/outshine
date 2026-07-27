---
name: f16-systems
description: F-16C systems knowledge for FlightBox — complete rebuild spec distilled from the DCS Viper Guide plus the official DCS F-16C Early Access Guide (Eagle Dynamics) plus researched engineering depth (FLCS control laws, TP-1538 aero provenance, F110 engine, HUD symbology, procedures, weapons/SMS delivery-mode logic, RWR/CMDS, radar mode taxonomy, datalink) — AND a full description of the actual JSBSim flight model (aero tables, FLCS XML, engine, measured envelope, mk82/aim120). Load when working on flight-control laws, HUD fidelity, procedures, weapon-system rebuild (SMS/CCIP/CCRP/HARM/Maverick logic), FCR mode logic, RWR/CMDS logic, judging F-16 behavior or a measured flight-model number, or building a knowledge base for another airframe.
---

# F-16C Systems Reference

The knowledge base lives in `<repo>/doc/f16/` — 17 files (15 subsystem files + 1 cross-cutting command
list + `flight-model.md`, whose source is the JSBSim model tree rather than a guide), each with a
Chuck's-Guide distillate (facts/tables/steps, page-cited) plus a `Technical depth
(researched — for rebuild)` section (engineering sources, confidence-flagged). **Most files
additionally carry an ED EA Guide section** (official Eagle Dynamics module manual, `doc/DCS F-16C
Early Access Guide EN.pdf`) — cite tags are always `Chuck p.NN` vs `ED EA Guide p.NN` so you can tell
which source backs which claim. Where the two disagree, both values are kept and the conflict is
flagged explicitly (see `flight-controls-flcs.md`'s autopilot bank-angle-limit discrepancy for the
pattern). Start at `INDEX.md`; coverage map + exact unprocessed-page ledger in `PROGRESS.md`.

Read the files relevant to the task at hand:

| Task | Read |
|---|---|
| **What the aircraft ACTUALLY does (Prinzip-5 ground truth) / judging a measured number / porting to a new airframe** | `flight-model.md` — the JSBSim model tree itself (`sim/assets/aircraft/f16` + `mk82` + `aim120`), not a PDF: geometry/mass/gear (incl. the fact that JSBSim declares **no** breaking load — the monitor derives its own), F100 thrust tables + spool law + the **throttle 0.5 = MIL, 1.0 = max AB** mapping, all 41 aero functions / 35 tables per axis with breakpoint grids (**α only −10…+45°, so no deep stall in this model**), the FLCS as XML with every gain converted to "full stick = X", **§7.11 the model-vs-real-FLCS deviation table**, the measured envelope (corner 380 KCAS / 5.625 g / 16.22 °/s; roll rate saturates at ~186 °/s = the 180 °/s command scale), **§9 twelve accepted model properties that must not be "fixed"**, **§11 the transferable checklist for the next airframe's knowledge base** |
| FBW / control laws / autopilot | `flight-controls-flcs.md` (FULL: signal flow, g/AoA blend, limiters, gains, actuators, sensors, FLCC; **+ ED autopilot addendum** with quantified command-authority numbers and a flagged Chuck/ED bank-limit discrepancy) |
| FDM validation / envelope | `aerodynamics-performance.md` (FULL: TP-1538 provenance, limits, deep stall) |
| HUD implementation / judging | `hud-symbology.md` (FULL: every element + exact scale numbers from ED + MIL-STD-1787 conventions + a **"Was der Pilot wirklich sieht"** instrumentation-ground-truth checklist), `cockpit-displays.md` (FULL on ICP/UFC/DED — every DED page's fields + the propose/commit/reject edit protocol — and MFD — OSB layout, Format Selection Master Menu, DTE upload partitions; + ED analog-gauge detail: AoA indicator bands, ADI, VVI, full Caution Light Panel trigger table; Aux/Left/Right Console subsections remain the residual gap) |
| **Avionics command-block model (next FlightBox build phase)** | `controls-commands.md` — the pilot command list re-cut across ICP/DED, MFD, HOTAS, and autopilot mode switches as trigger→precondition→effect→feedback→failure rows; the DED's propose/commit/reject cycle as the reference command pattern; a collected rejection/precondition taxonomy for the reject-reason design; a derived HOTAS-vs-DED command-latency-class split (0.5 s/1.0 s switch timing vs. multi-second DED edits) as a sourced upper bound on pilot-KI command rate. **The pilot-KI must drive avionics through this same command vocabulary a human uses — no state shortcuts.** |
| Engine modelling | `engine-fuel.md` (FULL: F110 ratings, spool dynamics, DEEC) |
| **Approach / ILS / nav modes / autonomous-approach guidance** | `navigation-ils.md` — **FULL against ED**: INS alignment mechanism, nav-solution blending (300 ft GPS/INS threshold), System Altitude/ACAL/DTS-TRN, cursor-slew vs. position-fix vs. altitude-cal, TACAN facts, and a full ILS chapter incl. **glideslope-intercept-altitude and descent-rate tables directly usable as `FBAutopilot::Direct` guidance inputs**; ⚠️ flags 2 discrepancies (steerpoint count 99 vs 127, ADI glideslope scale 0.7° vs 2.5°/dot) |
| **Takeoff/landing speeds & procedures / pilot-AI approach law** | `procedures-takeoff-taxi.md`, `procedures-landing.md` (now gives an **11–13° AoA range**, not flat 11°, for base-turn-through-rollout — a candidate refinement for `FBF16Pilot`; ⚠️ flags a break-G discrepancy 3–4G vs ~3G), `procedures-startup.md` (+ full INS-alignment-type/timing/CEP-scale mechanism) |
| **Weapon-system rebuild (SMS/release-computation logic, next FlightBox phase)** | `weapons.md` — **FULL**: SPI/cursor mechanism, station/carriage data, gun EEGS funnel/dispersion logic, AIM-9/AIM-120 DLZ + guidance-phase logic, CCIP/CCRP/DTOS/LGB/JDAM/JSOW/WCMD/HARM(HAS/POS)/Maverick(MBC/ripple/force-correlate) release-computation logic, munition specs, confidence-tiered researched+derived ballistics/fuzing numbers (§4, includes a fall-time derivation formula and explicit "measurable in fb-gym once weapon-FDM exists" notes on remaining gaps) |
| **FCR mode logic (parallel dev in progress — this file is additive-only, don't reword)** | `radar-sensors.md` — CRM/ACM mode taxonomy + exact scan geometry (ACM sub-mode angular volumes, GM/GMT/SEA parameters, MTR thresholds), TWS/SAM/STT/DTT transition state machine, NCTR gates, TGP zoom/track-mode facts. HTS/HMCS chapter detail still a flagged gap. |
| **RWR/CMDS logic** | `defence-rwr-cm.md` — **FULL**: RWR blind-spot geometry, CMDS mode×CMS×ECM-XMIT interaction state machine, per-program chaff/flare parameter schema, ECM jamming/burnthrough principle |
| **Datalink (TNDL)** | `datalink-iff.md` — TNDL section **FULL** (TDMA/STN/3-channel/System-Track-File mechanism); IFF procedure section still at Chuck depth (flagged gap) |
| Aerial refueling | `air-refueling.md` — FULL, + ED pre/post-refueling emitter checklists, director-light color coding, formal breakaway procedure |

**"What the pilot actually sees"** — `hud-symbology.md`'s dedicated section is the prep-work checklist
for auditing any pilot module: it tabulates exactly which quantities the HUD/DED/EHSI/instrument panel
display and at what resolution/format (e.g. AoA is a bracket cue on the HUD, NOT a numeric value — the
numeric source is the separate analog AoA indicator). `FBF16Pilot` currently uses omniscient internal
FDM state rather than gating through this, which the file documents as a deliberate, now-explicit
simplification — read this section before adding new pilot-visible-quantity assumptions.

Ground rules when applying this knowledge: the operative artifact is the vanilla JSBSim F-16 model
(reference chain NASA TP-1538 → JSBSim → FlightBox, CLAUDE.md Prinzip 5) — **and that artifact is now
written down in `flight-model.md`, so "the model says X" is a checkable claim, not an appeal**;
researched real-jet values are DESIGN TARGETS, not defect criteria. Confidence flags in the files are honest —
do not present medium-confidence numbers as certainties. Flagged Chuck/ED discrepancies (bank-angle
limit, break-G, steerpoint count, ADI glideslope scale) are kept as **both values, explicitly flagged**
— never silently resolve one away; when a rebuild needs one number, ED (official module manual) is
generally the more belastbar source per this project's source hierarchy, but the vanilla JSBSim model
remains the actual ground truth per CLAUDE.md Prinzip 5 above either guide. **Combat systems are no
longer categorically "out of current scope"** — weapons.md/defence-rwr-cm.md/radar-sensors.md/
datalink-iff.md were SHALLOW placeholders precisely because flight+rendering was the only scope; the
weapon-JSBSim-instance phase (own FDM per fired weapon, mass/CG carry effect, external forces, impact
physics) is coming next per CLAUDE.md's stated build order (FCR → BFM AI → weapons), and `weapons.md`
is the reference for it.

## Reference implementation

**FlightGear F-16 by NikolaiVChr — https://github.com/NikolaiVChr/f16** — the one open-source
F-16 built on the SAME stack as FlightBox: a JSBSim FDM with a fully modelled FLCS (control laws,
limiters, gain schedules as JSBSim `<system>` XML), plus HUD symbology, avionics, and systems logic
(Nasal/XML). Consult it when designing FLCS/autopilot behavior, HUD element dynamics, or system
interactions — it shows how each concept maps onto JSBSim in practice. License is **GPL-2.0**: read
it to understand approaches and cross-check numbers; do NOT copy code/XML into FlightBox's
differently-licensed tree.
