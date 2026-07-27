# MiG-29A (9-12) — Navigation: SN-29 / A-323 RSBN, ADF, PRMG Landing System

**Variant scope**: izdeliye **9-12** (Fulcrum-A). Deltas in §8.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 16–18, 23–25, 41–43, 45, 63, 81–84. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf`
(**printed page = PDF page − 6**); pages used: 12–13, 23, 26, 41–42.
Research in **§6 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **FULL** on the A-323 control panel, the four navigation options and the
RETURN / LANDING / MISSED-APPROACH state machine including their engage conditions and altitude
plateaus — `DCS-EA p.81–84` is exceptionally precise. **MEDIUM** on the ground infrastructure (RSBN /
PRMG) — researched. **SHALLOW** on INS/AHRS behaviour, drift and alignment quality (§7).

---

## 1. The one architectural thing to internalise

The MiG-29 does **not** navigate by a stored INS waypoint list the way an F-16 does. It navigates by
**nine programmed points in three logical classes**, and the *correction* of its dead-reckoning
solution comes from **ground radio beacons (RSBN)**, not from an inertial platform's own quality:

```
  SN-29 navigation system
    ├── IK-VK-80        attitude/heading reference system (gyro MAIN + STBY)
    ├── SVS-M-72-3-2I   air data system
    ├── A-323 "PION"    RSBN short-range radio navigation  ← contains THE navigation computer
    └── BK-55           switching block
  + ARK-19 ADF, A037 radio altimeter, MRP-56P marker receiver
