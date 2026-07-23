# F-16C HUD Symbology

Sources: DCS F-16C Viper Guide (Chuck's Guide) —
- HUD control switches: Part 3 (Cockpit), p. 64.
- AOA indexer: Part 3, p. 35.
- Core flight/landing symbology: Part 6 (Landing), pp. 128–131.
- Master-mode / steering symbology: Part 16 (Navigation), pp. 705–711.
- ILS symbology: Part 16 (ILS Tutorial), pp. 771–772.
- Pull-up cues: Part 8 (ALOW), p. 156.

> The guide presents the HUD as annotated screenshots; element positions below are the DCS/real F-16C
> standard layout confirmed by the callouts. This is the reference our HUD (MIL-STD-1787) is built against.

## HUD control switches (what is displayed) — p.64

| Switch | Positions |
|---|---|
| **HUD Scales** | FWD **VV/VAH** (vertical velocity + velocity/alt/heading) · MID **VAH** (velocity/alt/heading only) · AFT OFF |
| **FPM (Flight Path Marker)** | FWD **ATT/FPM** (FPM + attitude reference bars) · MID **FPM** (marker only) · AFT OFF |
| **HUD Velocity** | FWD **CAS** (calibrated) · MID **TAS** (true) · AFT **GND SPD** |
| **HUD Altitude** | FWD **ALT RADAR** · MID **BARO** · AFT **AUTO** (radar < 1500 ft AGL, else baro) |
| **Depressible Reticle** | FWD **STBY** (standby reticle, removes all other symbology) · MID **PRI** (primary reticle, keeps symbology) · AFT OFF |
| **DED Data** | FWD **DED** (DED data on HUD) · MID **PFL** (Pilot Fault List on HUD) · AFT OFF |
| **HUD Brightness** | FWD Day · MID Auto · AFT Night |

## Core flight symbology (layout)

| Element | Position | Meaning |
|---|---|---|
| **Flight Path Marker (FPM)** / velocity vector | Center, follows flight path | Where the jet is actually going (aim point). Landing aim; markpoint slew cursor anchor |
| **Horizon Line** | Across HUD at true horizon | Attitude reference; align FPM on it = level flight |
| **Pitch Ladder** | Above/below FPM, per-degree bars | Climb/dive angle; **2.5°** lines used as landing glidepath reference |
| **Attitude reference bars** | Flank FPM (ATT/FPM mode) | Pitch/roll attitude when FPM caged |
| **Airspeed scale (kts)** | **Left** | CAS/TAS/GND per Velocity switch |
| **Altitude scale (ft)** | **Right** | Radar/baro per Altitude switch; "Radar Altitude (ft)" tagged on landing HUD |
| **Bank Angle scale** | Bottom (below FPM) | Roll angle |
| **Acceleration (G)** | **Top-left** | Normal load factor |
| **Heading tape** | Top | Magnetic heading |
| **Speed Brake indicator** | HUD annotation | Shown when speedbrakes deployed |
| **Master Mode label** | HUD | **NAV / A-A / A-G**; pressing A-A/A-G ICP button toggles, reverts to NAV |

## AOA indexer & HUD AOA bracket (on-speed cue)

**AOA Indexer** (3 lights, glareshield) — approach on-speed reference:
| Light | AoA | Meaning |
|---|---|---|
| Top (red) | > 14° | Too slow (on-speed AoA too slow) |
| Center (green doughnut) | 11–14° | **On speed** (13° = on-speed AoA for landing) |
| Bottom (yellow) | < 11° | Too fast for approach |

**HUD AOA Bracket** ("E" bracket): the "[" / "]" bracket beside the FPM; on-speed when FPM sits centered
in the bracket. Appears **on landing-gear deployment**. Target approach AoA **11°**, touchdown **≤ 13°**
(green circle). **> 15° AoA on rollout** risks speedbrake/nozzle strike on the runway.

## Landing HUD usage (Part 6)

- Align **FPM on Horizon Line** → level turn (overhead break, ~70° bank, 3–4 G).
- Final: align **FPM + the 2.5° pitch-ladder lines with the runway threshold** for glidepath, hold 11° AoA.
- Short final: shift FPM forward to a point **300–500 ft down the runway**, flare, do **not** level off.
- Control AoA with **throttle, not pitch trim** — FBW sets AoA.

## Navigation / steering symbology (Part 16, pp. 705–711)

| Element | Meaning |
|---|---|
| **Steerpoint Diamond** | Points to active steerpoint; **crossed-out** = steerpoint out of HUD field of view |
| **Steerpoint Tadpole** | Line points toward steerpoint: **UP = ahead**, **DOWN = behind**; centered + up = flying at it |
| **Distance to Steerpoint (nm)** | Slant range; range provider letter (**B** = computed from steerpoint/baro elevation) |
| **TTG** | Time to go to steerpoint |
| **HMC (HUD Mark Cue)** | Circle slewed by Radar Cursor to designate a HUD markpoint |
| **HUD Reference Cross** | Fixed alignment cross for HMCS helmet-cross alignment (startup) |

Fly to steerpoint: align the **tadpole with the FPM**.

## ILS symbology (Part 16, pp. 771–772)

| Element | Meaning |
|---|---|
| **Localizer Steering Bar** (vertical) | Lateral runway alignment; bar right of FPM → fly right to center |
| **Glide Slope Steering Bar** (horizontal) | Vertical guidance (3° glideslope); bar above FPM center → below glideslope, climb |
| **Command Steering Symbol** (circle) | Flight-director steering to the approach; **tic mark** appears when near glideslope center = pitch steering valid |
| **Glide Slope Fail Flag** | Shown until close enough for valid GS; disappears when GS guidance valid |

"**Center the bars**": both bars centered on the FPM forming a perfect cross = on localizer + on glideslope.
On capture, deploy gear → "E" AoA bracket appears; LANDING light UP; deploy speedbrake.

## Advisory cues

- **Pull-Up cues (X)**: displayed on the HUD when below the CARA ALOW radar-altitude floor (Part 8, ALOW).
- **AL** flashes + VMS "ALTITUDE" below CARA ALOW; see `aerodynamics-performance.md`.

---

# Technical depth (researched — for rebuild)

The standard our HUD is built against is **MIL-STD-1787** (*Aircraft Display Symbology*, adopted
1984-12-10; current rev D:2018). Sources cited inline. This section gives the symbology *conventions* and
geometry a faithful HUD must obey — beyond the guide's element list.

## MIL-STD-1787 conventions
- Scope: standardizes symbols/formats/information content for electro-optical displays across **takeoff,
  navigation, terrain following/avoidance, weapon delivery, and landing** (globalsecurity / FAS mirror of
  MIL-STD-1787A).
- **Aircraft Reference Symbol (ARS)**: may be the **pitch marker**, the **flight path marker**, or the
  **climb-dive marker** depending on mode — the FPM is the ARS in normal flight.
- Core aircraft-reference set: **flight path marker, climb-dive angle marker, climb-dive scale (pitch
  ladder), acceleration cue, speed worm**. Our elements map onto these.
- **Pitch ladder** = level-flight reference plus climb/descent reference; bars are **earth-referenced**
  (bend/point toward the horizon), positive bars solid, negative (dive) bars dashed — the 1787 convention
  our per-degree ladder should follow.

## F-16 HUD geometry (the four reference points)
MIL-STD-1787 defines four HUD reference points; the **left-hand and right-hand reference points are
specific to the F-16 HUD design** (DTIC ADA430578; MIL-STD-1787):
1. Center of the **TFOV** (Total Field Of View),
2. the **aircraft reference point** (boresight / gun cross),
3. **left-hand reference point**, 4. **right-hand reference point**.
- **TFOV vs IFOV**: the FPM/pitch symbology is drawn in the **total** field of view; the instantaneous FOV
  (what one eye sees through the combiner) is smaller, so symbology near the edges may clip — a faithful
  HUD must clamp/ghost symbols (steerpoint diamond "crossed-out" when out of FoV is exactly this).
- **Confidence**: the F-16-specific left/right reference points are documented; exact TFOV angular size
  (typ. ~20–25° class for this generation of HUD) is not firmly public — treat as an implementation
  parameter, cross-checked against the rendered picture.

## Implications for our MIL-STD-1787 HUD
- FPM is compressed/caged when wind drift would push it off-HUD — matches the DRIFT C/O switch
  (`cockpit-displays.md`): DRIFT C/O keeps the FPM centered regardless of wind.
- Airspeed **left**, altitude **right**, heading tape **top**, FPM/pitch ladder centered — the 1787/F-16
  layout the guide's landing screenshots confirm.
- Steering/ILS bars (localizer vertical, glideslope horizontal) form a cross on the FPM — a flight-director
  presentation per 1787 landing symbology; deflection scaling in `navigation-ils.md`.

## Hardware (LRUs, for context)
- **HUD**: F-16C uses a **wide-angle raster/stroke HUD**; the **Pilot Display Unit (PDU)** combiner +
  optics. The **HUD Electronics Unit** was a separate LRU on early jets but is **absorbed into the MMC**
  (Modular Mission Computer) on Block 50+ (airforce-technology; see `cockpit-displays.md`).
- **Raster** capability lets the HUD overlay FLIR video (the guide's FLIR polarity/gain/contrast/brightness
  wheels drive this); **stroke** draws the symbology. Day/Auto/Night brightness per the guide switch.
- Symbology is generated by the **MMC** from sensor/mission data and drawn on the PDU.

## Sources
- MIL-STD-1787A/D *Aircraft Display Symbology* (globalsecurity.org; man.fas.org mirror) — ARS, pitch
  ladder, reference points.
- DTIC ADA430578 *New Flight Display Formats* — ARS/FPM/climb-dive set, F-16 left/right reference points, TFOV.
- airforce-technology.com F-16 — MMC absorbs the HUD Electronics Unit.
