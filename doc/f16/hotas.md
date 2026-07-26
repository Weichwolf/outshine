# F-16C HOTAS

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 9 — HOTAS, pp. 158–160.
Plus `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — **Hands-On Controls (HOTAS)**
chapter, p.82–88 (cross-check pass, this round) — Chuck's table below is confirmed **not contradicted**
by ED anywhere; ED adds the precise **SOI-dependent action matrices** (what each switch direction does
changes with Sensor-of-Interest and master mode), **exact short/long press timing thresholds**, and
several numeric facts Chuck's table omits entirely (speedbrake deflection angles, RET DEPR range).
Cite tags `Chuck p.NN` vs `ED EA Guide p.NN`. A subset of the entries below, reorganized as discrete
pilot commands with precondition/feedback/failure columns, is in
**[`controls-commands.md`](controls-commands.md)**.

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

## ED EA Guide addendum — Hands-On Controls, FULL cross-check (official, p.82–88)

**Press-duration convention** (ED explicit, applies to every switch below unless noted): **short press
< 0.5 s**, **long press > 0.5 s**. Exceptions requiring a **full 1 s** press are called out inline
(bolded in ED's own table — TMS FWD-long "Cycle HARM POS", TMS RIGHT-long "IR Pointer ×2").

### SSC — additions/precision over Chuck's table
| Control | ED precision beyond Chuck |
|---|---|
| **Trigger** (2-stage) | 1st detent = laser designator/ranger (needs TGP powered); **2nd detent** = fires gun **if selected+armed**, OR fires the laser designator/ranger for **30 s** if in CCIP/STRF sub-modes (i.e. the trigger's 2nd-stage meaning is **mode-conditional**, not a fixed "gun" action) |
| **Weapon Release button** | press-and-**hold** to fire A-A missiles or release A-G stores (not a single-press pickle — a held-duration command) |
| **MSL STEP button** (Chuck's "NWS A/R DISC & MSL STEP") | full per-context table below — 3 distinct functions depending on ground/air state and master mode |
| **Paddle switch** | precisely: disengages autopilot **for the duration held**; on release, autopilot **captures NEW reference values** of pitch/roll/altitude per whatever PITCH/ROLL modes are selected on the MISC panel (`flight-controls-flcs.md`) — a "release re-trims to current attitude," not "resumes old target" |
| **Expand/FOV button** | SOI-dependent cycle table below; also toggles AGM-65 seeker FOV or steps AGM-88 FOV/POS sub-modes when WPN is SOI |

#### MSL STEP button — full context table (ED p.82–83)
| State | Short press (<0.5s) | Long press (>0.5s) |
|---|---|---|
| Ground | Toggle nosewheel steering (NWS) on/off | — |
| Air, AIR REFUEL switch OPEN | Manually disconnect refuel boom | — |
| Air, NAV mode | No action | No action |
| Air, A-A / Missile Override / Dogfight mode | Select next missile station (same type) | Cycle missile type |
| Air, A-G mode | Cycle CCIP → DTOS → CCRP | — (AGM-65/AGM-88 selected: select next station instead) |

#### TMS (Target Management Switch) — SOI-dependent 4-way × short/long matrix (ED p.83)
| Direction × duration | HUD SOI | HMCS SOI | FCR SOI | HSD SOI | HAD SOI | TGP SOI | WPN SOI |
|---|---|---|---|---|---|---|---|
| FWD short | Designate (DTOS/VIS) | Designate | Designate / ACM BORE | Designate | (No Action) | Point Track | Track/Force Correlate |
| FWD long | Show Ring (on release) | (No Action) | (No Action) | (No Action) | (No Action) | Spotlight Scan | Cycle HARM POS **(1 s)** |
| LEFT short | Area Track Scan | (No Action) | TV/FLIR/NCTR Interrogate | (No Action) | DED→SEAD | FLIR Polarity | HARM Table/MAV Polarity |
| LEFT long | (No Action) | (No Action) | Initiate TDOA | (No Action) | (No Action) | (No Action) | (No Action) |
| RIGHT short | *Sighting-Point Rotary Step | Target Step | Area Track/Inertial Track | HARM Target Step | (No Action) | Target Step | (No Action) |
| RIGHT long | ACM 30×20 | (No Action) | TWS Toggle | (No Action) | IR Pointer ×2 **(1 s)** | (No Action) | (No Action) |
| AFT short | Target Reject / SOI→HUD | Target Reject | Drop PDLT / ACM 10×60 | Target Reject | Slave Mode Terminate/Hide Ring | Target Reject/MAV Slave | Target Reject/Cursor Zero |
| AFT long | (No Action) | (No Action) | DED→CNI/TDOA (while held) | (No Action) | Declutter (while held) | (No Action) | (No Action) |

*= only relevant in A-G master mode when HUD/FCR is SOI: TMS RIGHT cycles through available sighting points.

#### DMS (Display Management Switch) — SOI selector (ED p.83)
| Direction × duration | Effect |
|---|---|
| FWD short | SOI → HUD |
| HMCS-related short | SOI → HMCS |
| LEFT short | SOI → FCR (if displayed); LEFT long | Cycle **left** MFD format |
| RIGHT short | SOI → HSD (if displayed); RIGHT long | Cycle **right** MFD format |
| AFT short | SOI → HAD or TGP or WPN (whichever displayed) |
| AFT long | SOI → MFD (generic) |
| — | Helmet Display Unit ON/OFF (context-specific binding) |
| — | Swap SOI between the two MFDs |

#### CMS (Countermeasures Management Switch) — mode-dependent (ED p.84, cross-refs `defence-rwr-cm.md`
for the full CMDS mode×CMS state machine)
| Direction | Effect |
|---|---|
| FWD | Dispense 1× Manual Program 1–4 (per CMDS PRGM knob) |
| LEFT | Dispense 1× Manual Program 6 |
| RIGHT (CMDS=MAN) | Deactivate ECM emissions |
| RIGHT (CMDS=SEMI) | Disable ECM emissions |
| RIGHT (CMDS=AUTO) | Disable auto-program dispensing / interrupt current dispense |
| AFT (CMDS=MAN) | Activate ECM emissions (only if ECM set to Mode 3) |
| AFT (CMDS=SEMI) | Dispense 1× Auto Program; enable ECM if Mode 1/2 |
| AFT (CMDS=AUTO) | Enable continuous auto-program dispensing |

#### EXP/FOV button — SOI-dependent (ED p.84)
| SOI | Short press | Long press |
|---|---|---|
| FCR | Cycle FCR EXP modes | — |
| HSD | Cycle HSD EXP modes | HSD ZOOM mode (while held) |
| HAD | Cycle HAD EXP modes | — |
| TGP | Cycle FOV / ×2 zoom | — |
| WPN (AGM-65) | Toggle missile seeker FOV | — |
| WPN (AGM-88, HAS sub-mode) | Cycle WIDE→CTR→LT→RT | — |
| WPN (AGM-88, POS sub-mode) | Cycle EOM→RUK→PB | — |

### Throttle (TQS) — additions/precision over Chuck's table
| Control | ED precision beyond Chuck |
|---|---|
| **UHF/VHF Transmit switch** | AFT=UHF Tx, FWD=VHF Tx (matches Chuck); the **left/right (IFF OUT/IN)** positions Chuck listed as "filter datalink"/"toggle tracks" are precisely: **short press** LEFT=toggle datalink info on FCR, RIGHT=cycle datalink filters; **long press** RIGHT (>0.5s)=transmit selected steerpoint/SPI/SEAD-target over datalink, LEFT=no action |
| **MAN RNG/UNCAGE knob** | rotate = manual TGP zoom; **short depress (<0.5s)** = AIM-9 seeker uncage / TGP LST toggle / (gear-down, airborne) HUD declutter — removes Roll Indicator + ILS bars, repositions Heading Scale to top of HUD; **long depress (>1.0s)** = toggle Gun Strafe mode |
| **DOG FIGHT switch** | outboard=**Dogfight**: auto-selects ACM radar mode (radar goes to standby until commanded to transmit), HUD shows gun+missile symbology, "DGFT" HUD tag. Inboard=**Missile Override**: auto-selects RWS radar mode, HUD shows A-A missile symbology, gun unavailable, HUD tag "MRM"/"SRM"/"HOB" (by missile type) or "MSL" (no A-A missiles loaded). Center = revert to last master mode. **Overrides every mode except Emergency Jettison.** |
| **ANT ELEV knob** | rotary, center detent, manual FCR antenna elevation |
| **RDR CURSOR/ENABLE** | multi-directional slew (FCR/HSD/HAD cursor, TGP sensor, AGM-65 seeker); **depress**: in A-A mode, swaps AIM-9 BORE/SLAVE **for duration held**; in A-G mode with AGM-65, steps PRE→VIS→BORE |
| **SPD BRK switch** | 3-pos, aft-momentary, spring-loaded off-aft to center; extension/retraction continues for as long as held, so **any intermediate deflection is achievable**. **Full extension: 60°** when right main gear is NOT down-and-locked. **Limited to 43°** when right main gear IS down-and-locked (prevents lower surfaces striking ground on landing) — **overridable temporarily by holding SPD BRK aft**; once the nose gear compresses after touchdown, full 60° is available again without holding |

### Cross-check verdict
No numeric or logical contradiction found between Chuck's HOTAS table and ED's Hands-On Controls
chapter — Chuck's table is a correct but coarse summary; ED supplies the missing **state-dependent**
detail (SOI/master-mode matrices, exact press-duration thresholds, speedbrake angles) that a
command-block model needs to reproduce *when* a HOTAS input is valid/what it actually does, not just
*that* the switch exists.

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
