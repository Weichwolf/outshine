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

## Files

| File | Content | Source part / pages |
|---|---|---|
| [flight-controls-flcs.md](flight-controls-flcs.md) | **FLCS architecture, gains, CAT I/III limiters, AoA/G envelope, ARI, MPO, autopilot modes** + ED autopilot addendum (command-authority numbers, disengage-condition cross-check) | Chuck Part 15, 662–672; ED p.128–131 |
| [aerodynamics-performance.md](aerodynamics-performance.md) | Airspeed/G/weight limits, envelope, ALOW, VMS warnings | Chuck Part 8, 153–157 |
| [hud-symbology.md](hud-symbology.md) | Every HUD element (position + meaning), control switches, AOA bracket, steering/ILS symbology | Chuck Parts 3/6/8/16 |
| [cockpit-displays.md](cockpit-displays.md) | ICP/UFC, DED pages, MFD/OSB, instruments, caution/warning lights | Chuck Part 3, 13–83 |
| [procedures-startup.md](procedures-startup.md) | 81-step start-up (phases A–I) + engine idle params + INS align | Chuck Part 4, 84–118 |
| [procedures-takeoff-taxi.md](procedures-takeoff-taxi.md) | Taxi + takeoff steps, **takeoff-speed vs weight table** | Chuck Part 5, 119–125 |
| [procedures-landing.md](procedures-landing.md) | Overhead pattern (7 phases), speeds, AoA, flare, roll-out | Chuck Part 6, 126–133 |
| [engine-fuel.md](engine-fuel.md) | F110 engine, limits, throttle, PRI/SEC, EPU, relight, fuel system, BINGO/JOKER | Chuck Part 7, 134–152 |
| [navigation-ils.md](navigation-ils.md) | EHSI, HSD, steerpoints, TACAN, bullseye, INS drift/fix, **ILS approach** | Chuck Part 16, 673–777 |
| [hotas.md](hotas.md) | Full stick (SSC) + throttle (TQS) control mapping | Chuck Part 9, 158–160 |
| [radar-sensors.md](radar-sensors.md) | FCR A-A/A-G/A-Sea modes, TGP, HMCS — **now with exact ED scan geometry/timing/thresholds** (ACM sub-mode angles, CRM sub-mode state machine, TWS/SAM/STT transitions, NCTR gates, GM/GMT/SEA/MTR parameters, TGP zoom/track modes) | Chuck Part 10, 161–312; ED p.342–523 |
| [weapons.md](weapons.md) | **FULL**: SMS/SPI/cursor logic, station/carriage data, A-A employment (gun EEGS levels, AIM-9 DLZ, AIM-120 DLZ+guidance handover), A-G delivery-mode computation (CCIP/CCRP/DTOS/LGB/JDAM/JSOW/WCMD/HARM/Maverick), munition specs, gun/bomb ballistics research+derivation | Chuck Part 11, 313–573; ED p.34–42, 303–341, 524–632, 703–704 |
| [defence-rwr-cm.md](defence-rwr-cm.md) | **FULL**: RWR antenna geometry/blind spot/priority logic, CMDS mode×CMS×ECM-XMIT interaction state machine, per-program chaff/flare parameter schema (burst/salvo qty/interval), ECM jamming types/burnthrough | Chuck Part 12, 574–599; ED p.633–649, 680–683 |
| [datalink-iff.md](datalink-iff.md) | TNDL now **FULL** (TDMA/STN mechanism, 3-channel model, message-trigger table, System Track File correlation+staleness); IFF procedure still at Chuck depth (gap noted) | Chuck Part 13, 600–648; ED p.419–452 |
| [air-refueling.md](air-refueling.md) | Boom refueling procedure, PDI lights, disconnect | Chuck Part 17, 778–791 |

## Priority (per lead) — highest first
FLCS/autopilot → aero/performance → HUD → procedures+speeds → engine/fuel → navigation → rest.
The top files (FLCS, aero, HUD, procedures, engine, navigation) are full distillations. **weapons.md,
radar-sensors.md, defence-rwr-cm.md, datalink-iff.md were SHALLOW; the ED EA Guide pass (see
PROGRESS.md) raised weapons.md and defence-rwr-cm.md to FULL** (this became the current priority
because FlightBox's next build phase is weapon-system JSBSim instances), **deepened radar-sensors.md
additively** (parallel FCR development in progress, additive-only per instruction), and **deepened
datalink-iff.md's TNDL section to FULL** (IFF procedure itself remains at Chuck depth).

## Not separately distilled
- Part 1 (Introduction — history), Part 2 (DCS controls setup), Part 18 (Other Resources) — Chuck.
- Part 14 (Radio Tutorial) — Chuck: comms basics folded where relevant (COM1/COM2 in
  `cockpit-displays.md`, tanker/ATC contact in `air-refueling.md`).
- ED EA Guide chapters not yet touched: Cockpit Overview/Instrument Panel (p.43–127, overlaps
  `cockpit-displays.md`/`hud-symbology.md`/`hotas.md`), Procedures (p.132–162, overlaps
  `procedures-*.md`), Navigation (p.163–246, overlaps `navigation-ils.md`), Radio Communications
  (p.247–260), remaining HTS/HMCS WPN-format detail (p.492–523, noted as a gap in
  `radar-sensors.md`), IFF procedure detail (noted as a gap in `datalink-iff.md`), Appendices A/D/E/F
  (checklists/HOTAS quick-refs/glossary — mostly UX, low rebuild value; Appendix F formulas already
  folded into `weapons.md`). See PROGRESS.md for the full unprocessed-page ledger.

## FlightBox relevance
- `flight-controls-flcs.md` is the **highest-value** file: our FBW (`fcs/fbw-override`) commands this FLCS;
  the limiter/gain/autopilot spec is what FBW must reproduce or cleanly bypass.
- `hud-symbology.md` is the reference our MIL-STD-1787 HUD is built against.
- `aerodynamics-performance.md` + `procedures-*` give speeds/limits for envelope/mission checks.
- `weapons.md` is now the reference for the **upcoming weapon-JSBSim-instance phase**: SMS/SPI logic,
  release-computation logic (CCIP/CCRP/DTOS/LGB/JDAM/HARM/Maverick), station/carriage data, and
  researched+derived ballistics/fuzing numbers — see its "Technical depth" §4 for what's measurable in
  `fb-gym` once each weapon carries its own FDM, vs. what remains a genuine documentation gap.
