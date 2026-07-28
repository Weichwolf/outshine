# doc/mig29 — Source coverage ledger

Updated every run. Structure: **source part / page range → file → status**.
Status values: **DONE** (distilled to the declared depth) · **PARTIAL** (touched, gaps named in the
file's "Open gaps" section) · **OPEN** (not processed) · **N/A** (no rebuild value) ·
**OTHER-AGENT** (owned by the parallel `weapons.md` / `flight-model-spec.md` agent).

**Page-number convention** — restated here because it is easy to get wrong:
- **DCS-EA** (`DCS MiG-29A Early Access Manual EN.pdf`, 115 pp): **printed page == PDF page**.
- **DCS-FM** (`DCS MIG-29 Flight Manual EN.pdf`, 116 pp): **printed page = PDF page − 6**
  (front matter is roman-numbered). **Every file in this directory uses the printed page.**

> ✅ **Convention split RESOLVED (pass 2).** `weapons.md` and `flight-model-spec.md` were the two
> files written on the PDF-page convention and have been converted to printed pages. The conversion
> was **not** a blanket −6: each citation was matched against the extracted text of both candidate
> pages, because both files turned out to be *internally mixed*.
>
> | File | Tagged `DCS-FM p.N` citations | Converted (were PDF-page) | Left alone (already printed) |
> |---|---|---|---|
> | `weapons.md` | 86 | **45** | 41 |
> | `flight-model-spec.md` | 45 | **43** | 2 |
>
> Plus 4 bare `p.N` follow-on refs and the page lists in both file headers and both `Sources`
> sections. In `weapons.md` the split ran by section: R-27 (§3.1–3.2), R-60M (§3.4), the DLZ concept
> (§3.5) and the cluster-bomb/KMGU rows (§5.1) were already printed, while the gun (§4), R-73 (§3.3),
> FAB/BetAB, the rockets (§5.2) and the TTD-table rows were PDF — so `p.72` and `p.78` could denote
> the *same* physical page. In `flight-model-spec.md` only two citations were already printed
> (`p.11`, the upper-intake 200 km/h limit; `p.68–69`, the R-27E Su-27 attribution).
>
> **Verification:** every remaining `DCS-FM p.N` in all 12 files was re-scored against the extracted
> page text of PDF page *N* and PDF page *N+6*. Result across the directory: **125 citations resolve
> to the printed reading, 0 to the PDF reading**; the remainder carry no distinctive token to score
> (short table cells) and are covered by their section's neighbours. DCS-EA is unaffected (printed ==
> PDF) and untouched.

---

## Pass 1 — 2026-07-27 (this run)

Scope: the **systems** half of the knowledge base. `weapons.md` and `flight-model-spec.md` were written
in parallel by a second agent and are **not** covered by this ledger.

### DCS-EA — `DCS MiG-29A Early Access Manual EN.pdf`

