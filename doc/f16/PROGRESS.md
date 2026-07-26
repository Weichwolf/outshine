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

## COVERAGE: PARTIAL — see "Priority-4 sweep" above for exactly what's left
Pass-1 (Chuck's Guide) coverage remains COMPLETE. Pass-2 (ED EA Guide) coverage is complete for its
stated priority order (weapons.md → radar-sensors.md → defence-rwr-cm.md → datalink-iff.md → one
flight-controls-flcs.md addendum) but explicitly NOT complete for the broader "check every file
against ED" sweep, nor for the ED chapters listed as "not processed this pass" in the page map above
(Cockpit/HUD/UFC/MFD, Procedures, Navigation, Radio, HTS/HMCS detail, IFF procedure detail, Appendix
C threat tables, the full Appendix B ALIC/RWR table transcription).
