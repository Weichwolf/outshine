# F-16C Radar & Sensors

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 10 — Radar & Sensors, pp. 161–312 (152 pp).
This is a structural reference to the mode taxonomy + key display/HOTAS concepts; the guide contains
extensive per-mode tutorials not reproduced step-by-step here.

## Sensor overview
Sensors: **FCR** (AN/APG-68 Fire Control Radar), **TGP** (targeting pod), **HMCS** (helmet cueing),
Maverick seeker, HTS pod. Displayed on the MFDs; the active sensor = **SOI** (Sensor of Interest), set by
**DMS** (up = HUD, down = MFD sensor). Master modes: **NAV / A-A / A-G** (drive which sensor modes exist).

## FCR — Air-to-Air modes

Top-level: **BVR** (beyond visual range search) · **ACM** (air combat maneuvering, close-in) · **STT**
(single target track). Display: range = vertical axis, azimuth = horizontal.

### CRM (Combined Radar Mode) — default at power-up
Combines BVR search sub-modes under one interface (cycle sub-modes: hold **TMS right** > 1 s):
| Sub-mode | Behavior |
|---|---|
| **RWS** (Range While Search) | Default search; large-volume, all-aspect, all-altitude detection |
| **TWS** (Track While Scan) | Maintains up to **10 trackfiles** while still scanning; artificially limits scan volume (bars/azimuth), auto-centers; provides post-launch datalink for AMRAAM; less precise than STT |

### SAM (Situational Awareness Mode)
Hybrid RWS/STT: locking a target in RWS enters SAM. Radar periodically scans the locked target while
scanning the whole area. Acquire: cursor on target → **TMS forward** once → release (starts 4-bar,
±10° spotlight at last known position).

### STT / DTT (radar lock)
- **STT** (Single Target Track): all radar power on one target, highest accuracy.
- **DTT** (Dual Target Track): two targets tracked.

### Spotlight
In RWS/TWS, a spotlight scan can designate targets at longer range (concentrates the scan).

### ACM (Air Combat Mode) — close-in auto-acquisition
Sub-modes: **HUD Scan**, **Vertical Scan**, **Boresight**, **Slewable** — auto-lock the first target in a
close-range volume tied to HUD geometry.

### Other A-A
- **EXP (Expand)**: zoom the radar display.
- **HMCS radar lock**: lock the target the helmet cross is on.

## FCR — Air-to-Ground modes
| Mode | Purpose |
|---|---|
| **GM** (Ground Mapping) | Terrain/ground map picture |
| **Expanded (EXP1/EXP2)** | Zoomed ground map |
| **FTT** (Fixed Target Track) | Designate/track a fixed ground point |
| **GMT** (Ground Moving Target) | Detect moving ground targets |
| **BCN** (Beacon) | Beacon returns |

Designation via Radar Cursor slew + TMS; sets the SPI for A-G weapons and nav fixes.

## FCR — Air-to-Sea
- **SEA** mode: detect surface ships.

## TGP — Targeting Pod (Sniper/Litening class)
- Modes: A-G (point/area track), A-A. SOI via DMS down; slew with Radar Cursor; **TMS up** = point track,
  **TMS right** = area track, **TMS down** = slave to steerpoint.
- **Cursor Zero (CZ)** / steerpoint slaving, **Snowplow** mode (pod points at ground ahead), **LSS**
  (Laser Spot Search).
- Lasing: Laser Arm switch ARM → trigger 1st stage fires laser ("L" flashes on HUD); used for ranging,
  nav fix, and LGB guidance. Laser codes set on the LASR DED page.

## HMCS — Helmet-Mounted Cueing System
- Power-up + alignment (coarse then fine) against the HUD reference cross — see `procedures-startup.md`
  steps 66–68 and `hud-symbology.md`.
- HMD symbology mirrors key HUD/RWR elements off-boresight; used for high-off-boresight (HOBS) missile
  cueing (AIM-9X), ground target designation, and off-HUD radar lock.

## Weapon sensor: AGM-65 Maverick
Maverick seeker (IR: D/G; EO: H/K) is itself a sensor page — boresight, pre-planned (slaved to TGP/radar),
visual, and boresight employment modes (see `weapons.md`).

---

# Technical depth (researched — shallow pass — deepen when in scope)

> Combat sensors are outside the current rebuild scope (flight + rendering). This is an LRU/principle
> stub for future extension only.

## Components (LRUs)
- **FCR**: **AN/APG-68(V)** — mechanically-scanned (planar-array) pulse-Doppler fire-control radar,
  ~4 LRUs (antenna, transmitter, low-power RF, radar signal processor). Newer builds field the
  **AN/APG-83 SABR** AESA.
- **Targeting pod**: **AN/AAQ-33 Sniper ATP** or **AN/AAQ-28 LITENING** (EO/IR + laser designator/tracker).
- **HMCS**: **JHMCS** (Joint Helmet-Mounted Cueing System) — projects symbology on the visor for
  high-off-boresight cueing.
- **HARM targeting**: **AN/ASQ-213 HTS** pod (emitter geolocation).

## Functional principle
The APG-68 is a coherent pulse-Doppler set: it transmits phase-coherent pulse trains and range-Doppler
processes the returns, so airborne targets are separated from ground clutter by their closing-rate Doppler
shift (this is why look-down/all-aspect works). Air-to-air modes (RWS/TWS/STT) trade scan volume for track
quality; air-to-ground modes synthesize a ground map or detect moving targets by their Doppler. The pod
sensors are passive EO/IR with an active laser for ranging and guidance. All sensors are LRUs on the
1553/fiber avionics bus, arbitrated as the Sensor of Interest (SOI).

## Sources
- Wikipedia *AN/APG-68*, *AN/APG-83*; airforce-technology.com F-16 — radar/pod/HMCS designations.
- DCS guide Part 10 (mode taxonomy) — cross-referenced above.
