# F-16C Doc Distillation — PROGRESS

Source: `doc/DCS F-16C Viper Guide.pdf` (Charles Ouellet, "Chuck's Guide", 794 pp, PDF v1.7).
PDF page index == printed guide page number (verified: pdf p.3 → footer "3").
Extract tool: `pdftotext` (works cleanly). Full text cached during work.

## Part → page map (from sidebar-label scan)

| Part | Title | Pages | Target file |
|---|---|---|---|
| 1  | Introduction | 4–8 | (skip — history) |
| 2  | Controls Setup | 9–12 | (skip — DCS bindings) |
| 3  | Cockpit & Equipment | 13–83 | hud-symbology.md + cockpit-displays.md |
| 4  | Start-Up Procedure | 84–118 | procedures-startup.md |
| 5  | Taxi & Takeoff | 119–125 | procedures-takeoff-taxi.md |
| 6  | Landing | 126–133 | procedures-landing.md |
| 7  | Engine & Fuel Mgmt | 134–152 | engine-fuel.md |
| 8  | Flight & Aerodynamics | 153–157 | aerodynamics-performance.md |
| 9  | HOTAS | 158–160 | hotas.md |
| 10 | Radar & Sensors | 161–312 | radar-sensors.md |
| 11 | Offence: Weapons | 313–573 | weapons.md |
| 12 | Defence: RWR & CM | 574–599 | defence-rwr-cm.md |
| 13 | Datalink & IFF | 600–648 | datalink-iff.md |
| 14 | Radio Tutorial | 649–661 | (fold into datalink/nav if relevant) |
| 15 | Flight Controls & Autopilot | 662–672 | flight-controls-flcs.md |
| 16 | Navigation & ILS | 673–777 | navigation-ils.md |
| 17 | Air-to-Air Refueling | 778–791 | air-refueling.md |
| 18 | Other Resources | 792 | (skip) |

## Priority order (per lead)

1. FLCS/autopilot (Part 15) — HIGHEST, our FBW commands this FLCS
2. Aero/performance (Part 8)
3. HUD symbology (Part 3 HUD subsection)
4. Procedures + speeds (Parts 4/5/6)
5. Engine/fuel (Part 7)
6. Navigation/ILS (Part 16)
7. Rest (9 HOTAS, 10 radar, 11 weapons, 12 defence, 13 datalink, 17 refuel)

## Status — COMPLETE (15/15 files)

- [x] Page map established
- [x] flight-controls-flcs.md   (full)
- [x] aerodynamics-performance.md (full)
- [x] hud-symbology.md          (full — cross-part)
- [x] cockpit-displays.md       (full)
- [x] procedures-startup.md     (full)
- [x] procedures-takeoff-taxi.md (full + speed table)
- [x] procedures-landing.md     (full)
- [x] engine-fuel.md            (full)
- [x] navigation-ils.md         (full)
- [x] hotas.md                  (full)
- [x] radar-sensors.md          (structural — mode taxonomy + concepts, not per-tutorial)
- [x] weapons.md                (structural — arsenal + delivery modes, not per-weapon steps)
- [x] defence-rwr-cm.md         (full)
- [x] datalink-iff.md           (full)
- [x] air-refueling.md          (full)
- [x] INDEX.md

## Mandate 2 — "Technical depth" per subsystem (researched, cited, for rebuild)
Add a clearly separated researched section per file (real architecture/signal-flow, components, quantitative
params) from public engineering literature (NASA/AIAA, HAF/-1, USAF, Stevens-Lewis, AeroBench). Guide vs
research kept separate. Tools: WebSearch/WebFetch. Priority: FLCS first.

### Research-status column (Technical depth)

| Prio | File | Research | Primary sources |
|---|---|---|---|
| 1 | flight-controls-flcs.md | **FULL** | NASA TP-1538; AFTI/F-16 NTRS 19840012524; Droste&Walker AIAA 1980; Stevens-Lewis; AeroBenchVVPython (code-confirmed); Kim 2020; ryanporto; VT; falcon-bms |
| 2 | aerodynamics-performance.md | **FULL** | NASA TP-1538 NTRS 19800005879 (AoA −20..+90°, β ±30°, static+forced-osc, DMS); JSBSim devel list (TP-1538 = our FDM); VT Mason; f-16.net |
| 3 | engine-fuel.md | **FULL** | Wikipedia/GlobalSecurity/GE/HandWiki F110-GE-129; Stevens-Lewis power-lag |
| 4 | hud-symbology.md | **FULL** | MIL-STD-1787A/D; DTIC ADA430578; airforce-technology (MMC/HUD-EU) |
| 4 | navigation-ils.md | **FULL** | pilotscafe/code7700/PPRuNe (ILS scaling); airforce-technology + Wikipedia APG-68 (CARA/INS LRUs) |
| 4 | cockpit-displays.md | **FULL** | airforce-technology (MMC=XFCC+HUD-EU+XCIU); 1553-bus refs |
| 5 | radar-sensors.md | **SHALLOW** | APG-68/APG-83, Sniper/LITENING, JHMCS, ASQ-213 |
| 5 | weapons.md | **SHALLOW** | SMS/XCIU→MMC, M61A1, LAU-129/TER |
| 5 | defence-rwr-cm.md | **SHALLOW** | ALR-56M, ALE-47, ALQ-131/184/213 |
| 5 | datalink-iff.md | **SHALLOW** | MIDS-LVT (Link-16), AN/APX-113 |
| 5 | hotas.md | **SHALLOW** | force-transducer SSC, TQS |
| 5 | procedures-startup.md | **SHALLOW** | JFS, EPU (hydrazine H-70), RLG-INS |
| 5 | procedures-takeoff-taxi.md | **SHALLOW** | NWS, anti-skid carbon brakes |
| 5 | procedures-landing.md | **SHALLOW** | speedbrakes, carbon brakes, AoA probes |
| 5 | air-refueling.md | **SHALLOW** | UARRSI boom receptacle, PDI |

**Priorities 1–4 research = FULL; priority 5 = SHALLOW** (LRU designations + 3–5-sentence principle +
sources, explicitly marked "shallow pass — deepen when in scope"). Combat/EW/datalink is outside current
rebuild scope (flight + rendering); shallow stubs keep the set complete + extension-ready.

Iteration 2 deepened prio 1–3: FLCS (ISA actuators + dual hydraulics A/B + FLCC 4-channel voting + full
sensor suite), aero (TP-1538 reference dataset = our JSBSim FDM origin), engine (thrust-map/lapse + spool
power-lag). Iteration 3 = shallow LRU pass on the remaining 9 files.

## COVERAGE: COMPLETE
All 15 subsystem files carry guide distillation + a Technical-depth section (FULL for prio 1–4,
SHALLOW-marked for prio 5). INDEX.md + PROGRESS.md current.

Method note: pdftotext stays PRIMARY (real text layer over images). Visual `Read pages:"N-M"` only for pure
position-critical drawings (e.g. HUD symbology layout). No "needs visual pass" rollback of existing files.

## Notes on depth
- FLCS, aero, HUD, procedures, engine, nav: full distillation (small/medium parts).
- Radar (152 pp) + weapons (261 pp): distilled to mode/arsenal structure + key display/HOTAS concepts.
  The guide's exhaustive per-weapon employment tutorials are referenced, not transcribed step-by-step.
- Skipped: Part 1 (history), Part 2 (DCS bindings), Part 14 (radio — folded), Part 18 (resources).
