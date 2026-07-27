# F-16C Weapons & Armament

**Sources** (kept distinguishable throughout — cite the tag, not just a page number):
- **Chuck** = `doc/DCS F-16C Viper Guide.pdf` (Charles Ouellet, "Chuck's Guide"), Part 11 — Offence:
  Weapons & Armament, pp. 313–573. Tutorial-oriented, screenshot-driven.
- **ED EA Guide** = `doc/DCS F-16C Early Access Guide EN.pdf` (Eagle Dynamics, official module manual,
  704 pp). Pages used this pass: 34–42 (Weapons & Munitions overview), 303–341 (Weapon Delivery
  Sub-modes, SPI/cursors, SMS Inventory/Jettison), 524–632 (Air-to-Air / Air-to-Ground Weapons
  Employment), 703–704 (Appendix F formulas). Precise on **system/computation logic** (what is
  computed from what, when a cue appears, what a mode does) — this is the primary source for
  *rebuilding* SMS/weapon-delivery logic, not Chuck.
- Researched / derived material is in the **Technical depth** section at the end, confidence-tiered
  per the source hierarchy given in the task (T1 official/declassified docs > T2 manufacturer data >
  T3 established literature/DTIC/NASA/AIAA > T4 community/wiki, cross-check only).

This file was **SHALLOW**; this pass raises it to full depth on SMS logic, A-A employment (gun/AIM-9/
AIM-120), and A-G delivery-mode computation (CCIP/CCRP/DTOS/LGB/JDAM/JSOW/WCMD/HARM/Maverick) —
the exact scope FlightBox needs for the JSBSim-instance-per-weapon rebuild (own FDM, mass/CG carry
effect, external forces, impact physics, damage/effects).

## Spec

### 1. Chuck's Guide distillation (unchanged from previous pass)

#### SMS (Stores Management Set) page — Chuck p.313ff
Central weapons interface on the MFD: shows loaded stores per station, selected weapon, **profiles**
(PROF1…, each with a default release mode), release parameters. Master Arm switch (ARM/SIM/OFF) gates
employment. Weapon/station step via **NWS A/R DISC & MSL STEP** button.

#### Armament overview — Chuck
| Category | Weapons |
|---|---|
| Unguided bombs | MK-82 (low drag), MK-82AIR (high drag), MK-82SE (Snake Eye), MK-84 |
| Cluster | CBU-87, CBU-97, CBU-105 WCMD (wind-corrected, INS-guided) |
| Laser-guided | GBU-10 / GBU-12 / GBU-16 Paveway II |
| GPS-guided | GBU-38 JDAM, AGM-154A JSOW |
| A-G missiles | AGM-65D/G (IR seeker), AGM-65H/K (EO seeker) Maverick |
| Anti-radiation | AGM-88C HARM (homes on radar emissions → SAM sites; HTS pod) |
| Rockets | 2.75" rockets |
| Gun | M61A1 20 mm |

| Weapon (A-A) | Notes |
|---|---|
| M61A1 gun | 20 mm; EEGS sight |
| AIM-9L/M Sidewinder | IR, boresight/uncage |
| AIM-9X Sidewinder | IR, high-off-boresight (HOBS) via HMCS |
| AIM-120C AMRAAM | Active radar, BVR; TWS datalink post-launch |

#### Bomb delivery modes (A-G) — Chuck
| Mode | Concept |
|---|---|
| **CCIP** (Continuously Computed Impact Point) | Dive bombing: pipper on the bomb fall line shows live impact point; pickle when pipper crosses target |
| **CCRP** (Continuously Computed Release Point) | Straight-and-level; designate target, HUD gives release cue; auto-release while holding pickle |
| **DTOS** (Dive Toss) | Designate target on HUD, then toss |
| **LADD** (Low Altitude Drogue Delivery) | Not simulated in Chuck's coverage era |
| **MAN** (Manual) | Not simulated |

#### Guided-weapon employment (concepts) — Chuck
- **LGB**: designate + track with TGP, lase, release in CCRP; bomb homes on laser spot.
- **Maverick**: seeker is a sensor page; pre-planned/boresight/visual modes.
- **HARM**: HAS (self-detect) / POS (position, cued by HTS). ALIC tables = emitter threat priorities.
- **GPS (JDAM/JSOW)**: target coordinates → INS/GPS guidance; launch and leave.

#### Gun sights — Chuck
- A-G: M61A1 strafe with CCIP gun pipper.
- A-A: **EEGS** (Enhanced Envelope Gun Sight) — Level II (no radar, funnel) / Level V (radar-ranged
  lead-computing pipper).

---

### 2. ED EA Guide — official system logic (primary source for rebuild)

#### 2.1 Master modes & weapon-delivery sub-mode taxonomy (ED EA Guide p.294–311)
Master modes: **NAV / A-A / A-G / MSL (Missile Override) / DGFT (Dogfight) / JETT (selective) / JETT
(emergency)**. DOGFIGHT switch (3-position, throttle): outboard = DGFT (gun+AIM-9 HUD), inboard
(unlabeled) = MSL OVRD (AIM-120-only HUD, falls back to AIM-9 if none loaded), center = returns to
last master mode. Both override modes ignore ICP master-mode requests while active; radar/missile
settings changed while active persist for the mission (ED p.526–527).

**Weapon delivery sub-modes** are split into two families that determine how the target location enters
the fire-control solution (ED p.303–312):

| Family | Sub-modes | Target source |
|---|---|---|
| **"Pre-planned"** | CCRP, LADD, EO-PRE, EO-BORE, PRE, HARM (HAS/HTS variants) | steerpoint (a `sighting point`, see 2.2) |
| **"Visual"** | CCIP, DTOS, STRF, EO-VIS, VIS | pilot's eye / TGP video, via the TD Box, independent of steerpoints |

