# F-16C Radar & Sensors

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 10 — Radar & Sensors, pp. 161–312 (152 pp).
This is a structural reference to the mode taxonomy + key display/HOTAS concepts; the guide contains
extensive per-mode tutorials not reproduced step-by-step here.

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

## State

The FCR is built as a **mode set over one generic radar** — a scan volume *is* a mode, a lock is just
another volume. Everything else in this file (TGP, HMCS, HTS, Maverick seeker, A-G and A-Sea radar) does
not exist.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Generic active air-to-air radar: scan volume relative to the nose, range gate, frame time, firm/coast track life | **built** — `FBRadarSystem`; a target becomes a track after `kHitsToFirm` consecutive looks and coasts before it drops | [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) §4 |
| CRM as the power-up search mode | **built** — ±60°/±10.5°, 40 nm, no auto-lock | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §4 |
| ACM sub-modes | **built, all four** — HUD scan ±15°/±10°, boresight ±5° cone, vertical scan ±5°/−13°…+47°, slewable ±10° around the cursor; 10 nm, each auto-locking the nearest firm track | same |
| STT | **built as its own volume** — gimbal ±60°, 0.1 s frame, single-target: the lock costs every other track file | same |
| TMS-forward designation | **built** — `Designate()` takes the published track number; a designated lock that breaks falls back to search rather than grabbing the next contact | [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) §4 |
| The radar as an **emitter** | **built** — the mode being flown determines what is published to other units' RWRs (whole volume when searching, pencil beam in STT) | same, §4 (+ §5 for who hears it) |
| Chaff / Doppler notch | **built** — a clutter-filter model measured on own quantities only, no dice | same |
| Contact identity | **deliberately absent** — a contact carries range/bearing/az/el/closure and a track number, never a unit id, callsign or team; IFF Mode 4 is the only identity source and has no "hostile" value | same, §1 |
| SAM, TWS, DTT, spotlight, RWS-only sub-modes | **not implemented** — the mode set is CRM + 4×ACM + STT | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §4 |
| NCTR, AIFF, Mode 1/2/3 | **not implemented** — Mode 4 only | [`../flightbox/sim/sensors.md`](../flightbox/sim/sensors.md) Gaps 9 |
| A-G / A-Sea radar modes (GM, GMT, SEA, MTR) | **not implemented** — the radar filters on `FBUnitKind::Aircraft`, so ground targets and stores in flight are invisible to every radar | same, Gaps 3 |
| TGP, HMCS, HTS, Maverick seeker | **not implemented** | — |
| The missile's own seeker | **built** as a second derivation of the same system — ±10° searched field, ±45° gimbal after lock, off until the guidance switches it on | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §10 |

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
