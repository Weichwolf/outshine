# F-16C HOTAS

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 9 — HOTAS, pp. 158–160.

## SSC — Side Stick Controller (stick)

| Control | Function |
|---|---|
| **Trigger** (2-stage) | 1st stage = laser/camera; 2nd stage = gun fire |
| **Weapon Release button** | Pickle (release A-G stores) |
| **Trim Hat** (U/D/L/R) | Pitch/roll trim |
| **DMS** (Display Management Switch, U/D/L/R) | Sets Sensor of Interest (SOI), display management |
| **TMS** (Target Management Switch, U/D/L/R) | Target designation / track commands |
| **CMS** (Countermeasures Switch, F/A/L/R) | Countermeasure programs |
| **Paddle switch** | Overrides autopilot while depressed |
| **Expand/FOV button** | Cycles field-of-view for the selected sensor/system |
| **NWS A/R DISC & MSL STEP** | NWS: nosewheel steering. A/R (in flight, refuel door OPEN): disconnects boom latch. MSL STEP: in EO/A-A cycles next weapon station; in A-G cycles **CCRP → CCIP → DTOS** |

## TQS — Throttle Quadrant System (throttle)

| Control | Function |
|---|---|
| **Throttle** | OFF / IDLE / MIL / AB / MAX AB (see `engine-fuel.md`) |
| **Speed Brake switch** (3-pos, aft momentary) | AFT open · FWD close · center = hold |
| **Radar Cursor/Enable switch** (depress, multidirectional) | Slew FCR cursor / TGP / weapon video |
| **Radar Antenna Elevation knob** | Rotary, center detent |
| **MAN RNG/UNCAGE knob/switch** | Rotate or depress; function depends on master mode/system |
| **Comms UHF/VHF Transmit switch** (4-way) | AFT = UHF Tx · FWD = VHF Tx · RIGHT short = filter datalink on FCR · LEFT short = toggle datalink tracks |
| **Dogfight switch** (3-pos slide) | **DOGFIGHT** (outboard): HUD symbology for gun + A-A missile · **Missile Override** (inboard): A-A missile only · Center: return to last Master Mode |

---

# Technical depth (researched — shallow pass — deepen when in scope)

## Components (LRUs)
- **Side-Stick Controller (SSC)**: a **force-transducer** grip on the right console — the original F-16
  stick was **rigid (near-zero travel)**; production sticks add small motion but still command by force.
- **Throttle Quadrant (TQS)**: left-console throttle grip with the switches above.

## Functional principle
The SSC's force sensors output a signal proportional to applied pilot force (quadruplex, one per FLCC
channel), so pitch/roll/yaw commands are **force gradients**, not deflections — this is the input side of
the FLCS (`flight-controls-flcs.md`). HOTAS switches (DMS/TMS/CMS/pinky/pickle/trigger) are discrete
inputs multiplexed to the MMC/FLCC over the avionics bus; the design goal is hands-on-throttle-and-stick
sensor/weapon management without reaching to panels.

## Sources
- Wikipedia *General Dynamics F-16* (side-stick force controller); ryanporto F-16 FCS — force-sensing stick.
- DCS guide Part 9 (control mapping) — cross-referenced above.