Cycled via **NWS A/R DISC & MSL STEP** (SSC) or SMS OSB 2; category determines whether sighting-point
options are shown at OSB 10 on FCR/TGP MFD formats (pre-planned only).

#### 2.2 Sighting points, cursors, SPI (ED EA Guide p.299–319) — the shared aiming substrate
This is the **generic mechanism every "pre-planned" A-G mode and every guided-weapon handoff sits on
top of** — the single most rebuild-relevant piece of ED's documentation, because it defines exactly
what "the target" means to the fire-control computer at any moment.

- **SPI (System-Point-of-Interest)**: the one 3-D point the fire-control system currently computes
  weapon solutions against. It is *assigned* by whichever sensor/cursor currently "owns" it (steerpoint,
  TGP track, FCR ground-map cursor, Snowplow cursor, …).
- **Sighting point types** (selectable at OSB 10 when in a pre-planned mode): **STP/TGT** (direct
  sighting — steerpoint itself is the aimpoint), **OA1/OA2** (Offset Aimpoint — a known landmark at a
  programmed range/bearing/elevation from the steerpoint, used to correct alignment when the true
  target isn't recognizable), **IP** (Initial Point, used in VIP mode), **RP** (Reference Point, used in
  VRP mode).
- **7 independent cursors**, only one pilot-controlled at a time via RDR CURSOR/ENABLE (throttle):
  Navigation cursor (corrects ALL steerpoints simultaneously, no INS update), VIP cursor, VRP cursor,
  Markpoint cursor, HMCS cursor, Visual cursor (TD Box in CCIP/DTOS/VIS/EO-VIS), Snowplow cursor
  (fixed at aircraft centerline, 50% of FCR range setting, ground-stabilizes on TMS Forward — "post-
  designate"). **Cursor Zero (CZ)** zeroizes only the currently-active cursor.
- **VRP (Visual Reference Point)** / **VIP (Visual Initial Point)** sighting modes: pre-planned attack
  geometry (TGT + RP/IP + optional Pull-Up Point PUP) programmed on DED pages ahead of a low-level
  pop-up attack, so the HUD gives a visual cue to align on before the target itself is visible.
- Changing the selected steerpoint while any sensor is tracking breaks that sensor's track (prevents
  runaway cursor slews from a stale designation).

**Rebuild takeaway:** an A-G weapon release computation always resolves to *one SPI position* (lat/lon/
elev) at the moment of pickle/consent; everything upstream (steerpoint math, TGP track, cursor slews)
is just different ways of setting that one point. This maps cleanly onto a single "current target point"
value in a FlightBox weapon-release system.

#### 2.3 SMS Inventory / Selective / Emergency Jettison (ED EA Guide p.334–340)
- **INV page**: gun ammo quantity (10-round increments, e.g. "51"→510 rds) + type (M56/PGU-28), full
  external-stores wingform.
- **S-J (Selective Jettison)**: OSB-selected per-station jettison of fuel tanks / A-G weapons / A-G
  racks only (A-A missiles, A-A rails, ECM/travel pods **cannot** be jettisoned). Weapon Release button
  jettisons **unarmed**, regardless of Master Arm position.
- **E-J (Emergency Jettison)**: same store-class restriction, triggered by a dedicated cockpit button,
  overrides current master mode while held.

#### 2.4 Stations & external-stores codes (ED EA Guide p.335–336, 34–42)
9 hardpoints: **1/9** (wingtip, LAU-129 A-A rail only, 250 lb class per f-16.net community data — T4,
see Technical depth), **2/8** (outboard underwing, A-A rail only), **3/4/5/6/7** (multi-role — A-A/A-G
munitions, fuel tanks, ECM/travel pods; 3-line SMS inventory format since a rack/launcher + weapon may
both be present). Load examples in the guide: TER-9/A triple ejector rack (up to 3× Mk-82-class),
BRU-57/A "Smart Multiple Carriage Rack" (2× JDAM/JSOW/WCMD-class), MAU-12 single ejector rack,
LAU-117/LAU-88 (Maverick, 1 or 3 respectively), LAU-118(V)2/A (HARM).

Weapon/store SMS codes (selection): `M82`/`M84` (Mk-82/84 slick), `B49`/`B50` (Mk-82/84 AIR-retarded),
`M82S` (Snakeye), `GB12`/`GB10C`/`GB24A` (GBU-12/10/24 LGB), `GB38`/`GB31A`/`GB31B` (JDAM variants),
`CB87B`/`CB97B` (unguided cluster), `CB103`/`CB105` (WCMD-guided cluster), `A154A` (JSOW), `AG65D/G/H/K`
(Maverick variants), `AG88` (HARM), `A120B/C` (AMRAAM), `A-9LM`/`A-9X` (Sidewinder), `L03`/`L68`/`L131`
(rocket pods), `TK300`/`TK370` (fuel tanks), `AL131`/`AL119` (ECM pods).

#### 2.5 Air-to-Air weapons employment (ED EA Guide p.524–548)

##### M61A1 20 mm cannon — gun/EEGS (ED p.528–533)
- **6-barrel Gatling, 512-round capacity, 6,000 rd/min** (ED figure; cross-check with researched values
  below).
- **Dispersion**: 8 mil diameter cone for 80% of rounds, 12 mil for 100% (ED cites **MIL-DTL-45500/1A**:
  "at 1,000 in range, 80% of a 75-round min. burst within an 8.0 in diameter circle"). 1 mil = 1/1000 rad
  → 8-ft circle at 1,000 ft, growing linearly with range. Dispersion is circular only against a target
  perpendicular to the flight path; elliptical against a horizontal (ground) target.
- **EEGS (Enhanced Envelope Gun Sight) levels**:
  | Level | Condition | Symbology |
  |---|---|---|
  | I | RSU + INS failure (rare) | Boresight Cross only |
  | II | No radar lock | Boresight Cross + EEGS Funnel + MRGS (Multiple Reference Gunsight) lines |
  | III/IV | Transitional (rarely seen by pilot) | — |
  | V | After radar lock + firing solution | Level II symbology **replaced** by Target Designator/T-Symbol/Range/Closure/Level-V Pipper |
