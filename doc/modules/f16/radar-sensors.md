# F-16C Radar & Sensors

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 10 — Radar & Sensors, pp. 161–312 (152 pp).
This is a structural reference to the mode taxonomy + key display/HOTAS concepts; the guide contains
extensive per-mode tutorials not reproduced step-by-step here.

Plus, since this round, `doc/TO 1F-16CMAM-34-1-1 BMS.pdf` (BMS Dash-34, change 4.38) — cite tag
**`[BMS-34 p.NNN]`** — **pp.190–222** (A-A FCR MFD page, scan geometry, ACM submodes) and
**pp.226–241** (A-G FCR: AGR, GM, SEA, GMT, FTT, EXP/DBS, page fields). See the *"BMS Dash-34
addendum — the FCR MFD page, field by field"* section below. **That manual documents a SIMULATOR, not
the factory jet**; the full caveat is at the head of `hud-symbology.md` and its "not implemented"
markers are carried through verbatim.

## Spec

### Sensor overview
Sensors: **FCR** (AN/APG-68 Fire Control Radar), **TGP** (targeting pod), **HMCS** (helmet cueing),
Maverick seeker, HTS pod. Displayed on the MFDs; the active sensor = **SOI** (Sensor of Interest), set by
**DMS** (up = HUD, down = MFD sensor). Master modes: **NAV / A-A / A-G** (drive which sensor modes exist).

### FCR — Air-to-Air modes

Top-level: **BVR** (beyond visual range search) · **ACM** (air combat maneuvering, close-in) · **STT**
(single target track). Display: range = vertical axis, azimuth = horizontal.

#### CRM (Combined Radar Mode) — default at power-up
Combines BVR search sub-modes under one interface (cycle sub-modes: hold **TMS right** > 1 s):
| Sub-mode | Behavior |
|---|---|
| **RWS** (Range While Search) | Default search; large-volume, all-aspect, all-altitude detection |
| **TWS** (Track While Scan) | Maintains up to **10 trackfiles** while still scanning; artificially limits scan volume (bars/azimuth), auto-centers; provides post-launch datalink for AMRAAM; less precise than STT |

#### SAM (Situational Awareness Mode)
Hybrid RWS/STT: locking a target in RWS enters SAM. Radar periodically scans the locked target while
scanning the whole area. Acquire: cursor on target → **TMS forward** once → release (starts 4-bar,
±10° spotlight at last known position).

#### STT / DTT (radar lock)
- **STT** (Single Target Track): all radar power on one target, highest accuracy.
- **DTT** (Dual Target Track): two targets tracked.

#### Spotlight
In RWS/TWS, a spotlight scan can designate targets at longer range (concentrates the scan).

#### ACM (Air Combat Mode) — close-in auto-acquisition
Sub-modes: **HUD Scan**, **Vertical Scan**, **Boresight**, **Slewable** — auto-lock the first target in a
close-range volume tied to HUD geometry.

#### Other A-A
- **EXP (Expand)**: zoom the radar display.
- **HMCS radar lock**: lock the target the helmet cross is on.

### FCR — Air-to-Ground modes
| Mode | Purpose |
|---|---|
| **GM** (Ground Mapping) | Terrain/ground map picture |
| **Expanded (EXP1/EXP2)** | Zoomed ground map |
| **FTT** (Fixed Target Track) | Designate/track a fixed ground point |
| **GMT** (Ground Moving Target) | Detect moving ground targets |
| **BCN** (Beacon) | Beacon returns |

Designation via Radar Cursor slew + TMS; sets the SPI for A-G weapons and nav fixes.

### FCR — Air-to-Sea
- **SEA** mode: detect surface ships.

### TGP — Targeting Pod (Sniper/Litening class)
- Modes: A-G (point/area track), A-A. SOI via DMS down; slew with Radar Cursor; **TMS up** = point track,
  **TMS right** = area track, **TMS down** = slave to steerpoint.
- **Cursor Zero (CZ)** / steerpoint slaving, **Snowplow** mode (pod points at ground ahead), **LSS**
  (Laser Spot Search).
- Lasing: Laser Arm switch ARM → trigger 1st stage fires laser ("L" flashes on HUD); used for ranging,
  nav fix, and LGB guidance. Laser codes set on the LASR DED page.

### HMCS — Helmet-Mounted Cueing System
- Power-up + alignment (coarse then fine) against the HUD reference cross — see `procedures-startup.md`
  steps 66–68 and `hud-symbology.md`.
- HMD symbology mirrors key HUD/RWR elements off-boresight; used for high-off-boresight (HOBS) missile
  cueing (AIM-9X), ground target designation, and off-HUD radar lock.

### Weapon sensor: AGM-65 Maverick
Maverick seeker (IR: D/G; EO: H/K) is itself a sensor page — boresight, pre-planned (slaved to TGP/radar),
visual, and boresight employment modes (see `weapons.md`).

---

## ED EA Guide — precision addendum (official source, additive)

> **Additive per task instructions**: a developer agent is implementing the FCR in parallel and may be
> reading this file. Nothing below rewords the sections above; it only adds numeric/logic precision
> from the official Eagle Dynamics module manual (`doc/DCS F-16C Early Access Guide EN.pdf`, pages
> 342–523 this pass: FCR employment 342–418, TGP 453–491, HTS 492–515, HMCS 516–523). Where ED
> precises a term already used above (e.g. ACM sub-mode names), it is **consistent**, not a
> correction — flagged explicitly only where it genuinely differs.

