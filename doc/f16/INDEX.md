# F-16C Systems Reference — Index

Two source documents, kept distinguishable in every file (cite the tag, not just a page number):
- **Chuck** = `doc/DCS F-16C Viper Guide.pdf` (Charles "Chuck" Ouellet, 794 pp) — tutorial/screenshot-
  oriented third-party guide. The original basis for all 15 files below.
- **ED EA Guide** = `doc/DCS F-16C Early Access Guide EN.pdf` (Eagle Dynamics, official module manual,
  704 pp) — precise on **system behavior**: modes, logic, limits, symbology definitions. Added in a
  second pass (see PROGRESS.md) to `weapons.md`, `radar-sensors.md`, `defence-rwr-cm.md`,
  `datalink-iff.md`, and (one addendum) `flight-controls-flcs.md`. Where ED precises or conflicts with
  Chuck, both values are kept and the conflict is flagged explicitly — never silently resolved.

Neither guide is the primary source for FlightBox flight-model fidelity — that remains the vanilla
JSBSim F-16 itself (CLAUDE.md Prinzip 5). These files are a reference for system **behavior/logic**
(modes, limits, symbology, computation logic), which JSBSim's flight-dynamics tables don't carry.

**And that primary source now has its own file:** [`flight-model.md`](flight-model.md) is the only
file here whose source is not a PDF but the model tree `sim/assets/aircraft/` itself — it is the
written-down form of what Prinzip 5 calls "the reference". Every other file describes the *real* jet
(design targets); `flight-model.md` describes what our aircraft actually *does*. Its §7.11 is the
deviation table between the two, and its §11 is the transferable template for the next airframe.

## How to read a file

Every topic file below carries the **same four sections as [`doc/flightbox/`](../flightbox/INDEX.md)**
— with the meaning a *reference* base gives them (this base documents the real jet; `doc/flightbox/`
documents the implementation):

| Section | What it is here | Use it when |
|---|---|---|
| `## Spec` | what the real jet documentably does — the distilled source material, the **design targets**. The bulk of every file, page-cited, cite tags intact. | you need the real-world behaviour, number or procedure |
| `## State` | what **FlightBox** implements of it — a few honest lines with links into `doc/flightbox/`, never a copy of that content | you need to know whether the thing exists in the simulator |
| `## Gaps` | both kinds, each labelled: **source gaps** (unprocessed pages, SHALLOW research passes, unresolved Chuck/ED discrepancies) and **implementation gaps** (modelled / partially / not at all) | you are looking for work, or about to trust a number |
| `## Knowledge` | the researched engineering depth — the former `Technical depth (researched …)` sections, marker and sources intact | you need architecture, signal flow or a derivation |

Together, the `State` sections are the **coverage map FlightBox-vs-real-jet**: read them top to bottom
and you know what the simulator does and does not have.

Two exceptions. This index and [`PROGRESS.md`](PROGRESS.md) are **meta files** and carry no schema. And
[`flight-model.md`](flight-model.md) inverts it, because it documents *our* model rather than the real
jet: its `Spec` is one line (the pinned model as flown plus the declared deltas —
[`sim/assets/MODEL-DELTAS.md`](../../sim/assets/MODEL-DELTAS.md)), §1–§10 are its `State`, §12 its
`Gaps`, and §11 (the handover checklist for the next airframe) its `Knowledge`.

**Nothing was revised when the schema landed** — content moved under headings 1:1, page citations,
confidence tiers and the flagged discrepancies are untouched.

## Files