- **Funnel geometry (Level II)**: each point on the funnel = the target's *known wingspan* at the range
  for which the gun is correctly aimed *right now*; closing range → target must sit higher in the funnel
  (closer to Boresight Cross) for correct lead. **Funnel range limits: top ≈ 600 ft (min), bottom ≈
  3,000 ft (max)** — target smaller than the funnel bottom = out of range.
  Requires the target's actual wingspan to be configured for correct scaling.
- **MRGS lines**: 5 line segments near the HUD bottom pointing at the Gun Bore Line, aiding lateral lead
  for long-range/high-aspect shots; target smaller than the line = out of range or faster than expected
  (needs more lead), larger = slower (needs less lead).
- **Level V (radar-locked) T-Symbol**: **"+"/one-G pipper** = lead for a non-maneuvering target;
  **short horizontal bar/nine-G pipper** = lead for a target turning at max sustained rate. Level V
  Pipper = the actual computed firing solution from current target range+rates; maneuver-potential
  lines either side of the 1-G pipper scale with the target's out-of-plane maneuver potential.
  Target Range Caret on the circular Target Designator maps clock position → range: **12 o'clock =
  12,000 ft, 9 = 9,000 ft, 6 = 6,000 ft, 3 = 3,000 ft** (i.e. clock angle is a linear 0–12,000 ft radial
  scale, wrapped).
  **BATR (Bullets-At-Target-Range)** symbol appears from first-round-at-target-range to last, radar-lock
  + Level III/IV/V only.

##### AIM-9M/X Sidewinder (ED p.534–542)
- **SPOT/SCAN** (SMS OSB): seeker narrow FOV (SPOT, longer detection range) vs wide FOV via nutation
  (SCAN, shorter range; **SCAN not implemented in DCS**).
- **SLAVE/BORE** (SMS OSB, or hold RDR CURSOR/ENABLE to override momentarily): SLAVE = seeker follows
  radar LOS; BORE = seeker stares down aircraft boresight.
- **WARM/COOL**: argon-bottle seeker cooling; COOL auto-selected on entering DGFT/MSL OVRD.
  **Average argon supply ≈ 90 minutes** (ED — varies with OAT/pressure/bottle charge).
- **Missile Diamond** = seeker LOS indicator (starts at boresight, unlatches to follow radar LOS or a
  locked target); **Missile Reticle** = seeker FOV size (SPOT/SCAN dependent). **Growl tone** = IR
  detection; pitch rises on **uncage** (Cage/Uncage button) → seeker self-track lock, diamond latches.
- **HMCS BORE employment**: same mechanism, but LOS source = HMCS Dynamic Aiming Cross instead of HUD
  boresight; diamond clamps + shows an "X" if commanded beyond missile seeker gimbal limits.
- **HMCS Radar-BORE employment**: ACM BORE radar mode slaved to HMCS cross (TMS Forward arms), radar
  locks first target in the ACM BORE symbol; AIM-9 SLAVE then follows radar LOS.
- **DLZ (Dynamic Launch Zone)**, shown once radar-locked: **missile diamond flashes** inside **Raero**
  (max aerodynamic/kinematic range — effective only vs a non-maneuvering target); **missile reticle
  flashes** inside **Rtr** (turn-and-run range — guaranteed hit even if target immediately reverses).
  DLZ shows range scale, closure rate/range caret, min missile range.

##### AIM-120 AMRAAM (ED p.543–548)
- **Active-radar-homing (ARH)** BVR missile; **initial guidance = datalink command from launching
  aircraft**, **transitions to onboard active-radar terminal homing** once in seeker range — this
  datalink→active handover is *the* system-logic fact needed to model launch/guidance phases.
  Can be launched **BORE** (no lock) — tracks first target detected in the HUD reticle after launch.
  Engagement range highly dependent on aspect/altitude/launch speed/target maneuvering — can be
  <10 nm even though max range is quoted >20 nm.
- **Line of Sight**: SLAVE (missile LOS slaved to aircraft radar; receives datalink steering until
  in radar range, then attempts track) or BORE (scans straight ahead, tracks first post-launch
  detection). RDR CURSOR/ENABLE toggles SLAVE↔BORE.
- **HUD, no lock**: Missile Diamond (seeker LOS) + **ASEC (Allowable Steering Error Circle)** — the
  zone the **ASC (Attack Steering Cue)** must sit inside pre-launch for high-Pk; **ASEC radius grows
  as range-to-intercept-point shrinks** (closer target tolerates more steering error at launch).
- **HUD, target locked — DLZ fields** (this is the AMRAAM-specific launch-zone staple, distinct
  numerically from the AIM-9 DLZ):
  | Cue | Meaning |
  |---|---|
  | **Raero** | max kinematic range — hits only a non-maneuvering target |
  | **Closure caret** | current target range against the DLZ + closure rate (knots) |
  | **Rtr** (turn-and-run range) | guaranteed hit even if target reverses 180° at launch |
  | **Radar Activation Range** | range at which the missile's own seeker goes active, no longer needs launch-aircraft support |
  | **Rmin** | closest range allowing seeker activate+lock+arm+detonate safely |
  | **Countdown timer** (post-launch) | "A"+seconds-to-activation, then "T"+seconds-to-predicted-impact |
- **FCR post-launch symbology**: target with AMRAAM in flight = **magenta, solid tail** (opposite trend
  vector); once missile **goes active** = **red, flashing tail**; at **predicted impact time** = flashing
  "X" through the target.
- **Simultaneous multi-target employment**: up to **4 simultaneous in-flight AMRAAMs vs up to 4
  targets**, via TWS or RWS DTT; each target designated ("bugged") via TMS Forward, cycle bug with
  TMS Right between shots. DLZ is only shown for the *currently bugged* target.

