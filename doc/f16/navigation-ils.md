# F-16C Navigation & ILS

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 16 — Navigation & ILS Landing, pp. 673–777.
Plus `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — **NAVIGATION** chapter,
p.163–246 (INS, navigation database/steerpoints, TACAN, ILS) — see the "ED EA Guide addendum" section
below the guide distillation. Cite tags `Chuck p.NN` vs `ED EA Guide p.NN`.

## Spec

### Navigation sources
Navigation via **HSD** (Horizontal Situation Display), **EHSI**, **HUD**, and **ADI** localizer/glideslope
bars. Standby magnetic compass = backup. DED/ICP consult & edit nav data; FCR page also shows steerpoints.
**TACAN and ILS supported; NDB/ADF navigation is not.**

### Navigation point types
| Type | Purpose |
|---|---|
| **Steerpoints** (waypoints) | Pre-planned route reference points; create/edit/build flight plans |
| **Markpoints** | Mark a point of interest (overflown area, enemy sighting) |
| **Anchor Point / Bullseye** | Common geographic reference shared by friendly forces |
| **Reference points** | VIP, VRP, PUP, OAP (offset targeting) |

### Steerpoint database (99 total)
| # | Function |
|---|---|
| 1–24 | Navigation route / flight planning |
| 25 | Bullseye (auto-assigned) |
| 26–30 | Ownship markpoints |
| 31–54 | HSD lines (4 lines × up to 6 points) |
| 56–70 | Pre-planned threats |
| 71–80 | Datalink markpoints |
| 81–89 | Open (pilot use) |
| 90–99 | AGM-84 HARPOON (some blocks); open on Block 50 |

DED pages: **STPT** (ICP STPT/4, edits steerpoint, affects HSD) · **DEST** (LIST→1, edit without affecting
HSD; UTM→DIR→OA1/2 via Dobber SEQ) · **BULLSEYE** (LIST→0 MISC→8 BULL).

#### Steerpoint navigation (HUD)
- Align the **Steerpoint Tadpole with the FPM**; tadpole UP = ahead, DOWN = behind.
- **Steerpoint Diamond** points at steerpoint; crossed-out = out of HUD FoV.
- HUD shows distance (nm), TTG, direction. See `hud-symbology.md`.

### EHSI (Electronic Horizontal Situation Indicator)
Primary gauge for steerpoint + TACAN navigation (extra data not on HUD/DED; battle-damage backup).

Elements: current heading (lubber line), aircraft symbol, course pointer + setting, course deviation
scale/line (CDI), bearing pointer, heading bug, range indicator (nm), MRK BCN light.

**Mode Selector ("M" button)** cycles: **NAV** · **PLS/NAV** (ILS + nav) · **TCN** (TACAN) · **PLS/TCN**
(ILS + TACAN). Course knob OUT = set course; pressed IN = brightness.

### HSD (Horizontal Situation Display)
Plan-view tactical picture: ownship, steerpoints, flight plan, range rings, datalink contacts, threats.
- **Range rings**: outer = display range, middle = ⅔, inner = ⅓.
- **SOI** via DMS DOWN → cursor bearing/range from selected steerpoint (or bullseye if active) to cursor.
- **Expand/FOV**: NORM → EXP1 (2:1) → EXP2 (4:1); press-hold >½ s = Zoom (auto-scale to flight members,
  down to 5 nm).
- **CPL/DCPL**: couples HSD range to FCR range (one range change scales both).
- CNTL pages toggle overlays: AIFF, PRE, FCR ghost cursor, NAV1–3 routes, LINE1–4 map lines, RINGS,
  A/G TGTS, A SURV, G FRND, SAM (+ threat rings), LAR, SHIP, PDLT RNG. Datalink XMT: OFF / TNDL.

### TACAN
Directional + distance (airdromes, tankers, carriers; VORTAC = collocated VOR+TACAN).
1. MIDS LVT knob ON (TACAN is part of MIDS). 2. CNI page (Dobber LEFT/RTN). 3. ICP **T-ILS(1)** →
TACAN-ILS DED page. 4. Dobber DOWN to CHAN, keypad channel (e.g. 44), ENTR. 5. M-SEL(0)+ENTR toggles
band X/Y. 6. Dobber RIGHT (SEQ) to **TCN T/R**. EHSI mode → TCN.

### Bullseye (anchor point, default steerpoint 25)
- Bearing + range from bullseye to aircraft shown on HUD, FCR, HSD.
- Activate: LIST → 0 (MISC) → 8 (BULLS) → M-SEL(0) to toggle active.
- Reassign: BULL DED page → Dobber DOWN to BULL field → inc/dec or steerpoint number + ENTR.

### Reference points
- **VIP** (Visual Initial Point), **VRP** (Visual Reference Point), **PUP** (Pull-Up Point) — A-G attack geometry.
- **OAP** (Offset Aimpoint): a steerpoint offset by true bearing + range (ft) + separate elevation; up to
  two per steerpoint (OA1/OA2), stored on DEST OA1/OA2 pages, move with their parent steerpoint. Shown as
  a HUD triangle in A-G mode with CCRP + OA1 sighting on the TGP.

### INS drift & navigation fix
INS drifts over time. A **nav fix** re-aligns it by designating a known steerpoint's true location:
1. Master Mode NAV; CNI page; select steerpoint via DED inc/dec.
2. Designate with a sensor (TGP/FCR/HUD/OFLY). TGP method: SOI the TGP (DMS DOWN), slew reticle to the
   expected steerpoint location, TMS UP (point track). Laser ARM + trigger for ranging ("L" flashes on HUD).
3. ICP **8 (FIX)** → FIX DED page; Dobber RIGHT (SEQ) selects fix method. DELTA field = position drift.
4. Within **10 nm** of the steerpoint, TMS UP freezes DELTA → ENTR performs the fix (returns to CNI).

### ILS approach tutorial (example: Batumi RWY 13)
Example data: ILS freq **110.30**, runway heading **120 mag / 126 true**.

#### Tune
1. RADAR ALTIMETER — ON (FWD); adjust ILS audio.
2. CNI page (Dobber LEFT/RTN).
3. ICP **T-ILS(1)** → TACAN-ILS DED page.
4. Dobber DOWN to **ILS FRQ**, keypad "11030" → ENTR.
5. **CMD STRG** highlights when ILS signal received.
6. **CRS** field auto-selects → set course to runway heading (120) → ENTR.
7. EHSI mode "M" → **PLS/NAV** (slaves EHSI to ILS).
8. Verify **NAV** Master Mode on HUD (A-A/A-G ICP button reverts to NAV).

#### Fly
9. Align with runway using EHSI bearing pointer, CDI, ADI localizer bar, and **HUD localizer steering bar**.
10. Close enough → **Glide Slope Fail Flag** disappears; vertical guidance for a **3° glideslope**.
11. Fly to glideslope: center the **Glide Slope Steering Bar** + **Localizer Steering Bar** into a perfect
    cross on the FPM ("center the bars").
    - GS bar **above** FPM center = below glideslope → climb.
    - Localizer bar **right** of FPM = fly right to center.
12. Valid localizer → **Command Steering Symbol** (circle) on HUD; **tic mark** = pitch steering valid.
13. Localizer + glideslope captured → deploy landing gear → **"E" AoA bracket** appears.
14. LANDING light UP. 15. Deploy speedbrake. Then fly the approach at 11° AoA (see `procedures-landing.md`).

---

### ED EA Guide addendum — official navigation system detail (pp.163–246)

The ED EA Guide's Navigation chapter is architecturally much deeper than Chuck's tutorial-style guide:
it documents the **sensor inputs, solution-blending logic, and update mechanisms** behind the
steerpoint/TACAN/ILS *procedures* Chuck already covers. This section is organized by system, not by
page order.

#### Navigational sensors (ED p.164)
| Sensor | Feeds | Provides |
|---|---|---|
| Nose pitot probe | CADC + FLCC | Static/total pressure → analog Altimeter, Airspeed/Mach indicator |
| Side-mounted air data probe | CADC + FLCC | Static/total pressure, **AoA, sideslip** |
| AoA transmitters (both sides of nose) | FLCC | Proportional AoA |
| Static pressure ports (both sides of nose) | FLCC | Differential sideslip |
| Total temperature probe (right LEX underside) | CADC | True airspeed / air density |
| **CARA** (Combined Altitude Radar Altimeter) | ALOW, HUD | AGL |
| GPS antenna (top fuselage) | GPS receiver | Satellite position/timing |
| Upper/lower TACAN antennas | MIDS LVT | TACAN bearing/range |

#### Three navigation solutions + the GPS/INS blend threshold (ED p.165, 177)
The MMC (Modular Mission Computer) maintains **three separately-tracked navigation solutions**:
1. **INS-only** — pure inertial (accelerometers + gyros), drifts over time.
2. **GPS-only** — pure satellite position/timing.
3. **Blended** — Kalman filter combining both, used as the actual "system navigation solution" that
   drives HUD/HMCS/weapon-release symbology.

**Key mechanism**: GPS-derived corrections are only applied to the blended solution if the **delta
between INS-only and GPS-only exceeds 300 ft (91 m)**. Below that delta, the MMC leaves the INS-only
solution as the system solution unmodified — i.e. **GPS correction is a threshold-triggered snap, not a
continuous low-pass blend** below 300 ft of disagreement. If a manual position fix pushes the INS-only
solution more than 300 ft from the GPS-only solution, the **Kalman filter gradually removes the
manual update over time** until system accuracy returns to HIGH — manual fixes are not "sticky" once
GPS is available and disagrees.

**System-accuracy display** (`NAV STATUS` DED page, 3-state, not numeric — see also
`hud-symbology.md`'s "what the pilot actually sees"):
| Displayed state | System accuracy | GPS accuracy (separately shown) |
|---|---|---|
| HIGH | <300 ft | GPS tracking ≥4 satellites, <300 ft horizontal error |
| MED | 300–6,000 ft | — |
| LOW | >6,000 ft | GPS tracking <4 satellites or >300 ft error, or NO TRK |

**Rebuild implication**: a FlightBox INS model doesn't need Kalman-filter fidelity to be useful — the
threshold-snap behavior (apply GPS correction only when disagreement >300 ft; otherwise trust INS-only
and let it drift) is a much simpler state machine that reproduces the documented pilot-visible behavior
(HIGH/MED/LOW display + occasional visible "jumps" in steerpoint symbology when a correction snaps in).

#### INS alignment — see `procedures-startup.md`'s ED addendum
Full alignment-type/timing/status-CEP-scale table lives there (Normal Gyrocompass ~8 min, Stored
Heading ~90 s, In-Flight requires GPS or manual heading input; status 99→10 scale with CEP multiplier).
Not duplicated here — this file cross-references it because INS alignment is precisely the navigation
system's bootstrap step.

#### System Altitude (SALT) — source priority + ACAL thresholds (ED p.179–184)
The MMC's system altitude (used for HUD/HMCS symbology and weapon-release geometry) is derived from
one of three sources, **in ascending priority** (highest available wins):
1. **CADC altitude** (raw external air-pressure data) — fallback of last resort.
2. **INS-only altitude** (CADC-initialized, then integrated from INS vertical acceleration).
3. **Blended altitude** (INS-only + GPS or DTS/Terrain-Referenced-Navigation vertical correction).

**AUTO ACAL accuracy thresholds** (the vertical-position-accuracy gates that must be met before a
GPS/DTS correction is trusted into the blended altitude) — **note the master-mode-dependent tightening
in A-G mode**, directly relevant to weapon-delivery altitude accuracy:
| ACAL source | A-G master mode threshold | All other master modes |
|---|---|---|
| GPS | <50 ft | <100 ft |
| DTS (Terrain Referenced Nav) | <20 ft | <100 ft |

If neither source is within threshold, SALT reverts to INS-only; if INS-only itself is invalid
(powered off / not aligned / failed), SALT reverts to raw CADC altitude. **ACAL = MAN** disables
blended altitude entirely (SALT = INS-only always).

**DTS / Terrain-Referenced Navigation (TRN)** — the F-16's onboard DTED-based terrain-correlation
altitude/position aid, independent of GPS:
- Requires ground speed **~150–640 kt** to function reliably; requires valid INS alignment.
- CARA (radar altimeter) is the sensor TRN correlates against terrain — CARA usable to **50,000 ft
  AGL**, attitude limits **±60° pitch/roll below 3,000 ft AGL** (tighter above).
- TRN accuracy depends on terrain relief (flat terrain → poor position-fix confidence; hilly/mountain
  terrain → tighter fix) — a physically real effect a terrain-correlation model would need to
  reproduce, not obviously worth FlightBox's current scope but noted for completeness.

#### Navigation update mechanisms — cursor slew vs. position fix vs. altitude calibration (ED p.185–189)
Three distinct correction mechanisms, **not interchangeable**:
| Mechanism | Effect | Persistence |
|---|---|---|
| **Cursor slew** (slew Nav cursor to a known landmark) | Corrects **displayed symbology and weapon-release geometry only** | Temporary — does NOT touch the INS-only solution; removable via **Cursor Zero (CZ)** |
| **Position fix** (ICP 8/FIX page, designate a known steerpoint via sensor or HUD) | Updates the **INS-only horizontal solution itself** | Persistent (until it drifts again or GPS Kalman-filter removes it, see above) |
| **Altitude calibration** (ICP 9/A-CAL page, `RALT`/`FCR`/`TGP` sensor option) | Updates the **INS-only altitude** | Persistent |

Worked example from ED's altitude-calibration walkthrough (illustrates the delta-freeze mechanism):
overhead a steerpoint, INS-only altitude reads 2,296 ft MSL; radar altimeter reads 2,032 ft AGL against
a known steerpoint elevation, producing a **72 ft delta**; pressing ENTR applies that 72 ft delta to
correct INS-only altitude to 2,368 ft MSL. The general pattern (freeze a sensor-measured delta at a
known reference point, then apply it as an additive correction) generalizes directly to a FlightBox
INS-drift model: drift accumulates as an additive offset; a "fix" zeros that offset at a known-position
event.

#### Steerpoint database — ⚠️ discrepancy: Chuck's 99 vs. ED's 127
- **Chuck** (`procedures`/original table above): **99 total steerpoints**, ranges 1–24 nav / 25
  bullseye / 26–30 markpoints / 31–54 HSD lines / 56–70 threats / 71–80 datalink markpoints / 81–89
  open / 90–99 Harpoon.
- **ED EA Guide** (p.201–203, official): **127 unique steerpoints in 7 partitions**: Navigation (1–25,
  incl. MGRS-input 21–25), Markpoints (26–30), Geographic Lines, Pre-planned Threats (56–70),
  Datalink (**500+** for datalink markpoints — not 71–80), Destinations (**81–99**), Extended Datalink
  (**SEAD targets 107–127**).

The **1–25 nav / 26–30 markpoint / 56–70 threat / 81–99 destination ranges agree** between the two
sources (Chuck's 90–99 "Harpoon, open on Block 50" overlaps ED's "81–99 Destinations" range — plausibly
the same physical slots described differently by version/scope). The **datalink-markpoint numbering
disagrees outright** (Chuck: 71–80; ED: 500+) and **ED's total (127) exceeds Chuck's (99)** because ED
counts an Extended Datalink partition (107–127, SEAD targets) Chuck's guide doesn't mention at all.
Likely explanation: ED's official manual reflects a more complete/later steerpoint-database revision
(TNDL SEAD-target datalink partition is documented in `datalink-iff.md`'s ED-sourced TNDL section,
consistent with ED being the newer, TNDL-aware source). **Treat ED's 127/7-partition structure as
primary** for a rebuild's steerpoint-database sizing; Chuck's 1–70 range numbers remain useful
cross-checks since they agree where both sources overlap.

#### Steerpoint symbols (ED p.202–203, refines Chuck's HUD-only symbol list)
Steerpoint sub-type shapes (shown on HSD/HAD, not HUD): **circle** = navigation steerpoint (STPT),
**square** = Initial Point (IP), **triangle** = Target (TGT). Selected = solid white; unselected active
route = hollow white; unselected inactive route = hollow gray. Markpoints = yellow X (ownship) or large
white X (datalink-received). Pre-planned threats = yellow text+ring (white/red text+ring if ownship is
inside the ring — i.e. **expect engagement**). SEAD targets = yellow text with a slash.

#### Navigation by steerpoints — HUD/HSD/EHSI element cross-reference
The **Great Circle Steering Cue** (ED's name for Chuck's "Tadpole") and its great-circle bearing
computation, plus the **HSD Azimuth Steering Line**, are fully documented in `hud-symbology.md`'s ED
addendum — not duplicated here. Key nav-specific fact: the **Diamond Symbol** shows the steerpoint's
full 3-D position (position + altitude, not just azimuth) — an X is superimposed when it's outside the
HUD field of view (same clamp philosophy as the Great Circle Steering Cue's edge-pin, `hud-symbology.md`).

#### EHSI — course logic detail (ED p.227–231, 2396–2433 region)
- **NAV mode**: Bearing Pointer + Course Deviation Indicator reference the selected steerpoint;
  standard fly-to-center CDI logic (turn toward the CDI needle until centered on the course line).
- **TCN mode**: same instrument, referenced to the tuned TACAN station instead.
- **NAV/PLS or TCN/PLS mode**: once a localizer signal is acquired, the **Course Deviation Indicator
  and Course Pointer switch to localizer-course deviation** (independent of the EHSI's own course
  setting), while the **Bearing Pointer keeps pointing at the steerpoint/TACAN station** — i.e. PLS
  mode is a partial override: lateral deviation comes from the ILS receiver, bearing info stays on the
  originally-selected nav source. This is a genuinely non-obvious detail for a faithful EHSI
  implementation (bearing needle and CDI needle can be referenced to *different* sources simultaneously
  in PLS mode).

#### Tactical Air Navigation (TACAN) — quantitative facts (ED p.232–237)
- **126 channels × 2 bands (X/Y) = 252 usable navigation channels.**
- Three modes: **REC** (bearing only) / **T/R** (bearing + DME range) / **A/A T/R** (air-to-air distance
  between two TACAN-equipped aircraft, 00.1–99.9 nm resolution).
- TACAN requires line-of-sight; **reliable range ~130 nm** (less at low altitude).
- MIDS LVT (the same radio terminal that carries Link-16/TNDL, `datalink-iff.md`) must be powered for
  TACAN function on Block 50 — TACAN and tactical datalink share one LRU.
- Radial navigation model: a TACAN radial is a fixed-bearing line from the station (not the aircraft's
  course) — standard VOR/TACAN radial-intercept geometry, relevant if FlightBox ever models a
  TACAN-based holding/approach pattern independent of steerpoints.

---

### ED EA Guide addendum — ILS (pp.238–246)

This supersedes/refines the generic-ILS-standard "Technical depth" numbers already in this file below
(pilotscafe/code7700/PPRuNe-sourced) with the **F-16C's own documented ILS behavior** — flagged where
they conflict.

#### Localizer/glideslope hardware and frequency plan
- Localizer transmitter: opposite end of runway from approach direction, **VHF, 108.00–111.95 MHz**,
  **40 channels at 5 kHz spacing, only "odd" kHz values used** (108.10, 108.15, 108.30, 108.35, … up to
  111.95). Glideslope frequency is auto-paired to the tuned localizer frequency (UHF, not
  independently tuned).
- Standard glidepath angle: **3°**, explicitly noted as **variable** (ED gives worked tables at 2.5°,
  3.0°, and 3.5° — see below), matching this file's existing generic-ILS "varies with terrain" note.

#### Marker beacons (new — not in Chuck's file, and not in the prior generic-ILS technical-depth section)
| Marker | Tone | Morse pattern | Typical use |
|---|---|---|---|
| **Outer** | 400 Hz | dash-dash-dash (▬▬▬) | Glideslope-intercept / descent-initiation point |
| **Middle** | 1300 Hz | dot-dash alternating (●▬●▬) | Decision-height / missed-approach-if-not-visual point (≥200 ft AGL DH) |
| **Inner** | 3000 Hz | rapid dots (●●●) | Lower decision height (<200 ft AGL), requires specific aircrew/equipment qualification |

All three illuminate the same Marker Beacon light beside the EHSI; distinguishing them in the sim
requires audio-tone or explicit-marker-type modeling, not just a boolean "over a marker" flag.

#### Glideslope intercept altitude — worked table (ED p.241, directly usable for autopilot ILS guidance)
Altitude **above runway elevation** (ft, rounded to nearest 100) at which the glideslope is intercepted,
by slant distance and glideslope angle:
| Slant dist (nm) | GS 3.5° | GS 3.0° | GS 2.5° |
|---|---|---|---|
| 15 | +5,600 | +4,800 | +4,000 |
| 14 | +5,200 | +4,500 | +3,700 |
| 13 | +4,800 | +4,100 | +3,400 |
| 12 | +4,500 | +3,800 | +3,200 |
| 11 | +4,100 | +3,500 | +2,900 |
| 10 | +3,700 | +3,200 | +2,700 |
| 9 | +3,300 | +2,900 | +2,400 |
| 8 | +3,000 | +2,500 | +2,100 |
| 7 | +2,600 | +2,200 | +1,900 |
| 6 | +2,200 | +1,900 | +1,600 |
| 5 | +1,900 | +1,600 | +1,300 |

(This is just `altitude = slant_distance_ft × tan(glideslope_angle)` — a derivable table, but having
ED's own rounded reference values is a useful sanity-check for a guidance implementation.)

#### Glideslope descent-rate — worked table (ED p.242, directly usable for autopilot ILS guidance)
Descent rate (fpm, rounded to nearest 10) required to hold glideslope, by ground speed and angle:
| GS (kt) | GS 3.5° | GS 3.0° | GS 2.5° |
|---|---|---|---|
| 200 | −1,170 | −1,000 | −830 |
| 190 | −1,110 | −950 | −790 |
| 180 | −1,050 | −900 | −750 |
| 170 | −990 | −850 | −710 |
| 160 | −930 | −800 | −670 |
| 150 | −880 | −750 | −630 |
| 140 | −820 | −700 | −580 |
| 130 | −760 | −650 | −540 |
| 120 | −700 | −600 | −500 |
| 110 | −640 | −550 | −460 |

(Formula: `descent_rate_fpm ≈ groundspeed_kt × 101.27 × tan(angle) / 60`... approximately; ED's rounded
values are close to but not exactly this — e.g. 150 kt/3.0° gives ≈796 fpm by the exact trig formula vs
ED's tabulated 750 fpm, a ~6% difference likely from ED's own rounding/approximation convention. **Use
ED's table as the DCS-F-16-behavior reference, the trig formula as the physically-exact fallback** if
building a continuous (non-tabulated) guidance law.)

#### Decision Height (DH) and Missed Approach Point (MAP)
- DH typically **no lower than ~200 ft AGL** above the touchdown-zone runway elevation (matches the
  Middle Marker's documented role above).
- At DH, if the approach lighting system or runway is not visually acquired, **execute a missed
  approach**: increase throttle, arrest descent, establish climb, gear up on positive rate, retract
  before 300 KCAS (identical mechanics to `procedures-landing.md`'s go-around).
- **CARA ALOW / MSL FLOOR automation**: setting CARA ALOW = DH value flashes "AL <DH>" on the HUD at
  that radar altitude; setting MSL FLOOR = DH + runway-threshold-elevation-MSL triggers the VMS
  "Altitude…altitude" voice call at the same point via barometric/system altitude instead — this is the
  existing ALOW system (`aerodynamics-performance.md`) repurposed as the ILS missed-approach trigger,
  not a separate mechanism. (Duplicated briefly in `procedures-landing.md`'s ED addendum since it's
  landing-relevant; documented here as the ILS-specific configuration.)

#### ⚠️ Discrepancy: ADI glideslope deviation scale vs. this file's prior generic-ILS numbers
- **This file's existing "Technical depth" section below** (pilotscafe/code7700/PPRuNe-sourced generic
  ILS standard): glideslope full-scale deflection **±0.7°** (1.4° beam width), **0.14°/dot**.
- **ED EA Guide** (p.244, F-16C ADI, official): **"Each white dot corresponds with a 2.5° vertical
  separation from the glideslope. If the Glideslope Indicator is aligned with the top or bottom white
  dot..., the aircraft is 5° above or 5° below the glideslope."**
  This is a **much wider** scale than the generic-ILS-standard figure — a ~18× difference per dot
  (2.5° vs 0.14°). Two readings are possible and neither is confirmed from a primary F-16 T.O.: (a)
  ED's ADI scale is genuinely F-16-specific and wider than the raw ILS beam-width convention (a design
  choice trading precision for a less "twitchy" needle), or (b) ED's "2.5°/dot" describes the ADI
  needle's *travel calibration relative to the beam's own internal full-scale*, not the raw glideslope
  beam angle itself (i.e. the needle is scaled so ±1 needle-width = the ILS receiver's own full-scale
  deflection, and that full-scale threshold itself might still be the ~0.7° industry-standard value —
  ED's prose doesn't disambiguate). **Not silently resolved**: ED's number is kept as the primary
  figure for the F-16C ADI specifically (official, aircraft-specific source); the generic ±0.7°/0.14°-
  per-dot figures remain below as the aviation-industry-standard baseline. A FlightBox ILS
  implementation should prefer **ED's dot-scale for the ADI/cockpit-instrument visual**, but the
  underlying **physical beam-angle saturation** (what actually drives command-steering/autopilot logic)
  is still most defensibly the industry-standard ±0.7°/±2.5° localizer figures absent a primary F-16
  flight-manual number — this is a genuine open gap, not a resolved conflict.
- **Localizer deviation scale**: ED's ADI section (p.244) describes the Localizer Deviation Bar's
  behavior qualitatively (centered = on runway centerline, left/right = off-course) but gives **no
  explicit degree-per-dot number** for the localizer, unlike its explicit 2.5°/dot glideslope figure —
  remains a genuine gap in the ED source itself, not just our extraction; the generic ±2.5° full-scale
  localizer figure below is the only quantified source we have for it.

#### Command Steering Symbol — control-law framing (ED p.245, sharper than this file's prior "flight-
director law" note)
- **Lateral displacement** of the Command Steering Symbol corresponds to the **bank angle required** to
  achieve the localizer-intercept turn rate — i.e. it's a bank-angle command, not a raw deviation
  display. Pilot banks toward the symbol until it aligns with the FPM.
- **Vertical displacement** corresponds to the **vertical velocity required** for glideslope intercept
  — pilot adjusts throttle to drive the FPM toward the symbol vertically.
- The **tic mark** on the symbol is present only when a valid glideslope signal exists; its absence
  means localizer-only command steering is available.
This confirms and sharpens the existing "flight-director law combining localizer + glideslope error"
note below: the ED source explicitly frames it as **bank-angle command (lateral) + vertical-velocity
command (vertical)** — the exact two control-law outputs `FBAutopilot::Direct` would need to compute
for a faithful ILS-coupled approach mode.

#### HUD decluttering during approach (MAN RNG/UNCAGE, ED p.245–246, new — not in Chuck)
Pressing MAN RNG/UNCAGE to UNCAGE during approach (or after Decision Height) removes the Heading Tape
from its normal position (moves it to the top), removes the Roll Indicator, and removes the ILS
localizer/glideslope deviation bars — but **the Command Steering Symbol remains**. A deliberate
decluttering step separate from Chuck's "if desired" note in the overhead-pattern section.

## State

FlightBox navigates by **one steerpoint and a bullseye**, computed in planar ENU geodesy. None of the
radio navigation in this file exists: no TACAN, no ILS, no EHSI/HSD, no INS with alignment or drift.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Steerpoint navigation (active point, bearing, distance, TTG) | **built** — `FBNavSystem`, published in the Nav block and read by the HUD | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §7 |
| Waypoint sequencing | **built and owned by the actor** — `FBNavSystem::AdvanceWaypoint`, called by the module, never by the mission runner | same |
| Bullseye | **built** as an anchor point with bearing/range | same |
| Steerpoint database (99 vs. 127 points, types, MARK points, offsets) | **not implemented** — the flight plan is mission data (`.fbm`), a plain waypoint list | [`../flightbox/sim/units-and-missions.md`](../flightbox/sim/units-and-missions.md) |
| Steerpoint elevation | **approximated** — the module uses the elevation sample **under the aircraft** this tick; a declared waypoint elevation is mission-format work. Correct over gentle terrain, wrong over mountains | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) Gaps 11 |
| Magnetic variation | **not implemented** — `MagVarDeg` is hard 0, so every "magnetic" label currently shows true | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) Gaps 8 |
| INS: alignment, drift, position fix, ACAL, SALT source priority, GPS/INS blending | **not implemented** — position is exact, always | — |
| TACAN (A/A and A/G, channels, ranging) | **not implemented** | — |
| ILS: localizer/glideslope, marker beacons, DH, MAP, command steering | **not implemented**. The approach is flown by the pilot phase machine on `FBAutopilot` COURSE guidance against the mission's runway, not against a beam | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §2 |
| EHSI / HSD as displays | **not implemented** — no cockpit displays at all; only the HUD is drawn | [`../flightbox/clients/clients.md`](../flightbox/clients/clients.md) |

**Directly usable, still unused:** the glideslope-intercept-altitude and glideslope-descent-rate tables
and the CARA-ALOW-as-decision-height mechanism above are computable inputs for a future ILS-coupled
approach mode; ALOW already exists as a system.

## Gaps

**Source gaps** (this file vs. its sources)
- **Two discrepancies stay open and flagged** — steerpoint count Chuck 99 vs. ED 127, and the ADI
  glideslope deviation scale 0.7° vs. 2.5°/dot. Both values are kept above; neither is resolved.
- ED's Radio Communications chapter (pp.247–260) is **not processed** (PROGRESS.md, low-priority call).
- The MARK DED page was **not found** in the UFC chapter and is suspected to live in this chapter's ED
  pages; not independently re-checked — see `cockpit-displays.md`.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* steerpoint steering and sequencing, bullseye, distance/bearing/TTG.
- *Partially:* the navigation *solution* — exact by construction, so everything this file says about
  blending, drift, fixes and altitude sources has no counterpart; and steerpoint elevation is a sample
  under the aircraft rather than a database value.
- *Not at all:* TACAN, ILS and its whole approach apparatus, INS alignment/drift, magnetic variation,
  EHSI/HSD, steerpoint database semantics (types, MARK, offset points).

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

ILS geometry and deviation scaling for a faithful localizer/glideslope model. Sources cited inline.

### ILS beam geometry & deviation scaling
| Element | Value |
|---|---|
| Standard glidepath angle | **3°** (varies with terrain) |
| Localizer full-scale CDI deflection | **±2.5°** from runway centerline |
| Glideslope full-scale deflection | **±0.7°** (beam width 1.4°) |
| Glideslope "dot" | **0.14°** per dot |
| Localizer course width at threshold | **~700 ft** (angular width set so full width = 700 ft at threshold) |

Sources: pilotscafe / code7700 / PPRuNe tech-log — standard HSI/CDI ILS scaling.

### Modeling implications
- The **HUD localizer/glideslope steering bars** and the **EHSI CDI** should both be driven from the same
  angular deviations: lateral = angle off the localizer centerline (saturating at ±2.5°), vertical = angle
  off the 3° glidepath (saturating at ±0.7°). This gives the correct "bar sensitivity increases as you
  approach the runway" behavior (constant angular width → shrinking linear width).
- **Command Steering symbol** = flight-director law combining localizer + glideslope error into a single
  steer-to cue; the **tic mark** appears when pitch (glideslope) data is valid (guide). A rebuild computes
  a director command from the two deviations, not just displays raw needles.
- **Glideslope capture** at ~0.7° (one full-scale) is where the guide's "Glide Slope Fail Flag" clears.
- On the F-16, ILS is tuned as **frequency** (e.g. 110.30) via the DED **T-ILS** page and the EHSI is
  slaved with **PLS/NAV** — the receiver provides the angular deviations above to both HUD and EHSI/ADI.

### Navigation system context
- Steerpoint navigation is **INS + GPS** based (the FLCS/FCC use INS for sideslip and steering); TACAN
  provides bearing/range to a ground station; ILS provides the precision-approach angular guidance. NDB/ADF
  is not supported (guide). A rebuild needs: INS position (drifting, correctable via nav-fix), GPS
  correction, TACAN ρ/θ, and ILS localizer+glideslope deviation.
- **INS drift + nav-fix** (guide §12): the INS accumulates position error; a sensor designation (TGP/FCR/
  HUD) of a known steerpoint computes a DELTA and re-aligns — model INS as slowly-drifting truth with
  discrete fix corrections.

### Hardware (LRUs, for context)
- **Radar altimeter (CARA)**: **AN/APN-232** Combined Altitude Radar Altimeter (Gould) — feeds the ALOW
  system and HUD radar altitude; must be ON for ALOW (guide). Typical usable range to ~5000 ft AGL.
- **INS**: ring-laser-gyro INS — **Litton LN-39 / LN-93**, or **Honeywell H-423**; later blocks use an
  **EGI** (Embedded GPS/INS, e.g. **LN-260**) fusing GPS with the RLG INS. The F-16 was the first
  operational US aircraft with GPS. INS drift + nav-fix (guide §12) reflects the RLG-INS error growth.
- **TACAN/ILS/MIDS**: TACAN is part of the **MIDS** radio (guide); ILS via a separate marker-beacon/LOC/GS
  receiver feeding EHSI (PLS mode) and HUD/ADI steering bars.
- **FCR**: **AN/APG-68** mechanically-scanned radar (4 LRUs) also displays steerpoints (see
  `radar-sensors.md`).

### Sources
- pilotscafe.com *Understanding ILS*; code7700.com *ILS*; PPRuNe tech-log *Full-scale deflection on CDI* —
  localizer ±2.5°, glideslope ±0.7°/1.4° beam, 0.14°/dot, 3° glidepath, 700 ft course width.
- airforce-technology.com F-16; Wikipedia *AN/APG-68* — AN/APN-232 CARA, LN-39/93 / H-423 INS, EGI/GPS, APG-68.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — NAVIGATION chapter p.163–246:
  Navigational Sensors p.164; Inertial Navigation System p.165–200 (alignment cross-ref
  `procedures-startup.md`); Navigation Solutions/System Altitude/DTS p.177–184; Navigation Updates
  p.185–189; Navigation Database/Steerpoints p.201–203; Navigation by Steerpoints (HUD/HSD/EHSI)
  p.225–231; Tactical Air Navigation (TACAN) p.232–237; Instrument Landing System (ILS) p.238–246
  (marker beacons, glideslope-intercept/descent-rate tables, Decision Height, ADI scale discrepancy,
  Command Steering Symbol control-law framing).
