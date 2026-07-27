# MiG-29A (9-12) — Sensors: N019 Radar, OEPS-29 IRST/KOLS, Shchel-3UM Helmet Sight

**Variant scope**: izdeliye **9-12** (Fulcrum-A), N019 "Rubin". Deltas in §9.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 12–13, 37, 59, 63, 69, 86–97. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf`
(**printed page = PDF page − 6**); pages used: 12–13, 37–39, 43–57, 83–90.
Research in **§7 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **FULL** on mode taxonomy, cockpit controls and the employment sequences (both
manuals are dense here). **FULL** on the *tactical* IRST/helmet story — this is the file's most
important content for FlightBox doctrine. **MEDIUM** on scan geometry and detection ranges (two
sources disagree; both recorded). **SHALLOW** on scan-bar patterns, frame times, track memory and
resolution cells (§8).

> ⚠️ **Two manuals, two fidelity levels.** `DCS-FM` describes the **FC3** module — a deliberately
> simplified avionics model whose mode names (ОБЗ/СНП/СНП2/РНП) and numbers are partly **ED design
> decisions**. `DCS-EA` describes the **full-fidelity 9-12** and uses the *real* switch names
> (RAD/IR/CC/HELM/OPT/BS on the PSR-31 "WCS MODES" selector). Where they differ, **DCS-EA wins for the
> 9-12** and the FC3 value is kept as a cross-check, flagged.

---

## Spec

### 1. The one architectural thing to internalise

The MiG-29's fire control is built around **two co-equal sighting channels**, selectable by a single
rotary switch, and a **third, purely angular channel** (the helmet):

```
                      WCS MODES selector (PSR-31)
   TOSS  NVG   RAD        IR     CC    HELM    OPT     BS
          |     |          |      |      |      |       |
          |  RLPK-29    OEPrNK-29 (KOLS) |   optical  missile
          |  = N019     = IRST + laser   |   manual   seeker
          |  ACTIVE      PASSIVE      helmet          only