#### 2.6 Air-to-Ground weapons employment (ED EA Guide p.549–632)

##### M61A1 strafe (ED p.551–554)
- STRF sub-mode (A-G master mode). **Strafe Reticle** pipper = aim point; **ranging reticle** winds/
  unwinds around it, position encoding slant range on the same clock convention as the A-A gunsight
  (**12 o'clock = 12,000 ft ... 3 o'clock = 3,000 ft**, each quarter-tick = 3,000 ft of slant range).
  Pilot-settable **In-Range Cue** distance overlay.
- **Effective engagement range ≈ 2,500–7,000 ft** (ED); closer is better vs armor, attack from behind
  where armor is weakest.

##### 2.75" rockets — CCIP (ED p.555–557)
- SGL (one launcher) / PAIR (both launchers, requires stations 3+7 loaded) release option.
- **In-Range Cue appears when slant range < 8,000 ft** — rockets "most effective" inside that range.

##### Unguided bombs — CCIP / CCIP post-designate / CCRP (ED p.558–571)
General setup shared by all A-G bomb types (guided or not): **profile** (PROF1/PROF2, retains
delivery-mode/fuze/spacing/quantity settings), **fuzing option** (NOSE/TAIL/NSTL — NSTL = both,
typical default for redundancy; **special cases**: Mk-82 AIR/SE → NSTL/TAIL = high-drag deployed,
NOSE = low-drag "slick"; CBU-87/97 → NSTL = normal dispense per SMS settings, NOSE = immediate
dispense on release, TAIL = dud), **single/pair** release (opposite stations 4/6 or 3/7), **release
interval distance** (10–999 ft between weapons in a "stick"), **release pulses** (ripple count).

- **CCIP** (visual delivery mode): CCIP Pipper = live continuously-computed impact point. If the
  computed impact point is outside the HUD FOV, a **Time Delay Cue (TDC)** — a short horizontal line
  on the Bomb Fall Line — replaces the pipper; **when TDC is gone, the pipper position IS the impact
  point if released now**. **Pull-Up Anticipation Cue (PUAC)**: visual altitude margin required either
  for (a) the fuze to arm or (b) to avoid ground impact, whichever is more restrictive — moves toward
  the FPM as altitude is lost; **releasing with FPM below PUAC → dud** (insufficient arming time/
  altitude). Bombs release **at the center of the stick** for ripple deliveries.
- **CCIP post-designate**: used when the target can't be kept in the HUD FOV (shallow dive / high
  altitude). Pilot places the pipper on the target *while the TDC is showing* and **presses+holds**
  Weapon Release to designate that ground point; symbology then switches to a **CCRP-style Steering
  Line + Solution Cue**, and release happens automatically when the **Solution Cue passes the FPM**
  while the pilot keeps the FPM on the Steering Line and the button held.
- **CCRP** (computed, automatic release from any attitude — dive, level, nose-high): requires a target
  designation (steerpoint OR TGP designation) to build the solution. **Steering Line (SL)** + **Solution
  Cue** at its top, falling down the SL as range/time-to-release decreases; pilot flies FPM onto the SL
  and **holds** Weapon Release — release fires automatically **when the Solution Cue reaches/passes the
  FPM**. **Target Locator Line (TLL)** extends from the Gun Cross toward an off-HUD target, with
  relative-angle readout. Same PUAC logic applies. **"Time to Release" countdown** shown bottom-right.

##### Laser-guided bombs — GBU-10/12 Paveway II (ED p.572–579)
- **CCG (Computer Control Group)** on the nose: laser detector + processor driving 4 canards, "bang-
  bang" guidance (canards always full-deflect, no proportional control — hence a sinusoidal terminal
  path). Guidance canards + rear wing assembly are the whole kit; no aircraft datalink after release.
- **3 flight phases**: **ballistic** (unguided, established at release — delivery attitude/airspeed at
  this point matters because velocity loss here directly reduces terminal maneuverability), **transition**
  (begins at laser acquisition, weapon aligns velocity vector with LOS to the laser spot), **terminal**
  (keeps velocity vector aligned with instantaneous LOS; when aligned, canards trail and the bomb flies
  ballistically, gravity-biased toward the target).
  **Rebuild-relevant point:** LGB guidance is a bounded corrective (bang-bang) overlay on an otherwise
  ballistic trajectory, not free-flight guidance — a JSBSim weapon instance could model it as a
  discrete-deflection aero-control law layered on the bomb's own ballistic aero model.
- **Laser PRF code** must match between bomb seeker (set on the ground, immutable in flight — kneeboard
  page in DCS) and designator (LASR DED page). Lase window: **squeeze trigger to lase no later than
  8–12 seconds before impact** (ED); L flashes on HUD while firing.
- CCRP delivery identical to unguided bombs, plus the laser-designation requirement; a 30–45° check
  turn post-release avoids TGP gimbal roll while continuing to track/lase.

##### JDAM — GBU-38/GBU-31 (ED p.580–586)
- **INS/GPS fire-and-forget**, coordinates **downloaded to the weapon during the release-button hold**
  — releasing the button *before the download completes* leaves a **hung store** (unusable). This
  makes the "hold weapon-release" duration a hard functional gate, not just a UX convenience.
- **Alignment**: powers on with "A10" (unstable), counts down to "RDY". Takes on the order of minutes.
- **Profile parameters** (4 profiles, editable on the SMS Control page): **Arming Delay** (selectable
  4.0–25.0 s in the guide's option list, plus 14/21/25 s), **Fuzing** — AIR (air-burst, less
  penetration/more area effect) / GND (impact, selectable fuzing delay 0/5/15/25/45/60/90/180/240 ms)
  / GND DLY (delayed post-impact detonation, 0.25–24 hours selectable), **Impact Angle**, **Impact
  Azimuth** (0 = no constraint; e.g. 360 = attack from due south heading north), **Impact Vertical
  Velocity** (ft/s — higher = more penetration).