| Printed pp. | Chapter | File | Status |
|---|---|---|---|
| 1–6 | Front matter, table of contents | — | N/A |
| 7–11 | Cockpit overview, front panel layout, panel-group philosophy, colour code | `cockpit-displays.md` §intro | DONE |
| 12–13 | **PSR-31 weapon control panel** (MASTER ARM, ZONE, IR GAIN, PREPARE, ALL/SINGLE, **SPAN**, WCS MODES) | `radar-sensors.md` §2.2 | DONE |
| 13–14 | IAS indicator USM-2AE, altimeter UV-30-2 | `cockpit-displays.md` §1 | DONE |
| 15–18 | IP-52-03, ADI KPP-SI, HSI PNP-72-12, course setting, master caution | `cockpit-displays.md` §1, §3.2 | DONE |
| 19 | **Combined AoA / g meter** (15°/25° markers, 7 g red mark) | `flight-controls.md` §4; `cockpit-displays.md` §1 | DONE |
| 20–23 | DA-200P, TAS indicator, clock AChS-1M, nose-wheel brake handle, ADF outer/inner switch | `cockpit-displays.md` §1; `navigation.md` §3.1 | DONE |
| 24 | **Countermeasures dispenser control panel** (GROUND/FHS/RHS, jettison, counter) | `defence-rwr-cm.md` §3 | DONE |
| 24–25 | **Radar altimeter indicator** (0–3,000 ft, threshold bug, flag, test) | `navigation.md` §3.2 | DONE |
| 25–27 | **ITE-2TB tachometer, ISTR4 fuel/range, ITG-1 EGT** | `engines-fuel.md` §2, §4.2 | DONE |
| 28–30 | Oxygen indicator, IKG-1 pressures, ramp position indicator, KI-13 compass | `cockpit-displays.md` §1; `engines-fuel.md` §2–3 | DONE |
| 31–32 | **AEKRAN** monitoring/warning system, all three modes | `cockpit-displays.md` §3.3 | DONE |
| 33–34 | **SPO-15LM indicator + control panel** | `defence-rwr-cm.md` §2.3 | DONE |
| 35–37 | Voltmeter, pitot selector, M-2A brake gauge, cockpit temp, PC-31 (N/I), emergency gear handle | `cockpit-displays.md` §1 | DONE |
| 38–39 | Right console inventory (21 items) | `cockpit-displays.md` §4 | DONE |
| 40 | **TLP annunciator behaviour** | `cockpit-displays.md` §3.1 | DONE |
| 41–42 | **A-323 navigation control panel** (20 controls, 9-point store) | `navigation.md` §2 | DONE |
| 43 | **VIWAS**, AM/FM switch | `cockpit-displays.md` §3.4, §6 | DONE |
| 44–47 | Lighting panels, **ADF panel**, air conditioning, heating, electrical power panel | `cockpit-displays.md` §4; `navigation.md` §3.1 | DONE |
| 48–49 | **IFF controls / ID index coder / guidance panel** (all "not implemented") | `datalink-gci.md` §1 | DONE |
| 50 | **Engine start control panel** | `engines-fuel.md` §5.1 | DONE |
| 51–52 | System power panel, test/check panel, **left console inventory** | `cockpit-displays.md` §4–5 | DONE |
| 53–55 | Oxygen system, decompression, suit ventilation, throttle friction, **feel unit control panel** | `cockpit-displays.md` §5; `flight-controls.md` §3 | DONE |
| 56 | **R-862 radio** (bands, presets, ranges vs altitude, 45° bank shading) | `cockpit-displays.md` §6 | DONE |
| 57 | **Flap/slat control logic + speedbrake logic** | `flight-controls.md` §7 | DONE |
| 58 | Emergency control panel (all N/A in DCS) | `engines-fuel.md` §4.4 | DONE |
| 59–60 | **PU-S31 weapon panel** (incl. the **IFF FOE/FRIEND lock gate**), chute release + **mandatory-chute rules** | `datalink-gci.md` §1, §3; `procedures.md` §7.3 | DONE (weapon-release aspects → OTHER-AGENT) |
| 60–62 | External stores selector, canopy, gear lever, lights, emergency missile launch | `cockpit-displays.md` §5 | DONE |
| 63 | **PUR-31 radar control panel** (modes, ILLUM/DUMMY/OFF, ECCM, TWF FHS/RHS) | `radar-sensors.md` §2.1 | DONE |
| 64 | **AFCS control panel** (6 modes) | `flight-controls.md` §5.1 | DONE |
| 65–67 | **Unified Indication System: HUD FOV, HDD, adjustment controls, base symbol set** | `cockpit-displays.md` §2 | DONE (geometry: **gap**) |
| 68–69 | **Stick and throttle HOTAS inventory** | `cockpit-displays.md` §7 | DONE |
| 71–72 | **Preflight interior check** (25 steps) | `procedures.md` §2 | DONE |
| 72–73 | **Engine start** + monitored parameters | `engines-fuel.md` §5.1; `procedures.md` §3 | DONE |
| 73–75 | **Post-engine-start** (17 steps, incl. AFCS BIT and **COC** check) | `procedures.md` §4; `flight-controls.md` §5.2, §4 | DONE |
| 75–76 | **Taxi** | `procedures.md` §5 | DONE |
| 76–78 | **Normal / afterburner / crosswind takeoff** | `procedures.md` §6 | DONE |
| 79–80 | **Normal / crosswind landing + pattern speed diagram** | `procedures.md` §7 | DONE |
| 81–84 | **Navigation: options, point-to-point, RETURN, LANDING, MISSED APPROACH** | `navigation.md` §4 | DONE |
| 86–97 | Air-to-air employment: RAD/SCAN/LOCK/CC, IR, IR CC, HELM, OPT, BS, RETICLE, gun | `radar-sensors.md` §3, §5, §6 (sensor logic) | DONE for sensors; **weapon employment → OTHER-AGENT** |
| 98–104 | Air-to-ground: OPT/BS/TOSS/RETICLE, rockets, gun, bombs | — | **OTHER-AGENT** (`weapons.md`) |
| 105–112 | **SPO-15LM full chapter** + countermeasure dispensers | `defence-rwr-cm.md` §2–3 | DONE |
| 113–115 | Abbreviations | — | N/A (used as lookup) |