```
*"The connecting link of this system is the computer that is part of the A-323 system"* —
`DCS-FM p.12–13`.

**Consequences that matter for a FlightBox pilot module:**
- Position accuracy is a function of **being in range of a programmed RSBN beacon**. The **CORR
  ("КОРР") lamp** tells the pilot whether radio correction is currently being applied
  (`DCS-EA p.42`).
- **RETURN mode only works if a programmed RSBN beacon lies within 43.2 nm of the destination airfield**
  (`DCS-EA p.82`). Without it, the system degrades to bearing/distance to the aerodrome reference point.
- Waypoint sequencing is **manual** — *"When the distance to the waypoint is zero, **press the next
  waypoint button** and proceed"* (`DCS-EA p.82`). This is the opposite of FlightBox's
  `FBNavSystem::AdvanceWaypoint` auto-sequencing, and it is a *pilot action* that a MiG-29
  `FBPilot` must issue over the command bus.
  ⚠️ The FC3 manual says the opposite — *"When the current route point is reached, the sighting mark
  will automatically switch to the next waypoint"* (`DCS-FM p.42`). **Conflict**; the EA (full-fidelity)
  behaviour is the one to model.

---

## 2. A-323 navigation control panel (**FULL**) — `DCS-EA p.41–42`

**Storage: up to 9 navigation points**, in three logical classes of three: **WP (waypoints)**,
**A/D (airfields)**, **BEACON (RSBN)**. *"The categorisation is logical rather than physical — any
stored point can be used as a navigation target."*
⚠️ `DCS-EA p.81` restates this as *"three waypoints, three airfields and three RSBN beacons"* and then
says *"**six** navigation points can be programmed and are selected by the WP–A/D switch"* — i.e. the
WP/AD row is the 6 steerable points, the BEACON row is the 3 correction sources.

| # | Control | Function |
|---|---|---|
| 1 | **GYRO switch** | **MAIN / STBY** — selects the gyro (AHRS) used for navigation |
| 2 | **WP / A/D buttons 1-2-3** | combined pushbutton-lights: select a navigation point or an aerodrome |
| 3 | **WP–A/D switch** | selects which function the three buttons perform |
| 4 | **BEACONS buttons 1-2-3** | select the beacon used for **nav-system update (correction)** |
| 5 | **RETURN button** | lamp-button, activates RETURN (§4.1) |
| 6 | **LANDING switch** | selects the ILS/PRMG if not automatically switched in |
| 7 | **IDENT button** | signal on ground request — **not available** |
| 8 | **RSBN type selector knob** | selects desired RSBN type in MAN |
| 9 | **RSBN channel selector knob** | selects desired RSBN channel in MAN |
| 10 | **ILS selector knob** | selects the **PRMG** channel |
| 11 | NAVIGATION channel window | manually selected RSBN beacon type + channel |
| 12 | LANDING channel window | manually selected PRMG channel (frequency code of the channel, "FCC") |
| 13 | **COURSE switch** | **"0–179°" / "180–359°"** — selects the **hemisphere for the approach**, i.e. which runway direction |
| 14 | **CIRCLE switch** | **LEFT / RIGHT** — direction of the landing pattern |
| 15 | **RESET button** | deselects the previously selected BEACON |
| 16 | **COMP. ZERO button** | resets the computation results in the navigation computer |
| 17 | **REL BEARING switch** | **RSBN / ADF** — which source drives the HSI bearing pointer |
| 18 | **CHANNELS switch** | **AUTO** (automatic beacon use) / **MAN** (manual beacon type + channel entry) |
| 19 | **CORR lamp** | lit when **radio correction has been applied** in auto modes |
| 20 | **"D<21" lamp** | lit when distance to the target nav point is **less than 21 nm** |

⚠️ *"With the CHANNELS switch in manual, **the final course must be dialled in on the HSI** to receive
steering commands"* (`DCS-EA p.82`) — manual beacon selection also disables navigation-computer update.

**Programming**: *"All programming and data input operations for the Navigation System are performed by
the **Data Transfer Cartridge manager**"* (`DCS-EA p.81`) — i.e. there is **no in-cockpit coordinate
entry**. A MiG-29 mission must load its route on the ground; the pilot can only *select* among the nine
stored points in flight. **This is a hard constraint on any `.fbm` mission design.**

---

## 3. Sensors feeding the nav solution

### 3.1 ADF — ARK-19 (`DCS-EA p.45`, `DCS-FM p.13`)
| Property | Value |
|---|---|
| Frequency range | **150 … 1299.5 kHz** |
| Heading-angle output error to the HSI | **±3°** (source prints "±30", read as ±3.0°) |
| Range with a PAR-8 ground station | **≥183 nm at 33,000 ft**, **≥97 nm at 3,300 ft** |
| Programmed stations | **8**, set on the ground |
| Modes | **COMP** (automatic bearing → HSI) · **ANT** (no homing; listening only, e.g. Morse ident) |
| Panel | channel select **1/2/3/4/P** · **VOICE/CW** · volume · **LOOP** button · **COMP/ANT** switch |
| **OUTER / INNER switch** | separate cockpit toggle; **switching from outer to inner beacon is automatic on landing**, and the **"BEACON INNER" lamp** lights (`DCS-EA p.23`) |

### 3.2 Radio altimeter — A037 (`DCS-EA p.24–25`)
Decimetre-wave, **true altitude 0–3,000 ft regardless of visibility or surface type**. Provides the
**"low altitude" threshold** signal — settable bug → light **and voice** callout on descent. **Red flag
retracted = readings reliable.** Test → needle at **45 ft** (`DCS-EA p.72`).
⚠️ FC3 states the range as **0–1,000 m** and *"accurate readings cease with excessive bank"*
(`DCS-FM p.23`). Same device, metric statement; the bank-limit note is not in the EA manual but is
physically correct for a fixed-beam radar altimeter and is worth modelling.

**Threshold usage documented in procedures**: bug set to **200 ft** for the standard landing pattern
(`DCS-EA p.71`), and to **2,000 ft** before engaging RETURN (`DCS-EA p.82`).

### 3.3 AHRS and air data
**IK-VK-80** attitude/heading reference system with **MAIN and STBY gyros**, both separately powered
(`DCS-FM p.12`, `DCS-EA p.51`). **SVS-M-72-3-2I** air data system feeds the altimeter, TAS/Mach
indicator and the HUD (`DCS-FM p.12`, `DCS-EA p.14, 21`).
Alignment procedure and timings are in `procedures.md` §2.

---

## 4. Navigation modes (**FULL**) — `DCS-EA p.81–84`

Five options, selected on the A-323 panel: **point-to-point · RETURN · landing approach · traffic
re-entry (missed approach) · manual station selection.**

### 4.1 Point-to-point
- Select **WP–A/D** switch + one of the three illuminated pushbuttons.
- **Course** to the point → HSI course pointer + course window; **distance** → HSI range indicator **and
  the HUD**.
- **"D<21" light** at **21 nm** to run.
- **At 3 nm the HUD direction marker stabilises** (`DCS-EA p.82`).
- **Passing the point, "lost bearing" indication is shown until 3.2 nm outbound.**
- Waypoint advance is **manual** (§1).
- HUD procedure (`DCS-EA p.82`): select **NAV** on the WCS MODES knob; after gear retraction observe the
  **circular flight-direction marker** and the distance; **align the fixed crosshair with the direction
  marker**. ⚠️ *"Flight direction marker indicates **azimuth, not altitude** reference."*

### 4.2 RETURN (ВЗВ)
**Precondition**: landing at a **programmed airfield**, with correction from a **programmed RSBN beacon
located less than 43.2 nm from that airfield**. May be engaged at any distance within that beacon's
working area.

Setup before engaging (`DCS-EA p.82`):
1. Set the obtained **barometric pressure** on the altimeter.
2. Set the **radar-altimeter danger bug to 2,000 ft**.
3. Confirm the **KORR (CORR) lamp is lit**.
4. **ADF/RSBN switch → RSBN**.
5. **COURSE switch** → the airfield's landing course hemisphere.
6. **CIRCLE switch** → the direction of the traffic circle.
7. Set/verify the **RSBN and PRMG channels** of the landing airfield (in case of computer failure).

Engage: press **RETURN**, press the **A/D lamp-button** of the landing airfield, press the **BEACONS
lamp-button** of the correction beacon; confirm all three lamps lit.

Behaviour:
- Provides **bearing to a lead point for the nearest 9.2 nm final intercept**, given the correct landing
  direction on the COURSE switch and a working update function. **Slant range to the A/D coordinates is
  displayed.**
- If automatic update is inoperative → only **course and distance to the aerodrome reference point**.
- **Glide-path information is displayed on the ADI for a 7° glide slope to the final intercept point at
  3,700 ft AGL/QFE.**
- **Descent profile table** (`DCS-EA p.83`) — the RETURN mode's altitude-vs-distance schedule:

| Altitude (ft) | 15,000 | 12,000 | 9,000 | 6,000 | 3,000 |
|---|---|---|---|---|---|
| **Distance (nm)** | **30** | **25** | **20** | **15** | **10** |

  Deviation of more than **±560 ft** from that profile drops the HUD direction marker up or down,
  mirrored by the ADI glideslope director bar.
  → **Derived gradient**: 3,000 ft per 5 nm ≈ **600 ft/nm ≈ 5.6°** — consistent with the stated 7°
  glide slope to the intercept point being *steeper than* the profile's average. Directly usable as an
  `FBAutopilot::Direct` descent schedule.

### 4.3 LANDING (ПОС) — automatic engagement
The automatic landing mode engages when **all** of the following hold (`DCS-EA p.83`):
| Condition | Value |
|---|---|
| Lateral position | within **±0.8 nm** of the runway centreline (the "glidepath groove") |
| Heading | **|heading − landing course| < 60°** |
| Altitude | **below 3,700 ft** |
| Distance to runway centre point | **between 4.5 and 19 nm** |

On engagement:
- Both **GS and LOC failure flags** on the HSI disappear; the **HUD displays "GS" and "L" markers**.
- The **HSI course arrow aligns with the true landing course**.
- **The HSI small needle switches from RSBN to the outer NDB bearing.**
- The HSI vertical director bar and the **HUD flight-direction circle** give steering to centreline.
- The **ADI horizontal position pointer mirrors the HSI course director**. The **ADI vertical pointer
  shows deviation from the 2,000 ft plateau altitude before glideslope entry**, and mirrors the HSI
  glideslope indicator once established.
- **CORR extinguishes; the RETURN lamp stays lit**, indicating the system is still ready to execute
  MISSED APPROACH.
- Manual engagement by throwing the **LANDING switch UP** is allowed but **requires the correct PRMG
  channel set**.

### 4.4 MISSED APPROACH / traffic re-entry
Navigation-computer side (`DCS-EA p.81`): with the landing select switch off, pressing MISSED APPROACH
supplies steering for a traffic pattern — **5.4 nm downwind leg and final intercept**; pattern direction
from the **CIRCLE** switch; **glidepath information for a 2,000 ft AGL/QFE pattern altitude**.

Full procedure (`DCS-EA p.84`):
1. Establish a climb.
2. Remove the frame/covers; press **REPEAT APPROACH** on the ACS panel; confirm the lamp.
   → LANDING deactivates, **RETURN lamp remains lit**, HSI course pointer and HUD ring point at the
   set point in the direction of the first turn.
3. **Select 600 ft and 270 kts.**
4. Turn at **30° bank**, climbing to **2,000 ft**.
5. Check the outer route leg — **5.4 nm** — at the KTA (airfield reference point).
6. **Start the stopwatch. Estimated time to the second turn: 2 minutes.**
7. At the second-turn point: **distance from KTA 10–12 nm** on the range counter; REPEAT APPROACH
   deactivates, **RETURN reactivates**, and the HSI/HUD show the course to the turn onto the landing
   course.
8. Then proceed as for RETURN.

⚠️ Note the **AFCS "MISSED APPROACH" mode is *not implemented*** (`DCS-EA p.64`) while the
**navigation-computer** missed-approach logic **is** — two different systems with the same name.

### 4.5 Manual station select
CHANNELS switch in **MAN**: bearing and distance to a selected beacon are shown, **navigation-computer
update is not provided**, and the final course must be dialled on the HSI (§2).

### 4.6 FC3 nav sub-modes (cross-check)
`DCS-FM p.41–42`: three HUD navigation sub-modes plus a no-task mode — **МРШ (ROUTE) · ВЗВ (RETURN) ·
ПОС (LANDING)**, cycled by repeated key presses. In RETURN the sighting mark shows the **glide-slope
intercept point**, and *"after reaching the glide-slope intercept point, RETURN automatically switches
to LANDING"*. In LANDING the HUD director circle points at the airfield and **ILS deviation indicators
appear on the ADI**.

---

## 5. Approach/landing infrastructure used

| System | Role | Notes |
|---|---|---|
| **RSBN** (A-323 "PION" airborne set) | short-range azimuth + range navigation and **nav-computer correction** | Soviet analogue of TACAN; the airborne side stores 3 beacons |
| **PRMG** | the **landing** beacon pair (localizer + glidepath) — the "ILS" of the A-323 panel | Channel set by the **ILS selector knob**; the LANDING channel window shows the **frequency code of the channel (FCC) of the PRMG beacon** (`DCS-EA p.42`) |
| **NDB / PAR-8** outer and inner beacons | ADF homing and the outer/inner marker sequence | ADF auto-switches outer → inner on approach (§3.1) |
| **MRP-56P** marker receiver | marker passage | listed in `DCS-FM p.13`, not described further |

---

## 6. Technical depth (researched)

### 6.1 RSBN-4N (ground segment)
- **Azimuth channel**: continuous power **80 W**, pulse power **30 kW**, band **873.6–935.2 MHz**.
- **Range (DME) channel**: pulse power **30 kW**, band **939.6–1000.5 MHz**.
- Capacity: **unlimited users on the azimuth channel, up to 100 aircraft on the range channel.**
- Source: `armedconflicts.com` RSBN-4N entry — **T4**.

### 6.2 PRMG
- **PRMG-4 KM** in service from 1974; guides RSBN-equipped aircraft to **ICAO Category I** minima.
- The **PRMG-5** successor "produces equi-signal zones that determine the exact position of the aircraft
  in the heading and descent planes" — i.e. equi-signal, not the ILS's 90/150 Hz modulation depth.
- Source: `armedconflicts.com` PRMG-5 entry — **T4**.
- ⚠️ **No public source found for the PRMG nominal glide-path angle.** The `DCS-EA` **7°** figure
  (`p.81`) is the *RETURN-mode ADI director glide slope to the final intercept point*, **not** the
  PRMG approach glide path — do not conflate them. A normal PRMG approach angle would be ~2.6–3°;
  **that value is NOT sourced here and must not be used until it is.**

### 6.3 Component designations (`DCS-FM p.12–13`, source-internal)
SN-29 = **IK-VK-80** AHRS + **SVS-M-72-3-2I** air data + **A-323 "PION"** RSBN + **BK-55** switching
block. Plus **ARK-19** ADF, **A037** radio altimeter, **MRP-56P** marker receiver, **SO-69** ATC
transponder, radio **"Juravl-30"** (the R-862 in the EA cockpit), **"Reper-M"** radio altimeter and
**"Olenek"** ADF named as the *planned* fit.
⚠️ `DCS-FM p.12` mixes the *planned* equipment list with the *delivered* one ("the equipment **was
planned** to include…"). Where the EA cockpit names a device (A037, ARK-19, A-323), prefer the EA name.

---

## 7. Open gaps (honest)
1. **No INS.** The 9-12 has an AHRS + air data + radio correction, not an inertial navigator. Neither
   manual states **dead-reckoning drift rate**, so the *cost* of flying outside RSBN coverage is
   undocumented. This is a first-order gap for any autonomous navigation model.
2. **"FAST PREP" and the INS OPER/PREPARE switch** (`DCS-EA p.51–52, 72`) imply an alignment mode with
   a fast option — **no timings or accuracy figures** are given beyond "confirm FAST PREP is lit".
3. **PRMG glide-path angle and beam widths** — not sourced (§6.2).
4. **RSBN accuracy** (azimuth error, range error) — not sourced.
5. **HSI course-deviation scale sensitivity** (dots per degree, full-scale deviation) — not stated.
6. **The 21 nm "D<21" and 3 nm / 3.2 nm marker-stabilisation thresholds** are stated but not
   explained — presumably a computation-mode change in the A-323. Model them as documented behaviour.
7. **Whether the LANDING auto-engage conditions (§4.3) are checked continuously or once** — unstated;
   a disengage condition set is not given at all.

---

## 8. Variant notes
- **9-13 / MiG-29S**: identical navigation fit per `DCS-FM p.21` ("cockpit instruments … are
  identical"). No documented change.
- **Export/Warsaw-Pact aircraft**: the RSBN/PRMG infrastructure is theatre-specific — an aircraft
  operating outside RSBN coverage loses the CORR function and therefore RETURN/LANDING. Worth
  representing as a mission property rather than an aircraft property.
- **MiG-29SMT/MiG-35**: true INS/GPS navigation; nothing in §4 carries over.
