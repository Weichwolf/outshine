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
is built against it element by element, inside the real combiner aperture.

| Item of this reference | FlightBox | Where |
|---|---|---|
| FPM, conformal pitch ladder, horizon line, bank scale, waterline | **built** | [`module.md`](module.md) §12 |
| Heading / CAS / altitude tapes, G load, Mach, peak G, master-mode text | **built** | same |
| Steerpoint diamond (crossed out beyond the real F-16C TFOV/2) + Tadpole / Great Circle Steering Cue | **built** | same |
| Right status block: R (radar altitude) → AL (ALOW) → 'B' slant range → TTG → distance to steerpoint | **built**; 'R' is carried as a documented FlightBox addition with no FlightGear counterpart | same |
| The combiner aperture itself (~25° TFOV, aspect-correct), conformal symbology scissored at the window edge | **built** — tapes and blocks at the aperture edges, conformal elements clipped, the diamond clamp coincident with the window edge | [`../flightbox/render/hud.md`](../../render/hud.md) |
| Bitmap font + MAX7456 look | **built** as two separate things: a generic coverage-antialiased font system in `render/`, and an F-16-specific chip hook (`FBF16Max7456`) that is a real, instantiated NoOp | [`module.md`](module.md) §11 |
| ILS symbology (localizer/glideslope bars, command steering) | **not implemented** — there is no ILS receiver | — |
| A-A / A-G weapon symbology beyond the release cue (TD box, locked-target symbol, DLZ scale, EEGS funnel drawing) | **not implemented** — and deliberately so for the lock: this file documents neither a TD box nor a locked-target symbol, so none is invented; the lock lives in the bus, telemetry and events | [`../flightbox/sim/sensors.md`](../../sensors.md) Gaps 11 |
| Pull-up / breakX cues | **not implemented** | — |

**"Was der Pilot wirklich sieht" is the acceptance list:** that section is the instrumentation ground
truth a pilot module is validated against — which quantities are actually readable heads-up, at what
resolution, and which require an eyes-down glance (which FlightBox spends as command-bus latency).

## Gaps

**Source gaps** (this file vs. its sources)
- The source set documents **no TD box and no locked-target symbol**; the radar-adjacent entry is the
  HMCS, a different function. This is a genuine source gap, not an implementation choice — anything
  drawn there would be invented.
- Chuck Parts 3/6/8/16 and ED pp.89–96 + 225–226 are fully processed.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the whole core flight/navigation symbology set, in the real aperture, with the real
  scales.
- *Partially:* the status blocks — present with the documented fields, but fed by FlightBox's own
  computation (e.g. steerpoint elevation is sampled under the aircraft, not declared).
- *Not at all:* ILS symbology, weapon-specific symbology (TD box, DLZ, EEGS drawing), pull-up/breakX
  cues, HUD declutter modes, MAN RNG/UNCAGE behaviour, HMCS.

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