| File | Content | Source part / pages |
|---|---|---|
| [flight-model.md](flight-model.md) | **THE JSBSim MODEL ITSELF** — geometry/mass/gear, the F100 thrust tables + spool law + the throttle→AB mapping, all 41 aero functions and 35 tables per axis with their breakpoint grids, the FLCS as XML (11 channels, 58 components, every gain converted to "full stick = X"), **the model-vs-real-FLCS deviation table**, the measured envelope (corner 380 KCAS/5,625 g/16,22 °/s, roll rate saturating at ~186 °/s), 12 accepted model properties, plus mk82/aim120 | `sim/assets/aircraft/f16/` (2165 lines / 6 files), `mk82/`, `aim120/`; JSBSim engine source; own measurements |
| [flight-controls-flcs.md](flight-controls-flcs.md) | **FLCS architecture, gains, CAT I/III limiters, AoA/G envelope, ARI, MPO, autopilot modes** + ED autopilot addendum (command-authority numbers, disengage-condition cross-check) | Chuck Part 15, 662–672; ED p.128–131 |
| [aerodynamics-performance.md](aerodynamics-performance.md) | Airspeed/G/weight limits, envelope, ALOW, VMS warnings | Chuck Part 8, 153–157 |
| [hud-symbology.md](hud-symbology.md) | Every HUD element (position + meaning), control switches, AOA bracket, steering/ILS symbology — **now with ED's exact scale/geometry numbers (TFOV 25°/10.5°, all tick spacings), full Slant-Range letter table, Master Mode text tags, Great Circle Steering Cue mechanism, and a "Was der Pilot wirklich sieht" instrumentation checklist** | Chuck Parts 3/6/8/16; ED p.89–96, 225–226 |
| [cockpit-displays.md](cockpit-displays.md) | ICP/UFC, DED pages, MFD/OSB, instruments, caution/warning lights — **now FULL on ICP/UFC/DED (every DED page's fields + the propose/commit/reject edit protocol) and MFD (OSB layout, Format Selection Master Menu, DTE upload partitions)**, plus the earlier ED analog-gauge detail (AoA Indicator 11.1°/13.9° bands, VVI, full ADI, full Caution Light Panel trigger table); Aux/Left/Right Console subsections remain the residual gap | Chuck Part 3, 13–83; ED p.43–81 (partial), p.97–127 (full) |
| [controls-commands.md](controls-commands.md) | **The pilot command list** — every discrete command a pilot can issue (ICP/DED, MFD, HOTAS, autopilot mode switches) as trigger→precondition→effect→feedback→failure rows, re-cut across all panels for the avionics command-block model; the DED propose/commit/reject cycle as the canonical command pattern; a derived HOTAS-vs-DED command-rate/latency-class analysis; a collected list of every documented rejection/precondition pattern | re-cut of cockpit-displays.md + hotas.md, no new pages |
| [procedures-startup.md](procedures-startup.md) | 81-step start-up (phases A–I) + engine idle params + INS align — **+ ED FLCS-BIT/DBU/TRIM/MPO/air-refuel/EPU ground-check detail + full INS alignment type/timing/CEP-scale mechanism** | Chuck Part 4, 84–118; ED p.132–139, 165–176 |
| [procedures-takeoff-taxi.md](procedures-takeoff-taxi.md) | Taxi + takeoff steps, **takeoff-speed vs weight table** (cross-validated exact vs ED) — **+ NWS-gain-vs-groundspeed principle, exact CAT I/III loadout logic, crosswind-takeoff technique** | Chuck Part 5, 119–125; ED p.140–146 |
| [procedures-landing.md](procedures-landing.md) | Overhead pattern (7 phases), speeds, AoA, flare, roll-out — **+ AoA range refinement (11–13°), base-turn bank angles, aerobraking Roll-Indicator technique, crosswind landing, go-around rule, ALOW/decision-height link; ⚠️ flags a break-G discrepancy (Chuck 3–4G vs ED ~3G)** | Chuck Part 6, 126–133; ED p.147–156 |
| [engine-fuel.md](engine-fuel.md) | F110 engine, limits, throttle, PRI/SEC, EPU, relight, fuel system, BINGO/JOKER | Chuck Part 7, 134–152 |
| [navigation-ils.md](navigation-ils.md) | EHSI, HSD, steerpoints, TACAN, bullseye, INS drift/fix, **ILS approach** — **now FULL against ED**: nav-solution blending (300 ft threshold), System Altitude/ACAL/DTS-TRN mechanics, cursor-slew vs. position-fix vs. altitude-cal distinction, full TACAN/ILS quantitative facts incl. glideslope intercept-altitude/descent-rate tables and Decision Height mechanics; ⚠️ flags two discrepancies (steerpoint-count 99 vs 127, ADI glideslope-scale 0.7° vs 2.5°/dot) | Chuck Part 16, 673–777; ED p.163–246 |
| [hotas.md](hotas.md) | Full stick (SSC) + throttle (TQS) control mapping — **now cross-checked against ED's Hands-On Controls chapter**: exact SOI-dependent TMS/DMS/CMS/EXP-FOV action matrices, 0.5 s/1.0 s press-duration thresholds, speedbrake 60°/43° deflection limits — no contradiction found, only added precision | Chuck Part 9, 158–160; ED p.82–88 |
| [radar-sensors.md](radar-sensors.md) | FCR A-A/A-G/A-Sea modes, TGP, HMCS — **now with exact ED scan geometry/timing/thresholds** (ACM sub-mode angles, CRM sub-mode state machine, TWS/SAM/STT transitions, NCTR gates, GM/GMT/SEA/MTR parameters, TGP zoom/track modes) | Chuck Part 10, 161–312; ED p.342–523 |
| [weapons.md](weapons.md) | **FULL**: SMS/SPI/cursor logic, station/carriage data, A-A employment (gun EEGS levels, AIM-9 DLZ, AIM-120 DLZ+guidance handover), A-G delivery-mode computation (CCIP/CCRP/DTOS/LGB/JDAM/JSOW/WCMD/HARM/Maverick), munition specs, gun/bomb ballistics research+derivation | Chuck Part 11, 313–573; ED p.34–42, 303–341, 524–632, 703–704 |
| [defence-rwr-cm.md](defence-rwr-cm.md) | **FULL**: RWR antenna geometry/blind spot/priority logic, CMDS mode×CMS×ECM-XMIT interaction state machine, per-program chaff/flare parameter schema (burst/salvo qty/interval), ECM jamming types/burnthrough | Chuck Part 12, 574–599; ED p.633–649, 680–683 |
| [datalink-iff.md](datalink-iff.md) | TNDL now **FULL** (TDMA/STN mechanism, 3-channel model, message-trigger table, System Track File correlation+staleness); IFF procedure still at Chuck depth (gap noted) | Chuck Part 13, 600–648; ED p.419–452 |
| [air-refueling.md](air-refueling.md) | Boom refueling procedure, PDI/director lights, disconnect — **+ ED pre/post-refueling emitter checklists, director-light color coding, weight-effect trim note, formal breakaway procedure** | Chuck Part 17, 778–791; ED p.157–162 |

## Priority (per lead) — highest first
FLCS/autopilot → aero/performance → HUD → procedures+speeds → engine/fuel → navigation → rest.
The top files (FLCS, aero, HUD, procedures, engine, navigation) are full distillations. **weapons.md,
radar-sensors.md, defence-rwr-cm.md, datalink-iff.md were SHALLOW; the ED EA Guide pass (see
PROGRESS.md) raised weapons.md and defence-rwr-cm.md to FULL** (this became the current priority
because FlightBox's next build phase is weapon-system JSBSim instances), **deepened radar-sensors.md
additively** (parallel FCR development in progress, additive-only per instruction), and **deepened
datalink-iff.md's TNDL section to FULL** (IFF procedure itself remains at Chuck depth).

A later "pilot-KI" pass (see PROGRESS.md Pass 3) targeted exactly the ED chapters most directly
translatable into autonomous-pilot flight behavior — **Procedures, Navigation, and HUD are now fully
cross-checked against ED**, and `cockpit-displays.md` gained the analog-instrument (AoA indicator, ADI,
VVI, Caution Light Panel) detail. `air-refueling.md` also picked up its ED chapter as a low-effort
bonus since the text was already extracted alongside Procedures.

A fourth "Bedienlogik" pass (see PROGRESS.md Pass 4) closed exactly the gap Pass 3 named as its biggest
remaining one: `cockpit-displays.md`'s ICP/UFC/DED and MFD chapters are now **FULL** (every DED page's
fields, the propose/commit/reject edit protocol, MFD OSB layout + Format Selection Master Menu + DTE
upload partitions), `hotas.md` picked up its ED Hands-On Controls cross-check (no contradictions, added
precision), and a new **[`controls-commands.md`](controls-commands.md)** re-cuts all of it into a
single command list — the direct template for the avionics command-block model FlightBox is building
next.

## Not separately distilled
- Part 1 (Introduction — history), Part 2 (DCS controls setup), Part 18 (Other Resources) — Chuck.
- Part 14 (Radio Tutorial) — Chuck: comms basics folded where relevant (COM1/COM2 in
  `cockpit-displays.md`, tanker/ATC contact in `air-refueling.md`).
- ED EA Guide chapters still not touched: Left/Right Auxiliary Console + Left/Right Console subsections
  of Cockpit Overview (p.43–81, partially covered — analog gauges + Caution Light Panel only); Radio
  Communications (p.247–260); remaining HTS/HMCS WPN-format detail (p.492–523, noted as a gap in
  `radar-sensors.md`); IFF procedure detail (noted as a gap in `datalink-iff.md`); Appendices A/D/E/F
  (checklists/HOTAS quick-refs/glossary — mostly UX, low rebuild value; Appendix F formulas already
  folded into `weapons.md`). See PROGRESS.md for the full unprocessed-page ledger.

## Coverage at a glance — FlightBox vs. the real jet

One line per file, taken from its `## State` section. Read the file's own State/Gaps before acting on
a row; details and links live there, not here.

| File | FlightBox implements |
|---|---|
| [flight-model.md](flight-model.md) | **is** the state — the model as flown |
| [controls-commands.md](controls-commands.md) | **near-full**: the command bus is built from this file (propose/ack/reject, two latency classes, rejection catalogue, output blocks with `Held`) — but only commands whose target system exists |
| [hud-symbology.md](hud-symbology.md) | **near-full** for flight/nav symbology in the real combiner aperture; no ILS, no weapon symbology, no TD box (source-silent) |
| [radar-sensors.md](radar-sensors.md) | **FCR only**: CRM + four ACM sub-modes + STT, designation, emission, chaff notch, IFF Mode 4. No SAM/TWS/DTT, no A-G/A-Sea, no TGP/HMCS/HTS |
| [defence-rwr-cm.md](defence-rwr-cm.md) | **both halves built**: ALR-56M geometry + rangeless threats, ALE-47 programs/modes/consent, chaff clouds. No ECM, no MWS, no threat library, flares inert |
| [datalink-iff.md](datalink-iff.md) | **cooperative net built** (range, radio horizon, 1 Hz cycle, staleness, POWER/XMT) + IFF Mode 4. No TDMA/STN/PPLI, no STF correlation, no HSD |
| [weapons.md](weapons.md) | **three weapons** (AIM-120, Mk-82, M61A1) + damage; CCIP/CCRP from one integration; no SPI/cursor model, no AIM-9/LGB/JDAM/HARM/Maverick, no strafing |
| [flight-controls-flcs.md](flight-controls-flcs.md) | the **model's** FLCS plus an FBW outer loop; own guidance modes instead of the relief modes; no CAT I/III, ARI, MPO |
| [aerodynamics-performance.md](aerodynamics-performance.md) | the envelope as **measured on the model** + ALOW; no enforced speed/G limits, no VMS voice |
| [engine-fuel.md](engine-fuel.md) | thrust/spool/fuel via JSBSim (**F100-PW-229**, not the F110 this file describes) + BINGO + damage effects; no engine systems |
| [procedures-takeoff-taxi.md](procedures-takeoff-taxi.md) | **takeoff flies** with the Vr-vs-weight table; no taxi, no CAT I/III, no crosswind |
| [procedures-landing.md](procedures-landing.md) | **landing flies** (straight-in); flown to a fixed CAS, not the 11–13° AoA band; no pattern, no ILS, no go-around |
| [procedures-startup.md](procedures-startup.md) | **nothing of the sequence** — the spawn is trimmed and configured declaratively; no INS alignment |
| [cockpit-displays.md](cockpit-displays.md) | **no displays** — but this file's DED protocol and the "gear-down freezes" precedent shaped the avionics bus |
| [hotas.md](hotas.md) | **no binding** — the actions exist as bus commands, no SOI, no press-duration classes |
| [navigation-ils.md](navigation-ils.md) | **steerpoint + bullseye only** — no TACAN, no ILS, no INS drift/alignment, no EHSI/HSD |
| [air-refueling.md](air-refueling.md) | **nothing** — fuel state exists, the task does not |

## FlightBox relevance
- `flight-model.md` is the **ground truth per Prinzip 5** and the first place to look when a measured
  number surprises you: it explains why full aft stick buys 5.6 g and not 9 (the model's pitch channel
  is a *rate* command), why roll rate plateaus at ~186 °/s (the 1/0.31821 command scale), why roll
  input costs ~2 g of lift (a sign error in the flaperon mixer), and what `fcs/fbw-override` actually
  bypasses (pitch only). Its §9 lists the 12 accepted model properties that must never be "fixed".
- `controls-commands.md` is the **direct template for the upcoming avionics command-block model**: the
  DED's propose→commit/reject cycle is the reference pattern for command blocks with acknowledgment +
  rejection reasons, and the file's §6 collects every documented precondition/rejection case found
  across the source guides. The pilot-KI is meant to drive avionics through exactly this command
  vocabulary — no shortcuts. Its §5 derives a HOTAS-vs-DED command-latency-class split (sub-second
  switch actions vs. multi-second head-down DED edits) as a concrete bound on pilot-KI command rate.
- `flight-controls-flcs.md` is the **highest-value** file: our FBW (`fcs/fbw-override`) commands this FLCS;
  the limiter/gain/autopilot spec is what FBW must reproduce or cleanly bypass.
- `hud-symbology.md` is the reference our MIL-STD-1787 HUD is built against — now also the home of the
  **"Was der Pilot wirklich sieht"** checklist: the instrumentation ground-truth a pilot module should
  be validated against (what quantities are actually displayed, at what resolution/format).
- `aerodynamics-performance.md` + `procedures-*` give speeds/limits for envelope/mission checks;
  `procedures-landing.md` now gives an **AoA target range (11–13°, not flat 11°)** for the base-turn-
  through-rollout segments — a concrete candidate refinement for `FBF16Pilot`'s approach law.
- `navigation-ils.md` now carries the **glideslope-intercept-altitude and glideslope-descent-rate
  tables** — directly usable inputs for a future ILS-coupled `FBAutopilot::Direct` approach mode, plus
  the CARA-ALOW-as-missed-approach-trigger pattern (reuses the already-simulated ALOW system).
- `weapons.md` is now the reference for the **upcoming weapon-JSBSim-instance phase**: SMS/SPI logic,
  release-computation logic (CCIP/CCRP/DTOS/LGB/JDAM/HARM/Maverick), station/carriage data, and
  researched+derived ballistics/fuzing numbers — see its "Technical depth" §4 for what's measurable in
  `fb-gym` once each weapon carries its own FDM, vs. what remains a genuine documentation gap.
