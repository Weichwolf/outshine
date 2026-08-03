# F-16C HUD Symbology

Sources: DCS F-16C Viper Guide (Chuck's Guide) —
- HUD control switches: Part 3 (Cockpit), p. 64.
- AOA indexer: Part 3, p. 35.
- Core flight/landing symbology: Part 6 (Landing), pp. 128–131.
- Master-mode / steering symbology: Part 16 (Navigation), pp. 705–711.
- ILS symbology: Part 16 (ILS Tutorial), pp. 771–772.
- Pull-up cues: Part 8 (ALOW), p. 156.

Plus `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — **Head-Up Display (HUD)**
chapter, p.89–96 (full element list + exact scale/geometry numbers) and **Navigation by Steerpoints**,
p.225–226 (Great Circle Steering Cue — ED's name for Chuck's "Tadpole"). Cite tags `Chuck p.NN` vs
`ED EA Guide p.NN` throughout below.

Plus, since this round, the two BMS manuals (see the *"BMS Dash-34 addendum"* section below):
- `doc/TO 1F-16CMAM-34-1-1 BMS.pdf` (669 pp, change 4.38, 1 Jul 2025) — cite tag **`[BMS-34 p.NNN]`**.
  HUD chapter pp.96–113; CARA/altitude-scale pp.114–118; collision/altitude advisories pp.119–126;
  ACM HUD cues pp.217–221; HMCS/HUD blanking p.299; A-G weapon symbology pp.410–438, 542–543;
  HARM HUD cues p.489; A-A weapon symbology pp.593–636.
- `doc/TO 1F-16CMAM-1 BMS.pdf` (404 pp, change 4.38) — cite tag **`[BMS-1 p.NNN]`**.
  AOA displays p.115; ILS/flight-director HUD symbology pp.166–168; HUD fuel warnings p.56.

> **These two manuals describe a SIMULATOR, not the factory jet.** They are the Falcon BMS 4.38
> documentation set written in USAF tech-order style (the "-34" and "-1" numbering is imitation, not
> provenance). They document what BMS *implements*, and they say so wherever they diverge — e.g.
> "This switch is not implemented" (HUD TEST switch, `[BMS-34 p.102]`), "Contrast (CONT) Control …
> currently serves no purpose in BMS as it is nonfunctional" `[BMS-34 p.98]`, "PGCAS – Not implemented"
> `[BMS-34 p.119]`, "The Loft Angle and Apex Altitude are not implemented" `[BMS-34 p.489]`,
> "P is displayed if the range is computed using DTS passive ranging. (N/I)" `[BMS-34 p.430]`. Every
> such marker is carried through below. Where BMS states a number the real jet's public documents do
> not, it is cited as **a BMS number**, not as an F-16 number. FlightBox's target is a faithful F-16
> HUD; where these are the only source of a geometry, that is stated.

> The guide presents the HUD as annotated screenshots; element positions below are the DCS/real F-16C
> standard layout confirmed by the callouts. This is the reference our HUD (MIL-STD-1787) is built against.

## Spec

### HUD control switches (what is displayed) — p.64

| Switch | Positions |
|---|---|
| **HUD Scales** | FWD **VV/VAH** (vertical velocity + velocity/alt/heading) · MID **VAH** (velocity/alt/heading only) · AFT OFF |
| **FPM (Flight Path Marker)** | FWD **ATT/FPM** (FPM + attitude reference bars) · MID **FPM** (marker only) · AFT OFF |
| **HUD Velocity** | FWD **CAS** (calibrated) · MID **TAS** (true) · AFT **GND SPD** |
| **HUD Altitude** | FWD **ALT RADAR** · MID **BARO** · AFT **AUTO** (radar < 1500 ft AGL, else baro) |
| **Depressible Reticle** | FWD **STBY** (standby reticle, removes all other symbology) · MID **PRI** (primary reticle, keeps symbology) · AFT OFF |
| **DED Data** | FWD **DED** (DED data on HUD) · MID **PFL** (Pilot Fault List on HUD) · AFT OFF |
| **HUD Brightness** | FWD Day · MID Auto · AFT Night |

### Core flight symbology (layout)

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

### AOA indexer & HUD AOA bracket (on-speed cue)

**AOA Indexer** (3 lights, glareshield) — approach on-speed reference:
| Light | AoA | Meaning |
|---|---|---|
| Top (red) | > 14° | Too slow (on-speed AoA too slow) |
| Center (green doughnut) | 11–14° | **On speed** (13° = on-speed AoA for landing) |
| Bottom (yellow) | < 11° | Too fast for approach |

**HUD AOA Bracket** ("E" bracket): the "[" / "]" bracket beside the FPM; on-speed when FPM sits centered
in the bracket. Appears **on landing-gear deployment**. Target approach AoA **11°**, touchdown **≤ 13°**
(green circle). **> 15° AoA on rollout** risks speedbrake/nozzle strike on the runway.

> **ED EA Guide precision** (p.44, 83 — `cockpit-displays.md`'s ED addendum): the indexer/indicator
> band edges are exactly **Low ≤11° / Optimal 11.1°–13.9° / High ≥14°**, and the AoA Indexer is
> **always powered regardless of gear position** (not gated to landing config). Both Chuck's ~11–14°
> bands above and ED's 11.1–13.9° are effectively the same cue; ED's is the more precise official
> figure.

### Landing HUD usage (Part 6)

- Align **FPM on Horizon Line** → level turn (overhead break, ~70° bank, 3–4 G).
- Final: align **FPM + the 2.5° pitch-ladder lines with the runway threshold** for glidepath, hold 11° AoA.
- Short final: shift FPM forward to a point **300–500 ft down the runway**, flare, do **not** level off.
- Control AoA with **throttle, not pitch trim** — FBW sets AoA.

### Navigation / steering symbology (Part 16, pp. 705–711)

| Element | Meaning |
|---|---|
| **Steerpoint Diamond** | Points to active steerpoint; **crossed-out** = steerpoint out of HUD field of view |
| **Steerpoint Tadpole** | Line points toward steerpoint: **UP = ahead**, **DOWN = behind**; centered + up = flying at it |
| **Distance to Steerpoint (nm)** | Slant range; range provider letter (**B** = computed from steerpoint/baro elevation) |
| **TTG** | Time to go to steerpoint |
| **HMC (HUD Mark Cue)** | Circle slewed by Radar Cursor to designate a HUD markpoint |
| **HUD Reference Cross** | Fixed alignment cross for HMCS helmet-cross alignment (startup) |

Fly to steerpoint: align the **tadpole with the FPM**.

### ILS symbology (Part 16, pp. 771–772)

| Element | Meaning |
|---|---|
| **Localizer Steering Bar** (vertical) | Lateral runway alignment; bar right of FPM → fly right to center |
| **Glide Slope Steering Bar** (horizontal) | Vertical guidance (3° glideslope); bar above FPM center → below glideslope, climb |
| **Command Steering Symbol** (circle) | Flight-director steering to the approach; **tic mark** appears when near glideslope center = pitch steering valid |
| **Glide Slope Fail Flag** | Shown until close enough for valid GS; disappears when GS guidance valid |

"**Center the bars**": both bars centered on the FPM forming a perfect cross = on localizer + on glideslope.
On capture, deploy gear → "E" AoA bracket appears; LANDING light UP; deploy speedbrake.

### Advisory cues

- **Pull-Up cues (X)**: displayed on the HUD when below the CARA ALOW radar-altitude floor (Part 8, ALOW).
- **AL** flashes + VMS "ALTITUDE" below CARA ALOW; see `aerodynamics-performance.md`.

---

### ED EA Guide addendum — full HUD element spec (official, p.89–96)

The ED EA Guide's HUD chapter gives the **exact geometry and scale numbers** the guide screenshots
above only imply. This resolves the earlier "TFOV not firmly public" confidence gap in the Technical-
depth section below with an **official number**.

#### HUD field of view (resolves prior TFOV gap)
- **Combining-glass display surface: 25° diameter**, extending down to a line **10.5° below the
  field-of-view center** (ED EA Guide p.89) — i.e. the visible area is *not* symmetric top/bottom
  around the boresight; it extends further below center than above, consistent with a HUD combiner
  shaped to show more of the ground/runway during approach. **High confidence, official source** —
  supersedes the "typ. ~20–25° class, not firmly public" note in the researched section below.
- Symbology is focused at infinity, superimposed on the outside world along the flight path.

#### Full element list with exact scale/geometry (ED EA Guide p.89–92)
| # | Element | Detail |
|---|---|---|
| 1 | Great Circle Steering Cue | see "Great Circle Steering Cue" below — ED's name for Chuck's Tadpole |
| 2 | Current G | **±9.9 G**, nearest 0.1 G |
| 3 | Diamond Symbol | 3-D steerpoint position (position + altitude); **X superimposed** when out of HUD FOV |
| 4 | Horizon Line | 0° pitch reference, part of the Attitude Bars |
| 5 | Velocity & Velocity Scale | **60–900 kt CAS** range (below 60 kt CAS, HUD shows 0); major tick = **50 kt** (2-digit label), minor tick = **10 kt**; reverts to CAS automatically in Dogfight mode or gear-down |
| 6 | Master Arm Status | `ARM` / blank (OFF) / `SIM` |
| 7 | Mach Number | to hundredths |
| 8 | Maximum G | peak G this flight; reset via Drift C/O switch → **WARN RESET** |
| 9 | Master Mode Status | text tag, full list below |
| 10 | Ownship Bearing & Distance from Bullseye | toggled via BULL DED page |
| 11 | Attitude Bars | **5° intervals** (10° intervals above ±60° pitch); solid = positive pitch, dashed = negative; caged to FPM in azimuth (Drift C/O overrides) |
| 12 | Boresight Cross | fuselage reference line, shown in all master modes |
| 13 | Flight Path Marker | circle + 3 lines at 12/3/9 o'clock; X when out of FOV; **flashes on weapon release** in A-G mode |
| 14 | Altitude & Altitude Scale | nearest 10 ft; major tick = **500 ft** (2-digit label), minor tick = **100 ft** |
| 15 | Radar Altitude | boxed "R" prefix, nearest 10 ft; blank if RDR ALT in STBY/off |
| 16 | Altitude Low Setting | CARA ALOW value; flashes + VMS "Altitude…altitude" below it |
| 17 | Slant Range | letter-coded range source, see table below |
| 18 | Time To Go | to selected steerpoint, from current ground speed |
| 19 | Distance to Steerpoint/Steerpoint Number | nm left of chevron / number right of chevron |
| 20 | Roll Indicator | marks at **0°, 10°, 20°, 30°, 45°** bank |
| 21 | Heading Scale | major tick = **10°** (2-digit label), minor tick = **5°**; GND SPD mode adds a ground-track triangle |
| 22 | Bank Angle Indicator | replaces Roll Indicator in NAV mode + Scales=VV/VAH; marks at **15°, 30°, 45°, 60°**, caged to FPM, wings of FPM indicate bank |
| 23 | Vertical Velocity Scale | NAV mode + Scales=VV/VAH only; major tick = **1000 fpm**, minor tick = **500 fpm** |
| 24 | Manual Bombing Reticle | Primary: 2-mil dot + dashed 50-mil ring + solid 100-mil ring; Secondary: 2-mil dot + dotted 50/100-mil rings + four 6-mil tick marks at 12/3/6/9 o'clock; depressible 0…−260 mils via ICP RET DEPR knob, horizontally fixed on centerline (not wind-corrected) |

**Slant Range source-letter codes** (element 17 — refines/completes the guide's "B = computed from
steerpoint/baro elevation" note with the full set):
| Letter | Source |
|---|---|
| **B** | Barometric altitude + steerpoint elevation |
| **R** | Radar altimeter |
| **F** | FCR ranging |
| **L** | TGP laser ranging |
| **M** | Manual range (A-A modes, or A-G CCIP) |

Range format: **>1.0 NM** → 4-digit, nearest 0.1 nm (e.g. `015.2`); **<1.0 NM** → 3-digit, nearest 100
ft (e.g. `055` = 5,500 ft).

**Master Mode Status text tags** (element 9 — the exact strings the HUD displays, useful for a HUD
implementation's mode-label lookup table): `NAV`, `CCIP`, `CCRP`, `DTOS`, `LADD`, `MAN`, `VIS`, `PRE`,
`BORE`, `STRF`, `HARM`, `HTS` (A-G sub-modes); `AAM`, `MRM`, `SRM`, `HOB`, `EEGS` (A-A sub-modes);
`MSL` (Missile Override, no type selected); `DGFT` (Dogfight master mode); `JETT` (Selective or
Emergency Jettison). See `weapons.md` for what each sub-mode computes.

#### HUD Control Panel (ED EA Guide p.93–96 — refines Chuck's switch table above)
Confirms Chuck's switch list (p.64) but with exact per-position behavior:
- **Scales switch**: `VV/VAH` (adds Bank Angle Indicator around FPM, removes Roll Indicator, if FPM
  shown) / `VAH` (no vertical-velocity scale) / `OFF` (digital readouts only, no analog scales).
- **Flightpath Marker switch**: `ATT/FPM` (Attitude Bars + FPM + Steering Cue) / `FPM` (FPM + Steering
  Cue, no Attitude Bars) / `OFF` (none of the three).
- **DED/PFLD switch**: `DED DATA` (removes Roll Indicator, shows DED repeater) / `PFL` (removes Roll
  Indicator, shows Pilot Fault List repeater) / `OFF` (Roll Indicator shown).
- **Depressible Reticle switch**: `STBY` (Standby Reticle only, all other HUD elements removed) /
  `PRI` (Primary Reticle, all symbology retained) / `OFF` (no reticle).
- **Velocity switch**: `CAS`/`TAS`/`GS` — matches Chuck; GS mode adds the heading-tape ground-track
  triangle (element 21 above).
- **Altitude switch, Test switch**: marked **N/I** (not implemented in DCS) by ED — real-jet-only
  functionality, not simulated even in the reference module.

#### Great Circle Steering Cue (ED's name for Chuck's "Tadpole")
ED EA Guide p.89–90, 225–226 — same physical HUD element Chuck's guide calls the "Steerpoint Tadpole"
(p.705–711), but ED's official manual gives the underlying **computation method**, not just the visual
behavior:
- Provides **lateral** steering to the selected steerpoint using the **great-circle** method (shortest
  path across the WGS84 sphere), not a fixed 2-D heading — directly matches FlightBox's planar-ENU
  `FBNavSystem::home_bearing` mechanism at short range, but the guide is explicit this is a
  great-circle computation, relevant if FlightBox ever adds long-range (>~100 nm) steerpoint legs
  where planar-ENU bearing starts to diverge from great-circle bearing.
- The cue is a horizontal-plane indicator adjacent to the FPM, aligned to the horizon; a line from the
  cue shows **relative bearing** to the steerpoint (12 o'clock = ahead, 3/9 o'clock = 90° off, 6
  o'clock = behind) — this is exactly Chuck's tadpole "up=ahead/down=behind" behavior, generalized to
  the full clock-face.
- **Boundary-pinned**: if the steerpoint's HUD-relative position exceeds the cue's lateral displacement
  limits, the cue clamps to the left/right edge of its travel range rather than disappearing — same
  clamp philosophy as the Diamond Symbol's crossed-out-when-outside-TFOV behavior.

#### HSD Azimuth Steering Line (ED EA Guide p.226 — new element not in Chuck's HUD file, belongs on the
HSD MFD, see `cockpit-displays.md`/`navigation-ils.md`)
A line on the HSD showing aircraft-heading alignment relative to the selected steerpoint/SPI/weapon
solution; if offset from the display's centered watermark, turn toward the line until centered — the
HSD-format equivalent of the HUD's Great Circle Steering Cue, useful cross-check for a nav-guidance
implementation that must drive both displays from one steering-error computation.

---

### BMS Dash-34 addendum — HUD symbology geometry

*Source: `TO 1F-16CM/AM-34-1-1 BMS` pp. 96–127, 217–221, 299, 410–438, 489, 542–543, 593–636, plus
`TO 1F-16CM/AM-1 BMS` pp. 56, 115, 166–168. This section closes the standing gap "the source set
documents no TD box and no DLZ drawing" — it does not. Read the simulator-vs-factory caveat in the
header first.*

**Unit convention in this section:** `mR` = milliradian, the manual's own unit for HUD symbol size,
measured as an angle in the HUD's total field of view (TFOV). A symbol quoted at 25 mR subtends
25 mrad ≈ 1.43°. Degrees are used where the manual uses degrees (displacements from the boresight
cross, ladder spacing, arc sweeps). **Every size below is quoted verbatim from the manual with its
page.** Where the manual shows only a labelled figure with no dimensions, the entry says
*"figure without coordinates"* — that is the honest state, not an omission to be filled by guessing.

#### A1 — HUD hardware and controls `[BMS-34 pp.96–102]`

| Item | Content |
|---|---|
| LRUs | Pilot Display Unit (PDU), PDU Mount, HUD Control Panel `[p.96]` |
| PDU | chassis, combiner glass, CRT, integrated control panel, self-test, optics; combiner doubles as ejection/canopy-loss blast deflector `[p.97]` |
| Combiner patterns | **WAC** = Wide Angle Conventional (Block 50/52) · **WAR** = Wide Angle Raster (Block 40/42) `[p.97]` — figure without dimensions |
| Mount | aligned to aircraft longitudinal axis and weapon firing line ⇒ PDU replaceable without boresighting `[p.97]` |
| Drawing | stroke-written symbols + video raster; symbols commanded by the **MMC** `[p.96]` |
| Backup | standby reticle is generated independently of the primary symbology electronics — usable for A-G delivery after a HUD symbology failure `[p.96]` |
| Camera | ACCTVS colour TV camera records symbology over the outside world / sensor video `[p.96]` |

**ICP-mounted HUD controls** `[p.98]`

| Control | Function |
|---|---|
| **SYM** | rotary + OFF detent; OFF cuts power to the **stroke symbology** generator; CW increases stroke brightness |
| **BRT** | rotary + OFF detent; OFF cuts power to the **raster video** circuits; CW increases raster brightness |
| **RET DEPR** | depresses the selected (primary or standby) reticle **0 … 260 mR**; the depression amount is displayed digitally on the HUD |
| **CONT** | **nonfunctional in BMS** — explicit N/I marker |
| **DRIFT C/O** | `NORM` = FPM and steering bar show the true wind-affected flight path · `DRIFT C/O` = FPM inhibited from drifting in azimuth, which centres the pitch ladder between the side scales · lower momentary `WARN RESET` = clears the HUD `WARN` message |

**HUD Control Panel switches** `[pp.99–102]` — supersedes/refines the Chuck + ED tables above:

| # | Switch | Positions and effect |
|---|---|---|
| 1 | Scales (VV/VAH) | governs vertical-velocity, airspeed, altitude, heading scales **and the BAI**; `VV/VAH` · `VAH` · `OFF` |
| 2 | FPM | governs FPM, attitude bars, roll indicator, **"FLYING W" steering cue**, BAI. `ATT/FPM` = all · `FPM` = FPM + BAI only · `OFF` = FPM, attitude bars, roll indicator, BAI all removed |
| 3 | DED/PFLD DATA | places the **five rows** of DED or PFLD data in the lower HUD windows |
| 4 | DEPR RET | `STBY` = standby reticle, **all other symbols cleared** (needs ICP **BRT** knob OFF to be visible) · `PRI` = primary reticle, other symbology unaffected · `OFF` = no reticle |
| 5 | Velocity | `CAS` · `TAS` · `GND SPD`. **Gear handle down ⇒ CAS forced regardless of switch/airspeed**; **DGFT ⇒ CAS auto-selected**; `GND SPD` additionally displays magnetic **ground track** |
| 6 | ALT | `RADAR` (AGL) · `BARO` (MSL, and selects **CADC** as the altitude source) · `AUTO` (switches scales by condition, see A5) |
| 7 | Brightness | `DAY` off→max · `NIGHT` off→½ max · `AUTO BRT` maintains symbol/background contrast from the ambient light sensor. **Also drives HMCS**: HMCS day max **10,000 foot-Lamberts**, night max 1/20 of that = **500 foot-Lamberts**; in AUTO the HMCS uses its own ABC sensor |
| 8 | TEST | **not implemented** |

#### A2 — Master-mode window layout (figures, positions only)

The manual gives four full-HUD callout figures. They are **drawings with labelled leader lines and no
coordinate grid** — the tables below record *which element sits where*, which is exactly what the
figures support, and nothing more. Sizes come from §A3, never from measuring these figures.

**NAV master mode** `[BMS-34 p.103]`

| Region | Elements, outer → inner |
|---|---|
| Top centre | boresight cross; steerpoint diamond; **bank angle indicator** arc marks flanking the ladder |
| Top right | flight members 1–4; **PDLT** (octagon symbol with a 2-digit number) |
| Left column, top→bottom | **SOI asterisk** (above the airspeed scale); current G; desired airspeed caret (only if TOS/HOME/RNG/EDR selected); velocity scale tape with the `C`/`T`/`G` mnemonic above the fixed index; boxed digital airspeed with right-pointing chevron; **flight member TOI** (boxed member number); Mach no.; maximum G since reset; **master-mode text**; magnetic bearing/range from bullseye (2 groups of 3 digits) |
| Centre | attitude bars + horizon line; FPM; great circle steering cue (adjacent to FPM); magnetic heading tape at bottom centre with the boxed 3-digit heading; roll indicator below it; team members 5–8 flanking; steerpoint-symbol-out-of-HUD (crossed diamond) pinned at the bottom |
| Right column, top→bottom | offset aimpoint triangle; vertical velocity symbol; altitude scale tape; boxed digital altitude with left-pointing chevron; radar altitude window (`R` or boxed `AR` + value); **`AL` + ALOW value**; sighting-option slant range (source letter + value, e.g. `B005.9`); time to steerpoint `mmm:ss`; distance-to-steerpoint `>` steerpoint number (e.g. `005>04`) |

**A-A master mode** `[BMS-34 p.104]` (AIM-120 slaved example)

| Region | Elements |
|---|---|
| Centre | FCR **target designator box** with the **missile diamond** inside it; target altitude (thousands of ft) immediately below the TD box; **TGP TD box** drawn dashed, separately; **ASEC** (large circle); **ASC** (small circle) inside it; target aspect triangle on the missile reticle |
| Left | boxed digital airspeed; `ARM`; Mach; A-A mode + selected weapon and quantity (e.g. `2 MRM`); bullseye bearing/range |
| Right | **DLZ / linear missile scale** with `RMAX2` at the top of the staple, the **maneuver zone** as an inner box, `RMIN2` at its bottom; loft angle / DMC above the scale; boxed digital altitude; `AL`; slant range; time to steerpoint; distance>steerpoint |

**DGFT master mode** `[BMS-34 p.105]`

| Region | Elements |
|---|---|
| Top centre | aspect angle text (`AA` + 2-digit + `L`/`R`) above the boresight cross |
| Centre | FCR TD box drawn as the **EEGS TD arc/circle** with the **RNG cue** (unwinding arc) and the **aspect caret** |
| Right | DLZ with `RMAX1` / maneuver zone / `RMIN1`; closure rate left of the scale; time of flight below it |
| Bottom | **Attitude Awareness Arc** spanning between the airspeed and altitude references; ghost horizon line; `ARM` |
| Not shown in DGFT | missile reticle, ASEC and ASC are **not displayed** `[p.594]`; the TD box is replaced by the EEGS TD arc; DLZ and aspect angle are shown automatically |

**A-G master mode (CCRP example)** `[BMS-34 p.106]`

| Region | Elements |
|---|---|
| Centre | **bomb fall / steering line** = a vertical line through the display; **solution cue** = short horizontal line on it above the FPM; FPM on the line; FCR TD box / TGP TD designator; crossed TD box pinned at the bottom when the target is out of HUD |
| Right | **release angle scale**; altitude tape; `AL`; slant range (letter + value); time delay; bearing and range to target |
| Left | current G; velocity scale; `ARM`; Mach; A-G submode text (`CCRP`) |

**CCIP** `[BMS-34 p.413]` adds, on the bomb fall line: the **PUAC** staple above the FPM, the steerpoint
diamond, the **pipper** (circle + centre dot) below the FPM, and the right-column readouts
`AL` / slant range / **time-to-go to release** / time to steerpoint.
**CCRP loft** `[BMS-34 p.433]` adds the **max toss anticipation cue** (large circle) and the
**vertical steering cue** (horizontal line *below* the FPM, opposite the solution cue).

#### A3 — Element geometry (the numbers the earlier sources did not have)

*All from the HUD chapter unless noted. `X` = the limit-X overlay drawn when a symbol is clamped at
the edge of the field of view.*

| Element | Geometry | Page |
|---|---|---|
| **Flight Path Marker** | 10 mR circle + 10 mR wings (aircraft-stabilized) + **5 mR tail upward**; `X` overlaid when clamped; **flashes on A-G release/jettison consent** until WPN REL is released | `[p.108]` |
| **"W" steering cue** | if FPM is not displayed and ILS is selected, the W-cue is drawn **11 mR below the boresight cross** | `[p.108]` |
| **Great Circle Steering Cue** | **12 mR line** radiating from a **6 mR circle**; roll-stabilized; line at 12 o'clock = 0° bearing to steerpoint, 3 o'clock = 90° right | `[p.108]` |
| **Steerpoint diamond** | position = selected steerpoint + cursor offsets; **hidden when it coincides with the A-G TD box**; **not displayed at all in A-A, DGFT or MSL override** | `[p.108]` |
| **Offset aimpoint triangle** | isosceles, **12 mR high × 6 mR wide**; appears when OA1/OA2 sighting is selected on the MFD, in A-G or NAV | `[p.108]` |
| **Boresight cross** | *incomplete* plus symbol at 0° azimuth = fuselage reference line; **displayed in all modes** | `[p.108]` |
| **Primary reticle** | 2 mR pipper + **dashed 50 mR** inner circle + **solid 100 mR** outer circle; depression 0–260 mR by RET DEPR, digits at HUD window 30 | `[p.109]` |
| **Standby reticle** | 2 mR pipper + **dotted 50 mR** + **dotted 100 mR**, the outer circle carrying **four 6 mR tics at 3/6/9/12 o'clock** | `[p.109]` |
| **FCR A-A TD box (primary)** | **25 mR square** | `[p.109]` |
| **FCR A-A TD box (secondary)** | **15 mR square** | `[p.109]` |
| **TD box in ACM or RWR** | box and target-locator-line box drawn **dotted** | `[p.109]` |
| **TGP A-A TD box** | **25 mR square, dotted** | `[pp.104, 109]` |
| **Target Locator Line (TLL)** | **40 mR dotted line** from the gun boresight cross, oriented at the relative bearing to the target; text = source letter (`T` = TGP, `F` = FCR) + **2-digit target angle**, updated in **1° increments** | `[pp.104, 109]` |
| **Roll indicator** | marks on a circular reference of **radius 70 mR**, the circle **centred 50 mR below TFOV centre**; marks every **10°** except the outermost pair at **45°**; aircraft-fixed; caret travel limited to **±45°**, beyond which the caret's outer portion disappears | `[p.110]` |
| **Bank Angle Indicator (BAI)** | arc of marks spanning **120° around the FPM**; three large **3 mR** marks at **0°/30°/60°** bank; small **1 mR** marks at **10°** and **20°**; horizon-aligned, caged to the FPM; **the FPM's tail is the index**; shown whenever the FPM is shown *and* the vertical velocity scale is selected | `[p.110]` |
| **Airspeed scale** | tape in tens of knots, marks every **50 kt**; mnemonic `C`/`T`/`G` above the fixed index; digital value **left** of the scale, **cannot be decluttered** | `[p.110]` |
| **Barometric altitude scale** | hundreds of feet, digital labels every **500 ft**, reference tics every **100 ft**; thousands comma after the first digit; **last digit always 0**; radar altitude digits below the scale in an `R` window (blank if unavailable) | `[pp.110–111]` |
| **Landing-expanded altitude scale** | scale expands **×5**: tics become **20 ft**, labelled intervals **100 ft** | `[p.111]` |
| **Radar altitude scale** | same tape geometry as baro; `R` above the index line; **below 1500 ft descending, commas and leading zeros are dropped and digits read in feet rather than hundreds of feet** | `[p.111]` |
| **Automatic radar altitude (thermometer)** | selected by ALT=`AUTO`, displayed **below 1200 ft descending / below 1500 ft ascending**; fixed tics **0…1500 ft**; digital readout to the nearest **10 ft** marked `AR`; if radar altitude is lost the digits blank, **the window flashes**, and the baro scale replaces it; carries the **`AL` window and a T-bar on the scale at the ALOW value**; `AL` **flashes** below the ALOW setting | `[pp.111, 117–118]` |
| **Vertical velocity scale** | moving scale against a fixed index shared with the altitude index (zero VV reference); reference tics every **500 fpm** | `[p.111]` |
| **Attitude bars** | **solid 25 mR lines above** the horizon, **dashed below**; labelled with 1–2 digits; **5° spacing from ±5° to ±85°**, drawn at a **1:1 ratio**; **at least three pitch lines always visible**; above the horizon the elbows point inward, below they point toward the horizon (negative bars curve into a funnel); **the −2.5° line does not bend and appears only in landing mode**; bars have a centre gap for the FPM; roll-stabilized | `[p.112]` |
| **Horizon line** | wider than the attitude bars; **solid** = extended horizon line (EHL, true horizon inside TFOV); **dashed** = ghost horizon line (GHL); longer than the HUD horizon line but routed so it never obscures the airspeed/altitude/heading digital boxes or the A-A/A-G missile-launch-envelope data | `[p.112]` |
| **Ghost horizon line** | drawn on the edge of an imaginary circle of **radius 8°** centred in the HUD FOV, rotating about that centre with bank (at 90° bank it sits on the side of the circle) | `[p.112]` |
| **Zenith symbol** | at **+90° pitch**; elongated star, longest arms pointing toward the horizon line | `[p.112]` |
| **Nadir symbol** | at **−90° pitch**; **31.4 mR circle** with a **15.7 mR line** pointing toward the horizon, plus **twelve internal lines** parallel to the horizon | `[p.112]` |
| **Attitude Awareness Arc (AAA)** | DGFT only; **radius 99.5 mR from TFOV centre**, drawn between the airspeed and altitude references; wings-level ⇒ two tics per side extending **5.2 mR radially outward** toward the scales; arc **length minimum 10.35 mR at +87° pitch**; at **−87°** it has closed to a circle with a **10.35 mR gap**; **blanked when landing conditions are met** even in DGFT | `[p.113]` |
| **AAA/GHL stabilization** | roll-stabilized about the **TFOV centre**, unaffected by wind or yaw; the arc represents the ground, the chord joining its ends represents the horizon; a full semicircle = 0° pitch wings level; inverted flight draws an inverted arc at the top of the FOV | `[p.595]` |
| **HUD SOI marker** | **asterisk** at the upper left, **above the airspeed scale** | `[p.108]` |
| **HUD WARN** | flashing `WARN` at HUD **centre** + VMS "WARNING-WARNING"; **not** reset by MASTER CAUTION — only by the ICP **WARN RESET** momentary; a flashing `FUEL` may appear **below** it; a steady `TRP FUEL` **replaces the NAV mode indication on the left** | `[p.107]` |

**HUD cursor** — when the HUD is SOI (commanded **DMS up**), an asterisk appears top-left and a movable
cursor / TD box is drawn; slewed by the throttle cursor switch, designated by **TMS up**. The two uses
are **HUD markpoints** and **A-G missile VIS mode**. The cursor's slew rate is optimized to track the
**nearest-in-range** of: A-G TD box, steerpoint diamond, offset aimpoint symbol, pop-up point symbol
`[pp.103, 107]`.

#### A4 — Occlusion, declutter, blanking

**Symbology occlusion priority** `[BMS-34 p.113]`
- Boresight cross and limit-X symbols are **never occluded in EEGS submode**, except by the CCIP
  reticle, the in-range cue, the A-G TD box and OAP symbols; the boresight cross may additionally be
  occluded by the AIM-9 and AIM-120 missile diamonds.
- **All symbology is occluded inside** the A-A or A-G TD box.
- When CCIP reticle + OAP + A-G TD box are simultaneously displayed, priority is
  **1. the 12 mR CCIP reticle (with or without its 16 mR in-range cue) → 2. the A-G TD box → 3. the OAP symbol.**

**Landing declutter** `[BMS-34 p.113]` — in NAV and landing modes, a press-and-release of **UNCAGE**
shifts the **heading scale to the UP position** and removes: roll indicator · ILS bars · flight
director symbol · DED data. It is restored by any of: leaving NAV · leaving landing mode (gear up) ·
weight-on-wheels · a second UNCAGE press.

**HUD blanking (HMCS declutter)** `[BMS-34 p.299]` — removes **all HMCS symbols** when the HMCS LOS is
inside the HUD instantaneous FOV; the blanking region is **|ΔAz| < 10° and |ΔEl| < 10°** between the
HMCS LOS and the HUD CTFOV. Selected on the DED HMCS DISPLAY page (asterisks on `HUD BLNK`, M-SEL).
Cockpit blanking (`CKPT BLNK`) removes all HMCS symbols **except** missile diamond, steerpoint diamond,
aiming cross, ACM bore symbol and TD box when the HMCS LOS is below the canopy rails.

#### A5 — Radar altimeter, ALOW and the altitude advisories `[BMS-34 pp.114–126]`

| Parameter | Value | Page |
|---|---|---|
| CARA principle / range | FMCW, **0 – 50,000 ft** | `[p.114]` |
| CARA accuracy | **±2 % below 5000 ft**, **±1 % above 5000 ft** | `[p.114]` |
| CARA tracking envelope | below **3000 ft AGL**: ±30° pitch, ±60° roll; at **50,000 ft AGL**: ±10° both | `[p.114]` |
| RDR ALT switch (SNSR PWR) | `OFF` · `STBY` (powered, not transmitting) · `RDR ALT` (transmitting/tracking) | `[p.115]` |
| CARA IBIT | started from **RALT OSB on the MFD TEST page**; ≈**5 s**; `300 feet` mnemonic shown under RALT; **no radar altitude on the HUD during IBIT** | `[p.116]` |
| Known false track | engine-inlet return read as a valid altitude, typically **110–180 ft** | `[pp.115, 124]` |
| ALOW reset after EPU check | electrical transients can reset ALOW to **500 ft** and freeze the digital radar altitude — cycle CARA power | `[pp.115, 124]` |

**ALOW (CARA altitude low)** `[pp.115, 123–124]`
- Below the ALOW setting: **flashing `AL` mnemonic + flashing altitude-low readout** on the HUD, plus a
  single VMS **"ALTITUDE-ALTITUDE"**.
- Aural message suppressed when: gear handle down · radar altitude **< 15 ft** · above the ALOW setting ·
  **climb rate > 1200 fpm**.
- The flashing HUD indication stops when radar altitude **< 15 ft** or above the ALOW setting; it is
  also inhibited when **airspeed < 80 kt**.
- CARA rearms for a repeat warning on: loss of ground track · **climb rate > 1200 fpm** · ALOW lowered
  below current altitude · climbing above the ALOW setting.
- Set on the **DED ALOW page** (ICP priority key **2**), rounded up to the nearest **10 ft**.

**Line-in-the-Sky / MSL FLOOR** `[pp.125]` — barometric-MSL advisory, set on the same DED ALOW page via
DCS; independent of CARA. One **"ALTITUDE-ALTITUDE"** on each descent through the value after having
been above it. **0 disables it**; valid range **−1500 ft … 80,000 ft**; retained across MMC power cycles.

**GAAF break-X (the pull-up cue)** `[pp.119–122]` — *this is the F-16's pull-up cue and its geometry
class ("break-X"), which the previous source set did not name.*
- Enabled when: **gear up** · **AGL > 50 ft** · **descent rate ≥ 960 fpm (> 16 ft/s)** · an active
  sensor can determine height above target.
- ≈**2 s before** the advisory altitude: **flashing break-X on each MFD**. At the advisory altitude:
  **flashing break-X on the HUD** + VMS **"PULL UP - PULL UP"**.
- In A-G modes the **PUAC** rises toward the FPM as advance warning; **the HUD break-X and the voice
  message trigger when the PUAC reaches the FPM**.
- AGL sensor hierarchy (deliberately different from the SPI hierarchy; **system/BARO altitude is never
  used**, so with none of these available no GAAF advisory is issued):
  slaved TGP laser ranging → FCR AGR → CARA → tracking TGP with laser ranging → FCR FTT → tracking TGP with CFOV ranging.
- Recovery model: assumed **pilot reaction 1.0 s**, reduced to **0.45 s** in A-G master modes when dive/climb
  ≤ 15°, bank ≤ 20° and **CAS ≥ 375 kt** (anti-nuisance during strafe and low-angle dive bombing);
  wings-level pull to **4.0 g** or max available g; g-onset compensation **1 s at ≥4 g, 0 s at ≤1 g,
  linear between**; roll-response time from max roll rate for the SMS configuration and the CAT I/III switch.
- Clearance margin: **buffer = 25 % of predicted altitude loss at ≤325 kt, 12.5 % at ≥375 kt** (linear
  between) **plus a pad of 150 ft at ≤325 kt / 50 ft at ≥375 kt** (linear between).
- **TUGAA** = (AGL height − computed break-X altitude) ÷ vertical velocity.
- **PGCAS is explicitly not implemented** in BMS.

**DWAT** `[p.123]` — aural-only "ALTITUDE-ALTITUDE", **once per takeoff**, gear up, within **3 min** of
takeoff, aircraft between **+300 ft and +10,000 ft** above runway altitude, and the current descent rate
would reach runway altitude within **30 s**.

**Attitude Advisory Function** `[p.126]` — flashing boxed **`CHECK ATTITUDE`** on **all MFD formats of
both displays** (not the HUD) when a TGP format is displayed in A-G, INS attitude valid, aircraft below
the DED MSL FLOOR, and either **bank > 75° with pitch < 0°** or **pitch < −20°**. Default colour red,
DTC-loadable. Disabled by MSL FLOOR = 0.

#### A6 — Air-to-air weapon symbology

**Missile reticle diameters (FOV of the seeker)** `[BMS-34 pp.611–612]` — the plate on p.612 is a
matrix of **seeker state (columns) × LOS/FOV option (rows)**; read verbatim:

| Missile / option | radar search, missile caged | radar locked, missile caged | uncage commanded | uncage commanded **and self-tracking** |
|---|---|---|---|---|
| AIM-9L/M **BORE/SPOT** | 65 mR | 65 mR | 65 mR | 65 mR |
| AIM-9L/M **BORE/SCAN** | 100 mR | 100 mR | 100 mR | **65 mR** |
| AIM-9L/M **SLAVE/SPOT** | 65 mR | 65 mR | 65 mR | 65 mR |
| AIM-9L/M **SLAVE/SCAN** | 100 mR | 100 mR | 100 mR | **65 mR** |
| **AIM-9P** | 35 mR | 35 mR | **N/A** | 35 mR |
| **AIM-9X / Python 4/5 / IRIS-T** | 65 mR | 65 mR | 65 mR | 65 mR |

The SCAN→65 mR collapse in the last column is the plate's way of stating the p.611 rule: the AIM-9L/M
reticle is **65 mR whenever SPOT is selected *or* the missile is self-tracking**, **100 mR only while
SCAN is selected and the seeker is not yet self-tracking**.

Note `[p.611]`: the AIM-9L/M reticle is **65 mR with SPOT selected or while self-tracking**, **100 mR
with SCAN**. On the reticle, within **12,000 ft** slant range: four reference tics at 3/6/9/12 o'clock;
a **range-to-target cue** (thick tic + range gap) scaled **100 ft per clock position, clockwise from 0 ft
at 12 o'clock**; a **solid target aspect triangle** (0–90° = departing, 90–180° = closing). Beyond
12,000 ft, and in search, the tics/range tic/range gap/aspect caret are all removed. **The reticle
flashes when the target is inside the maneuver zone.**

**Missile diamonds** `[pp.613–614, 628]`

| Symbol | Geometry |
|---|---|
| AIM-9L/M diamond | square rotated 45°, **6 mR per side**; represents the seeker LOS |
| AIM-9 uncaged / self-tracking | diamond **expands to 18 mR** — the visual confirmation, with the tone, that the missile is tracking |
| AIM-9 boresight rest position | centred in azimuth, **3° below the boresight cross** (= the armament datum line, the pylon longitudinal axis) |
| AIM-9 SCAN | diamond **precesses** — circles around the selected LOS, in SLAVE or BORE |
| AIM-120 "hairy diamond" | the caged-SRM diamond **with four tics added**; sits at the **centre of the TD box** when slaved to a bugged track; in BORE it sits at the **centre of the steering error circle, ≈6° below the borecross** |
| Clamped | limit **X** overlaid at the FOV edge |
| In range | the AIM-120 diamond **flashes** when slant range is between **RPI and RMIN** |

**AIM-120 bore reticle** `[p.629]` — circle of **262 mR diameter**, HUD only (not drawn on the MFD),
shown when the sighting option is BORE, or SLAVE with no designated target and weapon status RDY/SIM.

**Linear Missile Scale (LMS) / DLZ** — the drawing the earlier source set lacked.

*Composition* `[p.615]`: upper and lower range-scale tics (**HUD only**) · radar range scale value ·
DLZ · target range caret · target closure rate (or `COAS` in main-beam-clutter coast) · pre-launch range.
The LMS may still be drawn on HUD/HMCS when a DLZ cannot be computed; on the **MFD it is not**.

*Scaling* `[pp.615–616]`:

| Form | Condition | Upper tic represents | Lower tic represents |
|---|---|---|---|
| Unexpanded SRM LMS | target range **> 110 % of RMAX1** | the **radar range scale value** | zero range (**at 25 % height on the MFD**) |
| Expanded SRM LMS | target range **≤ 110 % of RMAX1** | **110 % of RMAX1** | zero range (25 % height on the MFD) |
| Unexpanded AIM-120 DLZ | target range **> 125 % of R_AERO** | FCR range scale ("dynamic range tic mark") | zero range tic mark |
| Expanded AIM-120 DLZ | target range **≤ 125 % of R_AERO** | R_AERO grows up to the former tic mark | zero range tic mark |

*SRM DLZ staple* `[p.616]` — drawn **on the right side of both HUD and MFD**; outer bracket from
**RMAX1 (top) to RMIN1 (bottom)**, inner narrow box = **maneuver zone from RMAX2 down to RMIN2**:

| Range | Definition |
|---|---|
| **RMAX1** | non-maneuvering target, **low** termination criteria |
| **RMAX2 (RTR, Range Turn and Run)** | target performs an **8 g turn (derated for altitude) away** at launch then accelerates; **high** termination criteria — the same maneuver assumed for the AIM-120 RTR |
| **RMIN2 (RTC, Range Turn to Close)** | same maneuver, but turning **toward** the shooter; high termination criteria |
| **RMIN1** | target unmaneuvering, low termination criteria |

*AIM-120 DLZ stack* `[pp.628–633]` — top→bottom on the scale, this is the literal drawing order:

| Marker | Drawn as | Meaning |
|---|---|---|
| FCR range scale value | 2-digit number above the upper tic | scale of the unexpanded DLZ |
| **loft angle** or **DMC** | number above the range caret; loft carries a **`°`** suffix | required pitch (loft) *or* the target heading change that degrades the shot; **mutually exclusive** |
| closure speed | number left of the caret | kt |
| **target range caret** | `>` | own range to target |
| A/M-pole | number below the caret (e.g. `30M`) | range at MPRF-active / at impact |
| **R_AERO** | **open left-pointing triangle `◁`** | max kinematic range, optimal loft/steering, nominal termination |
| **R_OPT** | **small circle `○`** | as R_AERO but **high** termination criteria |
| **R_PI** | top of the **outer staple** (opens right) | high termination criteria **without** lofting or azimuth change |
| **R_TR** | top of the **inner box** (maneuver zone) | max range if the target turns tail-on at launch |
| **R_MIN** | bottom of the outer staple / inner box | minimum range |
| zero range tic | short line at the bottom | scale origin |
| post-launch line | below the DLZ, e.g. `24M`/`A05`, `23M`/`M03`, `10F`/`T06` | predicted A-pole or F-pole + **time to HPRF active (A) / MPRF active (M) / impact (T) / termination (L)**, counting down dynamically |

**ASEC — Allowable Steering Error Circle** `[p.634]`

| Condition | Radius |
|---|---|
| Target outside R_AERO … R_OPT | **11 mR** (minimum) |
| R_OPT → R_PI | **grows** |
| At R_PI | **maximum**, representing **45° of allowable steering error** |
| R_PI → midpoint R_TR | holds maximum |
| Midpoint R_TR → R_MIN | shrinks back to minimum |
| Slaved, overall range | **11 mR … 56 mR** radius |
| Bore shot | **static 131 mR radius (262 mR diameter)** |

Displayed when AIM-120 selected, sighting = SLAVE, a bugged target exists **and** weapon status is
RDY or SIM. **Flashes when target range is inside the maneuver zone.** Identical in function on the MFD.
**Not displayed in DGFT.**

**ASC — Attack Steering Cue** `[p.634]` — **8 mR diameter circle on the HUD**, 10-pixel-radius circle on
the MFD. Steering law by range:

| Target range | Steering provided |
|---|---|
| **> 1.2 × R_AERO** | horizontal **aircraft** steering, bounded by the ASEC limits and a **45° LOS-to-target limit** |
| **1.2 R_AERO … R_AERO** | blended aircraft + missile steering |
| **inside R_AERO** | optimal **horizontal and vertical missile** steering |
| **inside midpoint R_TR** | **R_MIN steering** (shortest LOS), loft component blended out |

Technique: roll until the ASC is on the HUD centreline above the ASEC centre, then pull to put the ASC
in the centre of the ASEC. A **limit cross (X) inside the ASC** means no AIM-120 shot exists even with
a loft — displayed when range > R_AERO **or** when the required lead angle **exceeds 60°**.
**Not displayed in DGFT.**

**Loft solution cue** `[p.635]` — a **digital pitch readout** on the AIM-120 LMS, appearing when target
range reaches R_AERO: the pitch that aligns R_PI with R_OPT. Removed once inside R_PI. **Not shown on
the HMCS**; **suppressed whenever a DMC solution is available** (mutually exclusive).

**EEGS — Enhanced Envelope Gunsight** `[BMS-34 pp.596–600]` — five algorithm levels, escalating
automatically as radar range, velocity and acceleration become valid.

| Element | Description |
|---|---|
| **EEGS funnel** | drawn without a radar lock; **funnel width derived from the target wingspan entered on the ICP LIST 5 (MAN) page**; firing solution = target's wingtips touching both sides |
| **MRGS lines** | Multiple Reference Gunsight lines along the **bottom of the HUD**; for high-LOS-rate snapshots; **disappear once locked** |
| **FEDS** | with the trigger held and **no** lock: simulated rounds as **dot pairs** running down the funnel sides like tracers |
| **TD circle** | appears over a locked target; **unwinds counter-clockwise to show target range inside 12,000 ft** |
| **Funnel after lock** | radar velocity aligns the funnel with the **target's plane of motion** |
| **1 G pipper** | **plus** symbol inside the funnel — lead for a non-maneuvering target |
| **Max-G pipper** | **minus** symbol — lead for a target pulling **7.3 g** directly at you |
| **Out-of-plane potential** | the two lines flanking the plus — how far the target could jink laterally during bullet time of flight |
| **Predicted lead** | a **4 mil circle**, once acceleration is solved and in range — lead assuming the target holds its current flight path |
| **BATR** | **6 mil circle** at the bullets' position as they pass the target, drawn instead of FEDS whenever a target is locked; disappears as the last rounds of the burst pass |
| **All pippers** | ≈**¼ s settling time** — track ¼ s before firing |

Other gunsights `[pp.598–599]`:
- **SNAP**: a gun "snake" tracing where past rounds are, with **horizontal ticks at ½ s, 1 s, 1½ s** of
  bullet time of flight, plus a small range circle at the **manual range setting (700 or 1500 ft)** or at
  target range when locked.
- **LCOS**: pipper **sized to the target wingspan at the set range** (700/1500 ft) unlocked; when locked
  the outer ring becomes a **"range L" that unwinds** like the EEGS TD circle, and an **overtake caret**
  shows closure at **100 kt per clock position** (12 o'clock = 0, right = positive, 6 o'clock = ≥600 kt);
  a **lag line** from the pipper centre shows which way the pipper is moving.
- **SSLC**: the SNAP snake plus the LCOS pipper **without** the lag line.

**ACM HUD cues** `[BMS-34 pp.217–221]` — these are FCR-driven but drawn on the HUD:

| ACM submode | HUD cue | Scan geometry |
|---|---|---|
| **30° × 20°** | **no HUD symbology** | body-stabilized, 4-bar, scan centre **6° below the HUD bore cross**; covers slightly more than the HUD FOV |
| **BORESIGHT** | a **cross whose intersection is 3° below the bore cross** | one-beamwidth, non-scanning, on the fuselage reference line; TMS-up-and-hold inhibits acquisition and allows slewing the cross within the HUD FOV |
| **10° × 60°** | a **vertical line from the HUD bore cross to the bottom of the HUD** | body-stabilized, 4-bar, scan centre **23° above the bore cross**, coverage **53° above to 7° below** |
| **SLEWABLE** | an **8 mR circle** antenna-pointing symbol at the scan centre, with min/max search altitudes printed above/below it; a **large cross fixed at 3° below the boresight cross** marks the initialized position | space-stabilized in pitch/roll, ≈**20° high × 60° wide**, initialized at 0° az / horizon |

All four use a **10 NM contact range scale**. On entry to ACM the transmitter is off and **`NO RAD` is
displayed above the HUD boresight cross** `[p.222]`.

**SRM correlation arrowheads** `[p.618]` — at the end of an FCR or TGP target locator line:
**single arrowhead** = SRM caged and correlated to that TLL; **double arrowhead** = SRM uncaged and
correlated. Target locator **angle** text is drawn on the HUD only, never on the HMCS.

#### A7 — Air-to-ground weapon symbology

**Strafe reticle (STRF)** `[BMS-34 p.410]`

| Element | Geometry / rule |
|---|---|
| Outer circle | **50 mR** |
| Partial circle | **40 mR** |
| In-range cue | **2 mR diameter** |
| Pipper | **1 mR** |
| Bullet track line | from the **centre of the gun boresight cross**, extending **5 mR beyond the pipper centre** |
| Range tics, moving target indices | MTI show the lead for a target crossing at **30 kt**, from slant range and bullet time of fall |
| In-range trigger | slant range ≤ the Ammo In-range value (MFDS or DTE), enterable **0 … 99,990 ft**, 4 digits, tens digit forced 0. Defaults: **4,000 ft with M-56**, **12,000 ft with PGU-28** |
| Slant range | digital, **lower right of the HUD** |

**CCIP** `[BMS-34 pp.412–415]`

| Element | Geometry / rule |
|---|---|
| **CCIP pipper** | **1 mR dot at the centre of a 12 mR circle**; limit **X** at the edge of the TFOV |
| In-range cue | **16 mR** (from the occlusion priority list, `[p.113]`) |
| Impact point below the nose | pipper is parked **≈14° below the boresight cross** and a **time-delay cue** is drawn on the bomb fall line; the delay is computed from pipper-vs-impact-point disparity |
| Post-designate | **azimuth steering line + solution cue** appear; automatic release when the solution cue reaches the **centre of the FPM**; **the FPM flashes** while WPN REL is held during a computed release |
| Ripple | CCIP symbol is offset by **half the bomb train length** so the string straddles the target |
| Ranging priority | **TGP laser → AGR (FCR) → selected backup bombing sensor (RALT / BARO / PR)**; on loss of lock the last data is held **3 s**, then BARO; then steerpoint elevation is used as target elevation |
| Laser | trigger 1st detent = laser while held; 2nd detent = continuous; stops at release, submode change or **30 s** |
| Delivery-mode rotary (MSL STEP) | **CCRP → CCIP → DTOS** |

**DTOS** `[BMS-34 pp.416–418]` — TD box starts **on the FPM**, slewed by CURSOR/ENABLE, ground-stabilized
by TMS-forward or WPN REL. **Max toss anticipation cue = a fixed-radius circle, 100 mR diameter**,
appearing **2 s before the solution cue**, flashing when max toss range is reached and for **2 s more**.
Solution cue first appearance means a **2-second incremental 4 g pull to a 45° climb** will hit at max
toss range. Post-designate slews re-initialize slant range; between slews the MMC uses the slant range
captured at designation.

**CCRP / loft** `[BMS-34 pp.427–434]`

| Element | Geometry / rule |
|---|---|
| **A-G TD box** | **10 mR square with a 1 mR pipper at its centre**; limit **X** when outside the HUD FOV, or replaced by the AGTLL; positioned on the INS representation of the steerpoint |
| **Azimuth Steering Line (ASL)** | vertical line; its lateral displacement is the turn required to bring it onto the FPM; **blanked when weapon status is MAL or blank** |
| **Solution cue** | short **horizontal line on the ASL above the FPM**; appears at **time-to-pull = 10 s** (release angle > 5°) or **time-to-release = 10 s** (≤ 5°); walks down to the FPM; coincidence with the FPM = automatic release with consent held |
| **Vertical Steering Cue (VSC)** | large **horizontal line on the ASL below the FPM**, mirroring the solution cue at equal distance; both converge on the FPM as time-to-pull counts to `000:00`, then **both reset above the FPM** and the VSC commands the pull (**keep the FPM directly below the VSC** = smooth 4 g in 2 s). **Not displayed when the release angle is ≤ 5°** |
| **Max toss anticipation cue** | **100 mil circle at 0° azimuth, −3° elevation** (below the boresight cross); appears **2 s before** max toss release range; **flashes for 2 s** once the range caret reaches the max-release tic. Assumptions: 4.0 g pull initiated when the cue appears and held, **MIL power selected at pull initiation**, smooth 1→4 g in **2 s**, airspeed bleed accounted for; **pulling more than 4.0 g lands short** |
| **Time to pull / time to release** | `mmm:ss`; time-to-pull is **not displayed** at release angles ≤ 5° or for non-loftable high-drag munitions; it is replaced by time-to-release when it reaches `000:00` |
| **PUAC** | a **staple**; **maximum displacement 4° below the FPM**; walks up to the FPM. Serves **fuze arming** (armed from the A-G SMS arming delay / burst altitude) **and** ground avoidance, whichever is more immediate; on reaching the FPM it resets for the ground-avoidance role. **`LOW` mnemonic below the FPM** when below minimum fuze-arming altitude with time-to-release < 10 s. For ripple releases it uses the bomb with the shortest arming time. **Not applicable to rockets, strafe, EO or manual deliveries.** A **large X flashes at 5 Hz** to command an immediate 4.0 g (in 2 s) pull-up |
| **AGTLL** | **40 mR line** from the gun boresight cross toward the target, **0° = straight up, positive angle rotates clockwise**; the target bearing is printed next to the bore cross. **Suppressed when the target is straight down ±10°** to avoid confusion with the ASL — instead the TD box with an X is drawn at the bottom of the HUD |
| **Bearing and range to target** | lower right; **first two digits = bearing in tens of degrees**, second group = range in nm |

**CCRP/LOFT release angle scale** `[BMS-34 pp.432–433]` — displayed when range to target **< 15 nm**,
bearing to target **< 50°**, and the weapon is loftable. Composition top → bottom:

| Element | Meaning |
|---|---|
| scale value above the upper tic | scale range; **upper tic = 10 nm, lower tic = 0 nm** |
| range caret `>` | target range within the 10 nm scale; **pinned adjacent to the upper tic between 15 nm and 10 nm** |
| predicted release angle, left of the caret | climb angle at release for a 4.0 g pull in 2 s; **disappears at or below the level-release tic** |
| outer bracket, DLZ-like | **upper tic = maximum release range (45° toss)**, **lower tic = minimum range (level release)** |
| number below the lower tic | **predicted altitude at release, in hundreds of feet**; disappears at or below the level-release tic |

The caret may sit **below** the lower bracket during a dive delivery.

**Slant range source letters** `[BMS-34 p.430]` — supersedes the 5-letter ED list above:

| Letter | Source |
|---|---|
| `B` | steerpoint elevation / barometric |
| `R` | radar altimeter |
| `F` | FCR (GM fixed-target-track, or AGR selected via CCIP/DTOS/VIS/STRF FTT) |
| `T` | TGP in A-G, priority sensor, in a track mode |
| `L` | as `T`, **and the laser is firing** |
| `P` | DTS passive ranging — **N/I** |
| `H` | HTS providing range |
| `XXX` | **no ranging data available** |

Format: **tenths of a nautical mile above 1 nm, hundreds of feet below 1 nm** `[p.430]`.

**IAM (JDAM/JSOW/WCMD) HUD DLZ** `[BMS-34 pp.542–543]`
- Displayed when an IAM is selected, valid **LAR** data is in the MMC, submode is MPPRE/PRE/VIS
  post-designate, weapon status is REL/RDY/ALN/SIM, and INS + CADC data are valid.
- **No range scale value above the upper tic.** For JDAM and WCMD the DLZ is **normalized so the RMAX1
  tic always sits at 70 % of the weapon's kinematic range**; JSOW PRE and VIS also normalize to 70 %.
- **Outer staple `[` opening right = RMAX1/RMIN1** (kinematic release zone; release is inhibited outside
  it except for JSOW). **Inner staple `]` opening left = RMAX2/RMIN2** (optimum release zone, end-game
  parameters met). JSOW PRE/VIS, CBU-103 and CBU-104 have **no RMAX2/RMIN2**.
- **`JIZ`** (JSOW In-Zone) is printed next to the range caret when the weapon reports in-range; the
  pickle button is always hot while JIZ is shown.
- **Required turn angle below the DLZ** (JDAM only, shown above 60° offset from target bearing):
  one letter `L`/`R` + two digits, e.g. `L05`.
- DLZ blanking: JDAM/WCMD below **Mach 0.5** or above **Mach 1.5**; JDAM also when target bearing
  exceeds ±60°, pitch exceeds ±60°, impact angle < 20°, or impact velocity > 1200 ft/s; WCMD also below
  the weapon's fuze function altitude or target bearing beyond ±45°. JIZ blanks below **Mach 0.6** /
  above **Mach 0.95**, climb/dive beyond ±30°, target bearing beyond ±60°, or above **40,000 ft**.

**HARM Launch Scale (HLS)** `[BMS-34 p.489]` — **right side of the HUD**; four ranges
(**RMAX1/RMAX2/RMIN1/RMIN2**), **two 10 mR tics**, and an in-range cue. The **HARM FOV box flashes when
in range** — that flash is the firing cue; the FOV box is smaller in EOM than in PB/RUK. Two loft
solution cues sit on the ASL, a third (optimal loft cue, two carets) in PB mode only.
**Loft Angle and Apex Altitude are not implemented.** MMZ/AMZ are not available in RUK.

#### A8 — Landing, ILS and AOA `[BMS-1 pp.115, 166–168; BMS-34 p.111]`

**HUD AOA bracket** `[BMS-1 p.115]` — three-point calibration, more precise than the earlier sources:

| FPM against the bracket | AOA |
|---|---|
| aligned with the **top** of the bracket | **11°** |
| **centred** in the bracket | **13°** |
| aligned with the **bottom** | **15°** |

**The HUD AOA display is available only with the NLG lowered.** By contrast the **AOA indexer operates
continuously with the gear handle up or down** `[BMS-1 p.115]` — which resolves, in favour of ED's
reading, the Chuck-vs-ED disagreement recorded above. The **AOA indicator** (instrument panel) is a
vertical tape reading **−5° to ≈+32°**, colour-coded **9°–17°** to match the indexer; the indexer's
correction is referenced to **≈13° AOA**.

**ILS HUD symbology** `[BMS-1 pp.166–167]`

| Element | Detail |
|---|---|
| Deviation bars | **localizer + glide slope**; **roll stabilized**; **tic marks at the one-dot and two-dot deflections**; **dashed bars = invalid data** |
| Replacement | with ILS selected the **great circle steering symbol is replaced by the ILS deviation bars** |
| Flight director (command steering) | a **circle**, a **tic mark at the top of the circle**, and a **reference caret on the heading/ground-track scale** |
| Flight director circle | referenced to the FPM; appears when localizer data is valid; commands a turn to roll out on course within **two dots** of localizer deviation |
| Tic mark | appears on the circle when glide-slope deviation nears centre = **pitch steering valid**; an **X over the tic** = pitch steering invalid (e.g. approaching the glide slope from above) |
| Reference caret | the heading required to hold the DED-selected course (heading scale) or the ground-track error relative to it (ground-track scale). **Course is changeable only via the DED** |
| Intercept limits | intercept the localizer from **≤45° off course** using **≤30° bank**; the flight director is designed to capture the glide slope **from below in level flight** |
| Enabling chain | localizer course on the DED **and** flight director mode-selected on the UFC **and** a PLS selection on the HSI M button |
| Landing-config differences with ILS selected | lower HUD windows (except distance-to-destination) are **not** blanked until NLG down **and inertial velocity > 80 kt**; altitude scale does **not** switch from 100 ft to 20 ft increments until NLG down; **AOA bracket not displayed until NLG down** |

Marker beacon: **75 MHz** fixed, MRK BCN light green, blinking to the beacon code `[BMS-1 p.168]`.

**HUD fuel warnings** `[BMS-1 p.56]` — `FUEL` in the HUD for a bingo/home-mode fuel low condition
(VMS "BINGO-BINGO" with weight off wheels); **flashing `TRP FUEL` + `FUEL`** for a trapped-external-fuel
condition. FWD FUEL LOW caution < **400 lb** forward reservoir, AFT FUEL LOW < **250 lb** aft reservoir.

#### A9 — What these sources still do not give

- **No coordinate table for HUD windows.** The manual refers to numbered "HUD windows" (e.g. "the
  depression amount … can be observed at **HUD window 30**" `[p.109]`, "MASTER ARM … is the word shown in
  **HUD window #3**" `[p.391]`) but **never publishes the window numbering, their pixel or angular
  extents, or their anchor points**. Element *positions* above are therefore region-level statements
  read off labelled figures, not coordinates. **TODO** — remains a gap.
- **No HUD total field of view number.** The BMS manuals discuss TFOV, CTFOV, HTFOV and instantaneous
  FOV extensively but never state the angular size. The ED EA Guide's **25° diameter / 10.5° below
  centre** remains the only sourced figure in this tree.
- **No stroke widths, font sizes, or character cell dimensions.** The DED's character grid *is*
  published (5 rows × 24 characters on a 192 × 64 pixel dot matrix, `[BMS-34 p.127]` — see
  `cockpit-displays.md`); the HUD's is not.
- **No colour specification** — the HUD is monochrome stroke; the MFD symbol colours are DTC-loadable
  `[BMS-34 p.81]` but no palette is published.

---

### Was der Pilot wirklich sieht (What the pilot actually sees — instrumentation ground truth)

Per the coordinator's explicit ask: this is the checklist against which FlightBox's autonomous pilot
(`FBF16Pilot`) should be validated — it may only reason over quantities that a real instrument actually
displays, at the resolution/format that instrument actually provides. Sources: this file + `PROGRESS.md`
priority-1..3 files. **This section is descriptive of the F-16C cockpit, not yet a FlightBox
enforcement mechanism** — no code change was made this pass; it is the reference for a future audit of
`FBF16Pilot`/`FBAirDataSystem`/`FBNavSystem` against these instruments.

#### On the HUD (primary; usable heads-up, always in view when Scales/FPM switches are on)
| Quantity | Resolution/format | Source instrument |
|---|---|---|
| CAS/TAS/GND speed | nearest 10 kt (minor tick), 60–900 kt range | Velocity Scale (switch-selectable) |
| Barometric or radar altitude | nearest 100 ft (minor tick) / nearest 10 ft radar digital readout | Altitude Scale / boxed "R" |
| Heading (magnetic) | nearest 5° (minor tick) | Heading Scale |
| Vertical velocity | nearest 500 fpm (minor tick) | Vertical Velocity Scale (NAV mode only) |
| Bank angle | discrete marks 0/10/20/30/45° (Roll Ind.) or 15/30/45/60° (Bank Angle Ind.) | Roll/Bank Angle Indicator |
| Pitch attitude | 5° bar spacing (10° above ±60°) | Attitude Bars |
| Normal load factor (G) | nearest 0.1 G, ±9.9 range | Current G / Max G |
| AoA (indirectly) | FPM position in AoA Bracket (on-speed cue), not a numeric AoA readout on the HUD itself | AOA Bracket ("E") — numeric AoA is on the **separate analog AoA indicator**, not the HUD |
| Range to steerpoint/target | format depends on source (B/R/F/L/M-coded), 0.1 nm or 100 ft resolution | Slant Range field |
| Time/distance to steerpoint | TTG in time; distance in whole nm | HUD nav block |
| Mach | nearest 0.01 | Mach Number field |

#### On the DED / EHSI / instrument panel (secondary; requires an eyes-down glance)
| Quantity | Resolution/format | Source |
|---|---|---|
| INS-only lat/lon | DD°MM.M' / DDD°MM.M' | INS DED page |
| INS alignment status/CEP | discrete 99→10 scale (see navigation-ils.md) | INS DED page |
| System-navigation accuracy | 3-state: HIGH(<300ft)/MED(<6000ft)/LOW(>6000ft) — **not a numeric error value** | NAV STATUS DED page |
| GPS accuracy | 3-state: HIGH/LOW/NO TRK — **not a numeric error value** | NAV STATUS DED page |
| Bearing/range to TACAN/steerpoint | analog needle + 4-digit range (0.1 nm resolution) | EHSI |
| Course deviation | analog CDI needle, saturates at full-scale deflection (angle not numerically displayed) | EHSI/ADI |
| Engine RPM/FTIT/oil/hydraulic pressure | analog gauge, no HUD repeater | Instrument panel gauges |
| Fuel quantity | nearest 100 lb increments on gauges; totalizer | Fuel gauges / BNGO DED page |

#### Architectural implication for FlightBox
- **A pilot module must not read ground-truth doubles it has no in-sim instrument for.** E.g. exact
  INS position error in feet, or a numeric glideslope-deviation angle, are **never directly displayed**
  — only discretized/needle-form. If `FBF16Pilot` or `FBAirDataSystem` ever expose a "system nav
  accuracy" or "GS deviation" quantity to guidance logic, the HONEST simulation of the F-16 either (a)
  uses the raw JSBSim/FDM truth internally (acceptable — FlightBox's pilot AI is allowed omniscient
  internal state since there's no separate "instrument" abstraction layer yet, per CLAUDE.md's current
  architecture) or (b) should eventually gate what's exposed to a HUD-only "instrument pilot" mode
  behind the same discretization real avionics impose, if FlightBox ever adds a fidelity mode that
  restricts the AI to instrument-only knowledge. This table is the prep work for that gate, not the
  gate itself.
- AoA is the clearest example already load-bearing for us: our `FBF16Pilot` approach logic targets an
  **AoA value** (11° on-speed doctrine), but the real HUD gives the pilot **only the bracket cue**, not
  a numeric AoA — the analog AoA indicator (separate gauge) is the only numeric AoA source. Our current
  implementation using the numeric AoA is a deliberate simplification (documented here, not silently
  assumed), matching a full-instrument-panel pilot rather than a HUD-only one.

## State

**This file is the closest match between reference and implementation in the whole set** — the F-16 HUD
is built against it element by element. Since this round it is drawn in the WINDSCREEN (the grid's top
two rows), not inside a drawn combiner aperture: the owner's ruling is that the aiming surface IS the
upper window and the symbology fills it.

### The cut, element by element (owner's rule: *does the pilot need this while looking through the
glass, to AIM or to NAVIGATE?*)

| Element | Verdict | Why / where it went |
|---|---|---|
| Flight path marker (FPM) | **stays** | it IS the aiming point |
| Pitch ladder (±45°, 5° rungs) | **stays** | the aiming reference against the world — and it carries BANK conformally, which is why the bank scale could go |
| Horizon line | **stays** | the ladder's 0° rung; attitude reference |
| Heading band (top edge) | **stays** | one of the three bands the owner named; navigation |
| CAS band (left edge) | **stays** | named band; the number one flies an attack or an approach on |
| Altitude band (right edge) | **stays** | named band |
| Steerpoint diamond + tadpole | **stays** | the navigation task itself |
| One steering line `STPT nn dd.d` | **stays** (was three lines) | which waypoint and how far — the minimum a diamond needs in words |
| **Bank scale** (arc below the FPM) | **GONE** → `SYS` page (`PIT/BNK`) | attitude is STATE, and the pitch ladder already shows bank where the eyes already are |
| **Bullseye bearing/range** (left block) | **GONE** → `HSD` page (`BULL bbb/rr`) | a position reference for the RADIO, read once, not an aiming cue — and the HSD is the picture it belongs in |
| **Time-to-go** (right block) | **GONE** → `HSD` page (`TTGmmm:ss`) | planning, not aiming |
| **Slant range + provider letter** (right block) | **GONE** → `HSD` page (`Bddd.d`) | same number as the steerpoint distance in a second unit; one of them is enough heads-up |
| G, peak G, Mach, ARM/SIM, radar altitude, ALOW | gone LAST round → `SYS`/`SMS` | `cockpit-displays.md` |
| Target symbology, weapon release cue | **BUILT this round, and the DRAWING is FlightBox's own** | the owner cut the HUD to target acquisition ALONE, so a HUD without it would have been empty. Every NUMBER is a published block field; every SHAPE is invented — declared below |

Nothing was deleted: every removed number is on a published block and is drawn on an MFD page.

### The projector was wrong, and the fix is what made the window fill

The HUD's conformal projector used a **80°** vertical field of view while the scene renders at **60°**
— `Kc = 429` px per unit tangent against the world's `623.5`. The symbology was therefore not
conformal at all: it shrank toward the boresight by a factor 1.45, which is a large part of why it
"stuck in the middle". Both now read ONE constant (`core/FBCamera.h`'s `kSceneVerticalFovDeg`).

**Measured** (`gpu_native`, the HUD horizon's ink row against the projection, 1280×720, `ViewH = 480`):

| Camera pitch | HUD horizon, measured | conformal (`Kc = 623.5`) | the old projector (`Kc = 429`) |
|---:|---:|---:|---:|
| +10° | 365.1 | 363.9 | 325.3 |
| 0° | 254.5 | 253.5 | 249.3 |
| −10° | 145.1 | 144.0 | 173.9 |

Residual ≤ 1.2 px (the ink centroid of a 3 px stroke) against the conformal prediction, up to 40 px
against the old one.

| Item of this reference | FlightBox | Where |
|---|---|---|
| FPM, conformal pitch ladder, horizon line, bank scale, waterline | **built** | [`module.md`](module.md) §12 |
| Heading / CAS / altitude tapes | **built** | same |
| G load, Mach, peak G, master-mode text, ARM/SIM, R (radar altitude), AL (ALOW) | **REMOVED from the HUD this round and moved to the MFD bank** ([`cockpit-displays.md`](cockpit-displays.md)) on the owner's rule *"HUD hat nur zielerfassung/wegpunkte und alles andere im heads down"*: each of them is aircraft STATE, and a HUD that carries state makes the pilot read at the wrong moment. Nothing was deleted — every one of them is on a published block and is drawn on the SYS or SMS page | this round |
| Steerpoint diamond (crossed out beyond the real F-16C TFOV/2) + Tadpole / Great Circle Steering Cue | **built** | same |
| Right status block: 'B' slant range → TTG → distance to steerpoint | **cut to ONE line this round**: `STPT nn dd.d`, bottom left, out of the aiming zone. Slant range and TTG are on the HSD page | same |
| Left status block: the bullseye bearing/range | **gone from the HUD this round** → HSD page | same |
| Bank scale | **gone from the HUD this round** → SYS page (`PIT/BNK`) | same |
| **The aiming cue at release** (DLZ scale, CCIP pipper, TD box) | **BUILT — and the invention is declared, not hidden.** The owner's instruction *"im hud sollte nurnoch zielerfassung sein"* removed everything else from the HUD, which made the previous ruling untenable: a HUD cut to target acquisition with no target acquisition in it is not a cut, it is a blank glass. The NUMBERS were never the gap — `DlzValid`/`InZone`/`RaeroM`/`RtrM`/`RminM`/`TargetRangeM`, `GunLeadAzDeg`/`ElDeg`/`GunSpanMr`/`GunFunnelTopMr`/`BottomMr`/`GunInRange`/`GunInFunnel`, `AgRangeM`/`AgImpactElevM`/`AgTimeToReleaseS`/`AgInRange` and `FBRadarContact::BearingDeg`/`ElevAngleDeg` are all published. **The GEOMETRY is the gap and remains one**: shapes, sizes, spacings and placements are FlightBox's own choice, matched to MIL-STD-1787 *conventions* rather than to any figure in the source set, because no source in `doc/modules/f16/` gives coordinates for them. A reader must not cite these proportions as documented. **Superseded in part since the BMS Dash-34 addendum: the SIZES are now documented** (CCIP pipper = 1 mR dot in a 12 mR circle, in-range cue 16 mR, DLZ staple composition, ASEC 11–56 mR, ASC 8 mR — §A3/§A6/§A7). What is still undocumented is *placement in HUD coordinates*, because the manual's HUD-window map is not published. The invented shapes should be replaced by the documented ones in a later round; until then this row stands as a declared deviation. | this round |
| The combiner aperture itself (~25° TFOV, aspect-correct), conformal symbology scissored at the window edge | **REPLACED this round by the windscreen** — the drawn window is the grid's top two rows inset by 10 px, bands at its edges, conformal elements clipped to it, the diamond clamp coincident with the window edge. The ~25° aperture is no longer drawn anywhere; it survives only as the reference this file documents | [`../flightbox/render/hud.md`](../../render/hud.md) |
| Bitmap font + MAX7456 look | **built** as two separate things: a generic coverage-antialiased font system in `render/`, and an F-16-specific chip hook (`FBF16Max7456`) that is a real, instantiated NoOp | [`module.md`](module.md) §11 |
| ILS symbology (localizer/glideslope bars, command steering) | **not implemented** — there is no ILS receiver. The drawing spec now exists (§A8, `[BMS-1 pp.166–167]`) | — |
| A-A / A-G weapon symbology beyond the release cue (TD box, locked-target symbol, DLZ scale, EEGS funnel drawing) | **BUILT this round, geometry invented (see the row above).** The locked track gets a boxed TD symbol with corner ticks, an unlocked contact four corners only — a deliberate visual distinction so "seen" never reads as "acquired". **The anti-cheat rule is untouched**: a contact carries no identity, so no affiliation is drawn; the ONLY identity mark is the IFF Mode 4 reply, and "no reply" stays unlabelled | [`../flightbox/sim/sensors.md`](../../sensors.md) Gaps 11 |
| Pull-up / breakX cues | **not implemented**. The spec now exists in full — GAAF break-X trigger logic, sensor hierarchy, 4.0 g recovery model, buffer/pad schedule, PUAC staple at max 4° below the FPM (§A5, §A7) | — |

**"Was der Pilot wirklich sieht" is the acceptance list:** that section is the instrumentation ground
truth a pilot module is validated against — which quantities are actually readable heads-up, at what
resolution, and which require an eyes-down glance (which FlightBox spends as command-bus latency).

## Gaps

**Source gaps** (this file vs. its sources)
- ~~The source set documents **no TD box and no locked-target symbol**; the radar-adjacent entry is the
  HMCS, a different function. This is a genuine source gap, not an implementation choice — anything
  drawn there would be invented.~~ **CLOSED** by the BMS Dash-34 addendum §A3/§A6:
  A-A primary TD box **25 mR square**, secondary **15 mR**, TGP A-A box **25 mR dotted**, ACM/RWR boxes
  **dotted** `[BMS-34 p.109]`; A-G TD box **10 mR square with a 1 mR centre pipper** `[BMS-34 p.427]`;
  the locked-target marks are the **missile diamond** (6 mR, 18 mR uncaged, "hairy" +4 tics for AIM-120)
  `[BMS-34 pp.613, 628]` and the **EEGS TD circle that unwinds CCW for range inside 12,000 ft**
  `[BMS-34 p.597]`.
- ~~no DLZ drawing~~ **CLOSED**: SRM DLZ staple RMAX1/RMAX2/RMIN2/RMIN1 with its scaling rule
  `[BMS-34 pp.615–616]`, AIM-120 DLZ stack R_AERO `◁` / R_OPT `○` / R_PI / R_TR / R_MIN with the
  125 %-R_AERO expansion rule `[BMS-34 pp.628–633]`, ASEC 11→56 mR (131 mR bore) `[BMS-34 p.634]`,
  ASC 8 mR `[BMS-34 p.634]`, IAM DLZ with the 70 % normalization `[BMS-34 p.542]`, HARM HLS
  `[BMS-34 p.489]`, CCRP/LOFT release-angle scale `[BMS-34 pp.432–433]`.
- ~~EEGS funnel drawing~~ **CLOSED** in kind (funnel width = entered target wingspan, MRGS lines, FEDS
  dot pairs, 1G `+` / Max-G `−` (7.3 g) pippers, 4 mil predicted-lead circle, 6 mil BATR, ¼ s settling)
  `[BMS-34 pp.596–600]` — **but the funnel's own outline equation is still not published**; only its
  width criterion is.
- ~~Pull-up / break-X cue undocumented~~ **CLOSED**: GAAF flashing **break-X** on MFDs ~2 s early and on
  the HUD at the advisory altitude with "PULL UP - PULL UP", plus the **PUAC staple, max 4° below the
  FPM**, and the **5 Hz flashing large X** `[BMS-34 pp.119–122, 434]`.
- ~~ILS symbology undocumented beyond Chuck's screenshots~~ **CLOSED**: roll-stabilized deviation bars
  with one-dot/two-dot tics, dashed = invalid, flight-director circle + top tic + heading-scale
  reference caret, X-over-tic when pitch steering invalid `[BMS-1 pp.166–167]`.
- **Still open, and now precisely named** (see §A9): the manual's **numbered HUD windows** are referenced
  (`window 30`, `window #3`) but **the window map is never published** — so no source in this tree gives
  HUD *coordinates*, only sizes and region-level placement. Likewise **no TFOV number** in BMS, **no
  stroke width / font metrics**, **no colour spec**.
- Chuck Parts 3/6/8/16 and ED pp.89–96 + 225–226 are fully processed. BMS-34 pp.96–127, 217–222, 299,
  410–438, 489, 542–543, 593–636 and BMS-1 pp.56, 115, 166–168 are processed for HUD content.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the aiming and navigation set — FPM, ladder, horizon, the three bands, diamond/tadpole —
  conformal at the scene's own pixels-per-radian, filling the windscreen.
- *Partially:* the steering readout — one line where this reference documents a whole block.
- *Not at all:* ILS symbology, weapon-specific symbology (TD box, DLZ, EEGS drawing), pull-up/breakX
  cues, HUD declutter modes, MAN RNG/UNCAGE behaviour, HMCS.
- **Named this round:** the drawn window is no longer the documented ~25° combiner aperture but the
  whole windscreen. That is the owner's ruling, not a reading of this reference, and it IS a deviation
  from the source — a real F-16 pilot does not see the pitch ladder out to the canopy rails. Recorded
  as a deviation rather than reasoned away.
- **Also named:** with the bank scale gone the HUD carries no attitude READOUT besides the ladder, and
  the AOA "E" bracket this file documents for the landing configuration is still not drawn. Neither is
  new this round; both are the same standing gap, now load-bearing for a HUD-only pilot.

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

The standard our HUD is built against is **MIL-STD-1787** (*Aircraft Display Symbology*, adopted
1984-12-10; current rev D:2018). Sources cited inline. This section gives the symbology *conventions* and
geometry a faithful HUD must obey — beyond the guide's element list.

### MIL-STD-1787 conventions
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

### F-16 HUD geometry (the four reference points)
MIL-STD-1787 defines four HUD reference points; the **left-hand and right-hand reference points are
specific to the F-16 HUD design** (DTIC ADA430578; MIL-STD-1787):
1. Center of the **TFOV** (Total Field Of View),
2. the **aircraft reference point** (boresight / gun cross),
3. **left-hand reference point**, 4. **right-hand reference point**.
- **TFOV vs IFOV**: the FPM/pitch symbology is drawn in the **total** field of view; the instantaneous FOV
  (what one eye sees through the combiner) is smaller, so symbology near the edges may clip — a faithful
  HUD must clamp/ghost symbols (steerpoint diamond "crossed-out" when out of FoV is exactly this).
- **Resolved** (was previously an open confidence gap): ED EA Guide p.89 gives the DCS F-16C's official
  HUD FOV as **25° diameter, extending to 10.5° below field-of-view center** — see the "ED EA Guide
  addendum" section above. This is now high-confidence for our rebuild target (it's the number the
  reference implementation we're matching actually uses), though whether it matches the *real* F-16C's
  exact combiner geometry is a separate, still-open question (DTIC ADA430578 doesn't give the number
  this precisely).

### Implications for our MIL-STD-1787 HUD
- FPM is compressed/caged when wind drift would push it off-HUD — matches the DRIFT C/O switch
  (`cockpit-displays.md`): DRIFT C/O keeps the FPM centered regardless of wind.
- Airspeed **left**, altitude **right**, heading tape **top**, FPM/pitch ladder centered — the 1787/F-16
  layout the guide's landing screenshots confirm.
- Steering/ILS bars (localizer vertical, glideslope horizontal) form a cross on the FPM — a flight-director
  presentation per 1787 landing symbology; deflection scaling in `navigation-ils.md`.

### Hardware (LRUs, for context)
- **HUD**: F-16C uses a **wide-angle raster/stroke HUD**; the **Pilot Display Unit (PDU)** combiner +
  optics. The **HUD Electronics Unit** was a separate LRU on early jets but is **absorbed into the MMC**
  (Modular Mission Computer) on Block 50+ (airforce-technology; see `cockpit-displays.md`).
- **Raster** capability lets the HUD overlay FLIR video (the guide's FLIR polarity/gain/contrast/brightness
  wheels drive this); **stroke** draws the symbology. Day/Auto/Night brightness per the guide switch.
- Symbology is generated by the **MMC** from sensor/mission data and drawn on the PDU.

### Sources
- MIL-STD-1787A/D *Aircraft Display Symbology* (globalsecurity.org; man.fas.org mirror) — ARS, pitch
  ladder, reference points.
- DTIC ADA430578 *New Flight Display Formats* — ARS/FPM/climb-dive set, F-16 left/right reference points, TFOV.
- airforce-technology.com F-16 — MMC absorbs the HUD Electronics Unit.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Head-Up Display chapter p.89–96
  (full element list, TFOV 25°/10.5°, all scale major/minor ticks, slant-range letter codes, Master
  Mode Status text tags, HUD Control Panel switch behavior); Navigation by Steerpoints p.225–226
  (Great Circle Steering Cue mechanism, HSD Azimuth Steering Line).