```

- **RLPK-29** — the radar sighting complex, N019 "Rubin"/"Sapfir-29" (`DCS-EA p.86`, `DCS-FM p.12`).
- **OEPrNK-29** — the opto-electronic sighting/navigation complex, containing **OEPS-29**, which is
  **KOLS** (*Quantum Optical-Laser Station*: IRST **collimated with a laser rangefinder/designator*)
  plus the **Shchel-3UM helmet-mounted sight** (`DCS-FM p.12, p.38`).
- **SUO-29** — the armament control system: central logic **BCL-10P-20P**, two missile control units
  **BUR-20PR-1/-2**, gun automation **BAP-20**, four unguided-weapon automation units, plus the special
  suspension control system (`DCS-FM p.13`).
- The whole thing is **SUV-29** (`DCS-FM p.12`).

**The FlightBox-relevant asymmetry**: an F-16 pilot who wants a track must radiate. A MiG-29 pilot has
a **passive first-class sensor with its own launch-authorising range computation** (via the laser
rangefinder) and a **helmet** that can hand a target to a missile seeker **without the aircraft ever
pointing at it**. That is the doctrinal opposite of `doc/f16/radar-sensors.md` and should drive an
entirely different `FBPilot` intercept phase — see §6.

---

### 2. Radar controls in the cockpit (**FULL**)

#### 2.1 PUR-31 radar control panel, left console (`DCS-EA p.63`)
| Control | Positions | Meaning |
|---|---|---|
| Antenna elevation selector | continuous | manual antenna elevation |
| **Radar modes switch** | **AUTO** | switches PRF automatically when target parameters are unknown |
| | **CLOSE CMBT** | optimised for close combat |
| | **HEAD ON** | intercepting approaching targets (high-PRF equivalent) |
| | **P** | chased/retreating targets (pursuit, medium-PRF equivalent) |
| **Emission control** | **ILLUM** | combat mode, radar fully operational (radiating) |
| | **DUMMY** | antenna-equivalent mode for testing (**does not radiate**) |
| | **OFF** | radar powered down |
| ECCM counteraction | **AJ / CAJ / OFF** | HUD symbology in the presence of jamming; **CAJ == AJ in the current implementation** |
| **TWF mode switch** | **FHS / RHS** | front hemisphere / rear hemisphere |
| Interference compensation | — | **not available** |

**Rebuild note**: the **ILLUM / DUMMY / OFF** three-position switch is exactly the FlightBox
`FBRadarSystem` power/emission split (`SetPowered` / EMCON), *pre-existing in the real jet*. DUMMY is a
non-radiating test state and maps to "powered, publishes no `FBEmitter` signature".

#### 2.2 PSR-31 weapon control panel (`DCS-EA p.12–13`)
| Control | Values | Sensor relevance |
|---|---|---|
| **MASTER ARM** | ARM / SAVE | connects triggers to weapon circuits |
| **ZONE** | **LEFT / CENTER / RIGHT** | **selects the radar azimuth scan sector** — the scan volume is *slewed in discrete thirds*, not continuously |
| **IR GAIN / HELM BRIGHT** | potentiometer | **KOLS receiver sensitivity in scan mode** *or* HMD symbol brightness in HELMET mode |
| **PREPARE MAN / AUTO** | 2-pos | missile launch-preparation mode; **manual is used if the WCS computer fails to determine target range** |
| **ALL / SINGLE 0.5 ALL** | 2-pos | paired vs single release per trigger press |
| **SPAN** | potentiometer, digitised in metres, indices **S / MED / L** | **target wingspan for indirect (angular) range computation** — *the fallback ranging method*. S = cruise missiles; **MED = MiG-21, F-15, F-16**; L = Tu-16, F-111, SR-71, B-1. Also programmes the semi-active R-27R |
| **WCS MODES** | TOSS · NVG · RAD · IR · CC · HELM · OPT · BS | the master sensor/mode selector (§3) |

**SPAN is a genuinely important rebuild item**: the MiG-29 can derive range from *angular subtense* when
it has no radar or laser range. That is a documented, pilot-set, error-prone ranging channel with no
F-16 analogue — a candidate for an honest "range estimate with a stated error source".

#### 2.3 Radar-related HOTAS (`DCS-EA p.68–69`)
Stick: **Target acquisition symbol control button (KU-31)** (the TDC slew) · **Break-lock button**.
Throttle: **Target range wheel** · **Lock On button** (dual-function: Lock-On airborne / NWS-High on
the ground with flaps up).

Also `DCS-EA p.59` (PU-S31): a **"LOCK" switch — FOE / FRIEND** that "defines if radar can lock on the
targets being detected by IFF as friendly". I.e. **IFF is wired into the lock permission logic**, not
merely displayed.

---

### 3. Radar modes as documented in DCS-EA (the 9-12 module) — **FULL**

#### 3.1 RAD (radar) — search
Doppler-limited detection envelope, stated numerically (`DCS-EA p.87`) — **the most valuable radar
numbers in either manual**:

| Condition | Requirement |
|---|---|
| Ranges **> 8 nm** | closure/lag speed **> 81 kts** |
| Ranges **< 8 nm** | closure/lag speed **> 27 kts** |
| **Not guaranteed** | closure/lag **< 32.4 kts** *and* range **< 5.4 nm** in HEAD ON mode |
| At lower closure | **automatic tracking may be disrupted** |

**This is the Doppler notch, quantified**, and it maps directly to FlightBox's existing
`FBRadarSystem::kDopplerNotchMs` mechanism. Note the *range-dependent* threshold — the notch is
**wider at long range** (81 kts) than up close (27 kts).

Procedure (`DCS-EA p.88`):
1. WCS MODE → **RAD**; emission switch → **ILLUM**.
2. Search by changing **antenna elevation ("H")** and **azimuth ZONE**; the antenna position is drawn
   as **search-zone marks by elevation and azimuth on both HUD and HDD**.
3. When the target enters capture range, slew the **capture strobe** onto it and press **LOCKON**.
4. **TWS**: set "TWS FHS–RHS" to **FHS** and "AJ–OFF–CAJ" to **OFF** → automatic target strobing.
5. Re-target: press the control button, re-aim the strobe, LOCKON. **RESET on the stick returns to
   automatic selection of the most dangerous target.**

#### 3.2 LOCK (single-target track)
On capture the HUD shows "**A**" and the display switches from scan to lock format (`DCS-EA p.88`).
HUD elements in lock (`DCS-EA p.88–89`): aiming ring · fixed crosshair · artificial horizon · roll
scale · pitch scale · **selected missile type** · **seeker readiness/capture mark** · availability
indication · **target aspect** · **D r min** · **D r max2** · **"Gorka" order (Г)** · **"Attack" order
(А)** · **D r max1** · current target range mark · **RLPK antenna position mark**.

Attack sequence (`DCS-EA p.89`): align aiming ring with the electronic crosshair → set H_target = 0 for
a smooth climb to target altitude *before* issuing the **"Gorka"** command (on issue, "Г" appears and
the aiming ring **jumps** up or down) → close maintaining sufficient closure → reach the selected
weapon's range → launch → **exit the attack on the "ОТВ" command**.

**Range-scale auto-switching**: **54 nm → 27 nm → 13.5 nm → 5.4 nm** as range decreases past each value
(`DCS-EA p.89`).

**Three-letter command vocabulary** (worth treating as the MiG-29's "cue language"):
| Cue | Meaning |
|---|---|
| **А** (A) | Attack — sensor has the target |
| **ПР** (PR) | **Launch permitted** (F-16 equivalent: in-DLZ / shoot cue) |
| **ОТВ** (OTV) | **Break off / exit the attack** (flashes at minimum permitted firing range) |
| **Г** (G) | **"Gorka"** — pull up, the computed climb to target altitude |

#### 3.3 RAD Close Combat (`DCS-EA p.89`)
- Used in direct visual contact; captures a target **in the vertical search zone** at ranges from
  **5.4 nm down to 1,500 ft**.
- "Stable automatic tracking is provided **at equal speeds and at a lag**" — i.e. CC mode deliberately
  drops the closure requirement of §3.1, which is why it is the dogfight mode.
- Display: **two vertical lines** and the "**РЛ**" symbol; range scale **5.4 nm**.
- **LOCKON must be pressed for no more than 2 seconds.**
- Then "ПР" → launch; "ОТВ" → exit **in the direction of the aiming ring**.

#### 3.4 Jamming behaviour (`DCS-EA p.63`, and FC3 detail in §4.4)
ECCM switch selects HUD symbology in the presence of jamming: **AJ** shows active-interference
indication; **OFF** shows jam indication without vertical notches, index only.

---

### 4. Radar modes as documented in DCS-FM (FC3) — cross-check, **flagged as simplified**

| FC3 mode | Real-mode equivalent | Documented behaviour |
|---|---|---|
| **ОБЗ / SCAN** | RAD search | Up to **24 targets detected**. Range scale selectable. Azimuth **3 discrete positions**: centre **±30°**, left **−60…0°**, right **0…+60°**. Elevation slewed smoothly *or* by the **range-angle method** (enter expected range in km + expected target Δaltitude in km → the scan elevation is computed). `DCS-FM p.43–44, 83` |
| **СНП / TWS** | TWS | **10 simultaneous tracks**. Retains elevation and velocity vector while continuing to search. **Auto-lock (transition to STT) at 85 % of computed Rmax**; earlier lock forced with Enter. **Requires ППС or ЗПС — the interleaved АВТ PRF mode is incompatible.** `DCS-FM p.46–47` |
| **СНП2 / TWS2** | — | **MiG-29S only**, R-77. Two targets. Lock limits: targets **≤ 8° apart in azimuth**, target **g ≤ 3**, **no organised ECM**. `DCS-FM p.49, 85` |
| **РНП / STT** | LOCK | All power on one target. **Tracks within 120° of azimuth.** **On missile launch the radar changes to continuous-wave illumination** — unambiguously read as a launch by hostile RWR. SARH (R-27R/ER) needs illumination **until impact**. `DCS-FM p.47–48` |

**PRF selection (FC3 wording, but the real jet's HEAD ON / P / AUTO switch, §2.1)** — `DCS-FM p.44`:
| Setting | PRF | Use | Cost |
|---|---|---|---|
| **ППС (HI)** | High PRF | approaching front-hemisphere targets, **longest detection range** | blind to receding |
| **ЗПС (MED)** | Medium PRF | receding rear-hemisphere targets | shorter range |
| **АВТ (ILV)** | interleaved HPRF/MPRF on alternate bars | all-aspect | **−25 % maximum range**, and **TWS unavailable** |

**Detection latency**: after orienting the scan zone, *"you may have to wait up to **six seconds**
before the target is detected … only after the radar has completed several scanning cycles"*
(`DCS-FM p.84`).

**Notch / inertial tracking** (`DCS-FM p.37–38`) — a good, explicit statement of the physics:
- Targets at aspect near **90°** are hard to detect (small radial closure, small Doppler shift). "Such
  blind angles are present in all radars that use the Doppler effect."
- On entering the notch the radar applies **inertial tracking for up to 6 seconds**, extrapolating the
  target's trajectory and pointing the antenna at the predicted exit point. **It works only if the
  target manoeuvres predictably**; an aggressive turn in the notch breaks lock with high probability.

**Target-size symbology (RCS dots)** — `DCS-FM p.44`:
| Dots | Target RCS |
|---|---|
| 1 | ≤ 2 m² |
| 2 | 2 – 30 m² (typical fighter) |
| 3 | 30 – 60 m² |
| 4 | ≥ 60 m² |
**Friendly IFF return = a second row of dots above the main one.** (HDD: friendly = circular mark.)

**Launch-range cues on the HUD scale** — three inward tick marks, top to bottom (`DCS-FM p.46`):
**Rmax** (max range vs non-manoeuvring target) · **Rtr** (max range vs manoeuvring target = the
no-escape zone) · **Rmin**. Compare `DCS-EA`'s **D r max1 / D r max2 / D r min** (§3.2) — same concept,
different labels.

#### 4.4 ECM / jamming (`DCS-FM p.52–53`)
- Under strong ECM, **TWS/TWS2 are unusable — SCAN only**.
- The radar cannot measure range: a **vertical jamming strobe** of randomly flashing marks appears on
  the jammer's bearing; the "**АП**" symbol appears when ECM is detected in the scan pattern.
- **AOJ (angle-of-jam) lock** is possible on the strobe → SARH missiles then guide in **HOJ**
  (home-on-jam) mode. **Range in an AOJ lock is not measured — it is entered by the pilot**, default
  **10 km**.
- **Burn-through at less than 25 km**: inside that range the radar recovers full target data including
  range and the display reverts to normal SCAN.

---

### 5. Close-combat acquisition modes (FC3 numbers; EA gives the taxonomy)

| Mode | Key | Scan volume | Lock | Notes |
|---|---|---|---|---|
| **Vertical Scan (VS)** | [3] | **3° wide × −10…+50° in elevation**; extends **~2 HUD heights above the HUD** | manual, **1–3 s** after target enters the zone with Enter held | **IRST is the default sensor**; default weapon is the IR missile. Two vertical lines on the HUD mark the zone. `DCS-FM p.54, 87–88` |
| **BORE** | [4] | **2.5° cone along the aircraft axis**, drawn as a 2.5° circle; slewable | manual | **Better aiming precision and slightly longer lock range than VS**. IRST default. `DCS-FM p.55, 88` |
| **HELMET (Shchel-3UM)** | [5] | pilot's head direction | manual | Reticle is **screen-fixed, not a HUD symbol**. Flashes **2 Hz** = ПР. **"X" through the reticle = beyond permitted designation angles.** `DCS-FM p.56, 89` |
| **Fi0 (longitudinal)** | [6] | **~2° conical missile-seeker FOV**, forward along the missile axis | seeker only | **Backup for total WCS failure.** LA appears **regardless of range** — the pilot must judge range by eye. **Does not trigger the target's RWR** → passive attack. `DCS-FM p.56–57, 90` |
| **RETICLE** | [8] | fixed calibrated grid | none | Not a combat mode: replaces HUD symbology with a collimator-sight image. **Central crosshair aligned with the gun axis; the missile seeker in Fi0 aims *lower*, at the "X" mark.** `DCS-FM p.61` |

DCS-EA equivalents (`DCS-EA p.89–94`): **CC** (radar close combat, §3.3) · **IR CC** (KOLS close combat,
rear hemisphere, target aspect up to **3/4**) · **HELM** · **OPT** (manual designation with the aiming
mark; "А" when radar or KOLS captures first) · **BS** (boresight: fixed electronic crosshair; **no
capture indication on the HUD — only the voice message "Launch permitted"**, plus an audio tone for
R-60(M)/R-73).

Close-air-combat engagement ranges are stated as **~10 km** overall (`DCS-FM p.87`).

---

### 6. OEPS-29 / KOLS — the IRST, and why it changes the doctrine (**FULL**)

#### 6.1 What it is
**KOLS** = *Quantum Optical-Laser Station*: an **IR search-and-track head collimated with a laser
rangefinder/designator**, in a transparent spherical radome **forward and to the right of the canopy**
(`DCS-FM p.9, p.38`). Together with the **Shchel-3UM** helmet sight it forms **OEPS-29**, which sits
inside **OEPrNK-29** alongside the **SN-29** navigation system, the **C100** mission computer and the
**SEI-31** indication system (`DCS-FM p.12`).

#### 6.2 Documented performance
| Quantity | Value | Source |
|---|---|---|
| **IR detection range, clean air** | **13.5 … 5.4 nm** (25 … 10 km) | `DCS-EA p.91` |
| **IR detection range under thermal (flare/IR) countermeasures** | **5.4 … 1.6 nm** (10 … 3 km), depending on interference level | `DCS-EA p.91` |
| IR detection range (research) | **15 km** | T4, §7.2 |
| **Laser rangefinder range** | **6 km** | T4, §7.2 |
| KOLS field of view (research) | azimuth **±30°** or **±15°**, elevation **±15°** | T4, §7.2 |
| IRST dwell to search one increment | **4–6 s** | `DCS-FM p.86` |

#### 6.3 Employment (`DCS-EA p.90–91`)
1. WCS MODES → **IR**. *"Turn on IR mode after takeoff."*
2. Attack hemisphere switch **FHS–RHS** → **RHS**; set target size with **SPAN**.
3. Detect the target mark on the HUD. Under interference, **reduce KOLS sensitivity with the IR
   GAIN knob** to thin out spurious marks — the "**ПП**" (passive interference) symbol then appears.
4. Slew the strobe onto the mark; **press and hold LOCKON 2…3 s** until capture. Missile seeker
   designation and preparation then happen **automatically** once KOLS goes into auto-track.
5. On "**ПР**" → launch. Disengage after launch or on "**ОТВ**".

Sign on the HUD that the IR channel is the **lead channel**: "**ТП**" (`DCS-EA p.91`; the FC3 manual
uses the same "ТП" symbol, `DCS-FM p.51`).

#### 6.4 Why this matters for the FlightBox pilot module
Stated outright by the sources:
- *"Unlike radar systems, OESS systems are passive, i.e. not radiating energy. **The enemy doesn't know
  that OESS is tracking his aircraft.** This significantly increases the attack stealthiness."*
  (`DCS-FM p.39`)
- *"Since the target's RWR cannot detect the laser rangefinder employed by the IRST, this sensor makes
  it possible to conduct a **'stealth' attack**."* (`DCS-FM p.51`)
- *"IR mode … is used … to **increase the stealth of approaching the target**."* (`DCS-EA p.90`)
- **The IFF interrogator does not operate with the IRST** — *"be absolutely sure that the target is an
  enemy aircraft before attacking"* (`DCS-FM p.86`). **Passive detection costs you identity.**
- IRST is **immune to active jamming** but has **much less detection range than radar**
  (`DCS-FM p.85`).
- IR detection is **more effective from the rear** (`DCS-FM p.85`), which is why **IR CC is a
  rear-hemisphere mode with an aspect limit of 3/4** (`DCS-EA p.92`).

**Doctrine consequence** (the direct counterpart to `doc/f16/`'s BVR model): a MiG-29 `FBPilot`
intercept phase should default to **radar OFF / DUMMY, IRST searching**, accept a **much shorter
detection range**, accept **no IFF**, and only bring the radar to **ILLUM** when it needs range for a
SARH shot — at which point the opponent's RWR lights up. Where the F-16 pilot's cost function is
"a lock warns the target", the MiG-29's is "**radiating at all** warns the target, and not radiating
costs range *and* identity".

#### 6.5 Shchel-3UM helmet-mounted sight
| Property | Value | Source |
|---|---|---|
| **Designation coverage** | **±60° azimuth, +60° / −14° elevation** | `DCS-EA p.92` |
| Function | provides target designation **to the radar, to KOLS, and to the IR seekers of the missiles**, based on the elevation/azimuth of the pilot's line of sight | `DCS-EA p.92`, `DCS-FM p.38` |
| Brightness | via the **IR GAIN / HELM BRIGHT** rheostat | `DCS-EA p.12, 92` |
| Symbol behaviour | ПР flashes **2 ×/s** when D_tech < D_r max; flashes **2 ×/s** also when the target is captured **by IR only** | `DCS-EA p.92` |
| Out-of-limits cue | "**X**" symbol above/through the ring | `DCS-FM p.56` |

**The critical mechanism** (`DCS-EA p.93`): *"When the LOCKON button is pressed, the target designation
of the missile's IR seeker is carried out **directly from the HMD, regardless of whether the target is
captured by KOLS or radar**."* And if radar or KOLS *does* capture, "ТП capture" is shown on the HMD,
the HUD switches to the corresponding capture format, and **on release of LOCKON the designation source
switches to RLPK or KOLS**.

Per-missile release discipline (`DCS-EA p.93`):
| Missile | Procedure |
|---|---|
| **R-73** | **Release LOCKON**, then launch, judging range by eye — target is no longer tracked via the HMD. *Alternatively* launch **without releasing** LOCKON, but then the target must be **tracked via the HMD** |
| **R-60(M)** | **Hold LOCKON**, track the target via the HMD, launch judging range by eye |

---

### 9. Variant notes
- **9-13 / MiG-29S**: N019M(E) "Topaz" — improved processor (Ts101M), **TWS2** with two simultaneous
  R-77 engagements, greater range (T4: N019M ≈ 90 km). Everything about the **sensor taxonomy** in this
  file still holds; the numbers move.
- **MiG-29SMT / MiG-35**: N010 Zhuk / Zhuk-AE — a different radar entirely. Out of scope.
- The **helmet sight and KOLS are unchanged** across 9-12 → 9-13; the doctrinal content of §6 is
  variant-independent.

---

## State

**Nothing in this file is implemented.** FlightBox has no MiG-29 module, no
`sim/src/modules/mig29/` and no JSBSim MiG-29 model. The airframe exists only as a **spec-first
contract** — [`../flightbox/aircraft/mig29.md`](../flightbox/aircraft/mig29.md), whose own status
line reads *"spec only. Nothing is built."* Everything below is therefore a **forward commitment**,
not a description of code.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for the sensor pair (N019 + KOLS/helmet sight) |
| **R6** — asymmetric weapons + RCS | the detection ranges here are stated against a target RCS; RCS as a unit property is exactly what R6 introduces, and this file is where the MiG-29 side of that number is read |
| **R7** — enemy units at BVR scale | the defining row: the mode taxonomy (§3) becomes a `FBRadarSystem::ActiveVolume` override, the quantified Doppler notch (§3) becomes its notch parameters, and **KOLS as a passive primary channel** (§6) inverts the F-16 BVR cost function from "a lock warns the target" to "**radiating at all** warns the target" |
| **R8** — JSBSim model | nothing directly — sensors are module code, not model XML |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../flightbox/roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — the file's own itemised list follows, section number unchanged. Note
that two research sites that would deepen §7 returned **403** to this pass and are recorded in
`PROGRESS.md`; the **GAF T.O. 1F-MIG29-1** remains the one T1 acquisition.

**Implementation gaps** — none statable yet: nothing is built (see State).

### 8. Open gaps (honest)
1. **Scan-bar patterns and frame times per mode** in the 9-12: only the T4 "2.5–5 s, 4-bar/6-bar"
   summary exists. `DCS-EA` never states a frame time.
2. **Track initiation criteria** (how many looks to firm a track) — nothing in either manual.
   FlightBox's `kHitsToFirm`/coast model has **no MiG-29-specific number** to anchor to.
3. **TWS scan volume in the 9-12** — `DCS-EA` documents a TWS switch (FHS/RHS) but never the volume;
   the 10-track figure comes from the FC3 manual and from research.
4. **N019 elevation/azimuth conflict** (§7.1) unresolved without a T1/T2 source.
5. **KOLS scan pattern and frame time** — the "4–6 s per increment" figure is FC3 (a *pilot technique*,
   not a system spec). The real KOLS raster is undocumented publicly.
6. **IRST detection range vs aspect and vs afterburner** — the sources say "more effective from the
   rear" and the FC3 manual notes an afterburning aircraft is an exception to the size rule
   (`DCS-FM p.86`), but no quantitative aspect law exists.
7. **The `DCS-EA` RAD-mode closure thresholds (§3.1) have no altitude qualifier** — real Doppler notch
   width depends on the clutter spectrum, hence on altitude and terrain. Model as stated, flag the
   simplification.

---

---

## Knowledge

### 7. Technical depth (researched)

#### 7.1 N019 "Rubin" / N019M "Topaz" — hardware facts
Source: toad-design MiG Alley "N019 Radar" article — **T4** (secondary compilation, evidently derived
from Russian published material; the most detailed public account found this pass).

| Parameter | Value |
|---|---|
| Mass | **385 kg** |
| Band | **X-band, ~3 cm** pulse-Doppler |
| **Beamwidth** | **3.5°** |
| **Azimuth limits** | **±65°** |
| **Elevation limits** | **+56° / −36°** |
| Antenna stabilisation | to **120° roll**, **±40°/±30° pitch** |
| Scan-sector structure | the 130° azimuth field divided into **three overlapping sectors** (left/centre/right) — *this is the "ZONE" switch of §2.2* |
| Scan cycle time | **2.5–5 s** depending on mode |
| **Encounter mode (HPRF)** raster | 6-bar or 4-bar; **lock-on 2–7 s** |
| **Pursuit mode (MPRF)** | **lock-on 1–4 s**; prone to ground clutter |
| **Close-combat mode** | fixed **±37° / −13°** scan, **2.5 s cycle**, **1–2 s lock**, **minimum range 250 m** |
| TWS capacity | **up to 10 targets** |
| Processor | **Ts100**, 170,000 ops/s, 8 K RAM, 136 K ROM, 32 kg |

Detection ranges against a **3 m² RCS fighter** (same source):
| Regime | Search | Track |
|---|---|---|
| Encounter (HPRF), above 3,000 m | **50–70 km** | **40–60 km** |
| Encounter, below 3,000 m | **40–70 km** | **30–60 km** |
| Pursuit (MPRF), above 3,000 m | **25–35 km** | **20–35 km** |
| Pursuit, 500–1,000 m altitude | **15–30 km** | **13–25 km** |

⚠️ **Conflict with the manuals**:
| Quantity | `DCS-FM p.12` | Research (T4) |
|---|---|---|
| Front-hemisphere detection | **70 km** | 50–70 km (HPRF, 3 m², high altitude) |
| Rear-hemisphere detection | **35 km** | 25–35 km (MPRF, high altitude) |
| Azimuth limits | **±67°** | **±65°** |
| Elevation limits | **+60° / −38°** | **+56° / −36°** |
The manual figures are the *headline* numbers (best case, no RCS qualifier). **The research set is the
one to model** because it is RCS- and altitude-qualified; keep the manual set as the "brochure" value.

Also `DCS-FM p.12`: *"In close air combat, the radar antenna rotates **only in the vertical axis**."* —
consistent with the ±37°/−13° fixed-azimuth CC scan above.

Known weaknesses (same source, **T4**): the automatic (interleaved) mode *"overloads the data computer
and generates numerous false returns"*; pursuit mode is *"prone to displaying false targets from ground
clutter especially at low altitudes"*. This corroborates the FC3 manual's **−25 % range penalty** for
АВТ (§4).

#### 7.2 KOLS / OEPS-29
IR detection range **15 km**; **laser rangefinder 6 km**; **FOV azimuth ±30° or ±15°, elevation ±15°**.
Sources: defence.pk technical thread on "S-31E2 KOLS", GlobalSecurity MiG-29 design page — **T4**.
The **6 km laser range** is the important number: it bounds the range at which the *passive* channel
can produce a **true** range for a launch computation. Beyond it the WCS must fall back on the **SPAN**
angular method (§2.2) — which is exactly why that knob exists.

#### 7.3 What the sources do **not** support
- The claim that the MiG-29 has **spoilers** or **digital fly-by-wire** appeared in one search summary
  and is **wrong for the 9-12** (that is MiG-29K/MiG-35 or an F/A-18 confusion). Recorded here so it is
  not re-introduced. See `flight-controls.md` §1.
- No public source found for N019 **PRF values in Hz**, **duty cycle**, **peak power**, **resolution
  cells**, or **track-file memory time**.

---
