# F-16C Cockpit Displays & Controls

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 3 — Cockpit & Equipment, pp. 13–83.
HUD covered separately in `hud-symbology.md`; EHSI/HSD detail in `navigation-ils.md`; engine gauges in
`engine-fuel.md`; AOA indexer in `hud-symbology.md`.

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
