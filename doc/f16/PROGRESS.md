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

## COVERAGE (Chuck's Guide pass): COMPLETE
All 15 subsystem files carry guide distillation + a Technical-depth section (FULL for prio 1–4,
SHALLOW-marked for prio 5). INDEX.md + PROGRESS.md current as of that pass.

Method note: pdftotext stays PRIMARY (real text layer over images). Visual `Read pages:"N-M"` only for pure
position-critical drawings (e.g. HUD symbology layout). No "needs visual pass" rollback of existing files.

## Notes on depth (Chuck's Guide pass)
- FLCS, aero, HUD, procedures, engine, nav: full distillation (small/medium parts).
- Radar (152 pp) + weapons (261 pp): distilled to mode/arsenal structure + key display/HOTAS concepts.
  The guide's exhaustive per-weapon employment tutorials are referenced, not transcribed step-by-step.
- Skipped: Part 1 (history), Part 2 (DCS bindings), Part 14 (radio — folded), Part 18 (resources).

---

## Pass 2 — ED EA Guide (official Eagle Dynamics module manual) integration

Source: `doc/DCS F-16C Early Access Guide EN.pdf` (Eagle Dynamics SA, 704 pp, PDF v1.7, "Updated 23
March 2026"). Extract tool: `pdftotext -f <first> -l <last>` in bounded page ranges (same method as
pass 1) — confirmed working cleanly on this PDF too (real text layer, not scanned images). TOC
extracted first (pp.3–12) to build the page map below; used to target extraction instead of a blind
front-to-back read (704 pp does not fit one run).

