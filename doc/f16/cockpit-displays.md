# F-16C Cockpit Displays & Controls

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 3 — Cockpit & Equipment, pp. 13–83.
HUD covered separately in `hud-symbology.md`; EHSI/HSD detail in `navigation-ils.md`; engine gauges in
`engine-fuel.md`; AOA indexer in `hud-symbology.md`.

Plus `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — **Cockpit Overview /
Instrument Panel** chapter, p.43–81 (**partial pass this round**: Instrument Panel analog gauges +
Caution Light Panel fully extracted below; Left/Right Auxiliary Console, Left/Right Console, and the
**Upfront Controls (UFC/DED) p.97–120 and Multi-Function Displays (MFD) p.121–127 chapters were NOT
processed this pass** — remaining gap, see `PROGRESS.md`). Cite tags `Chuck p.NN` vs `ED EA Guide p.NN`.

## ICP / UFC (Integrated Control Panel = Upfront Control)

The ICP drives the DED. Keypad + priority-function buttons, master-mode buttons, override buttons.

### Key controls
- **A-A / A-G Master Mode** buttons (also NAV = neither selected).
- **COM1 / COM2 / IFF / LIST** override buttons → their DED pages.
- **DCS (Data Control Switch / "Dobber")**: UP/DOWN, **RTN (left)** = return to CNI, **SEQ (right)** =
  sequence fields. Moves the asterisk on DED pages, toggles wind on CNI.
- **DED Increment/Decrement** switch: changes the selected field's value (fields with up/down arrows).
- **RCL (Recall)**: backspace one digit; press again restores original value.
- **ENTR**: commits typed value.
- **HUD Symbology Intensity** wheel (turns HUD on), Reticle Depression wheel (backup bombing), FLIR
  polarity/gain/level, HUD raster contrast/brightness wheels.
- **Drift Cutout/Warning Reset** switch: **DRIFT C/O** (cages FPM to HUD center regardless of wind) /
  **NORM** (used before landing) / **WARN RESET** (clears HUD WARN message).

## DED pages

Priority functions = ICP keypad buttons (button N → page):
| Btn | Page |
|---|---|
| 1 | T-ILS (TACAN & ILS) |
| 2 | ALOW (altitude-low) |
| 3 | (see LIST) |
| 4 | STPT (steerpoint) |
| 5 | CRUS (cruise: TOS/RNG/HOME/EDR) |
| 6 | TIME (HACK timer, DELTA TOS/ROLEX) |
| 7 | MARK (markpoints) |
| 8 | FIX (navigation fix) |
| 9 | ACAL (altitude calibration) |
| 0 | M-SEL |

**Override pages** (COM1/COM2/IFF/LIST buttons): CNI, COM1 (UHF), COM2 (VHF), IFF, **LIST**.

**LIST sub-pages**: DEST, BNGO, VIP, INTG, NAV, MAN (EEGS funnel width), INS, DLNK, CMDS, **MISC** (0).
**MISC sub-pages**: CORR, MAGV, OFP, INSM, LASR (laser codes), BULL (bullseye), HMCS, GPS, DRNG (n/s),
HTS (if HTS pod), HARM (A-G mode), VRP.
- **MODE** page = alternate master-mode change without the A-A/A-G buttons.

## MFDs (Multifunction Displays, left + right)

- 20 **OSBs** (Option Select Buttons) ring each MFD; gain/brightness/contrast/symbology rocker switches.
- 3 lower OSBs = **Direct Access (DA)** buttons: up to 3 saved pages per master mode. Cycle via HOTAS:
  **DMS LEFT** = left MFD, **DMS RIGHT** = right MFD.
- Access a page: press a DA OSB, then pick the page from the Main Menu.
- **Same page cannot display on both MFDs simultaneously** — selecting it on one removes it from the other.
- Available pages: FCR, SMS/WPN, HSD, TGP, FLCS, ENG, TEST, DTE, etc.

## Primary flight instruments (analog standby)
- **ADI** (Attitude Director Indicator): attitude + ILS localizer/glideslope steering bars.
- **SAI** (Standby Attitude Indicator): uncage during setup (startup step 49).
- Standby magnetic compass, altimeter (baro knob), airspeed, VVI, AoA indicator (deg), AoA indexer.
- Cabin pressure altitude indicator (×1000 ft), clock.

## Warning / caution / status
- **Master Caution** light (push to reset); eyebrow warning lights (ENGINE, HYD/OIL PRESS, FIRE, etc.).
- **TF FAIL** (terrain-following radar fail), **F-ACK** (fault acknowledge — clears PFL faults),
  **IFF IDENT** button/light.
- **Caution Advisory** panel (many lights); **PFLD** (Pilot Fault List Display).
- Landing gear indicator lights (down-and-locked / transition / up-and-locked).
- **RDY** light (e.g. refuel door open), fuel flow/quantity indicators (×100 lb), EPU fuel %, oxygen
  pressure/flow.

## HUD-related switches (panel) — full list in `hud-symbology.md`
Depressible Reticle, DED Data (DED/PFL/OFF), HUD Scales (VV/VAH · VAH · OFF), FPM (ATT/FPM · FPM · OFF),
Velocity (CAS/TAS/GND), Altitude (RADAR/BARO/AUTO), Brightness (Day/Auto/Night), RADALT power (ON/STBY/OFF).

---

## ED EA Guide addendum — analog instrument panel detail (official, p.43–81, partial pass)

The instrument-panel analog gauges are the F-16C's **battle-damage/avionics-independent backup path**
(EHSI/SAI/altimeter/ASI/AoA-indicator/VVI/ADI all work without the MMC — `flight-controls-flcs.md`'s
DBU concept extended to displays). ED gives exact scale numbers Chuck's guide doesn't quantify.

### AoA Indicator (center instrument panel) — precise band edges (ED p.83, refines the AOA-indexer
table in `hud-symbology.md`)
- **Scale**: **−5° to +32°**, major tick = 5°, minor tick = 1°.
- **Low AoA region**: 8.5°–11° ("energy gaining, less than optimal").
- **Optimal AoA region**: **11.1°–13.9°** ("on-speed").
- **High AoA region**: 14°–16.5° ("energy depleting, greater than optimal").

**This refines `hud-symbology.md`'s AOA Indexer table** (which gave the coarser bands "Top red >14° /
Center green 11–14° / Bottom yellow <11°", sourced from Chuck): ED's exact edges are **11.1°/13.9°**,
not a flat 11°/14° — a small but real precision gain for a pilot AI's AoA-hold deadband. The Indexer
(glareshield 3-light version) duplicates this exactly and is **always powered** regardless of gear
position (ED explicit — Chuck's guide implies but doesn't state the indexer is gear-independent).
ED also states explicitly for landing: **"the pilot should maintain between 11° and 13° AoA"** — this
is the same 11–13° range already folded into `procedures-landing.md`'s ED addendum, now cross-confirmed
from the instrument-panel chapter independently of the procedures chapter.

### Vertical Velocity Indicator (VVI) — analog backup to the HUD's VV scale
- Scale: **±6,000 fpm**, major tick = 500 fpm, minor tick = 100 fpm (finer resolution than the HUD's
  own Vertical Velocity Scale, which uses 1000/500 fpm ticks per `hud-symbology.md`'s ED addendum —
  the analog backup instrument is actually the more precise of the two).

### Attitude Director Indicator (ADI) — full instrument, backup to HUD attitude + ILS steering
- **Attitude sphere**: light-blue (climb/sky) vs dark-brown (dive/ground) hemispheres, rotates around a
  fixed aircraft-symbol waterline.
- **Pitch scale**: major tick 10°, minor tick 5°.
- **Bank angle** (two redundant indicators — upper index + lower scale): lower-scale major ticks at
  **30°/60°/90°**, minor ticks at **10°/20°**.
- **Slip indicator (ball)**: centered = coordinated turn (matches `flight-controls-flcs.md`'s ARI
  coordination goal); slides same direction as bank = slipping turn; opposite = skidding turn.
- **Rate-of-turn indicator**: 1 bar-width ≈ **1–1.2°/s**; aligned with outer bar = **standard-rate 3°/s
  turn**; halfway = half-standard-rate. A concrete, instrument-derived turn-rate reference distinct from
  the FLCS's own roll-rate command (`flight-controls-flcs.md`'s Ps command) — useful if a FlightBox
  autopilot ever wants a "standard rate turn" mode (3°/s, a common IFR/procedure-turn convention).
  **T3 general-aviation convention, not itself F-16-specific, but explicitly the F-16 ADI's calibration
  reference per ED.**
- **Glideslope Deviation Scale**: **2.5°/dot**, ±5° top/bottom — identical number already flagged as a
  discrepancy against the generic-ILS-standard figure in `navigation-ils.md`'s ED addendum (not
  repeated in full here, cross-reference only).
- **OFF flag** = no INS attitude data (INS off/failed); **AUX flag** = degraded INS attitude data
  (INS malfunction or failed to reach even coarse alignment) — two distinct failure-severity flags, a
  useful state-machine detail for an ADI-fidelity implementation.
- **Pitch trim knob**: ±0.5° per click, purely a display-zero adjustment (does not affect the real
  aircraft attitude, only where "zero" is drawn on the sphere relative to the waterline).

### Caution Light Panel — full trigger-condition list (ED p.58–59, official; Chuck's guide only lists
"many lights" without conditions)
| Light | Trigger condition |
|---|---|
| FLCS FAULT | Dual FLCC electronics malfunction, LEF locked, or FLCS BIT failed |
| ENGINE FAULT | Engine-related fault (clears on acknowledge) |
| AVIONICS FAULT | Avionics fault, or MUX bus lost comms with engine/FLCC |
| SEAT NOT ARMED | Ejection-seat arming lever up (disarmed) |
| ELEC SYS | Electrical fault (see ELEC Control Panel for which) |
| SEC | Engine in Secondary control mode |
| EQUIP HOT | Avionics-bay cooling insufficient — **auto-cuts FCR power** |
| NWS FAIL | Nosewheel-steering system failure |
| PROBE HEAT | Reduced airflow to pitot/air-data/AoA probes (icing) or heater/monitor failure |
| FUEL/OIL HOT | Engine fuel or oil overtemp |
| RADAR ALT | Radar altimeter malfunction |
| ANTI SKID | Anti-Skid switch OFF, **or** braking-system fault while ground speed **>5 kt** |
| CADC | Central Air Data Computer malfunction |
| INLET ICING | Engine inlet ice detected, or ice detector failed |
| IFF | Mode-4 interrogation received but reply inhibited (RF switch / MODE 4 REPLY switch / zeroized) |
| HOOK | Emergency arresting hook not up/locked |
| STORES CONFIG | STORES CONFIG (CAT I/III) switch mismatched to loadout — `flight-controls-flcs.md` |
| OVERHEAT | Engine bay, main-gear wheel wells, ECS bay, or EPU bay overheat |
| OBOGS | ECS air pressure **<10 PSI** |
| CABIN PRESS | Cockpit pressure altitude **>27,000 ft** |
| FWD FUEL LOW | Forward reservoir **<400 lb** |
| AFT FUEL LOW | Aft reservoir **<250 lb** |
| NUCLEAR / ATF NOT ENGAGED / EEC / BUC | Not implemented / no function in DCS |

This is directly useful as a FlightBox **flight-monitor cross-check list**: several of these conditions
(ANTI SKID groundspeed threshold, CABIN PRESS altitude threshold, fuel-low thresholds) are quantified
physical triggers a `FBFlightMonitor`-style system could reproduce if FlightBox ever adds a
caution/warning-light simulation layer — currently out of scope (CLAUDE.md's `FBFlightMonitor` is a
K.O./crash judge, not a caution-light simulator), noted here as a ready-made trigger table if that scope
ever expands.

### Other gauges (quantified)
- **HYD PRESS A/B**: 0–4,000 psi gauge, 500 psi increments; **normal 2,850–3,250 psi** (matches
  `engine-fuel.md`'s and `procedures-startup.md`'s existing hydraulic-pressure figures — cross-validated).
- **EPU Fuel Quantity**: percentage, 5% increments; **at 100%, EPU runs ~10–15 minutes** — a concrete
  endurance figure for the EPU (`engine-fuel.md`'s hydrazine emergency-power unit) not previously
  quantified in this doc set.
- **Cabin Pressure gauge**: pressure altitude, 0–50,000 ft, 1,000 ft increments (feeds the CABIN PRESS
  caution light above).
- **Mechanical clock**: 8-day wind-up backup timepiece — no functional relevance to FlightBox beyond
  completeness.

### Remaining gap this pass
Left/Right Auxiliary Console (Landing Gear Panel, CMDS Control Panel, HMCS Control Panel, Magnetic
Compass, Fuel Quantity Indicator, PFLD), Left/Right Console (FLT CONTROL panel, EPU Control Panel, ELEC
Control Panel, ENG CONT switch, MANUAL PITCH Override switch — cross-referenced already from
`flight-controls-flcs.md`/`procedures-startup.md` but not independently re-extracted from this chapter),
and critically the **Upfront Controls (UFC/ICP/DED) p.97–120** and **Multi-Function Displays (MFD)
p.121–127** chapters were **not processed this pass** — these are the biggest remaining gap for a
`cockpit-displays.md` full-depth pass (DED page field-by-field detail, MFD OSB/format-menu logic).
Flagged honestly in `PROGRESS.md`, not silently left out.

---

# Technical depth (researched — for rebuild)

Avionics computer/display architecture (LRUs) behind the panel. Sources cited inline.

## Central computer — MMC (Modular Mission Computer)
- The **MMC** is the F-16C's central mission computer. On Block 50+ it **replaces three earlier LRUs**:
  the **Expanded Fire Control Computer (XFCC)**, the **HUD Electronics Unit**, and the Stores Management
  System's **Expanded Central Interface Unit (XCIU)** (airforce-technology.com).
- The MMC generates HUD symbology, drives the MFDs, runs fire-control/SMS, and hosts the master-mode logic
  (NAV/A-A/A-G). Retrofit MMC is **~42% less volume, 55% less weight, 37% less power** than the boxes it
  replaced — indicates the functional consolidation a sim's "avionics" module should mirror (one computer,
  many display surfaces).

## Display LRUs
- **UFC/ICP + DED**: the Up-Front Controls (ICP keypad) drive the **Data Entry Display** (DED) — a small
  monochrome alphanumeric display for data entry/readout, separate from the MFDs.
- **MFDs**: two multifunction displays, each with 20 OSBs, fed by the MMC over a data bus (MIL-STD-1553).
  Same page cannot show on both simultaneously (guide) — a display-manager constraint.
- **EHSI**: electromechanical/electronic HSI for nav (steerpoint/TACAN/ILS) — see `navigation-ils.md`.
- **Standby instruments**: SAI (standby attitude), altimeter, ASI, magnetic compass — independent of the
  MMC for battle-damage reversion (the reason the guide stresses the EHSI as an FCC-independent backup).
- **Data bus**: avionics interconnect is **MIL-STD-1553** multiplex — the design rationale for the
  master-mode/SOI (Sensor of Interest) arbitration and why sensors/displays are loosely coupled LRUs.

## Sources
- airforce-technology.com F-16 — MMC replaces XFCC + HUD EU + XCIU; volume/weight/power figures.
- Wikipedia *AN/APG-68*; general F-16 avionics references — 1553 bus, LRU display architecture.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Cockpit Overview/Instrument Panel
  p.43–81 (**partial**: AoA Indicator p.83, VVI p.85, ADI p.87–88, Caution Light Panel p.58–59, misc
  gauges — extracted; Aux Consoles/Left-Right Console/UFC p.97–120/MFD p.121–127 NOT processed this
  pass, see `PROGRESS.md`).