### DCS-FM — `DCS MIG-29 Flight Manual EN.pdf`

| Printed pp. | Chapter | File | Status |
|---|---|---|---|
| i–vi, 1–8 | Front matter, MiG-29 history | — | N/A |
| 9–11 | **Aircraft construction**: surface areas and deflections, LEX/wing geometry, gear, intakes | `flight-controls.md` §2; `engines-fuel.md` §3 | DONE |
| 12–13 | **Avionics inventory**: SUV-29, RLPK-29/N019, OEPrNK-29, SUO-29, SN-29, "Birjuza", SPO-15, BVP-30-26, SPU-9 | `radar-sensors.md` §1; `navigation.md` §6.3; `defence-rwr-cm.md` §3; `datalink-gci.md` §1 | DONE |
| 14 | **Tactical/technical characteristics table** (9-12 vs 9-13) | `engines-fuel.md` §1, §6.3; `flight-controls.md` §6.4 | DONE |
| 15–19 | Game Avionics Mode | — | N/A |
| 21–29 | Cockpit instruments (central panel list, IAS, altimeters, mech-devices, AoA/g, ADI, HSI, VVI, clock, tachometer, fuel, EGT) | `cockpit-displays.md` §1 | DONE |
| 30–32 | **Radar Warning System + SPO-15 (FC3 version)** | `defence-rwr-cm.md` §2.10, §2.4 (conflict flagged) | DONE |
| 33 | **Trim mechanism** (authorities per axis, neutral annunciators) | `flight-controls.md` §5.3 | DONE |
| 34–36 | **AFCS SAU-451-02**: every mode with engage conditions, BIT, RESET | `flight-controls.md` §5 | DONE |
| 37–39 | **Sighting systems**: radar principles, Doppler/notch/inertial tracking, TWS caveats, OEPrNK-29/IRST passivity | `radar-sensors.md` §4, §6.4 | DONE |
| 40–42 | **HUD basic symbols + navigation sub-modes** | `cockpit-displays.md` §2.2; `navigation.md` §4.6 | DONE |
| 43–53 | **BVR modes**: SCAN, TWS, TWS2, STT, IRST scan, ECM/AOJ/burn-through | `radar-sensors.md` §4 | DONE |
| 54–57 | **Close-combat modes**: Vertical Scan, BORE, HELMET, Fi0 | `radar-sensors.md` §5 | DONE |
| 58–59 | Gun employment (LCOS, gun funnel) | `radar-sensors.md` §5 (sight modes only) | **OTHER-AGENT** for ballistics/ammo |
| 60–61 | Air-to-ground mode, reticle | `cockpit-displays.md` §2.3 (reticle) | **OTHER-AGENT** |
| 62 | **ECM station "Gardeniya"** (MiG-29S) | `defence-rwr-cm.md` §4 | DONE |
| 63–80 | MiG-29 weapons (missiles, bombs, rockets) | — | **OTHER-AGENT** |
| 81–82 | **Engine ground start / shutdown / in-flight restart** | `engines-fuel.md` §5 | DONE |
| 83–92 | Weapons delivery procedures (BVR, CAC, A-G) | `radar-sensors.md` §4–5 (sensor steps only) | **OTHER-AGENT** |
| 94–103 | Radio commands (wingman, AWACS/GCI, ATC) | `datalink-gci.md` §2.1 (AWACS/GCI only) | PARTIAL — wingman/ATC menus are DCS UX, N/A |
| 104–105 | **Voice messages and warnings** | `cockpit-displays.md` §3.5; `flight-controls.md` §4 | DONE |
| 107–111 | Acronym list | — | N/A |

---

## Research performed this pass (all recorded in per-file "Technical depth" sections)