- **HUD**: Azimuth Steering Line to the **LAR (Launch Acceptability Region)**, Current-Range caret vs
  In-Range Bracket, Bearing/Distance to the current SPI (the point the bomb will fly to after release).
- **PRE mode**: guides to SPI (steerpoint or TGP designation). **VIS mode**: HUD becomes SOI, TD box
  slewed by TDC, TMS Forward designates.

##### JSOW — AGM-154A (ED p.587–591)
- Inertially-aided **glide bomb**, up to **≈70 NM** depending on launch altitude/speed (ED). Same
  SPI-download-on-button-hold mechanism as JDAM; AGM-154A carries BLU-97/B submunitions and **cannot
  be re-targeted after launch**. Ripple setting: single or paired (longitudinal/lateral separation).

##### WCMD — CBU-103/CBU-105 (ED p.592–597)
- Tail kit (onboard INS, wind-corrected) turning CBU-87→CBU-103 and CBU-97→CBU-105. Profile page:
  **Burst Altitude** (height-of-function MSL — higher = wider dispersal), **Spin Rate** (RPM, CBU-103
  only — higher = wider dispersal), Attack Azimuth/Arming Delay (listed but **not implemented** in
  DCS at guide date), Wind Source (mission-planning/pilot-entered/system — only MP implemented).
  Same LAR/in-range-bracket HUD family as JDAM/JSOW.

##### AGM-88 HARM (ED p.598–611)
- **Three targeting modes**: **HAS** (HARM-as-Sensor, self-detect via missile's own radar receiver —
  bearing only, no range, so **the missile does not loft**, reducing effective range), **POS**
  (Position Known — target handed off from a steerpoint, radar, or HTS; profile choices **EOM**
  [equations of motion] / **PB** [pre-briefed] / **RUK** [range unknown]), **DL** (datalink — listed
  in the guide but **not implemented in DCS**).
- **ALIC (Aircraft Launcher Interface Computer)**, in the **LAU-118 launcher**: brokers HARM sensor
  video to the SMS and hands threat-type off to the missile; missile then homes on radar emissions
  matching the handed-off type after launch. **Threat tables** (TBL1/2/3, 5 entries each, editable via
  ALIC codes on the HARM DED page) hold up to 5 threat types per table simultaneously scanned;
  narrowing the active threat set or FOV **reduces scan cycle time** (faster detection).
- **HAS WPN format**: FOV setting (CTR/LT/RT/WIDE — which portion of the missile's forward hemisphere
  it searches), scan counter, **DTSB (Detected Target Status Box)** lists detected threat types;
  ALIC-video threat glyphs suffixed **"A" = actively radiating**, **"T" = tracking (guiding an
  in-flight missile against anyone)**, none = memory/co-located threat.