### FCR power/standby
- **STBY / OVRD**: Standby Override (OSB 4, any FCR MFD page) forces FCR to STBY independent of master
  mode, **stows the antenna to 60° left azimuth / +30° elevation**, and holds STBY across master-mode
  changes until OVRD is toggled off again (ED EA Guide p.375).
- **RF switch** (MISC panel) QUIET/SILENT inhibits FCR emission and freezes the antenna without
  changing FCR mode — independent of the FCR-internal STBY state (ED p.375).
- On the ground (WOW) FCR transmission is inhibited regardless of selected mode (ED p.375).

### CRM (Combined Radar Mode) — precise sub-mode taxonomy (ED EA Guide p.376–399)
ED names **six** CRM sub-modes, not just RWS/TWS as summarized above — this is additive precision,
not a contradiction (RWS/TWS were correctly identified as the two headline sub-modes; ED adds the
full state machine around them):

| Sub-mode | Class | Definition (ED, verbatim sense) |
|---|---|---|
| **RWS** (Range While Search) | Search | MPRF scan cycle, azimuth+range display; good hot/cold-aspect balance, moderate clutter rejection; **default on CRM entry** |
| **VSR** (Velocity Search w/ Ranging) | Search | interleaved MPRF+HPRF; better hot-aspect range/clutter rejection, worse cold-aspect detection |
| **TWS** (Track While Scan) | Search/Track | MPRF scan cycle; builds up to **10 track files** from repeated detections; lower reliability, higher pilot workload than SAM |
| **SAM** (Situation Awareness Mode) | Search/Track | alternates RWS scan with focused dwell on 1–2 designated targets (FCR TOI, + optional Secondary → **DT SAM**); higher reliability/lower workload than TWS, capped at 2 tracked targets |
| **DTT** (Dual Target Track) | Track-only | pure alternating track of 2 targets, **no scan**, auto-entered from DT SAM below **10 NM** to either target |
| **STT** (Single Target Track) | Track-only | continuous dwell on 1 target, highest reliability, **auto-entered from ACM on first detection**, or manually from VSR/TWS/SAM/DTT |

**Mode-transition triggers** (ED, precise — this is the part worth encoding as a state machine):
- RWS → SAM: TMS Forward over a Search Target (designates FCR TOI/Primary Target).
- SAM → STT: TMS Forward over the Primary Target, **or automatically once range < 3 NM**.
- SAM → TWS: TMS Right held **≥1.0 s**.
- SAM → RWS: TMS Aft (rejects Primary Target).
- SAM → DT SAM: TMS Forward over a second Search Target while already in SAM.
- DT SAM → DTT: **automatic once range to either target < 10 NM**.
- DT SAM/DTT → STT: TMS Forward over either target (tracks that one only); DTT also auto-narrows to
  STT on the closer target if range < 3 NM to either.
- TWS → SAM: bug a Track Target then TMS Right held ≥1.0 s (bugged target becomes SAM Primary).
- Any tracking sub-mode → previous search sub-mode: TMS Aft (from STT, returns to whichever of
  SAM/VSR/TWS was active before).

**SAM/DT SAM tactical rationale (ED, explicit)**: SAM's brief periodic dwell (vs. TWS's continuous
search-pattern track-file refresh) is called out as **reducing the chance the target's own RWR
recognizes an FCR lock** — i.e. SAM is documented as a genuine LPI-adjacent tactic, not just a
workload-reduction feature.

**TWS track-file management** (ED p.381–383, precise numbers):
- FCR maintains **up to 10 Track Targets** simultaneously, auto-generated from repeated Search Target
  detections; a track file is **dropped after 13 seconds of stale (unrefreshed) data**.
- Any Track Target may be manually upgraded to a **System Track Target** (TMS Forward with cursor over
  it); **only System Track Targets** can become a Cursor Target or Bugged Target (FCR TOI) — i.e.
  engagement/NCTR/elevated-priority-scan eligibility is gated through this upgrade step. TMS Right with
  no upgrades yet performed **bulk-upgrades all 10** Track Targets to System Track Targets at once.
- **Cursor Target / Bugged Target priority scan**: placing the cursor on (or bugging) a System Track
  Target overrides the current Azimuth/Elevation scan settings with a **±25°, 3-bar pattern centered on
  that target** to protect its track — reverts to prior settings when the cursor moves off / bug is
  rejected.