### ED EA Guide page map (from TOC, pp.3–12)
| ED chapter | Pages | Status this pass | Target file |
|---|---|---|---|
| DCS World Fundamentals / Flight Control basics | 15–27 | not processed (generic DCS UX, low value) | — |
| The F-16C Viper / Aircraft History | 28–33 | not processed (history) | — |
| Weapons & Munitions overview | 34–42 | **processed** | weapons.md §3 |
| Cockpit Overview / Instrument Panel / consoles | 43–81 | **not processed this pass** | cockpit-displays.md (future) |
| Hands-On Controls (HOTAS) | 82–88 | not processed this pass | hotas.md (future) |
| Head-Up Display (HUD) | 89–96 | not processed this pass | hud-symbology.md (future) |
| Upfront Controls (UFC/ICP/DED) | 97–120 | not processed this pass | cockpit-displays.md (future) |
| Multi-Function Displays (MFD) | 121–127 | not processed this pass | cockpit-displays.md (future) |
| **Autopilot** | 128–131 | **processed** | flight-controls-flcs.md addendum (found + flagged a genuine bank-angle-limit discrepancy vs Chuck: ED ±30° vs Chuck's 45°) |
| Procedures (start/taxi/takeoff/landing/refuel) | 132–162 | not processed this pass | procedures-*.md, air-refueling.md (future) |
| Navigation (INS/steerpoints/TACAN/ILS) | 163–246 | not processed this pass | navigation-ils.md (future) |
| Radio Communications | 247–260 | not processed this pass | — (low priority) |
| Tactical Employment overview / Master Modes / SOI/SPI / **Weapon Delivery Sub-modes** / VRP/VIP / HSD / **SMS** | 261–341 | **processed** | weapons.md §2.1–2.3 |
| **APG-68 FCR** (employment, MFD format, STBY, A-A modes CRM/ACM/NCTR/AIFF, A-G modes) | 342–418 | **processed** | radar-sensors.md addendum |
| **Tactical Net Datalink** (TNDL network/PPLI/tracks/markpoints/SEAD) | 419–452 | **processed** | datalink-iff.md §3 |
| **AAQ-33 TGP** | 453–491 | **processed** (sensor/zoom/track-mode facts only — full DED/MFD-format field walkthrough not extracted) | radar-sensors.md addendum |
| ASQ-213 HTS | 492–515 | **not processed this pass** (flagged gap in radar-sensors.md) | radar-sensors.md (future) |
| JHMCS | 516–523 | **not processed this pass** (flagged gap in radar-sensors.md) | radar-sensors.md (future) |
| **Air-to-Air Weapons Employment** (Dogfight/MSL OVRD, M61A1 gun/EEGS, AIM-9M/X, AIM-120) | 524–548 | **processed** | weapons.md §2.5 |
| **Air-to-Ground Weapons Employment** (strafe, rockets, bombs CCIP/CCRP, LGB, JDAM, JSOW, WCMD, HARM, Maverick) | 549–632 | **processed** | weapons.md §2.6 |
| **Defensive Systems** (RWR, CMDS, ECM) | 633–649 | **processed** | defence-rwr-cm.md §2 |
| Appendix A (checklists) | 651–679 | not processed (low rebuild value — UX checklists) | — |
| **Appendix B** (ALIC codes & RWR symbols) | 680–683 | **processed structurally** (table schema described, full ~60-row table NOT transcribed) | defence-rwr-cm.md §3 |
| Appendix C (HAD/WPN threat tables) | 684–685 | not processed this pass | weapons.md / defence-rwr-cm.md (future) |
| Appendix D (HOTAS quick-refs) | 686–692 | not processed (redundant with per-chapter HOTAS notes already folded in) | — |
| Appendix E (Glossary) | 693–702 | not processed (reference only, low rebuild value) | — |
| **Appendix F** (Formulas) | 703–704 | **processed** | weapons.md (Bingo/range/unit-conversion formulas — not weapon-ballistics formulas, kept as context only, not load-bearing) |

### Files raised this pass
- **weapons.md**: SHALLOW → **FULL**. SMS/SPI/cursor logic, station/carriage/stores-code data, full
  A-A employment (gun EEGS Level I–V + funnel geometry + dispersion, AIM-9 DLZ/SLAVE-BORE/SPOT-SCAN,
  AIM-120 DLZ/ASEC/guidance-phase-handover/multi-target), full A-G delivery-mode computation
  (CCIP/CCIP-post-designate/CCRP/LGB/JDAM/JSOW/WCMD/HARM-HAS-POS/Maverick-MBC/ripple/force-correlate),
  official munition-spec table, plus a researched+derived Technical-depth §4 (confidence-tiered:
  M61A1 ballistics, bomb fuzing/arming-time tables, AIM-9/AIM-120 seeker facts, station weight
  classes) with an explicit derived-fall-time formula and a "measurable in fb-gym once weapon-FDM
  exists" note on ballistics gaps. **This is the priority-1 deliverable for the upcoming weapon-JSBSim
  rebuild phase.**
- **defence-rwr-cm.md**: SHALLOW → **FULL** (task explicitly elevated CMDS to weapons.md-equivalent
  depth). RWR antenna geometry + blind-spot mechanics + priority-display logic; full CMDS mode×CMS×
  ECM-XMIT interaction state machine (MAN/STBY/SEMI/AUTO/BYP, each with different consent semantics);
  the actual per-program parameter schema (Burst Qty/Interval, Salvo Qty/Interval, 0–99 / 0.001–10 s /
  0.01–150 s ranges); ECM noise/deception/burnthrough physical principle + the 3-way XMIT-switch ×
  CMDS-mode-gate table. Researched §4: ALR-56M architecture/frequency band (T3), ALE-47 capacity
  cross-check (T3/T4, kept secondary to ED's own F-16C-specific number).
- **radar-sensors.md**: additive-only pass (parallel FCR dev agent in progress — instruction was
  explicit: do not reword existing content). Added: full CRM sub-mode taxonomy + transition-trigger
  state machine (RWS/VSR/TWS/SAM/DT-SAM/DTT/STT with exact entry/exit conditions), scan-frame
  timing (8 s @ ±60°/4-bar vs 2 s @ ±30°/2-bar), TWS track-file management (10-file cap, 13 s stale-
  purge, System/Cursor/Bugged Target priority-scan mechanism), exact ACM sub-mode scan geometry
  (30×20/10×60/BORE/SLEW angular volumes + ranges — this is the biggest single precision gain, Chuck
  only had the mode names), NCTR gates (≤25 NM, 0–30°/150–180° aspect), GM/GMT/SEA scan-geometry
  parameters + DBS1/DBS2 + MTR HI/LO threshold table, TGP zoom/track-mode facts. HTS/HMCS chapters
  (ED p.492–523) explicitly flagged as **not yet processed** — remaining gap for a future pass.
- **datalink-iff.md**: TNDL section SHALLOW → **FULL** (TDMA/STN mechanism, 3-channel Fighter/Mission/
  Special model with differing per-channel data richness, message-type/trigger table, System Track
  File partition+correlation+13s-staleness — same constant as FCR's TWS staleness, noted as likely a
  shared system parameter). **IFF procedure section left at Chuck depth** — explicitly flagged as a
  gap, not reprocessed this pass (ED's IFF chapter overlaps AIFF material already captured in
  radar-sensors.md's NCTR addendum, but wasn't independently re-extracted for datalink-iff.md).
- **flight-controls-flcs.md**: one addendum (autopilot, ED p.128–131) — added ED's quantified
  command-authority numbers (pitch +3.0/−1.0 G, roll ≤20°/s, bank ±30°) and its disengage-condition
  list, and **explicitly flagged a genuine numeric discrepancy** against Chuck's "bank limited to 45°"
  for HDG SEL/STRG SEL — both values kept, ED marked as the more belastbar source per the task's
  source hierarchy, JSBSim model itself remains the actual ground truth per CLAUDE.md Prinzip 5.

### Priority-4 sweep ("wo ED eine Aussage aus Chuck präzisiert/widerlegt, in ALLE übrigen Dateien
nachtragen") — status: **PARTIAL, not exhaustive**. Only `flight-controls-flcs.md`'s autopilot section
was checked and amended this pass (via the ED Autopilot chapter, p.128–131, which was extracted
anyway for weapons/radar/CMDS context). The remaining full-depth files (aerodynamics-performance.md,
hud-symbology.md, cockpit-displays.md, procedures-*.md, engine-fuel.md, navigation-ils.md, hotas.md,
air-refueling.md) were **not cross-checked against their corresponding ED EA Guide chapters this
pass** — those ED chapters (Cockpit/HUD/UFC/MFD pp.43–127, Procedures pp.132–162, Navigation
pp.163–246, Radio pp.247–260) are listed as **not processed** in the page map above. This is real,
bounded-run-honest remaining work, not a completeness claim.

### Research/derivation discipline used this pass (per task + coordinator clarifications)
- Source-hierarchy tiers applied throughout new Technical-depth sections: **T1** official/declassified
  mil docs (T.O./MIL-STD/AFMAN, DTIC/NASA/AIAA) > **T2** manufacturer datasheets > **T3** established
  literature/databases (FAS, GlobalSecurity, Jane's-class) > **T4** community/wiki (cross-check only,
  explicitly flagged inline every time it's used — e.g. F-16.net station-weight-class figures).
- **Derived-not-guessed** quantities (physics-derivable from documented inputs, formula shown): the
  drag-free free-fall-time/impact-velocity sanity-floor formula in weapons.md §4.2 (`t=√(2h/g)`,
  explicitly caveated as a lower bound only, not a release-solution substitute — real trajectories
  need JSBSim's actual store aero, which is the point of the upcoming weapon-FDM phase anyway).
- **Gaps correctly left open, not filled with plausible numbers**: M61A1 spool-up-time T1/T2 citation,
  individual PGU-round projectile mass, AIM-9X internal-cooler duty cycle, AIM-120 Rmin/seeker-
  activation-range as a fixed constant (it's engagement-geometry-dependent per ED's own DLZ cues, not
  a fixed spec number), HARM threat-table scan-cycle-time formula, JDAM/JSOW CEP figures, exact F-16C
  Block-50 CMDS cartridge type/mix, PPLI transmission-interval seconds value, full FMU-139-vs-JDAM-kit
  fuze cross-reference. Each is marked with a "measurable in fb-gym once weapon-FDM exists" note where
  applicable (per coordinator guidance — several ballistics/timing gaps convert from "research" to
  "simulate and read the CSV" once each weapon is its own JSBSim instance).

### Method note (pass 2)
`pdftotext -f <first> -l <last>` confirmed PRIMARY-viable on the ED EA Guide too (real text layer).
No visual/image-based pages were needed this pass — all extracted chapters were prose+table text.
Large chapters (FCR 342–418, A-G weapons 549–632) were extracted in ≤40-page bounded ranges per the
"process only what fits comfortably in one run" rule, cached to scratchpad, and read in ≤550-line
`Read` windows to stay within context.

## Outlook — noted, NOT this round's task
- `doc/DCS MiG-29 Flight Manual EN.pdf` and `doc/DCS MiG-29A Early Access Manual EN.pdf` are present
  in `doc/` but out of scope for this pass (F-16 only, per task). If a future task builds a
  `doc/mig29/` knowledge base, the same two-tier approach (official EA manual = primary system-logic
  source, any third-party guide = secondary) and the same pdftotext-bounded-range extraction method
  should transfer directly — noting this here per coordinator instruction, not acting on it.
- Multi-unit/adversary AI (per CLAUDE.md's "Ausblick Multi-Unit") will eventually need an equivalent
  systems reference for whatever aircraft face the F-16 in a mission — MiG-29 is the most likely first
  adversary given the manuals already sitting in `doc/`.

## COVERAGE (end of Pass 2): PARTIAL — see "Priority-4 sweep" above for exactly what's left
Pass-1 (Chuck's Guide) coverage remains COMPLETE. Pass-2 (ED EA Guide) coverage is complete for its
stated priority order (weapons.md → radar-sensors.md → defence-rwr-cm.md → datalink-iff.md → one
flight-controls-flcs.md addendum) but explicitly NOT complete for the broader "check every file
against ED" sweep, nor for the ED chapters listed as "not processed this pass" in the page map above
(Cockpit/HUD/UFC/MFD, Procedures, Navigation, Radio, HTS/HMCS detail, IFF procedure detail, Appendix
C threat tables, the full Appendix B ALIC/RWR table transcription).

---

## Pass 3 — ED EA Guide, pilot-KI priority chapters (Procedures / Navigation / Cockpit-HUD)

Goal per this round's task: pick up exactly the ED chapters Pass 2 flagged as "not processed" that are
**most directly translatable into flight-AI behavior** — Procedures, Navigation, then Cockpit/HUD/UFC/
MFD, in that order. `pdftotext -f <first> -l <last>` (same bounded-range method) confirmed clean again
on all pages extracted this pass; no visual/image-based pages needed.

### Pages processed this pass
| ED chapter | Pages | Status | Target file(s) |
|---|---|---|---|
| **Procedures** (Aircraft Start → Taxi → Takeoff → Crosswind Takeoff → Landing → Overhead Break → Crosswind Landing → Shutdown → Aerial Refueling) | 132–162 | **FULL — entire chapter extracted and distilled** | `procedures-startup.md`, `procedures-takeoff-taxi.md`, `procedures-landing.md`, `air-refueling.md` |
| **Navigation** (Navigational Sensors, INS, Nav Solutions/System Altitude/DTS, Nav Updates, Nav Database/Steerpoints, Nav by Steerpoints, TACAN, ILS) | 163–246 | **FULL — entire chapter extracted and distilled** | `navigation-ils.md`, `procedures-startup.md` (INS alignment), `hud-symbology.md` (Great Circle Steering Cue) |
| **Head-Up Display (HUD)** | 89–96 | **FULL** | `hud-symbology.md` |
| **Cockpit Overview / Instrument Panel** (analog gauges: AoA Indicator, VVI, ADI, Caution Light Panel, misc gauges) | 43–81 | **PARTIAL** — Instrument Panel's analog-gauge subsection extracted (the parts overlapping HUD/AoA/attitude/warning-light fidelity); Left/Right Auxiliary Console and Left/Right Console subsections **not** independently re-extracted (their content is largely switch positions already cross-referenced from `flight-controls-flcs.md`/`procedures-startup.md`/`engine-fuel.md`) | `cockpit-displays.md` |
| Hands-On Controls (HOTAS) | 82–88 | not processed this pass (Chuck's `hotas.md` already FULL; ED cross-check remains a gap) | — |
| Upfront Controls (UFC/ICP/DED) | 97–120 | **not processed this pass** — the single biggest remaining gap from this round's stated priority 3 | `cockpit-displays.md` (future) |
| Multi-Function Displays (MFD) | 121–127 | **not processed this pass** | `cockpit-displays.md` (future) |
| Data Transfer Equipment (DTE) | 126 (within MFD chapter) | not processed | — |
| HTS/HMCS (p.492–523) | — | still not processed (priority 4, out of budget this pass) | `radar-sensors.md` (flagged gap, unchanged) |
| Appendices | — | still not processed (unchanged from Pass 2) | — |

### Files raised this pass
- **procedures-startup.md**: added an "ED EA Guide addendum" — cross-validates every Chuck-sourced
  quantitative milestone exactly (JFS spool %, idle parameters), then adds procedure detail Chuck
  omits: FLCS PWR TEST (battery-only pre-engine-start relay check), FLCS BIT (~45 s), DBU/TRIM/MPO/
  air-refuel/EPU ground checks (the air-refuel check explicitly confirms AIR REFUEL OPEN triggers
  FLCS Takeoff&Landing gains — closes an inferred-vs-stated gap in `flight-controls-flcs.md`), and the
  full INS alignment mechanism (3 alignment types with exact timing, the 99→10 status/CEP scale, RDY/
  ALIGN flash logic).
- **procedures-takeoff-taxi.md**: cross-validates the takeoff-speed-vs-weight table exactly (no
  discrepancy — real confidence gain). Adds: NWS gain proportional to groundspeed (a quantifiable FBW
  design target, not previously stated), the exact CAT I/III loadout boolean logic (refines
  `flight-controls-flcs.md`'s summary table with ED's precise rule), and the crosswind-takeoff
  stick/rudder sequencing technique (new, not in Chuck's file at all).
- **procedures-landing.md**: cross-validates most numbers, refines the AoA target from a flat 11° to an
  **11–13° range** for base/final/rollout (with an explicit note on what this means/doesn't mean for
  our measured 165 kt/11°-AoA on-speed CAS — see below), adds base-turn bank-angle numbers (30–45°,
  absent from Chuck entirely), the Roll-Indicator aerobraking cross-check technique, a full crosswind-
  landing procedure (missing from Chuck's landing file), the explicit go-around-before-flare rule, and
  the CARA ALOW/MSL FLOOR → ILS-decision-height automation link. **Flags one genuine numeric
  discrepancy**: overhead-break G-loading, Chuck "3–4 G" vs ED "~3 G" (both kept, ED marked primary
  per this task-set's source-hierarchy convention, same pattern as `flight-controls-flcs.md`'s existing
  bank-angle-limit discrepancy).
- **navigation-ils.md**: by far the largest addition this pass — the entire ED Navigation chapter
  distilled into a new "ED EA Guide addendum" covering: navigational sensor inventory; the three
  navigation solutions (INS-only/GPS-only/Blended) and the **300 ft Kalman-correction threshold**;
  System Altitude (SALT) source-priority + the master-mode-dependent AUTO ACAL accuracy thresholds
  (A-G <50 ft GPS/<20 ft DTS vs. all-other-modes <100 ft — genuinely useful for a future weapon-release-
  altitude-accuracy model); DTS/Terrain-Referenced-Nav mechanics (150–640 kt groundspeed window, CARA
  50,000 ft/±60° limits); the cursor-slew vs. position-fix vs. altitude-calibration three-way mechanism
  distinction (with ED's own worked 72 ft delta example); the Great Circle Steering Cue (ED's name for
  Chuck's "Tadpole", now with its actual great-circle bearing computation documented, not just its
  visual behavior); full TACAN quantitative facts (252 channels, 130 nm reliable range); and the full
  ILS chapter — marker-beacon tone/Morse specs, the **glideslope-intercept-altitude table** and
  **glideslope-descent-rate table** (both directly usable as an autopilot ILS-guidance reference/
  sanity-check), Decision Height/Missed-Approach-Point mechanics, and the Command Steering Symbol's
  bank-angle/vertical-velocity control-law framing. **Flags two genuine discrepancies**: (1) the
  steerpoint database size/partition structure — Chuck's 99-steerpoint/8-range scheme vs. ED's official
  127-steerpoint/7-partition scheme, with the datalink-markpoint numbering (71–80 vs. 500+) an outright
  disagreement, not just a precision difference; (2) the ADI glideslope-deviation scale — ED's official
  F-16 ADI figure of **2.5°/dot (±5° top/bottom)** vs. this file's pre-existing generic-ILS-standard
  figure of **±0.7°/0.14°-per-dot** sourced from pilotscafe/code7700/PPRuNe — an ~18× discrepancy left
  explicitly unresolved (both plausible readings discussed, neither confirmed from a primary F-16
  flight manual; ED's number kept primary for the cockpit-instrument visual, the industry-standard
  figure kept as the best available guess for the underlying beam-angle physics until a better source
  turns up).
- **hud-symbology.md**: added the full ED HUD-chapter element list with exact scale numbers (velocity
  60–900 kt/50-10 kt ticks, altitude 500/100 ft ticks, heading 10°/5° ticks, attitude bars 5°/10°,
  G ±9.9, roll/bank-angle indicator mark sets, Manual Bombing Reticle mil dimensions), the full
  Slant-Range source-letter table (B/R/F/L/M — Chuck's file only had "B"), the Master Mode Status HUD
  text-tag list (useful as a literal string lookup table for a HUD-mode-label implementation), and the
  exact HUD Control Panel per-position switch behavior. **Resolves a previously-open confidence gap**:
  TFOV is now an **official, high-confidence 25° diameter, extending 10.5° below field-of-view center**
  (was "typ. ~20–25°, not firmly public" in the Technical-depth section — that note is now updated to
  point here). Added the required **"Was der Pilot wirklich sieht"** section (task mandate) — a
  two-table checklist (HUD-displayed quantities with their exact resolution/format vs. DED/EHSI/
  instrument-panel-displayed quantities) plus an explicit architectural note: our `FBF16Pilot`'s
  AoA-based approach law targets a **numeric** AoA value, which the real HUD does **not** display
  (only the bracket cue) — the numeric source is the separate analog AoA indicator. Documented as a
  deliberate, now-explicit simplification, not a silently-assumed one.
- **cockpit-displays.md**: partial pass — added the AoA Indicator's exact band edges (**11.1°/13.9°**,
  refining `hud-symbology.md`'s AOA-indexer table, cross-linked both ways), VVI scale (±6,000 fpm,
  500/100 fpm ticks — finer than the HUD's own VV scale), the full ADI instrument (attitude sphere,
  bank-angle scales, slip ball, rate-of-turn indicator with the "standard rate 3°/s" convention), and
  — the single highest-value addition — the **full Caution Light Panel trigger-condition table** (ED
  gives exact physical trigger conditions Chuck's guide only lists as light *names*: ANTI SKID >5 kt
  groundspeed, CABIN PRESS >27,000 ft, FWD/AFT FUEL LOW <400/<250 lb, OBOGS <10 PSI, etc.) — flagged as
  a ready-made trigger table for a future caution/warning-light simulation layer, currently out of
  `FBFlightMonitor`'s scope. **Explicitly left unprocessed this pass** (flagged, not silently skipped):
  Left/Right Auxiliary Console, Left/Right Console subsections, and — the biggest remaining gap for
  this file specifically — the **Upfront Controls (UFC/ICP/DED) chapter (p.97–120)** and the
  **Multi-Function Displays (MFD) chapter (p.121–127)**. These are exactly the pages this round's task
  named as priority 3 that didn't get processed; next pass should start here.

### On the "165 kt / 11° AoA" measurement question (explicit task ask)
Neither Chuck's guide nor the ED EA Guide states an approach CAS by weight anywhere in the Procedures
or Navigation chapters processed this pass. Both sources are consistently **AoA-referenced**: the
FLCS's takeoff/landing gain set is a pitch-rate/AoA command law (`flight-controls-flcs.md`), and the
guide's own approach doctrine is "control AoA with throttle, not pitch trim." **Nothing found this pass
confirms or contradicts our FBF16Pilot's measured 165 kt on-speed CAS at 11° AoA** — it remains a
JSBSim-model measurement (CLAUDE.md Prinzip 5), not a documentation fact, exactly as flagged before this
pass. What ED **does** newly support is broadening the AoA target from a flat 11° to an **11–13° range**
for the base-turn-through-rollout segments (11° remains the downwind-trim target) — see
`procedures-landing.md`'s ED addendum for the phase-by-phase table. This is a legitimate candidate
change for `FBF16Pilot`'s AoA setpoint schedule (tighten toward 13° approaching flare rather than a
constant 11°), though the 165 kt CAS-at-11°-AoA figure itself is unaffected since the model relationship
between CAS and AoA is JSBSim's, not the guide's.

### Concrete numbers this pass gives that `FBF16Pilot`/`FBAutopilot` could use directly
- AoA target band 11–13° (not flat 11°) through base/final/rollout, downwind stays 11° (`procedures-landing.md`).
- NWS gain should decrease with groundspeed (`procedures-takeoff-taxi.md`) — currently unquantified
  beyond "proportional", a JSBSim-model tuning question, not a book-value one.
- Overhead-break commanded G: **3 G** (ED, supersedes the 3–4 G range) if a break-turn AI needs a single number.
- Glideslope-intercept-altitude and glideslope-descent-rate tables (`navigation-ils.md`) — directly
  computable guidance-law inputs for a future ILS-coupled `FBAutopilot::Direct` approach mode.
- CARA ALOW = Decision Height as the missed-approach trigger mechanism (`navigation-ils.md`/
  `procedures-landing.md`) — reuses the already-simulated ALOW system rather than needing a new one.

### COVERAGE (Pass 3): PARTIAL
Procedures (p.132–162) and Navigation (p.163–246) chapters: **fully processed**, per this round's
priority 1–2. HUD chapter (p.89–96): **fully processed**, per priority 3. Cockpit Overview/Instrument
Panel (p.43–81): **partially processed** (analog gauges + Caution Light Panel only). **Not processed
this pass**: Hands-On Controls p.82–88 (ED cross-check of the already-FULL `hotas.md`), Upfront
Controls p.97–120, Multi-Function Displays p.121–127, Data Transfer Equipment p.126, Radio
Communications p.247–260 (unchanged from Pass 2's "low priority" call), HTS/HMCS p.492–523 (unchanged
gap), IFF procedure detail (unchanged gap), all Appendices except the already-folded Appendix F
formulas. `air-refueling.md` **was** additionally updated (ED's Aerial Refueling chapter, p.157–162,
was extracted alongside Procedures in the same bounded pdftotext range and folded in: pre/post-
refueling emitter checklists, director-light color coding, the weight-effect trim note, and the formal
breakaway procedure) — a small bonus beyond this round's stated priority list since the text was
already in hand.

---

## Pass 4 — ED EA Guide, Bedienlogik (UFC/ICP/DED, MFD, HOTAS cross-check + command list)

Goal per this round's task: close the single biggest gap flagged at the end of Pass 3 — the
**Upfront Controls (UFC/ICP/DED, p.97–120)** and **Multi-Function Displays (MFD, p.121–127)** chapters
— because FlightBox's next build phase is an **avionics command-block model** (typed output blocks +
typed command blocks with ack/rejection, the 1553-bus idea without addresses) that the pilot-KI must
drive through the **same command path a human uses**, no magic state jumps. `pdftotext -f <first> -l
<last>` (same bounded-range method as prior passes) confirmed clean on all pages extracted this pass —
no visual/image-based pages needed; both chapters are real text+table layout, well-suited to the
existing extraction method.

### Pages processed this pass
| ED chapter | Pages | Status | Target file(s) |
|---|---|---|---|
| **Upfront Controls (UFC/ICP/DED)** | 97–120 | **FULL** — entire chapter extracted and distilled: ICP's 14 physical controls, the DED's generic propose/commit/reject edit protocol, the full page map (Priority/LIST/MISC), and field-by-field tables for CNI/ALOW/CRUS(×4)/TIME/BNGO/MAN/MODE pages | `cockpit-displays.md` (new "ED EA Guide addendum — ICP & DED" section) |
| **Multi-Function Displays (MFD)** | 121–127 | **FULL** — OSB numbering (incl. the non-obvious bottom-row-reversed quirk), GAIN/SYM/BRT/CON rockers (+ GAIN's format/mode-dependent meaning), the full 14-format Format Selection Master Menu + reassignment procedure + uniqueness/eviction rule, Swap/Declutter buttons, full DTE format field table (14 upload partitions incl. the CMDS-STBY precondition gotcha) | `cockpit-displays.md` (new "ED EA Guide addendum — MFD" section) |
| **Hands-On Controls (HOTAS)** | 82–88 | **FULL cross-check** of the already-FULL Chuck-sourced `hotas.md` — no contradiction found; added the exact SOI-dependent TMS/DMS/CMS/EXP-FOV action matrices, 0.5 s/1.0 s press-duration thresholds, MSL STEP's 3-context table, and quantitative facts Chuck's table omitted entirely (speedbrake 60°/43° deflection limits, RET DEPR 0–260 mrad range, DOG FIGHT switch's "overrides everything except Emergency Jettison" precedence) | `hotas.md` (new "ED EA Guide addendum" section) |
| MARK DED page (priority key 7) | — | **not found** within p.97–120 despite the chapter's own cross-reference table implying it should be there | flagged TODO in `cockpit-displays.md`; likely lives in ED's Navigation chapter (p.163–246), not independently re-checked this pass |
| Left/Right Auxiliary Console, Left/Right Console (Cockpit Overview p.43–81 remainder) | — | still not processed (unchanged from Pass 3) | `cockpit-displays.md` (unchanged gap) |
| Appendix D (HOTAS quick-ref diagrams) | 686–692 | still not processed (redundant with per-chapter HOTAS notes already folded in, per Pass-2 call) | — (unchanged) |

### New file: `controls-commands.md`
Per this round's "besonderer Auftrag": a compact, machine-readable list of **every discrete pilot
command**, re-cut across ICP/DED, MFD, HOTAS (SSC+throttle), and autopilot mode switches — one row per
command with trigger/precondition/effect/feedback/failure columns. Chosen as a **new, separate file**
(not folded into `cockpit-displays.md` or `hotas.md`) because it reorganizes by *command* across all
physical panels, which is the shape the avionics command-block model needs, vs. the source files'
by-panel organization — reasoning stated explicitly in the file's own header. Contents:
- The DED's **propose → commit/reject** cycle documented as the canonical, reusable command pattern
  (select field → type/toggle → ENTR commits / RCL or RTN rejects) — directly maps onto "Kommandoblöcke
  mit Quittierung und Ablehnungsgründen."
- ~45 discrete commands tabulated across 4 sections (ICP/DED navigation + field-edit + page-specific +
  hardware; MFD; HOTAS SSC + throttle; autopilot mode switches).
- §5 **derives** (not sourced — explicitly marked `abgeleitet`) a HOTAS-vs-DED command-latency-class
  split from the only two quantitative timing facts either guide gives (0.5 s / 1.0 s press-duration
  discriminators): HOTAS commands are sub-second and usable while maneuvering, DED commands are
  multi-second/head-down/administrative. No book source states a numeric command-rate figure directly.
- §6 collects **every** documented precondition/rejection pattern found across both source guides (8
  distinct cases, from explicit pilot-reject to hardware-precedence-lockout to silent-no-op to
  value-clamp) — this is the closest thing to an "Ablehnungsgründe" taxonomy the source material
  supports, and is flagged as the direct design input for the command-block architecture's reject-reason
  enum.
- Explicitly flags the biggest genuine gap found: **neither guide documents per-field numeric
  range-validation** for any DED field — every documented "failure" is a pilot self-reject or an
  unrelated-control precondition, never a bounds-check on the field itself. A FlightBox command-block
  model will need its own validation policy here; the source material gives no number to copy.

### Discrepancies found this pass
**None.** The Hands-On Controls cross-check confirmed Chuck's `hotas.md` table is accurate but coarse
— ED adds precision (SOI matrices, timing thresholds, deflection angles) without contradicting anything
already stated. No numeric or logical conflicts found in the ICP/DED or MFD material either (both are
first-pass extractions from ED only, nothing in Chuck's guide covered this depth to conflict with).

### What this pass directly gives the command-block architecture
- The propose/commit/reject cycle (§ above) as the literal reference pattern for a command block's
  ack/reject semantics.
- A concrete, sourced example of **effect-side vs. command-side** precondition failure (CARA ALOW: the
  *set-threshold* command always succeeds; the *warning* only fires if the radar altimeter is powered) —
  a useful distinction for whether "reject" belongs on the command or on a downstream system state.
- A concrete example of "gear-down freezes value, doesn't invalidate it" (CRUS page computed fields) —
  a design note for how the output-block "Gültigkeitsflag" should behave under a similar staleness
  condition (hold-last-value vs. flag-invalid are different design choices; the real jet does the
  former).
- The DOG FIGHT throttle switch's explicit "overrides every mode except Emergency Jettison" precedence
  statement — the closest thing in the whole source set to a formal command-priority ordering, useful if
  the command-block model needs one.

### COVERAGE (Pass 4): the pages this round's task named as priority 1–3 are all processed
Priority 1 (UFC/ICP/DED, p.97–120): **FULL**. Priority 2 (MFD, p.121–127): **FULL**. Priority 3
(Hands-On Controls cross-check, p.82–88): **FULL**. Priority 4 ("wenn Zeit bleibt" — remaining
Aux/Left/Right Console subsections, IFF procedure detail) was **not reached** this pass (bounded-run
discipline — the priority-1/2/3 material plus the command-list deliverable filled the run). The MARK
DED page gap (above) is new information from this pass, not a pre-existing one. Everything else
unprocessed from Pass 2/3 (Radio Communications p.247–260, HTS/HMCS p.492–523, IFF procedure detail,
Appendices, Aux/Left/Right Console) remains **unchanged** — see Pass 2/3 sections above for the full
ledger, still current.

---

## Pass 5 — schema alignment onto the four-section form (roadmap R10, part C)

**No source pages were read this pass.** This was a pure reorganisation: every topic file was moved
onto the same `## Spec` / `## State` / `## Gaps` / `## Knowledge` form that `doc/flightbox/` uses, so
that a reader can hold reference and implementation side by side without learning two layouts.

### What the four sections mean in a REFERENCE base (owner-approved reading)

| Section | Content |
|---|---|
| `## Spec` | what the real jet documentably does — the existing guide distillation + ED addenda, unchanged. The bulk of every file. |
| `## State` | what FlightBox implements of it — brief, table-shaped, linking into `doc/flightbox/`; never a copy of that content. **Newly written this pass**, established by reading `doc/flightbox/aircraft/f16.md`, `sim/sensors.md`, `sim/systems.md`, `sim/weapons-and-damage.md`, `sim/pilot-ai.md`, `sim/fdm.md`, `aircraft/stores.md`, `clients/clients.md`, `journal.md` and `roadmap.md` — not guessed. |
| `## Gaps` | both kinds, each explicitly labelled: **source gaps** (SHALLOW research passes, unprocessed pages, unresolved Chuck/ED discrepancies — the existing declarations, moved or referenced in place) and **implementation gaps** (modelled / partially / not at all). |
| `## Knowledge` | the researched engineering depth — the former `Technical depth (researched …)` sections, moved under the heading with their marker line and sources intact. |

### Method / discipline

- **Reorganisation, not revision.** Existing body text moved 1:1; headings were demoted one level so
  the four sections really contain their material. Verified mechanically: a word-multiset comparison
  of every file against its pre-pass version shows **no lost content** — only `---` separators at the
  seams and the `Technical depth` heading, whose exact wording is preserved as a bold marker line
  directly under `## Knowledge`.
- **Section numbering left alone**, because code banners cite it (`weapons.md` §4.1/§4.5,
  `defence-rwr-cm.md` §2.1/§2.2, `controls-commands.md` §6.4/§6.6). Numbered gap sections
  (`weapons.md` §4.7, `defence-rwr-cm.md` §4.3) therefore stayed **in place** under Knowledge and are
  referenced from `## Gaps` instead of being moved. `controls-commands.md` §5 (explicitly `abgeleitet`,
  not sourced) moved to Knowledge but kept its number; `cockpit-displays.md`'s "Remaining gap (this
  file, cumulative)" block moved verbatim into `## Gaps`.
- **Discrepancies stay unresolved**: overhead-break G (Chuck 3–4 G vs. ED ~3 G), autopilot bank limit
  (45° vs. ±30°), steerpoint count (99 vs. 127), ADI glideslope scale (0.7° vs. 2.5°/dot), gun drum
  (510 vs. 512). All still carry both values and their flags.
- `flight-model.md` inverted, per its subject: `Spec` is one line (the pinned model as flown plus the
  declared deltas → `sim/assets/MODEL-DELTAS.md`), §1–§10 are `State`, §12 is `Gaps`, §11 (the handover
  checklist) is `Knowledge`.
- `INDEX.md` and `PROGRESS.md` are meta files and carry no schema. INDEX gained a statement of the
  schema plus a **coverage-at-a-glance table** (one line per file, from its `State`).

### Files raised this pass
All 17 topic files (`aerodynamics-performance`, `air-refueling`, `cockpit-displays`,
`controls-commands`, `datalink-iff`, `defence-rwr-cm`, `engine-fuel`, `flight-controls-flcs`,
`flight-model`, `hotas`, `hud-symbology`, `navigation-ils`, `procedures-landing`, `procedures-startup`,
`procedures-takeoff-taxi`, `radar-sensors`, `weapons`) + `INDEX.md` +
`.claude/skills/f16-systems/SKILL.md` (states what the four sections mean here).

### COVERAGE (Pass 5): COMPLETE for its own scope
Every topic file carries the four sections; every file's State is backed by a named `doc/flightbox/`
file. **Source coverage is unchanged by this pass** — the unprocessed-page ledger of Passes 2–4 stands
exactly as written above (Radio Communications p.247–260, HTS/HMCS p.492–523, IFF procedure detail,
Aux/Left/Right Console remainder of p.43–81, Appendices A/C/D/E, the MARK DED page, and the priority-4
"check every file against ED" sweep). Nothing was distilled and nothing was closed this pass.