- **POS WPN format** — the **HLS (HARM Launch Scale)**: combines **AMZ (Aircraft Maneuver Zone —
  reachable only if the launch aircraft lofts/turns toward the target first)** and **MMZ (Missile
  Maneuver Zone — reachable by the missile's own maneuvering alone)**; **the pickle button is only
  "hot" while the caret is inside the MMZ**. Min/max/optimal loft angle ticks on the ASL (optimal =
  max terminal energy, PB mode only); release-altitude/required-turn datablock computed assuming a
  4-g loft to optimal (or max) loft angle. **Loft cues and the HLS staple are inhibited in HAS mode**
  (no range data available). **In-flight missile datablocks** show predicted time-to-impact and the
  handed-off threat type/steerpoint per missile.

##### AGM-65 Maverick (ED p.612–632)
- Onboard **imaging seeker** (IR: D/G; CCD/EO: H/K) — fire-and-forget once locked. **Warm-up**:
  image-stabilizing gyros spin up (video usable before spin-up but not ground-stabilized; Uncage-button
  press activates video early at 90% gyro speed).
- **MBC (Missile Boresight Correlator)** — TGP-to-missile handoff mechanism: compares TGP image to
  missile-seeker image and **slews the missile seeker head until they match**; status progression
  shown per station: **S**(lave, not yet commanded) → **1**(slewing LOS) → **2**(slewing video match)
  → **T**(rack, missile commanded to track) → **C**(omplete) or **I**(mpossible, handoff failed).
- **Post-launch forced correlation**: once the tracked target fills **≈75% of the seeker FOV**, the
  missile switches from centroid tracking to forced correlation to continue guiding to impact.
- **Seeker gimbal limits (AGM-65D)**: **±42° horizontal, +30°/−54° vertical** (ED figure). Pointer
  cross flashes if seeker LOS > **10° off boresight** or track image too small — both are hard launch-
  inhibit conditions.
- **Polarity**: HOC/COH (hot-on-cold / cold-on-hot, IR contrast convention) toggle; **AREA mode**
  (AGM-65G/H only) = **force-correlate mode**, tracks a designated image *feature* (e.g. base of an
  antenna) via image correlation instead of a target centroid — useful against structures where a
  specific aimpoint on a static target matters.
- **Ripple fire ("quick-draw")**: up to 2 Mavericks queued against 2 separate targets simultaneously
  (requires LAU-117 pylons); 2× 10-mr LOS circles ("1"/"2") on HUD show tracking + fire order.
- **Duty cycle**: standby 1 hour, active video 30 minutes (ED figure — matches the 3-minute warm-up
  window noted separately in the same chapter).

---

### 3. Weapon & munition specs (ED EA Guide p.34–42) — official module-level numbers

| Weapon | Key ED-quoted figures |
|---|---|
| M61A1 Vulcan | 20×102 mm; 510-round drum; 6,000 rd/min; 6 ammo types (M56 HEI, M56/M242 HEI-T, M53 API, M55/M220 TP, PGU-28A/B SAPHEI, PGU-27A/B TP) |
| AIM-9 Sidewinder | Up to 5 scanning IR sensors, argon-cooled (L/M models); **max speed >Mach 2.5**; **max range ≈10–20 mi** (variant-dependent); **min range ≈3,000 ft** |
| AIM-120 AMRAAM | Command guidance + active radar homing; **max speed ≈Mach 4**; **max range 30–40 mi** (ED's own overview figure — see DLZ range caveats in §2.5) |
| AGM-88 HARM | Passive radar homing + inertial mid-course; **max speed Mach 1.84**; **operational range ≈80 NM**; laser proximity fuze |
| AGM-65 Maverick | **Max range ≈13 NM**; D/G = 125/300 lb shaped-charge (IR); H/K = 125/300 lb shaped-charge (CCD/EO) |
| Mk-82 | 500 lb GP bomb | 
| Mk-84 | 2,000 lb GP bomb, **945 lb H-6/Tritonal** explosive fill (Chuck) |
| CBU-87 CEM | 950 lb, SUU-65/B canister, **202× BLU-97/B**; footprint ≈200×400 m (Chuck) |
| CBU-97 SFW | 1,000-lb class, SUU-66/B canister, **10× BLU-108/B**, each releasing **4 "Skeet" EFP submunitions** (laser+IR fuzed, self-destruct if no vehicle detected) |
| WCMD (CBU-103/105) | Onboard INS, wind-programmable; **as low as 85 ft CEP** (ED) |
| GBU-10 | 2,562 lb (Mk-84-based Paveway II) |
| GBU-24A/B | BLU-109 2,000-lb hardened-penetration body + Paveway III kit (larger control surfaces, shaped-trajectory guidance, terminal-impact options) |
| JDAM (GBU-38/31) | Mk-82/84-based INS/GPS kit; cannot re-target or hit moving targets post-release |
| JSOW (AGM-154A) | Folding-wing glide bomb, **145× BLU-97/B submunitions**, range dependent on launch altitude/speed |
| 2.75" rockets (Hydra 70) | M151 HE, M156 WP (marking smoke), Mk5 HEAT, practice rounds (Mk61/WTU-1) |
| External tanks | 370-gal wing tank ≈2,500 lb fuel; 300-gal centerline tank ≈2,000 lb fuel |

## State

FlightBox carries **three weapons and one delivery apparatus**: AIM-120, Mk-82, M61A1 — plus ground
targets to drop on. The SMS/SPI machinery of this file is not rebuilt as displays and cursors; the parts
that *decide* something are.

| Item of this reference | FlightBox | Where |
|---|---|---|
| A released weapon as a real object | **built, and it is the foundation**: a store that leaves the jet becomes a full unit — own FDM on its own pinned model, own module, own telemetry file, judged by the same two monitors | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §1 |
| SMS: station inventory, master arm, the single release path | **built** — `FBStoresSystem`; a loaded station is a JSBSim point mass and the drag-area sum an external force, so carriage effects are engine physics | same, §2 |
| M61A1 gun | **built** — 20 mm, 6,000 rpm, 510-round drum, the burst cut into ballistic bundles, integrated rate so the drum empties in the right number of seconds | same, §3 |
| Gun ballistics + lead solution (the EEGS-class computation) | **built and shared** — the same arithmetic serves the fire-control solution before the shot and the projectiles after it | same, §4 |
| AIM-120 employment: DLZ (Raero/Rtr/Rmin), launch, midcourse uplink, terminal handover | **built** — the launch zone is a forward integration of the weapon's performance table against the current radar geometry; the missile flies proportional navigation with its own seeker and uplink | same, §10 + [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §8 |
| CCIP / CCRP | **built from ONE integration** — the same ballistic forward integration answers "where does it land" (pipper) and "when must I release" (countdown), so cue and countdown cannot disagree | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §4 |
| Weapon effects | **built** — fragment area density → specific energy → per-zone damage against a system-health register; no dice, no timers | same, §6–§8 |
| Station data (nine pylons) | **partially** — lateral offsets are modelled, longitudinally all nine sit on the CG station because §4.5's station data is itself flagged T4. A load therefore produces no pitching moment | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) Gaps 9 |
| Master modes, SPI, cursors, sighting points, SMS pages, inventory/jettison | **not implemented** — no display to hold them; the target is the active steerpoint, the "cursor" does not exist | — |
| AIM-9, LGB, JDAM, JSOW, WCMD, HARM, Maverick, DTOS, strafe | **not implemented** — no AIM-9 at all (roadmap R6), no guided A-G weapon, and gun rounds give up before reaching the ground so strafing is impossible | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) Gaps |

**Caveat that must travel with any accuracy number:** the measured CCIP/CCRP error (22 m total, 10.6 m
lateral) is measured against **our own ballistic table** with a Mk-82 model whose own note calls itself a
possibly crude approximation. It is a statement about internal consistency, not about real-release
fidelity — see [`../flightbox/aircraft/stores.md`](../flightbox/aircraft/stores.md).

## Gaps

**Source gaps** (this file vs. its sources)
- §4.7 above ("What's genuinely still a gap") is the itemised list and is left **in place under
  Knowledge** so its numbering stays citable: M61A1 spool-up time, individual PGU round mass, AIM-9X
  cooler duty cycle, AIM-120 Rmin/seeker-activation as fixed constants, HARM scan-cycle formula,
  JDAM/JSOW CEP, the Block-50 CMDS cartridge mix, FMU-139-vs-JDAM-kit fuzing.
- **Station data is T4** (community cross-check only) — flagged in §4.5, and the reason FlightBox
  declines to model a pitching moment from carriage.
- **510 vs. 512 rounds**: the same guide says both (§3 specification table vs. §2.5 text). Both are
  kept; FlightBox takes 510 (specification table wins) and records the difference rather than averaging.
- Chuck Part 11 and ED pp.34–42/303–341/524–632/703–704 are processed; ED Appendix C (threat tables) is
  **not**.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* stores management and release, gun firing and its ballistics, one A-A missile end to end,
  CCIP/CCRP, weapon effects on systems.
- *Partially:* carriage (mass and drag yes, pitching moment no); the A-A launch zone (no lofted
  midcourse, so every computed `Raero` is that of a flat-flying round); the gun (no round mass model,
  no installation angle, no strafing).
- *Not at all:* the SMS/SPI/cursor user model, master-mode-driven sub-mode taxonomy, jettison, every
  weapon outside {AIM-120, Mk-82, M61A1}.

## Knowledge

### 4. Technical depth (researched + derived — deepened this pass)

*Section number kept for cross-reference stability (`§4.1`, `§4.5` are cited from the code).*

Confidence tiers per source hierarchy: **T1** official/declassified mil docs (T.O./MIL-STD/AFMAN,
DTIC/NASA/AIAA) · **T2** manufacturer datasheets · **T3** established literature/databases (Jane's,
FAS, GlobalSecurity, peer-reviewed) · **T4** community/wiki — cross-check only, flagged explicitly.
**[derived]** = computed from documented inputs via a stated formula, not itself a cited fact.

#### 4.1 M61A1 Vulcan — quantitative firing model
- **Muzzle velocity ≈ 3,380 ft/s (1,030 m/s)** for standard rounds; **PGU-28/B ≈ 3,450 ft/s (1,050 m/s)**
  — T4 (f-16.net armament article, Wikipedia; consistent across sources, no T1/T2 found this pass).
- **Rate of fire**: 6,000 rd/min nominal (ED T-source-equivalent + T4 corroboration); **spool-up ≈0.3 s**
  to reach full rate before the first round exits — T4 (multiple community sources agree; **TODO**:
  find a T1/T2 spool-time citation, e.g. from GD-OTS/General Dynamics Ordnance & Tactical Systems
  data or a T.O. 1F-16C-34-1-1 excerpt).
- **Ammunition mass** (needed for FlightBox mass/CG-during-fire and projectile-ballistics modelling):
  **TODO** — round mass (~100 g class per ED's dispersion-formula reference to a 3.5 oz/100 g
  projectile) needs a T2/T3 citation before use as a simulation input; do not treat the ED dispersion
  footnote's projectile mass as authoritative spec data (it appears in service of the mil-spec
  dispersion-angle citation, not as a weapon datasheet entry).
  **Gym note**: once a gun/projectile FDM exists, muzzle velocity + spool curve + dispersion cone are
  all directly measurable in `fb-gym` from fired-round telemetry — cheaper to verify than to keep
  chasing a T1 citation for the spool curve.

