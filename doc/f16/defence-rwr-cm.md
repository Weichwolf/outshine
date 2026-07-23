# F-16C Defence — RWR & Countermeasures

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 12 — Defence: RWR & Countermeasures, pp. 574–599.

## RWR — AN/ALR-56M Radar Warning Receiver

Threat Warning Azimuth (TWA) indicator; symbols also on the HMD. Powered via the **RWR power button**
(runs a BIT). A status-change tone sounds when a new emitter symbol appears.

### Symbol state = radar mode
| Symbol decoration | Meaning | Tone |
|---|---|---|
| No circle | Radar in acquisition/search | New-threat tone |
| **Square** around symbol | Tracking / locked on you | Radar-lock tone |
| **Flashing circle** around symbol | Radar supporting a missile launched at you | Launch warning |
| **Diamond** | Highest threat level | — |
| "U" / "UKN" | Unknown emitter | — |

Position on the scope = relative bearing (own nose = top); distance from center ≈ threat proximity.

### TWP (Threat Warning Prime) panel
- **HANDOFF** button — mode select (not simulated): the four modes cycle which threats are prioritized.
- **T (Target Separation)** button — separates overlapping symbols; highest-priority symbol stays put.
- MISSILE LAUNCH light, UNKNOWN SHIP toggle, System Test, Mode Selector.
- RWR display modes: **PRIORITY** (5 highest threats) / **OPEN** (16 highest) — see Part 3.

### TWA auxiliary panel
- **RWR source** switch: enables RWR data for CMDS SEMI/AUTO dispensing.
- **Jammer source** switch: enables jammer data for CMDS SEMI/AUTO.
- **SEARCH** button: shows 'S' search-radar symbols (flashing S = acquisition radars filtered out; steady
  S = displayed).
- **LOW ALTITUDE** button, ACT/PWR indicator.

## CMDS — AN/ALE-47 Countermeasures Dispenser System

- **Chaff**: passive radar-reflective (defeats radar-tracking missiles).
- **Flares**: heat decoys (defeats IR missiles).
- FCDs in the body fairing; ground crew sets loadout, **max 120 combined** (typical 60 chaff / 60 flare).

### Programs
- 6 programs total. **PRGM knob selects 1–4**. **PRG 5** = always the slap switch (left sidewall).
  **PRG 6** = Bypass program. Programs 1–5 are programmable via DTC or UFC **only when CMDS mode = STBY**.

### CMDS mode knob
| Mode | Behavior |
|---|---|
| OFF | — |
| **STANDBY** | Reprogram via UFC (only mode that allows reprogramming); cannot dispense |
| **MAN** | Selected manual program dispensed by **CMS forward** |
| **SEMI** | System picks program by threat; **consent via CMS aft** |
| **AUTO** | System picks + dispenses automatically; enable **CMS aft**, disable **CMS right** |
| **BYP** (Bypass) | Manual 1 chaff + 1 flare when failures block other modes |

### CMS switch (stick) summary
FWD = dispense selected manual program · AFT = consent/enable SEMI/AUTO · RIGHT = disable AUTO ·
Slap button = PRG 5.

### Other controls
- Chaff/Flare **slap button** = PRG 5 (immediate third program without turning PRGM knob).
- **Jettison** switch (JETT/UP): dumps countermeasures even with CMDS OFF.
- CH/FL counters on display; **LO** = low quantity. VMS "LOW"/"OUT" (see `aerodynamics-performance.md`).
- MWS (Missile Warning System): **not functional on Block 50**.

### Usage tutorial (essentials)
1. RWR power button (BIT) — required for SEMI/AUTO. 2. CMDS RWR/JMR switches ON. 3. CH/FL switches ON.
4. Set mode knob (MAN/SEMI/AUTO). 5. Select program. Dispense per CMS logic above.

---

# Technical depth (researched — shallow pass — deepen when in scope)

> EW/defence is outside the current rebuild scope (flight + rendering). LRU/principle stub only.

## Components (LRUs)
- **RWR**: **AN/ALR-56M** Radar Warning Receiver (guide) — wideband superhet receiver, quadrant spiral
  antennas.
- **CMDS**: **AN/ALE-47** Countermeasures Dispenser System (guide) — threat-adaptive chaff/flare dispenser.
- **ECM**: internal **AN/ALQ-165 ASPJ** or podded **AN/ALQ-131 / AN/ALQ-184** jammer.
- **EWMS**: **AN/ALQ-213** Electronic Warfare Management System integrates RWR + jammer + CMDS.

## Functional principle
The ALR-56M measures each intercepted radar pulse's frequency, PRI, and scan, matches it against a threat
library to classify the emitter, and places a symbol by azimuth/priority (decorated by lock/launch state).
In SEMI/AUTO the ALE-47 uses that threat picture to pick and dispense a chaff/flare program (the EWMS
coordinates RWR → jammer → dispenser). Chaff blooms a radar-reflective cloud to seduce radar-guided
missiles; flares present a hotter IR source than the tailpipe to decoy heat-seekers. All boxes are LRUs on
the avionics bus, consented via the stick CMS switch.

## Sources
- Wikipedia *AN/ALR-56*, *AN/ALE-47*, *AN/ALQ-213*; airforce-technology.com F-16 — EW LRU designations.
- DCS guide Part 12 (RWR symbology, CMDS modes) — cross-referenced above.
