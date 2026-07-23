# F-16C Weapons & Armament

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 11 — Offence: Weapons & Armament, pp. 313–573 (261 pp).
Structural reference to the arsenal, the SMS, and delivery-mode concepts; per-weapon employment tutorials
in the guide are not reproduced step-by-step.

## SMS (Stores Management Set) page
Central weapons interface on the MFD: shows loaded stores per station, selected weapon, **profiles**
(PROF1…, each with a default release mode), release parameters. Master Arm switch (ARM/SIM/OFF) gates
employment. Weapon/station step via **NWS A/R DISC & MSL STEP** button.

## Armament overview

### Air-to-Ground
| Category | Weapons |
|---|---|
| Unguided bombs | MK-82 (low drag), MK-82AIR (high drag), MK-82SE (Snake Eye), MK-84 |
| Cluster | CBU-87, CBU-97, CBU-105 WCMD (wind-corrected, INS-guided) |
| Laser-guided | GBU-10 / GBU-12 / GBU-16 Paveway II |
| GPS-guided | GBU-38 JDAM, AGM-154A JSOW |
| A-G missiles | AGM-65D/G (IR seeker), AGM-65H/K (EO seeker) Maverick |
| Anti-radiation | AGM-88C HARM (homes on radar emissions → SAM sites; HTS pod) |
| Rockets | 2.75" rockets |
| Gun | M61A1 20 mm |

### Air-to-Air
| Weapon | Notes |
|---|---|
| M61A1 gun | 20 mm; EEGS sight |
| AIM-9L/M Sidewinder | IR, boresight/uncage |
| AIM-9X Sidewinder | IR, high-off-boresight (HOBS) via HMCS |
| AIM-120C AMRAAM | Active radar, BVR; TWS datalink post-launch |

## Bomb delivery modes (A-G)
| Mode | Concept |
|---|---|
| **CCIP** (Continuously Computed Impact Point) | Dive bombing: pipper on the bomb fall line shows live impact point; pickle when pipper crosses target. Lower = more vulnerable |
| **CCRP** (Continuously Computed Release Point) | Straight-and-level; designate target (radar/TGP/steerpoint), HUD gives release cue; auto-release while holding pickle |
| **DTOS** (Dive Toss) | Designate target on HUD, then toss |
| **LADD** (Low Altitude Drogue Delivery) | Not simulated |
| **MAN** (Manual) | Not simulated |

Cycle release mode via SMS or the **NWS A/R DISC & MSL STEP** button (A-G). CCIP dive sight-picture cues
(target vs canopy rail) given in the guide for 10/20/30/45° dives.

## Guided-weapon employment (concepts)
- **LGB (GBU-10/12/16)**: designate + track target with TGP, lase (Laser Arm + trigger), release in CCRP;
  bomb homes on the laser spot. Laser codes set on LASR DED page and must match.
- **Maverick (AGM-65)**: seeker is a sensor page. Modes — **pre-planned** (slaved to TGP or A-G radar),
  **boresight** (D), **visual** (H). Boresight the missile before employment; TMS to lock; pickle.
- **HARM (AGM-88C)**: **HAS** (HARM As Sensor — self-detect emitters), **POS** (position, cued by HTS/EOM).
  HTS pod feeds the **HAD** (HARM Attack Display). ALIC tables define emitter threat priorities.
- **GPS (JDAM/JSOW)**: target coordinates (steerpoint/designation) → INS/GPS guidance; launch and leave.

## Gun sights
- **A-G gun**: M61A1 strafe with CCIP gun pipper.
- **A-A gun (EEGS — Enhanced Envelope Gun Sight)**:
  - **Level II** (no radar): funnel sight; funnel width set on **MAN** DED page.
  - **Level V** (with radar): radar-ranged lead-computing pipper.

## HOTAS employment quick map (see `hotas.md`)
- **Trigger** 2nd stage = gun. **Weapon Release** = pickle A-G.
- **TMS** up/down/left/right = designate/track/reject. **DMS** = SOI/display management.
- **Dogfight switch**: DOGFIGHT = gun + A-A missile HUD symbology; Missile Override = A-A missile only.
- **MSL STEP** = cycle A-A station / cycle A-G mode (CCRP↔CCIP↔DTOS).

---

# Technical depth (researched — shallow pass — deepen when in scope)

> Weapons employment is outside the current rebuild scope (flight + rendering). LRU/principle stub only.

## Components (LRUs)
- **SMS** (Stores Management System): the SMS logic historically ran in the **XCIU** (Expanded Central
  Interface Unit), **absorbed into the MMC** on Block 50+ (see `cockpit-displays.md`).
- **Gun**: **M61A1 Vulcan** 20 mm 6-barrel rotary cannon (~511 rounds, ~6000 rd/min).
- **Pylons/launchers**: wingtip + underwing rails (e.g. **LAU-129** for AIM-120/AIM-9), **TER/BRU** bomb
  racks; 9 hardpoints + centerline.
- **Weapon designations** (already itemized above): AIM-9/AIM-120, AGM-65/-88, GBU-10/12/16/38,
  AGM-154, MK-82/-84, CBU-87/-97/-105.

## Functional principle
The MMC computes a firing/release solution from sensor track (FCR/TGP/HMCS) + own-ship state (INS/air data)
and the selected weapon's ballistics/seeker model, then presents CCIP/CCRP/DTOS cues on the HUD. Pickle
authority is gated by Master Arm; the SMS sequences stations and sends launch/release commands over the
1553 bus to the pylon/launcher. Guided weapons (LGB/Maverick/HARM/JDAM) receive designation or coordinates
pre-launch and then fly autonomously (laser spot, IR/EO lock, anti-radiation homing, or GPS/INS).

## Sources
- Wikipedia *General Dynamics F-16* armament; airforce-technology.com F-16 — SMS/MMC, gun, hardpoints.
- DCS guide Part 11 (arsenal, delivery modes) — cross-referenced above.