#### 4.2 Bomb ballistics & fuzing — derived + researched
- **FMU-139 series fuze** (used on Mk-82/83/84 low/high-drag, BLU-109/110/111, limited JDAM) — T3
  (NAVAIR 11-1F-2 family documents / DocsLib fuze-system excerpts). **In-flight-selectable arming
  times**: high-drag **2.0 / 2.6 / 4.0 / 5.0 s**; low-drag **4 / 6 / 7 / 10 / 14 / 20 s**. Detonation-
  delay options: instantaneous, 10 ms, 25 ms, 60 ms (T3 figures; note ED's own JDAM profile page lists
  a *different*, JDAM-specific delay set — 0/5/15/25/45/60/90/180/240 ms — these are NOT the same
  fuze family, don't conflate them).
  **This directly parametrizes FlightBox's PUAC logic** (§2.6): the PUAC altitude threshold is a
  function of release dive angle, release altitude, ballistic fall time to the selected arming time,
  and terrain clearance margin — i.e. it's a **derived** cue, not a fixed altitude, and the arming-time
  table above is the input needed to compute it once release-mode selection maps to a specific fuze
  timer.
- **[derived] Free-fall time / impact velocity, drag-free approximation**: for a release from height
  `h` with zero initial vertical velocity, `t_fall = sqrt(2h/g)`, `v_impact_vertical = g·t_fall` (g =
  32.174 ft/s²). This is a **lower bound on fall time** / **upper bound on nothing meaningful** for a
  real finned bomb — actual Mk-82-class ballistic trajectories are drag- and dive-angle-dependent and
  materially longer/shallower than the vacuum case, which is exactly why JSBSim (real aero coefficients
  per store) is the right tool rather than a hand formula. Use this formula only as a sanity floor
  (e.g. "a bomb released at 5,000 ft AGL cannot impact in under ~17.6 s even with zero drag"), never as
  a release-solution substitute.
  **Gym note**: true fall time/impact point/impact velocity per store become directly measurable once
  each weapon is its own JSBSim instance with the vendored aero model — this whole ballistics gap
  converts from "research it" to "simulate it and read the CSV" at that point.
- **CEP (Circular Error Probable)**: WCMD "as low as 85 ft" is ED's own figure (kept in §3, not
  independently re-verified this pass — no T1–T3 source found in this pass's research budget).
  **TODO**: JDAM/JSOW CEP figures (commonly cited ~5–13 m class in T3 literature) not yet cross-checked
  against a T1/T2 source; do not add a number without one.

#### 4.3 AIM-9 Sidewinder — seeker system facts
- **AIM-9M seeker gimbal limit ≈ ±30° (some T4 sources describe an off-boresight cueing figure closer
  to ±25°)**; AIM-9X gimbal significantly wider, commonly cited **up to ~90°** off-boresight given HMCS
  cueing — T4 (GlobalSecurity, Falcon BMS community technical threads; **TODO** find a T2 Raytheon
  datasheet or T1 NAVAIR/AFMAN figure — public unclassified specs on exact gimbal limits are thin).
- **Cooling**: AIM-9M uses an **argon-bottle** seeker cooler (ED's own figure of **~90 min average
  duration** is the most concrete number available this pass — keep as ED-primary, not independently
  re-derived). AIM-9X replaces the bottle with an **internal cryogenic cooler**, eliminating rail/
  launcher-supplied coolant dependency — T3/T4 (GlobalSecurity AIM-9X page). Exact AIM-9X internal
  cooler duty-cycle/duration: **TODO**, not found in this pass's public-source budget.
- **Minimum range**: ED's own armament overview states **≈3,000 ft**; this is consistent with general
  IR-missile minimum-range figures (arming distance + minimum seeker lock-on geometry) but no
  independent T1–T3 number was found this pass to refine it further. **Gap, not derivable** — minimum
  range is a doctrine/hardware-fuze decision, not something physics alone fixes.
- **Uncage behavior**: per ED (§2.5) — uncage releases the seeker from boresight-caged state to track
  within gimbal limits once IR detection criteria are met; this is confirmed generically by public
  Sidewinder-family descriptions (T3/T4) but the **specific IR-detection threshold logic (SNR gate,
  discrimination against flares) is not public** — correctly left as a gap, not guessed.

#### 4.4 AIM-120 AMRAAM — range/guidance-phase facts
- **Variant max range** (T3/T4, multiple corroborating sources — Sandboxx, Army Recognition,
  f-16.net, Wikipedia): **AIM-120A/B ≈40 NM; AIM-120C ≈49 NM (C-5/6/7 commonly cited 57–65 NM);
  AIM-120D ≈70–86 NM.** These are **max kinematic range figures under favorable geometry**, not the
  DLZ `Raero`/`Rtr` numbers the HUD shows in a given engagement (§2.5) — don't conflate a spec-sheet
  max range with an in-engagement DLZ cue; the DLZ is geometry/altitude/aspect-dependent and is what
  the sim needs to reproduce, not the headline spec number.
- **Guidance-phase handover** (T3, multiple sources incl. official AMRAAM program descriptions):
  inertial midcourse + **two-way datalink updates from the launch aircraft**, active-radar seeker
  activates for **terminal homing** — this matches ED's own "Radar Activation Range" cue (§2.5)
  exactly, i.e. ED's in-cockpit logic and the public program-level description agree. High confidence.
- **Seeker activation range / Rmin specifics**: doctrine-dependent, not independently published with
  precision — correctly a **gap** (ED's own DLZ cues are the only concrete numbers available, and
  those are per-engagement, not fixed constants).

#### 4.5 Stores, stations, carriage limits
- **Station structural weight classes** (T4, f-16.net forum consensus, cross-check only — **not**
  independently confirmed against a T1/T2 loading manual this pass): stations **1/9 ≈250 lb**,
  **2/8 ≈250 lb**, **3/7 ≈2,500 lb**, **4/6 ≈3,500 lb**, **5 (centerline) ≈2,500 lb**. The same source
  explicitly warns these are **not** simple "anything under this weight fits" limits — real carriage
  clearance is store-specific (aerodynamic/structural qualification per store+rack combination, not a
  single number per station) — treat as an **order-of-magnitude cross-check for FlightBox mass/CG
  modelling, not a certified loading-manual limit**. **TODO**: a T1 loading-manual or T.O. 1F-16C-
  weight-and-balance citation would materially raise confidence here; not found in this pass's budget.
- **Rack/launcher carriage rules actually documented in ED** (T-primary, high confidence, repeat from
  §2.4): TER-9/A (3× Mk-82-class), BRU-57/A (2× JDAM/JSOW/WCMD-class), MAU-12 (1× store), LAU-88
  (3× Maverick) vs LAU-117 (1× Maverick, required for ripple-fire per §2.6), LAU-118(V)2/A (HARM).
  Slant-load clearance restriction: CBU-87/97/103/105 limited to **2 per TER** (not 3) when wing
  external tanks are installed (ED, Chuck).

#### 4.6 CMDS-adjacent note
Countermeasures Dispensing Set (CMDS/ALE-47) parameters — chaff/flare capacity, program structure,
burst/salvo timing — are documented in depth in `defence-rwr-cm.md` (raised to the same depth as this
file in this pass, per task priority 3). Not duplicated here.

#### 4.7 What's genuinely still a gap (not guessed)
- M61A1 individual PGU-round projectile mass and full external ballistic table (drag coefficient vs
  Mach, retained velocity vs range) — needed for a from-first-principles gun-damage/dispersion model;
  public T1/T2 data is thin. **Gym note**: measurable once a projectile is its own lightweight ballistic
  object in the sim.
- Exact seeker cooling duty-cycle numbers for AIM-9X's internal cooler.
- HARM threat-table scan-cycle timing constants (ED shows the mechanism — narrower table/FOV → faster
  scan — but not the underlying seconds-per-scan formula).
- JDAM/JSOW CEP figures beyond ED's own WCMD number — not independently sourced this pass.
- Full FMU-139 vs JDAM-kit fuze-family cross-reference (which JDAM variants actually use FMU-139
  derivatives vs a JDAM-specific fuze) — ED's profile page numbers don't cite a fuze part number.

### Sources
- **ED EA Guide** (primary this pass): pp. 34–42, 303–341, 524–632, 703–704 (cited inline throughout
  §2–3 above).
- **Chuck's Guide**: Part 11, pp. 313–573 (§1 above, unchanged from previous pass).
- MIL-DTL-45500/1A gun-dispersion spec — cited via ED EA Guide's own footnote (ED p.528), not
  independently re-sourced this pass.
- T3/T4 web research (§4): f-16.net armament articles, Wikipedia (M61 Vulcan, AIM-9 Sidewinder,
  AIM-120 AMRAAM, AN/ALE-47), GlobalSecurity (AIM-9X), Sandboxx/Army Recognition (AMRAAM specs),
  DocsLib/NAVAIR-derived FMU-139 fuze-system summaries, f-16.net forum (station weight classes,
  explicitly flagged T4/cross-check-only).
