# MiG-29A (9-12) — Cockpit, Instruments, HUD/HDD, Warning Systems

**Variant scope**: izdeliye **9-12** (Fulcrum-A). Deltas in §9.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 7–69. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf` (**printed page = PDF page − 6**);
pages used: 21–36, 40–61, 104–105.
Research in **§8 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **FULL** on instrument inventory, ranges/scales, the AEKRAN + TLP + VIWAS
warning chain, and HOTAS switch inventory. **FULL** on HUD *symbol lists* (both manuals enumerate
them). **SHALLOW** on HUD *geometry* — the manuals give FOV but **no symbol positions, sizes or
spacings**; every symbology diagram is a figure with numeric callouts, and the callouts do not carry
coordinates. This is the biggest gap in the file (§7).

> **Cockpit philosophy** (`DCS-EA p.7`): *"everything is at hand"* — HOTAS-grouped controls,
> **predominantly analogue pointer instruments and light boards**, chosen for reliability and
> readability. Panels: **front instrument panel** (four sub-panels: two left, one middle, one right),
> **left console**, **right console**, and a power unit behind the seat (not modelled).
> **Colour code, uniform across all scales** (`DCS-EA p.8`): **blue = safe range · yellow = edge and
> threshold values · red = dangerous/prohibited**. Emergency controls are marked with **red frames,
> caps and fuses**.

---

## 1. Front instrument panel — flight instruments

| Instrument | Designation | Range / graduation | Notes | Source |
|---|---|---|---|---|
| **IAS indicator** | **USM-2AE** | **0–800 kts**, single pointer, **non-linear scale**; Mach on the inner scale | Directly pneumatically driven; **indicated Mach may differ from true by up to M 0.05** due to pitot non-linearity | `DCS-EA p.13` |
| **Barometric altimeter** | **UV-30-2** | **0–100,000 ft**; 4-digit counter + pointer; **QFE/QNH setting 700–1080 hPa** on a 4-digit counter | Powered by the **ADC** (it is part of the ADC) | `DCS-EA p.14` |
| **TAS/Mach indicator** | **UMS-2,5-2U** | long pointer **100–1,400 kts** on a linear outer scale; short pointer on the inner Mach scale | ADC + air-temperature driven | `DCS-EA p.21` |
| **Attitude director indicator** | **KPP-SI** | pitch tape **±80°**, division 5°, digitised every 10°, **white above / black below** the horizon; roll scale **±60°**, digitised every 15° to 45°, division 5° up to 30° and 15° beyond | Valid **0–360° roll and 0–360° pitch except the 80–100° band** in climb/descent. Carries **flight-director bars** (vertical + horizontal), a **zero-index circle**, the **slip indicator**, a **CAGE button-lamp**, and off-flags **"T" (pitch) / "K" (azimuth)** dropped by AFCS or the landing system | `DCS-EA p.15–16` |
| **Horizontal situation indicator** | **PNP-72-12** | distance **0–999 nm**; rotating compass card | Shows current course, set course, desired course angle, drift angle, distance, course deviation, **azimuth/heading to the primary radio station**, flight-path deviation, lateral deviation. Off-flags: range curtain, **"VL"** (azimuth/NDB sensor fail), **"GS"** (glide-path receiver fail). 36 VAC | `DCS-EA p.16–17` |
| **Combined AoA / g meter** | — | AoA scale with **15°** and **25°** markers and a **red sector**; g scale with a **red mark at 7 g** | AoA from **probes on the left and right forward fuselage**; g from an **external transducer**. The g-index reset button **has no function yet** in DCS | `DCS-EA p.19` |
| **VVI / turn / slip** | **DA-200P** | three instruments in one case | Variometer (static-pressure rate), turn needle (**direction only, not an accurate rate**), slip ball, plus a **variometer zero-set knob** | `DCS-EA p.20` |
| **Radar altimeter indicator** | (A037 set, `DCS-FM p.13`) | **0–3,000 ft**, decimetre-wave | Pointer + **settable "low altitude" threshold bug** driving a light and a **voice** callout on descent; **red flag out = readings reliable**; status-check button (test → needle settles at the **45 ft** test mark, `DCS-EA p.72`) | `DCS-EA p.24–25` |
| **Flight & landing indicator** | **IP-52-03** | mechanical position display | Upper/lower speedbrake, L/R wing slat ("toe"), L/R flap **TAKEOFF** and **LANDING** positions, nose gear, main gear, and a **red lamp** warning "gear needed" / gear not fully travelled | `DCS-EA p.15`, `DCS-FM p.24` |
| **Magnetic compass** | **KI-13** | liquid | Emergency reserve for the main heading references | `DCS-EA p.30` |
| **Clock** | **AChS-1M** | current time, **stopwatch to 30 min**, flight-time in h:min | Mechanical, three mechanisms, wound by the left crown; **runs 3 days, wind every 2 days** | `DCS-EA p.21–22` |
| **Combined oxygen indicator** | — | O₂ quantity %, cockpit pressurisation failure flag, cockpit differential index, flow blinker, cockpit-altitude indices **0–23,000 ft / 23,000–42,700 ft / above 42,700 ft** | — | `DCS-EA p.28` |
| **Combined pressure indicator** | **IKG-1** | hydraulics **0–300 kp/cm²**, pneumatics **0–260 kp/cm²** | Colour bands and indices — see `engines-fuel.md` §2 | `DCS-EA p.29` |
| **Brake pressure gauge** | **M-2A** | two gauges, each **0–16 kp/cm²**, division 0.5 | **Normal brake max 8 ± 0.5**; **run-up brake max 11 ± 1.0** | `DCS-EA p.36` |
| **Voltmeter** | — | 28 V DC bus | Check ≈ **28 ± 0.5 V** at power-up | `DCS-EA p.35, 71` |
| Engine instruments | ITE-2TB, ITG-1, ISTR4 | — | see `engines-fuel.md` §2, §4.2 | — |
| Intake ramp position indicator | — | ramp position scale, L/R marker areas, L/R setting handles | see `engines-fuel.md` §3 | `DCS-EA p.30` |

Also on the panel: **pitot selection lever (MAIN / STBY)** on the pedestal (`DCS-EA p.35`), the
**cockpit temperature setter** (`DCS-EA p.36`), the **countermeasures panel** (`defence-rwr-cm.md` §3),
the **SPO-15LM indicator** (`defence-rwr-cm.md` §2.3), **AEKRAN** (§3), and the **emergency landing-gear
handle** (red, below the left panel; *after pneumatic emergency extension, normal retraction is
impossible until ground servicing* — `DCS-EA p.37`).

**FC3 panel item list** for cross-reference (`DCS-FM p.21–22`): gear handle · WCS panel · AoA+g ·
master caution · ADI · VVI · radar altimeter · tachometer · **HDD** · "Ecran" · signal-lights panel ·
AFCS panel · IAS · barometric altimeter · mechanical devices indicator · HSI · clock · TAS/Mach ·
**flares counter** · EGT indicators · fuel quantity · **SPO-15**.
`DCS-FM p.21` notes that *"cockpit instruments of the MiG-29A and MiG-29S are identical"* and mostly
shared with the Su-27.

---

## 2. Unified Indication System (SEI-31): HUD + HDD

`DCS-EA p.65`. The **unified display system** integrates target acquisition, navigation and mission data
from the WCS. It comprises **HUD**, **HDD** (the "direct vision indicator") and power/image-generation
units.

| Property | Value |
|---|---|
| **Instantaneous FOV** | **13° azimuth × 18° elevation** at 600 mm from the lens axis |
| **Total FOV** | **circular 24°**, accounting for head movement |
| Optics | collimator + combining glass, focused at infinity; **light-limiting filter** for bright backgrounds |
| Image source | **CRT** |
| Inputs | navigation system · fire control system · **AFCS** · **AoA and sideslip vanes** · **radar altimeter** |
| **HDD** | duplicates and **partially supplements** the HUD; used on HUD failure or in direct sunlight; its CRT is optimised for those conditions; has a shading hood and a brightness knob. **"TARGET–TRACK" and "TAC–DUPL" switches above the screen — not available** in DCS |

⚠️ **Compare the F-16** (`doc/f16/hud-symbology.md`): TFOV 25° / IFOV 10.5°. The MiG-29's **IFOV is
larger (13×18°) but its TFOV is much smaller (24° circular)**. A FlightBox MiG-29 HUD must be drawn in
a **circular** aperture, not the F-16's wide combiner rectangle — that alone changes the whole layout
and the clipping rule.

**HUD adjustment controls** (`DCS-EA p.66–67`): mechanical **filter on/off lever** · **symbol
brightness knob** · **"DAY / NIGHT / GRID" mode switch** (symbol colour, or fixed grid) · **TEST
button-lamp** (HUD test image).
The **GRID / RETICLE** position is operationally important: it is the fallback aiming reference when the
WCS fails (`radar-sensors.md` §5) and must be selected manually if the electronic crosshair is missing
in BS mode (`DCS-EA p.94`).

### 2.1 Base HUD symbol set (`DCS-EA p.66`)
1. Indicated airspeed · 2. Heading reference · 3. **Baro alt or Rad alt** · 4. Pitch angle ·
5. Bank angle · 6. **Nav range** · 7. Artificial horizon · 8. **Steering circle** ·
9. Aircraft symbol · 10. **IAS trend indexer**.

### 2.2 FC3 base HUD set, with behaviour (`DCS-FM p.40–41`) — richer on *meaning*
| # | Element | Behaviour |
|---|---|---|
| 1 | **Required speed** | assigned airspeed for the current flight mode / route leg, shown **above** the current IAS |
| 2 | **IAS** | left of the scale |
| 3 | **Longitudinal acceleration** | triangular index under the speed numerics; **right = accelerating, left = decelerating** |
| 4 | **Aircraft datum** | centre; indicates pitch and roll |
| 5 | **Navigation mark (large ring)** | direction to fly for the route **and altitude** to the next waypoint; **centred on the datum = on-route** |
| 6 | **Flight mode** | lower-left corner |
| 7 | **Required altitude** | assigned altitude for the current leg |
| 8 | **Current altitude** | **below 1,000 m AGL: radio altitude to 1 m; above 1,000 m: barometric to 10 m** |
| 9 | **Heading tape** | upper portion; e.g. "11" = 110° |
| 10 | **Pitch ladder** | right side of the HUD |
| 11 | **Horizon line** | 0° pitch reference |
| 12 | **Distance to selected waypoint** | lower centre, in km |

⚠️ The **automatic baro/radar altitude source switch at 1,000 m** is a real, modellable HUD rule and has
no F-16 equivalent. Note the FC3 manual gives it in metres while `DCS-EA` is an English/feet cockpit —
the 9-12 module presents **feet and knots**; the underlying jet is metric. Flag every unit conversion.

### 2.3 Combat HUD formats
The full per-mode symbol lists (scan, lock, IR, helmet, gun, air-to-ground, TOSS) are in
`radar-sensors.md` §3–5 and (for release cues) `weapons.md`. Only the cross-cutting cue vocabulary
belongs here:

| Cue | Meaning | Source |
|---|---|---|
| **А** | Attack — the sensor has captured | `DCS-EA p.88` |
| **ПР** | **Launch permitted** | `DCS-EA p.89`, `DCS-FM p.48` |
| **ОТВ** | **Break off the attack** (flashes at minimum permitted firing range) | `DCS-EA p.89, 100` |
| **Г** | **"Gorka"** — the commanded pull-up to target altitude | `DCS-EA p.89` |
| **РЛ** | Radar is on and radiating | `DCS-EA p.89`, `DCS-FM p.44` |
| **ТП** | IR (KOLS) is the lead channel | `DCS-EA p.91`, `DCS-FM p.51` |
| **ПП** | Passive interference introduced (KOLS gain reduced) | `DCS-EA p.91` |
| **АП** | **Active jamming detected** in the radar scan zone | `DCS-FM p.53` |
| **Ц1 / Ц2** | Target 1 / Target 2 (TWS2, MiG-29S) | `DCS-FM p.49` |

Attack-symbol dynamics: after missile launch the attack symbol **flashes at 2 Hz** (`DCS-FM p.47`).

---

## 3. Warning chain: TLP → MASTER CAUTION → AEKRAN → VIWAS (**FULL**)

Four cooperating devices. Reproducing the *chain* matters more than any individual lamp.

### 3.1 TLP (Telelight Panel / annunciator) — `DCS-EA p.40`
**Red lights = critical events; green lights = normal-state conditions.** Normal behaviour: a
malfunction **flashes** a red TLP light **together with a flashing MASTER CAUTION**; after MASTER
CAUTION is reset the TLP light goes **steady** until the problem is resolved.

### 3.2 MASTER CAUTION lamp-button — `DCS-EA p.18`
Flashes whenever any TLP red light illuminates **or** an AEKRAN warning is displayed. Pressing it
extinguishes itself and switches the corresponding TLP displays **from flashing to steady or off**.

### 3.3 AEKRAN (ЭКРАН) built-in monitoring and warning system — `DCS-EA p.31–32`
The MiG-29's equivalent of a maintenance-grade fault display *plus* an in-flight message queue.

| Property | Behaviour |
|---|---|
| Composition | logic & control unit (LCU) + display unit; **EPROM recording** |
| Principle | LCU polls sensors on an **"OK" / "FAILURE"** basis; out-of-limit parameter or failed sensor → a short message is generated, **displayed and recorded** |
| Ordering | messages are issued **by priority**; a higher-priority arrival pushes the current one back into the **QUEUE** (QUEUE lamp lights) |
| Pilot interaction | failure → **flashing MASTER CAUTION** + VIWAS voice **"See AEKRAN"**; the message stays until the pilot presses **AEKRAN CALL**, then the next one is shown until the queue is empty |
| End of queue | message disappears, **MEMORY lamp lights**; stored messages can be recalled with AEKRAN CALL when the queue is empty |
| Unlisted events | recorded to EPROM **without being displayed** |
| Panel | **FAIL / TURN(QUEUE) / MEMORY** lights, message display, **AEKRAN CALL** button |
| Modes | **1. BIT** (pressed before engine start, with BATTERY and NAVIGATION on and the FAILURE lamp out) → 15 s later prints **"SELFTEST"** then **"AEKRAN READY"**. **2. Ground check** by technicians. **3. Normal monitoring**, entered automatically when ENGINE START is pressed |
| ⚠️ Interlock | *"If self-test is not performed before starting the engine, **AEKRAN will fail**."* |
| Failure signature | FAILURE light appears and/or no/distorted messages within 15 s; **if it fails in flight, continue the mission and rely on VIWAS alone** |

### 3.4 VIWAS — Voice Information and Warning System — `DCS-EA p.43`
Powers up with the battery. Two buttons on the right console: **CHECK VOICE WARN** (self-test) and
**REPEAT VOICE WARN** (repeats the last message). **Multiple malfunctions are queued by priority.**
Self-test response: **"BINGO BINGO"** (`DCS-EA p.73`).

### 3.5 Voice message catalogue (`DCS-FM p.104–105`) — FC3, but the message *set* is informative
| Trigger | Message |
|---|---|
| Right / left engine fire | "Engine fire right" / "Engine fire left" |
| Flight controls damaged or destroyed | "Flight controls" |
| **Gear down above 250 kts** | "Gear down" |
| Gear up on ILS final | "Gear up" |
| Fuel to reach nearest friendly base only | "Bingo fuel" |
| Fuel 1500 / 800 / 500 | "Fuel 1500 / 800 / 500" |
| ACS / NCS / ECM / hydraulics / MLWS / avionics failure | "ACS failure", "NCS failure", "ECM failure", "Hydraulics failure", "MLWS failure", "Systems failure" |
| EOS / radar / ADI failure | "EOS failure", "Radar failure", "Attitude indication failure" |
| Damage not involving fire or flight controls | "Warning, warning" |
| **Max AoA reached/exceeded** | "**Maximum angle of attack**" |
| **Max G reached/exceeded** | "**Maximum G**" |
| **Max speed or stall speed reached** | "**Critical speed**" |
| Hostile missile within 15 km | "Missile, \<clock\> o'clock low/high" ⚠️ **ED construct — no MAWS on the 9-12**, see `defence-rwr-cm.md` §5 |

**Rebuild note**: this maps almost one-to-one onto FlightBox's `FBWarningSystem` bitmask, and the
AEKRAN gives it something the F-16 model does not have — an explicit **priority queue with pilot
acknowledgement per message**, i.e. a warning system that is *itself* a command/acknowledge channel.

---

## 4. Right console — `DCS-EA p.38–51`
| # | Panel | Notes |
|---|---|---|
| 1 | **TLP annunciator panel** | §3.1 |
| 2 | **Navigation system control panel A-323** | `navigation.md` §2 |
| 3 | ID index coder | **not implemented** |
| 4 | **SPO-15LM control panel** | `defence-rwr-cm.md` §2.3 |
| 5, 7 | Cockpit air blow distribution / supply levers | — |
| 6 | **ADF control panel** | `navigation.md` §3 |
| 8 | **Guidance system panel** | **not available** — this is the **GCI/Lazur** panel, see `datalink-gci.md` |
| 9 | KD system panel | not available |
| 10 | **Engine startup panel** | `engines-fuel.md` §5.1 |
| 11 | **System power panel** | RADIO · ACFT SYST · GYRO MAIN · GYRO STBY · NAVIGATION · WEAPON · **ACS** (weapons control) · **AFCS** · IFF · RECORD · **"ALL ON" bracket**; plus the **INS "OPER/PREPARE" switch** and the lamps **LH INLET CHECK · NAV READY · RH INLET CHECK · FAST PREP** (`DCS-EA p.51–52`) |
| 12 | Control and test panel | — |
| 13 | **Electrical power panel** | 8 switches: main power (battery + ground) · DC/AC converter · AC generator · DC generator · **engine anti-surge system** · engine actuators and sensors power · **engine fuel pump** · "ALL ON" frame handle (`DCS-EA p.47`) |
| 14, 17 | AFT / FWD lighting control panels | brightness knobs: PANEL (pressed = manual, released = automatic), FLOODLIGHT, MAP ILLUM, CONSOLE, INSTRUMENT, LTS ILLUM BRIGHT; **NAV LTS** 4 positions: off / 100 % / **10 % dim** / flashing; **CONTROL LAMP** = lamp test (`DCS-EA p.44`) |
| 15 | Canopy close check pin | pin recessed = locked (`DCS-EA p.49`) |
| 16 | **IFF transponder** | **not implemented yet** (`DCS-EA p.48`) |
| 18 | Emergency canopy jettison lever | — |
| 19 | AM/FM switch | radio modulation select (`DCS-EA p.43`) |
| 20 | Cockpit air conditioning panel | 4-position heating switch AUTO/HOT/COLD/off + **PITOT HEAT** (`DCS-EA p.46`) |
| 21 | **VIWAS controls** | §3.4 |

---

## 5. Left console — `DCS-EA p.52–64`
| # | Item | Notes |
|---|---|---|
| 1–3 | Oxygen valve, suit-ventilation knob, **oxygen system panel** | 100 %/MIXT · emergency O₂ · helmet ventilation, all under guards. **100 % oxygen is mandatory on combat flights regardless of altitude**, and in case of smoke; emergency O₂ raises consumption **2–3×** (`DCS-EA p.53–54`) |
| 4 | **ARU control, emergency pumping station, MRK; drag-chute release** | see `flight-controls.md` §3 |
| 5 | **R-862 radio panel** | §6 |
| 6 | **Wing flap control panel** | 3 buttons: TAKEOFF · LANDING · UP; logic in `flight-controls.md` §7 |
| 7 | **Emergency control panel** | ramps, fuel shut-off, fire extinguisher, AB emergency shutdown, generator-drive shutdown, air-start switches — **not available** (`DCS-EA p.58`) |
| 8, 11 | **Radar control panels PU-S31 / PUR-31** | `radar-sensors.md` §2.1, and PU-S31 in `weapons.md` |
| 9 | Chute release button | §7 of `procedures.md` |
| 10 | **AFCS controls** | `flight-controls.md` §5.1 |
| 12 | Landing gear lever | — |
| 13 | **Emergency missile launch button** | hold to force-launch missiles/ASP (`DCS-EA p.62`) |
| 14 | Landing lights switch | off / taxi only / all landing lights (`DCS-EA p.62`) |
| 15–16 | Canopy handle (**open / partly open / closed+locked**) + "CLOSE CANOPY" warning light | `DCS-EA p.61` |
| 17 | **External stores selector** | left = **inner** pylons, right = **outer** pylons (`DCS-EA p.60`) |
| 18 | **IR sound volume knob + 3-position rudder trim switch** | the IR-seeker lock tone volume (`DCS-EA p.64`) |
| 19 | Throttle tightening handle | friction; **no function** in DCS (`DCS-EA p.55`) |
| 20 | Cockpit emergency decompression lever | — |
| — | **Feel unit control panel** | emergency pumping station switch (N/I) · nosewheel strut emergency shutdown (N/I) · **drag chute drop button** · **FEEL UNIT** (N/I) — see `flight-controls.md` §3 (`DCS-EA p.55`) |

---

## 6. Communications — R-862 (`DCS-EA p.56`)
| Property | Value |
|---|---|
| Bands | **VHF 100–149.975 MHz**, **UHF 220–399.975 MHz** |
| Preset channels | **20** |
| Guard frequency | **121.5 MHz**, with a dedicated receiver switch and lamp |
| **Range vs altitude** | ≥**65 nm** at 3,300 ft · ≥**135 nm** at 16,500 ft · ≥**189 nm** at 33,000 ft |
| Channel change time | **≤1.5 s** |
| Readiness | immediate after power-up |
| **Antenna** | in the **right wing tip** — *"when performing turns with a bank of more than **45°** the antenna may be shaded and radio communication may be lost"* |
| Modulation | **AM and FM**, selected by the AM/FM switch on the right console |
| Panel controls | guard lamp · guard receiver switch · **SQLCH** · **ADF audio switch** · master volume · channel indicator · preset selector |

**Rebuild note**: the **45° bank comms blackout** is a documented, geometry-driven communications
failure mode — directly analogous to (and modelled with the same machinery as) the RWR blind zone. It
is also the *only* documented degradation of the GCI voice channel (see `datalink-gci.md`).

Intercom: **SPU-9** (`DCS-FM p.13`). ATC guard commands can be received via the **ARK-19** radio compass
in an emergency (`DCS-FM p.13`).

---

## 7. HOTAS inventory

**Control stick** (`DCS-EA p.68–69`):
1. Four-position **trim** hat · 2. **AFCS MODES OFF** · 3. **Target acquisition symbol control (KU-31)**
= TDC slew · 4. **Break-lock** · 5. **Brake lever** · 6. **Levelling button** (level-to-horizon) ·
7. **Gun trigger** · 8. **Missile/armament launch trigger** · 9. **Autopilot cut-off** ·
10–11. movable arm rest + adjustment · 12. **Centreline tank jettison** (under a guard) ·
13. rudder-pedal adjustment ring · 14. **Run-up brake lever**.

**Throttle grips** (`DCS-EA p.69`):
1. **Target range wheel** · 2. Radio (PTT) · 3. **Airbrakes switch** (spring-loaded) ·
4. **Lock On** — dual function: **Lock On airborne** / **NWS High with WoW and flaps up** / PF ·
5. **Afterburner lock latches** · 6. **Countermeasures dispense button** · 7. **Idle throttle lock
latches**.

Two mechanical detents matter for a throttle model: the **idle lock** (must be lifted to shut down) and
the **afterburner lock** (must be released to enter AB — `DCS-EA p.78`, "AB detents unlock by
pressing [0]").

Weapon triggers have an **intermediate ("preliminary") position** used to arm the gun-sight and A-G
modes before the full press (`DCS-EA p.95, 99, 101`).

---

## 8. Technical depth (researched)
Little research was needed for this file — both manuals are dense on the cockpit. Two items:
- **SEI-31** as the designation of the indication system, and **C100** as the digital mission computer,
  come from `DCS-FM p.12` (source-internal, not research).
- **Shchel-3UM** HMD hardware detail: see `radar-sensors.md` §6.5. No public source was found for the
  HMD's **symbol set** beyond "aiming ring + ПР + X"; the monocle is a **right-eye reflector**
  (`DCS-FM p.56`, `DCS-EA p.92`).

---

## 9. Open gaps (honest)
1. **HUD symbol geometry** — no positions, sizes, spacings, tape scale intervals or clipping rules
   anywhere in either manual. Every DCS figure carries numeric callouts *without* coordinates. A
   FlightBox MiG-29 HUD cannot be built to the standard of `doc/f16/hud-symbology.md` from these
   sources. **This is the biggest single documentation gap in the whole `doc/mig29/` set.**
   Mitigation path: measure a screenshot grid against the stated **13°×18° IFOV / 24° circular TFOV**.
2. **TLP lamp inventory** — `DCS-EA p.40` describes the *behaviour* of the annunciator panel but
   **never lists its lamps**. Individual lamps appear scattered through the procedures ("BOTH HYDRO
   FAILURE", "STAB TRIM NEUTRAL", "AIL TRIM NEUTRAL", "RUD TRIM NEUTRAL", "DAMPER OFF", "COC FAIL",
   "NO COC RESERVE", "FEEL UNIT TAKEOFF–LANDING", "CLOSE CANOPY", "LH/RH ENG START"). A complete panel
   map would need a cockpit texture, not a manual.
3. **AEKRAN message catalogue** — the mechanism is fully documented, **not one actual message string**
   is listed beyond "SELFTEST" and "AEKRAN READY".
4. **HDD formats** — described only as "duplicates and partially supplements the HUD"; the two switches
   above it are not implemented. The FC3 manual mentions an **HDD top-down tactical view in TWS**
   (`DCS-FM p.46`) that the EA manual never describes.
5. **VIWAS message list for the 9-12** — only the FC3 (game) list exists (§3.5).
6. **Instrument lag/dynamics** — nothing beyond the stated ±M 0.05 pitot non-linearity.

---

## 10. Variant notes
- `DCS-FM p.21`: **MiG-29A and MiG-29S cockpit instruments are identical**; most instruments are also
  shared with the Su-27. So this file is valid for 9-13 as-is.
- **MiG-29SMT/MiG-35**: glass cockpit (MFDs), nothing here carries over except the HOTAS philosophy.