- **Auto range-scale management**: whenever an FCR TOI is designated (SAM Primary, TWS Bugged Target,
  or STT target), range scale auto-increments if target range **> 90%** of current scale, auto-
  decrements if **< 40%** — same 90%/40% thresholds apply uniformly across SAM/TWS/STT (ED, repeated
  verbatim at each mode's description — treat as one shared rule, not three separate ones).

**EXP (Expand) field-of-view** (ED p.390, CRM search/track sub-modes only — RWS/TWS/SAM/DTT, not
VSR): enlarges an **8×8 NM** area around the cursor (or centered on the FCR TOI if one is designated)
to fill the whole MFD, to help sort closely-spaced targets (e.g. a formation) for individual
designation. Removed automatically at the 160 NM range scale (box would be sub-pixel).

### Radar scan-frame geometry & refresh-rate tradeoff (ED p.359–361, quantified)
Two worked examples ED gives directly (this is the concrete azimuth/bar-count vs. refresh-time
tradeoff the mode-selection logic above is built on):
| Scan pattern | Azimuth × bars | Scan-frame time | Coverage |
|---|---|---|---|
| A6, 4B | ±60°, 4 bars | **8 s** | full search volume |
| A3, 2B | ±30°, 2 bars | **2 s** | 25% of the above volume, faster refresh |

Azimuth scan width options in CRM: **A6 (±60°) / A3 (±30°) / A1 (±10°)**, cyclic via OSB 18. Elevation
bar-scan options: **4B / 3B (TWS only) / 2B / 1B**, via OSB 17 — settings persist per CRM sub-mode.

### ACM (Air Combat Mode) — exact scan geometry per sub-mode (ED EA Guide p.391–399)
This is the biggest precision addition over the existing "HUD Scan / Vertical Scan / Boresight /
Slewable" summary above — same four sub-modes, now with **exact angular/range figures**:

| Sub-mode (ED short name) | Trigger | Scan volume | Max range |
|---|---|---|---|
| **30×20** ("HUD Scan") | TMS Right | **±15° azimuth, +4° to −16° elevation** (≈ HUD FOV) | 10 NM |
| **10×60** ("Vertical Scan") | TMS Aft (from NO RAD) | **±5° azimuth, +53° to −7° elevation** (narrow, tall — nose-high volume) | 10 NM |
| **BORE** ("Boresight Scan") | TMS Forward | fixed at **0° azimuth, −3° elevation** (or slaved to HMCS LOS) | **selectable 5/10/20/40 NM**, default 20 |
| **SLEW** ("Slewable Scan") | RDR CURSOR/ENABLE (any direction) | **4-bar, ±30° azimuth**, cursor-slewable | 10 NM |

Cyclic order via OSB 2 or TMS: **20 → 60 → SLEW → BORE → 20**. ACM always enters non-radiating
("NO RAD") on mode entry unless already in STT; any of the four trigger inputs above starts
transmission in that sub-mode. **First detection within any ACM sub-mode's volume → immediate,
automatic transition to STT** (voice "Lock" callout) — ACM never shows a multi-target picture, unlike
CRM's search sub-modes; this is why Chuck's summary calls these "auto-acquisition" modes. TMS Aft while
FCR is SOI rejects an STT lock and returns to NO RAD.
**Datalink tracks (including friendlies) are never shown on the FCR MFD while in any ACM sub-mode**,
regardless of declutter settings — ED explicitly flags this as an IFF/fratricide-risk callout (verify
via NCTR/AIFF before engaging a close-range STT lock, not via datalink correlation).

### NCTR & AIFF (ED EA Guide p.399–403)
- **NCTR (Non-Cooperative Target Recognition)**: analyzes Doppler returns from **rotating engine
  compressor/turbine blades**, which constrains it to **head-on or tail-on aspect only**. Trigger:
  TMS Left held **>0.5 s** on an FCR TOI (also fires a simultaneous AIFF LOS interrogation along the
  cursor azimuth). **Hard gates: range ≤ 25 NM, aspect angle 0°–30° or 150°–180°** (shown in Expanded
  Target Data as "3L"–"3R" / "15L"–"15R" — an o'clock-style aspect readout). Outcomes: successful
  classification → aircraft-type code + symbol recolor/reshape by coalition; signature doesn't match
  any preloaded type → **"UKN"**; outside range/aspect gate → **"INVL"**.
- **AIFF (AN/APX-113)**: LOS interrogation fires alongside an NCTR attempt (see above) — i.e. in ED's
  documented logic, NCTR and IFF-LOS-interrogation are triggered by the **same** pilot input, not two
  separate actions.

### FCR Air-to-Ground modes — precise scan/threshold parameters (ED EA Guide p.404–418)
- **GM / GMT / SEA** share identical scan-geometry controls: azimuth **A6 (±60°) / A3 (±30°) / A1
  (±10°)**; range scale **10 / 20 / 40 / 80 NM**. All three are Plan-Position-Indicator ground-map
  renders; GM = static terrain reflectivity map, SEA = same but clutter-rejection tuned for low/medium
  sea states, GMT = terrain map **plus** a GMTI (Ground Moving Target Indicator) Doppler-threshold
  overlay showing movers as solid white squares.
- **DBS1/DBS2 (Doppler Beam Sharpening)** — **GM mode only** (not available in GMT/SEA): higher-
  resolution "patch" zoom; **DBS2 gives a 64:1 increase in resolution over DBS1** at the cost of a
  slower refresh rate (ED states the ratio explicitly, exact patch sizes are range-scale-dependent per
  an ED table not reproduced here — see ED EA Guide p.187/299 for the per-range-scale patch-size
  table if implementing DBS).
- **MTR (Moving Target Rejection) thresholds** — pilot-selectable HI/LO on the FCR CNTL page, **and
  auto-adjusted upward as the antenna slews away from the aircraft centerline** (to reject stationary
  clutter more aggressively off-axis, at the cost of slow-mover detection off-axis):
  | MTR setting | Threshold band |
  |---|---|
  | **HI** | 16–75 kt (19–87 mph, 30–139 km/h) |
  | **LO** | 8–55 kt (10–64 mph, 15–102 km/h) |
  Independent MTR/TGT-HIS settings are retained separately for A-A and A-G FCR modes.
- **GMTT (Ground Moving Target Track)**: entered by TMS-Forward-designating a GMTI square in GMT mode;
  freezes the radar map (same visual state as manual Freeze/FZ) while the FCR **continues to emit and
  track** the single designated mover, shown as a solid diamond in the cursor. Designating a
  *stationary* return while in GMT instead enters **FTT (Fixed Target Track)**.
- **AGR (Air-to-Ground Ranging)**: auto-entered whenever a "Visual" A-G delivery sub-mode is selected
  (CCIP/DTOS/STRF/EO-VIS/VIS) or "HUD" is chosen as sensor on FIX/A-CAL/MARK DED pages; while in AGR,
  the FCR mode menu offers **only AGR and STBY** — it cannot be switched to GM/GMT/SEA while a Visual
  sub-mode is active (this is the FCR-side enforcement of the Pre-planned/Visual split documented in
  `weapons.md` §2.1).

### TGP (AN/AAQ-33-class pod) — sensor/zoom facts (ED EA Guide p.453–491)
- **Two sensor channels**: FLIR (thermal, day/night, White-Hot/Black-Hot polarity, two optical FOVs)
  and TV (daylight, higher magnification/clarity than FLIR at the cost of no night capability).
  Wide/Narrow FOV toggle via Expand/FOV button; a **TV Picture-in-Picture** overlay is available inside
  the FLIR Wide FOV frame.
- **Zoom**: variable digital zoom **1×–4×** (simple pixel enlargement, no resolution gain) plus a fixed
  **3× XR (Extended Range)** zoom that **is** processed for resolution/clarity enhancement (not a
  simple crop) — double-press Expand/FOV within 0.5 s to toggle XR; XR cannot combine with variable
  zoom. XR removes Crosshairs/Point-Track-Box/Meterstick-Length symbology unless the laser is firing.
- **Track modes**: **Point Track** (TMS Forward release — locks a discrete point/small object) vs
  **Area Track** (TMS Forward held — stabilizes a wider crosshair region, e.g. for a target too large/
  featureless for point tracking); breaking track re-slaves the TGP LOS to the SPI or to whichever
  other sensor (FCR/HTS) currently owns SPI.
- **Slave mode**: TGP LOS is driven by the current 3-D SPI whenever the MMC "owns" SPI (i.e. not
  actively tracking on its own) — large crosshair symbol signals Slave state on the TGP MFD format.

### HTS (AN/ASQ-213) and HMCS (JHMCS) — brief additive notes (ED EA Guide p.492–523)
Not deep-extracted this pass (budget prioritized FCR/TGP); confirmed consistent with the existing
summary above (HAS/POS modes, HAD threat display, alignment procedure). **TODO (future pass)**: HTS
WPN-format field definitions and HMCS DED alignment-page parameters (ED p.494–523) are not yet
distilled to the same depth as FCR/TGP above — flagged as a gap, not guessed.

## BMS Dash-34 addendum — the FCR MFD page, field by field

*Source: `TO 1F-16CM/AM-34-1-1 BMS` pp.190–222 (A-A) and pp.226–241 (A-G). Cite tag `[BMS-34 p.NNN]`.
**This is a simulator manual, not a factory manual** — see the caveat block at the head of
`hud-symbology.md`; N/I markers are carried through verbatim below. This section exists because the
prior sources gave the FCR *mode taxonomy* but not the *page*: what is written where, and which OSB
changes it.*

### R1 — OSB numbering (the frame, identical on both MFDs) `[BMS-34 p.63]`

```
            OSB  1   2   3   4   5
       OSB 20 ┌───────────────────┐ OSB  6
       OSB 19 │                   │ OSB  7
       OSB 18 │   display area    │ OSB  8
       OSB 17 │                   │ OSB  9
       OSB 16 └───────────────────┘ OSB 10
            OSB 15  14  13  12  11
```
Top row = **sensor modes**, bottom row = **display formats**, sides = **format/mode-specific
functions** `[p.63]`. The display area around a pressed OSB **briefly illuminates** as press feedback
`[p.63]`.

### R2 — A-A FCR base page (B-scope) `[BMS-34 pp.191–200]`

The A-A display is a **B-scope**: the radar cone's bottom edge is stretched across the whole bottom
axis, so **the entire bottom edge represents ownship**, not just the centre; a target symbol is the
**line of sight** to that target `[p.191]`.

| OSB / location | Label | Content |
|---|---|---|
| **1** | mode | `CRM` (Combined Radar Mode) · `ACM` (Air Combat Mode) |
| **2** | submode / track status | CRM: `RWS` · `ULS` · `VSR` · `TWS` — ACM: `20` (30×20°) · `SLEW` · `BORE` · `60` (10×60°) |
| **3** | FOV | `NORM` · `EXP` |
| **4** | `OVRD` | selects FCR standby page |
| **5** | `CNTL` | FCR control page. **Only TGT HIS and AIFF CPL/DCPL are implemented in BMS** |
| **6** | IDM data link mode rotary | `ASGN` · `CONT` · `DMD` |
| **7–10** | flight members 1–4 | pressing transmits an IDM assignment to that member; `XMT` shown for **2 s** |
| **10** (on CNTL page) | `CPL`/`DCPL` | couples the AIFF interrogator FOV to the FCR FOV in AIFF scan mode |
| **11** | `DCLT` | short press toggles declutter; **long press** opens the declutter-options page (4.36+) — altitude, attack steering, DLZ, target data, own weapon state, sensors |
| **16 (adjacent)** | AIFF interrogation data | interrogator type `M1`/`M2`/`M3`/`M4`/`M+`/`OFF` and mode `SCAN`/`LOS` |
| **16 (adjacent)** | bullseye circle | directional tic = bearing to bullseye, **2-digit range inside the circle** (max 99 NM), **3-digit magnetic bearing from bullseye to ownship below the circle**. Replaced by the **aircraft reference symbol** when bullseye is not mode-selected |
| **17** | `B` + 1/2/3/4 | elevation bar count |
| **18** | `A` + 6/3/2/1 | azimuth scan width: `6` = ±60° about the nose (not in TWS) · `3` = ±30° about the ACQ cursor (not in TWS) · `2` = ±25° about the cursor (**TWS and DT SAM only**) · `1` = ±10° about the cursor |
| **18** (on CNTL page) | `TGT HIS` 1–4 | frames of target history retained; each history symbol dims with age. **Power-up default in BMS = TGT HIS 2** |
| **19 / 20** | ▽ / △ | range rotary **5 / 10 / 20 / 40 / 80 / 160 NM** |
| below OSB 1–5 | expanded target data | see table below |
| right edge | DLZ | AIM-120 dynamic launch zone; missile time remaining / post-launch range / time of flight **below** the DLZ |

**Expanded target data block (below the OSB 1–5 mnemonics)** `[p.194]`

| Field | Position | Format |
|---|---|---|
| Target aspect angle | upper left | **tens of degrees**, plus `L`/`R` for the target wing nearest ownship. 0–70° = tail aspect, 70–110° = beam, 110–180° = front |
| Target magnetic ground track | immediately right of aspect | **10° increments** |
| NCTR result | under the FOV mnemonic | target ID (e.g. `MG29`) or `UNKN` |
| Target CAS | below the `OVRD` mnemonic | **10 kt increments**, KCAS |
| Target closure | upper right | KTAS; `COAST` displayed while coasting |
| Bugged target altitude | just below the bugged symbol | **thousands of feet MSL** |

**Scan geometry** `[pp.197–198]` — gimbal **±60° in azimuth and ±60° in elevation** (a 120°×120° block,
never scanned at once). Beam **4.9°** vertical; **bar spacing 2.2°** so 2/3/4-bar scans overlap without
gaps. Antenna tilt from the ANT ELEV thumbwheel, **+60° to −60°**, roll- and pitch-stabilized,
referenced to the horizon. **Elevation caret** = horizontal T on the **left** edge, +60° top to −60°
bottom, with **7 elevation tics each worth 10° (−30°…+30°)**. **Azimuth marker** = T along the
**bottom**, left edge −60°, centre 0°, right edge +60°; two vertical scan-limit lines are drawn when
not at ±60° (search, spotlight, SAM; also TWS, but **not** TWS Expand or STT), blanked as they reach
the display edge. **MIN/MAX search altitudes** (nearest 1000 ft) are printed right of the ACQ cursor in
TWS and RWS, computed at cursor range from beam width, bar spacing and antenna elevation.

**Cursor bumping (A-A)** `[p.200]` — slewing the ACQ cursor to the left/right edge toggles scan width
**±30° ↔ ±60°** (from ±10° it goes to ±30° first). Search-volume centre follows the cursor only when
the cursor centre is **within 4°** of the volume edge. Slewing to **< 5 % or > 95 %** of the range scale
steps the range scale down/up and re-places the cursor at **≈50 % range**, same azimuth — **unless that
would move a target track off the display** `[p.200]`.

**Range tics** `[p.200]` — three tic marks on the right edge divide the scale into **4 equal sections**.

**Target symbology** `[p.200]` — search target = **solid square** with a head/tail aspect indicator;
bugged target = **circle around a solid square with a velocity vector**; in STT all other search targets
are removed. The full MFDS symbol legend (search target high/low aspect, system track file,
extrapolated track file, bugged/priority track file, ECM/jamming, and the four AMRAAM-in-flight
overlays) is the figure on `[BMS-34 p.82]` — see `cockpit-displays.md` §M5.

**FCR informational messages** `[pp.190]` — `WAIT` during an FCR reset (clears in **4 or 12 s**
depending on cause); **`CHK FCR CONTROL PAGE`** after a restart + BIT, meaning all pilot-selectable
parameters (control-page values, azimuth, elevation bars, range scales) are **back to defaults**.

**FCR faults** `[p.241]`

| PFL | MFL | Effect | Action | Light |
|---|---|---|---|---|
| `FCR BUS FAIL` | FCR 003 | FCR inoperative | not recoverable | AVIONICS FAULT |
| `FCR XMTR FAIL` | FCR 094 | FCR inoperative | not recoverable | AVIONICS FAULT |

**FCR IFF interrogation mode (OSB 16)** `[p.241]`: `M+` = modes 1→4 in sequence · `M1` · `M2` · `M3` ·
`M4`. **TMS left long** interrogates in LOS mode.

### R3 — A-G FCR base page (PPI ground map) `[BMS-34 pp.226–241]`

**The A-G mode set is `GM`, `SEA`, `FTT`, `GMT`, `AGR` and `BCN` — BCN is not implemented** `[p.226]`.
Returns are drawn on a **plan-position-indicator (PPI, polar) sector format** `[p.238]`, in contrast to
the A-A B-scope. *This is the spec a ground-radar map implementation is built against.*

**Page layout, from the labelled figure** `[p.232]` (figure without coordinates; the vertical strings
on the right edge are the OSB 6–10 labels drawn one character per row):

| OSB / location | Label in figure | Function |
|---|---|---|
| **1** | `GM` | mode mnemonic: `GM` · `GMT` · `SEA` |
| **2** | `AUTO` | range-scale switching mode: `AUTO` (default) ↔ `MAN` |
| **3** | `NORM` | field of view: `NORM` · `EXP` · `DBS1` · `DBS2` |
| **4** | `OVRD` | FCR standby |
| **5** | `CNTL` | FCR control page |
| **6** | `BARO` | **backup bombing sensor** rotary: `BARO` (default at FCC/MMC power-up, uses system altitude) ↔ `RALT` (uses CARA). Also reachable from any OFF MFD page |
| **7** | `FZ` | freeze |
| **8** | `SP` | snowplow |
| **9** | `CZ` | cursor zero |
| **10** | `STP` | sighting-point rotary |
| **11** | `DCLT` | declutter (long press = options) |
| **12–15** | `TEST` `FLCS` `FCR` `SWAP` | display-format row (OSB 15 = SWAP, 14 = primary format highlighted) |
| **18** | `A6` | azimuth scan pattern rotary `A1`/`A3`/`A6` = ±10° / ±30° / ±60°; **initialized at ±60°** |
| **19 / 20** | ▽ / △ | range scale **10 / 20 / 40 / 80 NM** |
| above OSB 11 | — | TTG / TUI / TOF in the **lower right**, with TOT directly below |
| above OSB 15 | — | **bearing and range to SPI**: relative to the system steerpoint above the flying-W backup steering symbol, or relative to the bullseye if the mode-selectable bullseye is on (the flying W is then replaced by ownship bullseye symbology). SPI position **relative to ownship** is printed to the right of whichever symbol is drawn |

**Range scale and cursor bumping (A-G)** `[p.233]` — `AUTO` is on by default and is switched to `MAN`
by pressing OSB 2 or by any manual range change. In A-G search the auto bump **increases** range when
the cursor is at **95 % of the way up the MFD** and **decreases** it at **≤42.5 %**; the bump only
happens **while the cursor is not being slewed**. In FTT and GMTT the **target** position drives the
switch instead. Auto range scale is available in **GM, EXP, DBS1, DBS2, FTT, SEA and GMT**.

**Field of view / expansion** `[p.234]` — *this is the DBS spec:*

| Option | Availability | Effect |
|---|---|---|
| `NORM` | all A-G mapping modes | unexpanded PPI sector |
| `EXP` | all A-G mapping modes | **4:1 range and azimuth expansion** of the map patch around the cursor |
| `DBS1` | **GM only** | same FOV as EXP, **8:1 resolution** |
| `DBS2` | **GM only** | **roughly double the zoom of EXP/DBS1**, **64:1 resolution** |

Selected by **OSB 3** or the **EXPAND/FOV button on the stick**.
- **Expansion cues**: in normal `GM`, `SEA` and `GMT`, **four tick marks on the X-Y cursors** delimit the
  area that `EXP` would show; in `GM` with `DBS1` selected, the cues delimit what `DBS2` would show.
- **Situation awareness symbol**: in *any* expanded FOV (EXP, DBS1, DBS2) a **thin cross** marks where
  the X-Y cursors would sit on return to `NORM` — usable to read range to the sighting point.
- **Quarter-mile scale reference**: in any EXP-family FOV, a **horizontal line in the upper-left corner
  represents 0.25 NM (1500 ft)**.
- Range resolution improves **2:1 for each decrease in range scale** in NORM, EXP and DBS1;
  **changing range scale in DBS2 has no effect on range resolution** `[p.238]`.

**Range marks** `[p.238]` — concentric arcs, count and spacing by scale:

| Range scale (NM) | Range marks | NM per mark |
|---|---|---|
| 10 | 1 | 5 |
| 20 | 3 | 5 |
| 40 | 3 | 10 |
| 80 | 3 | 20 |

**Antenna symbols (A-G)** `[pp.237–238]` — azimuth = **T-symbol along the bottom**, display width
representing **±60°**, and **in A-G modes 0° is along the aircraft GROUND TRACK**, not the nose.
Elevation = **horizontal T on the left**, display height representing ±60°.
**Elevation scan is not selectable in A-G**: it is a **1-bar scan** except in FTT, GMTT and AGR
`[p.238]`. The GM 1-bar scan is roll- and pitch-stabilized `[p.238]`.

**Gain** `[p.238]` — the **GAIN rocker** (top-left of the MFD) trims map gain around the radar's own
default. Hands-on trim by rotating **MAN RNG/UNCAGE**, worth about **±20 %** of the base setting. A gain
indicator sits top-left next to the rocker: caret at the top = maximum, bottom = minimum.

**Freeze (FZ, OSB 7)** `[p.235]` — terminates transmission while the antenna keeps scanning; the frozen
map remains usable for navigation and weapon delivery, and the cursor can still be refined (the
cursor cross moves relative to the frozen map). An **aircraft position symbol drawn as a bold cross** is
continuously updated on the frozen scene, marking the ground point directly beneath the aircraft.
Deselected by changing FOV, changing FCR mode, or pressing FZ again.

**Snowplow (SP, OSB 8)** `[p.236]` — sighting LOS is **straight ahead in azimuth, referenced to no
steerpoint**. In GM/GMT/SEA the cursor is fixed **at the centre of the MFD** and the map "snowplows"
past it. Initially there is **no SOI and no cursor slew**: **TMS up** makes the radar SOI, ground-
stabilizes the cursor and enables slewing; **TMS up again over a target** commands track. The
ground-stabilized point becomes a **pseudo-steerpoint** that all NAV and weapon-delivery steering
(including great-circle steering) references. All SP cursor slews are **zeroed on deselect**, and the
display returns to the previous sighting point. **TMS down** only drops a ground lock, restoring the
pre-lock cursor position. SP is deselected by: OSB 8 · entering any A-G visual submode (CCIP, DTOS,
STRAFE, EO-VIS) · changing steerpoint **while ground-stabilized** · entering any A-A radar mode.

**Cursor zero (CZ, OSB 9)** `[p.236]` — zeroes accumulated A-G cursor slews. Available on **all A-G FCR
base pages, TGP base pages and OFF pages** while in A-G or NAV master mode.

**Sighting point rotary (OSB 10)** `[p.236]` — `TGT`/`STP` · `OA1`/`OA2` · `IP`/`RP`. **TMS right** does
the same when the SOI is the HUD or the FCR in A-G. An offset aimpoint with range zero is **skipped** in
the rotary. Changing the rotary via the MFD **breaks the A-G track**.

**Mode descriptions**

| Mode | Purpose and behaviour |
|---|---|
| **GM (Ground Map)** | Map for navigation and target detection; **ground-stabilized cursor** whose position also centres the scan coverage. Cursor is drawn at the intersection of the horizontal and vertical lines on the MFD **and mirrored by the steerpoint diamond in the HUD**. STP/OA/SP may be the initial cursor position; STP and OA continually resolve cursor range in all three axes. Transition to FTT available from `NORM`, `EXP`, `DBS1`, `DBS2` `[pp.238–239]` |
| **SEA** | Detects sea-borne targets in **low sea states**; identical control and operation to GM except that **more samples are integrated** (hence a slightly slower scan rate) and **there are no DBS submodes** — `NORM` and `EXP` only, plus `FZ` and transition to FTT `[p.239]` |
| **GMT (Ground Moving Target)** | Detects movers on land or sea (cars, tanks, trucks, ships, taxiing aircraft, helicopters) at low speed, over a background map that still serves navigation and stationary-target detection. `NORM` and `EXP` plus `FZ`. **GMTT (moving-target track) exists on the real APG-68 but is NOT modelled in BMS** — use a targeting pod to track a detected mover `[p.240]` |
| **FTT (Fixed Target Track)** | Automatic track of a stationary discrete return for weapon delivery; available from GM, SEA and DBS, initiated by **TMS up**, which searches around the cursor for returns brighter than background clutter. On track, the cursor lines become **target position lines with a solid diamond at their intersection**; **range rings, map information and expansion cues are removed**. Loss of track, TMS down, or reaching the antenna gimbal limit returns to the previous search mode with the cursor left at the last tracked (or gimbal-limit) position. Any mode change terminates the track `[pp.239–240]` |
| **AGR (A-G Ranging)** | Automatic slant range to a HUD/HMCS-indicated ground point, using a **pencil beam** and **LORO (Lobe On Receive Only)** — the receive beam squints above then below the transmit beam centre and the difference gives the range. Works at **all roll angles** (the LORO axis switches from up-down to left-right with roll). **Auto-commanded** by CCIP, DTOS, EOVIS, STRAFE, HUD MARK, HUD FIX (N/I), HUD ACAL (N/I) unless STBY/OVRD is selected `[pp.226, 241]` |

**Ground target track display** `[p.238]` — in GM, SEA and GMT a tracked target is a **solid diamond at
the X-Y cursor intersection**, and range appears in the **HUD slant-range window**.

**AGR page specifics** `[pp.228–229]` — range to the point is a **solid diamond on the right side of the
MFD**, positioned with **the top of the MFD equal to 10 NM**; **FOV options are unavailable and the
10 NM range scale cannot be changed**; **the gain gauge is not displayed**; antenna azimuth and
elevation markers are **body-referenced**. On loss of valid range the diamond becomes a **square at the
last valid range for ≈0.5 s**, then **pegs at the top-right corner**. A **St. George's cross** shows
antenna azimuth/elevation in addition to the edge carets.

**AGR ranging-point symbol by submode** `[p.241]`

| Submode | Symbol | Pointing method |
|---|---|---|
| CCIP | CCIP pipper (circle with dot) | manoeuvre aircraft |
| STRAFE | CCIP pipper (circle with dot) | manoeuvre aircraft |
| DTOS | A-G TD box (square with dot) | slew |
| EO-VIS | A-G TD box (square with dot) | slew |
| HUD Mark | — | slew |

**AGR accuracy notes** `[p.226]` — ranging works at all roll angles but **bombing accuracy improves as
release attitude approaches wings level**, increases with **grazing angle** and with **decreasing slant
range**. Using AGR/FTT/TGP-track/laser/RALT removes **SALT** from the HAT computation; a SALT error or a
wrong entered target elevation therefore produces a long or short miss.

## State

The FCR is built as a **mode set over one generic radar** — a scan volume *is* a mode, a lock is just
another volume. Everything else in this file (TGP, HMCS, HTS, Maverick seeker, A-G and A-Sea radar) does
not exist.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Generic active air-to-air radar: scan volume relative to the nose, range gate, frame time, firm/coast track life | **built** — `FBRadarSystem`; a target becomes a track after `kHitsToFirm` consecutive looks and coasts before it drops | [`../flightbox/sim/sensors.md`](../../sensors.md) §4 |
| CRM as the power-up search mode | **built** — ±60°/±10.5°, 40 nm, no auto-lock | [`module.md`](module.md) §4 |
| ACM sub-modes | **built, all four** — HUD scan ±15°/±10°, boresight ±5° cone, vertical scan ±5°/−13°…+47°, slewable ±10° around the cursor; 10 nm, each auto-locking the nearest firm track | same |
| STT | **built as its own volume** — gimbal ±60°, 0.1 s frame, single-target: the lock costs every other track file | same |
| TMS-forward designation | **built** — `Designate()` takes the published track number; a designated lock that breaks falls back to search rather than grabbing the next contact | [`../flightbox/sim/sensors.md`](../../sensors.md) §4 |
| The radar as an **emitter** | **built** — the mode being flown determines what is published to other units' RWRs (whole volume when searching, pencil beam in STT) | same, §4 (+ §5 for who hears it) |
| Chaff / Doppler notch | **built** — a clutter-filter model measured on own quantities only, no dice | same |
| Contact identity | **deliberately absent** — a contact carries range/bearing/az/el/closure and a track number, never a unit id, callsign or team; IFF Mode 4 is the only identity source and has no "hostile" value | same, §1 |
| SAM, TWS, DTT, spotlight, RWS-only sub-modes | **not implemented** — the mode set is CRM + 4×ACM + STT | [`module.md`](module.md) §4 |
| NCTR, AIFF, Mode 1/2/3 | **not implemented** — Mode 4 only | [`../flightbox/sim/sensors.md`](../../sensors.md) Gaps 9 |
| A-G / A-Sea radar modes (GM, GMT, SEA, MTR) | **not implemented** — the radar filters on `FBUnitKind::Aircraft`, so ground targets and stores in flight are invisible to every radar | same, Gaps 3 |
| TGP, HMCS, HTS, Maverick seeker | **not implemented** | — |
| The missile's own seeker | **built** as a second derivation of the same system — ±10° searched field, ±45° gimbal after lock, off until the guidance switches it on | [`../../weapons.md`](../../weapons.md) §10 |

**Two honest limits of the built radar**, both declared in the code: no terrain masking (air-to-air
line of sight is always clear), and no measurement error — geometry is exact, only availability,
volume, time and ageing are simulated, so a track can never jump to the wrong target.

## Gaps

**Source gaps** (this file vs. its sources)
- The `## Knowledge` section is an explicitly **SHALLOW** research pass (LRU designations + principle).
- **HTS/HMCS WPN-format and DED alignment-page detail (ED pp.492–523) is not distilled** to the depth of
  the FCR/TGP sections — flagged in the addendum above, not guessed.
- ED FCR/TGP pp.342–491 are processed; Chuck Part 10 is distilled to mode taxonomy plus display/HOTAS
  concepts, not tutorial by tutorial (PROGRESS.md, pass-1 depth note).
- ~~The sources give the FCR **mode taxonomy** but not the **page** — no OSB-by-OSB field map for the
  A-A or A-G FCR format, and no drawing spec for the ground map.~~ **CLOSED** by the BMS Dash-34
  addendum §R1–R3: OSB frame numbering `[BMS-34 p.63]`; A-A page fields, expanded target-data block,
  scan geometry, cursor bumping, range tics and target symbology `[BMS-34 pp.191–200]`; A-G page
  fields, EXP/DBS1/DBS2 ratios and expansion cues, range-mark table, gain, FZ/SP/CZ/sighting rotary,
  GM/SEA/GMT/FTT/AGR behaviour `[BMS-34 pp.226–241]`.
- **Still open (BMS-34)**: the **per-range-scale DBS patch dimensions** are not published — only the
  4:1 / 8:1 / 64:1 ratios and "DBS2 ≈ double the zoom of EXP/DBS1"; a map implementation must choose the
  patch size itself and declare it. **BCN mode and GMTT are explicitly not implemented in BMS**, so
  those two carry no spec at all. **HAD/TGP page field maps** (`[BMS-34]` HAD chapter from p.483, TGP
  chapter from p.316) are **not yet distilled** — next pass, flagged not guessed.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* CRM, the four ACM sub-modes, STT, designation, emission, chaff susceptibility, IFF Mode 4,
  and the anonymity of a contact.
- *Partially:* the mode *state machine* — FlightBox selects among volumes, but has none of the
  documented inter-mode transitions (e.g. TWS↔STT), and no radar display page to select them on;
  the mode is mission data (`set fcr_mode`) or a pilot command.
- *Not at all:* SAM/TWS/DTT/spotlight, all A-G and A-Sea modes, NCTR/AIFF, TGP, HMCS, HTS, Maverick
  seeker, terrain masking, measurement noise, ECM interaction.

## Knowledge

**Technical depth (researched — shallow pass — deepen when in scope)**

*Researched engineering depth. Kept separate from the guide distillation in `## Spec`; sources cited at
the end. This pass is explicitly **shallow** — deepen when the subsystem is in scope.*

> Combat sensors are outside the current rebuild scope (flight + rendering). This is an LRU/principle
> stub for future extension only.

### Components (LRUs)
- **FCR**: **AN/APG-68(V)** — mechanically-scanned (planar-array) pulse-Doppler fire-control radar,
  ~4 LRUs (antenna, transmitter, low-power RF, radar signal processor). Newer builds field the
  **AN/APG-83 SABR** AESA.
- **Targeting pod**: **AN/AAQ-33 Sniper ATP** or **AN/AAQ-28 LITENING** (EO/IR + laser designator/tracker).
- **HMCS**: **JHMCS** (Joint Helmet-Mounted Cueing System) — projects symbology on the visor for
  high-off-boresight cueing.
- **HARM targeting**: **AN/ASQ-213 HTS** pod (emitter geolocation).

### Functional principle
The APG-68 is a coherent pulse-Doppler set: it transmits phase-coherent pulse trains and range-Doppler
processes the returns, so airborne targets are separated from ground clutter by their closing-rate Doppler
shift (this is why look-down/all-aspect works). Air-to-air modes (RWS/TWS/STT) trade scan volume for track
quality; air-to-ground modes synthesize a ground map or detect moving targets by their Doppler. The pod
sensors are passive EO/IR with an active laser for ranging and guidance. All sensors are LRUs on the
1553/fiber avionics bus, arbitrated as the Sensor of Interest (SOI).

### Sources
- Wikipedia *AN/APG-68*, *AN/APG-83*; airforce-technology.com F-16 — radar/pod/HMCS designations.
- DCS guide Part 10 (mode taxonomy) — cross-referenced above.