| Topic | Outcome | Tier | Landed in |
|---|---|---|---|
| Hydraulic architecture (NP-103A, 207 bar, 80 L, chamber split, NS-58) | **Found, good detail** | T3 (Jane's-derived mirror) | `flight-controls.md` §6.1 |
| Boosters RP-260A/RP-280/RP-270, gear steering angles, per-tank fuel capacities | **Found** | T4 (`military.wikireading.ru`) | `flight-controls.md` §6.1; `engines-fuel.md` §4.1 |
| **ARU-29-2** function and composition | **Found (qualitative)**; **schedule NOT found** | T4 | `flight-controls.md` §3, §6.2 |
| **SOS-3M** 26° trip, stick pusher, LEF control, override, threshold history | **Found** | T4 ×3 independent | `flight-controls.md` §4, §6.3 |
| RD-33 cycle data, SFC, TIT, mass | **Found**, with a **conflicting SFC/thrust set** | T4 (Klimov-derived) | `engines-fuel.md` §6.1 |
| RD-33 spool times / AB limits / thrust lapse | **NOT FOUND** | — | `engines-fuel.md` §7 |
| **N019** ranges vs RCS+altitude, scan geometry, lock times, processor | **Found, the richest research result of the pass** | T4 (toad-design) | `radar-sensors.md` §7.1 |
| **KOLS/OEPS-29** ranges, laser range 6 km, FOV | **Found** | T4 | `radar-sensors.md` §7.2 |
| BVP-30-26 / PPI-26-1V quantities | **Found**, corroborates DCS-FM | T4 | `defence-rwr-cm.md` §6.2 |
| CM programme parameters (burst/salvo/interval) | **NOT FOUND** | — | `defence-rwr-cm.md` §7 |
| RSBN-4N / PRMG-4 ground segment | **Found (partial)**; **glide-path angle NOT found** | T4 | `navigation.md` §6 |
| **Lazur / Biryuza / E502-20** GCI datalink | **Found (qualitative + component list)**; message set NOT found | T4 | `datalink-gci.md` §4 |
| Vozdukh-1M / Rubezh / ALM-1 / ALM-4 | **Found (naming + which variant)** | T4 | `datalink-gci.md` §4.2 |
| Approach speeds vs weight | **NOT FOUND** — no T1–T3 performance manual reachable | — | `procedures.md` §9 gap 1 |
| Takeoff/landing distances | **NOT FOUND** | — | `procedures.md` §9 gap 2 |

**Blocked sources** (returned HTTP 403 to automated fetch; worth a manual pass):
- `secretprojects.co.uk` — threads *"Soviet GCI Command & Datalinks"*, *"MiG-29 Avionics"*,
  *"MiG-29 Supermanoeuvrability"*. **Highest-value single target for the next research pass** — the
  first would materially improve `datalink-gci.md`, the third `flight-controls.md` §4.
- `globalsecurity.org` MiG-29 design page.
- `ukr.bulletpicker.com` "Системы истребителя типа МиГ-29 (приложение)" — **8.3 MB scanned PDF, JBIG2
  images, no text layer**. Would need OCR; likely the best single T1-class systems document found.

---

## Cross-manual conflicts registered (never silently resolved)

| # | Topic | DCS-EA says | DCS-FM says | Recorded in |
|---|---|---|---|---|
| 1 | **SPO-15 threat-type letters** | П = CW-illuminator FCR (launch warning), З = AAA/SAM, Х = CW/HPRF-low, Н = strategic SAM, F = 4th-gen fighter, С = LPRF/other | П = airborne, З = long-range, Х = medium, Н = short, F = EW, С = AWACS | `defence-rwr-cm.md` §2.4 |
| 2 | **Waypoint sequencing** | **Manual** — press the next waypoint button | **Automatic** on reaching the point | `navigation.md` §1 |
| 3 | **Tachometer 100 %** | scale 0–110 %, idle 58–72 % | "full afterburner is shown as 100 %" | `engines-fuel.md` §2 |
| 4 | **Radar altimeter range** | 0–3,000 ft | 0–1,000 m, plus a bank-accuracy caveat | `navigation.md` §3.2 |
| 5 | **N019 gimbal limits** | (not stated) | ±67° az, +60/−38° el | vs research ±65°, +56/−36° — `radar-sensors.md` §7.1 |
| 6 | **Cockpit g red-line vs spec g-limit** | 7 g red mark | 9 g maximum operational | `flight-controls.md` §4 |

---

## What remains OPEN after this pass

**Nothing in the two source PDFs remains unprocessed within this agent's scope.** Every page is either
DONE, N/A, or explicitly OTHER-AGENT. What remains is **research**, not extraction:

1. **`flight-controls.md`** — ARU gear-ratio schedule; stick-force gradients; SAU-451 damper authority;
   rudder deflection limit; roll-mixing law. *(Blocks a faithful pitch/roll response model.)*
2. **`engines-fuel.md`** — spool times; AB time limits; thrust lapse tables; EGT numeric limits; relight
   envelope; fuel-flow rates. *(Blocks a faithful engine model; spool time is the worst of these.)*
3. **`procedures.md`** — any weight/OAT/altitude performance schedule at all; ground-roll distances;
   abort/go speeds; gear/flap limit speeds; emergency procedures. *(The task explicitly asked for
   approach speeds by weight — **the sources do not contain them**, and nothing was invented.)*
4. **`cockpit-displays.md`** — **HUD symbol geometry** (positions/sizes/spacings). The declared biggest
   gap in the set; mitigation path is measurement against the stated 13°×18° IFOV / 24° circular TFOV.
   Also: full TLP lamp inventory and the AEKRAN message catalogue.
5. **`defence-rwr-cm.md`** — CM programme parameters; chaff/flare split of the 60 cartridges.
6. **`radar-sensors.md`** — scan-bar patterns and frame times; track-firm criteria; KOLS raster.
7. **`datalink-gci.md`** — the whole Lazur message set; IFF system designation for the 9-12.
8. ~~**Citation-convention reconciliation** across the directory.~~ **CLOSED in pass 2** — see the ✅
   note at the top of this file. A DCS-FM page number can now be trusted without checking which file
   it came from.
9. **A loader skill** (`mig29-systems`, pattern `f16-systems`; both merged into the single
   `.claude/skills/flightbox/SKILL.md` in the Phase-3 mirror rebuild) — created after
   pass 1; updated in pass 2 (citation-trap warning removed, schema described). It still declares
   coverage honestly PARTIAL, because items 1–7 are open.

---

## Pass 2 — schema alignment (roadmap R10, part D)

Scope: **no new extraction.** Two things only.

1. **Citation reconciliation** (item 8 above) — closed, table at the top of this file.
2. **Schema alignment** onto `## Spec` / `## State` / `## Gaps` / `## Knowledge`, the same form as
   `doc/modules/f16/` and `doc/`. Content moved **1:1**: every original section survives with its
   original number and body text; only the heading level dropped one (`## 3.` → `### 3.`), so all
   `§n.m` cross-references still resolve. Tags (`[DER]`, `[ABL]`, `[SET]`, `[GAP]`, `[ED-MODEL]`,
   confidence tiers) and the six conflict registrations are untouched.

| File | Spec | Gaps | Knowledge |
|---|---|---|---|
| `flight-controls.md` | §1–5, §7, §8 | §9 | §6 |
| `engines-fuel.md` | §1–5, §8 | §7 | §6 |
| `cockpit-displays.md` | §1–7, §10 | §9 | §8 |
| `radar-sensors.md` | §1–6, §9 | §8 | §7 |
| `defence-rwr-cm.md` | §1–5, §8 | §7 | §6 |
| `navigation.md` | §1–5, §8 | §7 | §6 |
| `procedures.md` | §1–7, §10 | §9 | §8 |
| `datalink-gci.md` | §1–3, §7 | §6 | §4, **§5** (the design argument — reasoning, not source) |
| `weapons.md` | §1–6 | §8 | §7, Sources |
| `flight-model-spec.md` | §0–§11 (thin frame, see below) | §12 | Sources + pointer to the in-row derivations |

- **Variant notes stay Spec** — they are documented aircraft facts about 9-13/S/SMT, not research.
- **`## State` is new in every file and says the same honest thing: nothing is built.** It links the
  spec-first module contract [`module.md`](module.md) and
  names which roadmap stage (R3 / R6 / R7 / R8) consumes which part of that file.
- **`flight-model-spec.md` got a deliberately THIN frame.** Its three-column layout (documented /
  derivation path / open+`[SET]`) *is already* a Spec+Gaps hybrid; exploding those rows into separate
  sections would destroy the structure that makes the file usable. So the anchor tables (§8) and the
  build order (§11) sit under Spec together with the rest of §0–§11, the derivation-path column stays
  inside each row, §12 becomes Gaps, and a schema note at the top of the file states this explicitly.
- `INDEX.md` and this ledger are meta files and are exempt from the schema; `INDEX.md` states the
  schema and the resolved citation convention.

---

## Provenance note
No file in `doc/modules/mig29/` contains a number that is not either (a) cited to DCS-EA/DCS-FM with a printed
page, (b) cited to a research source with a confidence tier, or (c) explicitly marked as an inference,
a conflict, or a gap. Nothing was extrapolated to fill a table.
