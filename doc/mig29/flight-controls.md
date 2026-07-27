# MiG-29A (9-12) — Flight Controls, AFCS, Limiters

**Variant scope**: MiG-29 izdeliye **9-12** (Fulcrum-A). Variant deltas at the end (§8).

**Sources** (cite the tag, never a bare page number):
- **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (Eagle Dynamics, MiG-29 9-12 full-fidelity
  module, 115 pp, 2025). Printed page == PDF page. Pages used here: 19, 52, 55, 57, 60, 64, 68–69,
  73–74, 77.
- **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf` (Eagle Dynamics, FC3 MiG-29A/S, 116 pp, 2018).
  **Printed page = PDF page − 6.** Pages used here: 9–14, 33–36.
- Researched material is confined to **§6 Technical depth** and tiered:
  **T1** official/declassified mil docs · **T2** manufacturer data · **T3** established literature
  (Jane's, Piotr Butowski) · **T4** community/encyclopaedic consensus.

**Depth declaration**: **FULL** on the AFCS (SAU-451) mode logic and its engage/disengage conditions —
both manuals cover it precisely. **MEDIUM** on the mechanical/hydraulic control architecture, surface
geometry and the SOS-3M limiter — the DCS manuals barely touch it, §6 fills it from research.
**SHALLOW** on stick-force gradients, ARU gear-ratio *schedules* and damper gains — no public source
found this pass (§7).

---

## Spec

### 1. The headline fact for FlightBox: **this is NOT a fly-by-wire aircraft**

The MiG-29 9-12 has a **conventional mechanical control system with irreversible hydraulic actuators**
— rods and bellcranks from stick/pedals to the boosters, no electrical command path in the primary
loop. Everything "smart" is bolted *alongside* the mechanical run:

| Layer | Device | What it does to the mechanical run |
|---|---|---|
| Gearing | **ARU-29-2** | Changes the **transmission ratio** stick→stabilizer *and* the pitch feel-spring rate, as a function of measured pressures (q/altitude) |
| Damping/autopilot | **SAU-451** (3-channel) | Adds series/parallel actuator authority on top of pilot input |
| Envelope | **SOS-3M** | Limiter: **pushes the stick forward** at the AoA threshold; also drives the LEF |
| Feel | Artificial feel unit | Spring feel, hydraulically powered; trim shifts the neutral point |

**Contrast with the F-16 baseline (`doc/f16/flight-controls-flcs.md`)**: the Viper's FLCS *is* the
control system and the pilot commands rates through it. Here the pilot commands **surface position
through a variable gear ratio**, and the automation is an *addition* that can be switched off (the
aircraft remains flyable with DAMPER off). A FlightBox MiG-29 module must therefore be the mirror
image of the F-16 module: **no `fbw-override` analogue is needed to "get to the surfaces"** — the
surfaces *are* the primary interface; the ARU gearing and the SOS limiter are what must be modelled on
top.

Consequences that are stated outright in the sources:
- The limiter is **soft and overridable**: "the limiter is designed to be overridden by simply pulling
  harder on the stick" (§6.3, T4) — there is no hard command clamp.
- Trimming **moves the stick** (FFB users see the physical neutral shift), because trim biases the feel
  unit, not a command signal — `DCS-FM p.33`.
- "There is no feeling of true loads of the controls, according to which real aircraft are trimmed" —
  ED's own note on why a "trim to neutral" hotkey exists in the sim; `DCS-FM p.33`.

---

### 2. Control surfaces — geometry and deflection limits

All from `DCS-FM p.10` (the FC3 manual's "Aircraft construction" section) unless marked.

| Surface | Area | Deflection | Notes |
|---|---|---|---|
| **Leading-edge flaps** (3-section, per wing) | 2.35 m² | **20°** | Auto-extend at **AoA > 8.7°**; retract with, or synchronously to, the trailing-edge flaps |
| **Trailing-edge flaps** (single-slotted) | 2.84 m² | **25°** | — |
| **Ailerons** | 1.45 m² | **up 25° / down 15°** | **Neutral position = 5° up** (i.e. the aileron is rigged trailing-edge-up in cruise) |
| **Horizontal stabilizer** (all-moving, differential) | 7.05 m² | **takeoff ≈ 15° up / 35° down**; **in flight 5°45′ / 17°45′** | LE sweep 50°, anhedral 3°30′. The two deflection sets are the visible signature of the ARU gearing change (§3) |
| **Vertical fins** (each) | 1.25 m² | rudder deflection not given | First-series aircraft had **20 % smaller** fins |
| **Upper airbrake** | 0.75 m² | **+56°** | — |
| **Lower airbrake** | 0.55 m² | **−60°** | — |

Wing/LEX geometry (`DCS-FM p.10`): LEX sweep **73°30′**, LEX area **4.71 m²**; wing LE sweep **42°**,
TE sweep ≈ **9°**, anhedral **−3°**; wing area **38.056 m²**, span **11.36 m** (`DCS-FM p.14`).

⚠️ **Interpretation flag**: the "takeoff 15/35 vs in-flight 5°45′/17°45′" pair is quoted without units
of reference in the source. Read as *maximum available stabilizer travel in the two ARU gearing
regimes* — that is the only reading consistent with the ARU's documented function (§3). Do **not**
harden this into a JSBSim table until a T1/T2 source confirms which end is TE-up.

Gear geometry (`DCS-FM p.10`): wheel base **3.645 m**, track **3.09 m**; nose unit **2× KT-100
(570×140 mm)** with mudguard; mains **single KT-150 (840×290 mm)**, retract forward into the recesses
above the intake ducts with a **90° rotation**. **Nosewheel steering: ±30° taxi, ±8° takeoff**
(`DCS-FM p.10`; a Russian technical description gives **±31°/±8°**, §6.1). Flap extension **reduces**
the maximum nosewheel steering angle (`DCS-EA p.74`).

---

### 3. ARU-29-2 — the automatic gear-ratio changer (the number-one modelling item)

`DCS-EA` names it only as a cockpit control ("Control of the **ARU**, emergency pumping station and
MRK", left console item 4, `DCS-EA p.52`) and lists a **"FEEL UNIT TAKEOFF–LANDING" annunciator** that
must be lit before takeoff (`DCS-EA p.77`). ED's own "Feel unit control" panel entry says item 4
"FEEL UNIT controls the authority of AFCS in flight handling. **Not implemented yet**"
(`DCS-EA p.55`) — ⚠️ that description is almost certainly an **ED simplification/mislabel**, not real
jet behaviour; the ARU is a *stick-to-stabilizer gearing* device, not an AFCS authority knob.

What the device actually is (§6.2, T4 Russian technical documentation):
- **ARU-29-2** = *автомат регулирования управления*. It **automatically changes the transmission ratio
  in the stabilizer control linkage** *and* in the **pitch-stick loading (feel) channel**.
- Composition: control box + **gear-ratio mechanism (МПЧ)** + **pressure sensors**. It positions the
  МПЧ shaft from the pressure-sensor signals, reports the shaft position to a cockpit indicator, has
  a built-in test and continuous monitoring.
- The dependency is therefore on **dynamic/static pressure** (i.e. IAS and altitude), not on airspeed
  alone — the classic Soviet "q-scheduled gearing" solution to the problem the F-16 solves in software.

**Rebuild note**: this maps cleanly onto a **scheduled gain between stick position and commanded
stabilizer deflection**, plus a **matching feel-spring gradient** so that stick *force* per g stays
roughly constant. The observable side-effect the sim must reproduce: **the same stick displacement
produces much larger surface deflection at low q (takeoff/landing) than at high q** — which is exactly
what the two deflection sets in §2 encode. There is a **discrete "TAKEOFF–LANDING" state** with its own
annunciator, and a documented operational consequence: **drag chute is mandatory when the feel unit is
in the "Heavy" position** (`DCS-EA p.60`) — i.e. the gearing state changes the landing technique.

---

### 4. SOS-3M — AoA/g limiter (the "COC" of the DCS cockpit)

The DCS-EA cockpit calls it **"COC"**: *"Check if **COC** — the flight envelope protection system — is
functional by monitoring the **'COC FAIL'** and **'NO COC RESERVE'** lights extinguish on the
annunciator panel (TLP)"* (`DCS-EA p.73`). **COC is a transliteration of Cyrillic СОС** (система
ограничительных сигналов, "limit-signal system") — it is the SOS-3M, and the two lamps are its
BIT/redundancy state.

Documented function (§6.3, T4 Russian sources, consistent across three of them):
- Prevents departure into stall; **provides the pilot with current and maximum-permissible AoA and
  normal-g**; and **widens the usable AoA range by automatically driving the leading-edge flaps**.
  → The LEF auto-schedule of §2 (AoA > 8.7°) is a **SOS function**, not a standalone flap computer.
- On the MiG-29 and MiG-29UB the **SOS-3M trips at AoA = 26°**.
- The limiter's actuator is a **stick pusher**: SOS-3 "pushes the control stick forward when the
  permissible angle of attack is exceeded".
- Historic threshold progression (T4, one forum-expert account): VVS initially set **22° with no
  override**; then **24° on the 9-12 with a combat override added**; **26° in SOS-3M on 9-12A/9-13**.
  ⚠️ Treat the progression as T4 narrative; treat **26°** as the number to model for 9-12A.

Cockpit instrumentation of the limit (`DCS-EA p.19`, combined AoA/g meter):
| Element | Value |
|---|---|
| AoA scale marker | **15°** |
| AoA scale marker | **25°** |
| AoA red sector | above the 25° marker |
| g scale red mark | **7 g** |
| g pointer | instantaneous g, from an external transducer |
| Reset button for g-index tab | present, **no function in DCS yet** |

⚠️ **Conflict to keep visible**: the cockpit red-lines **7 g** while the FC3 spec table gives
**maximum operational g = 9** (`DCS-FM p.14`) and research gives **9 g below M 0.85 / 7.5 g above**
(§6.4, T4). Most plausible reading: the instrument mark is a *structural/loading-dependent* placard,
the 9 g is the clean-configuration operational limit. Do not silently pick one.

Aural limit warnings exist and are separate from the limiter (`DCS-FM p.105`):
"**Maximum angle of attack**", "**Maximum G**", "**Critical speed**" (max speed *or* stall speed).

---

### 5. AFCS — SAU-451 (**FULL**)

Designation: **SAU-451-02** per `DCS-FM p.34`; Russian technical sources give **SAU-451-03** and the
Lazur-equipped fit **SAU-451-04** (§6.5) — variant/blocks differ, function does not.
Three-channel automatic control system. Purpose: *"automatic and director control at the most
important stages of flight, and improved stability and controllability … with manual control over the
entire operational altitude range, airspeed and angle of attack"* (`DCS-FM p.34`).

#### 5.1 Mode list and panel
Panel, top to bottom (`DCS-EA p.64`): **DAMPER · AUTO RECOVER · ALT HOLD · ATT. HOLD · APPROACH ·
MISSED APPROACH** (last one *not implemented* in DCS).

| Mode | Function | Engage conditions | Exit / notes |
|---|---|---|---|
| **DAMPER** (ДЕМП) | Damps short-period oscillations in all three axes | Engages **automatically whenever any other AFCS mode is selected** | "In most cases of operations it must be enabled" (`DCS-EA p.64`) |
| **Attitude hold** (ATT HOLD) | Holds pilot-set bank, pitch **and heading** | Aircraft must be **trimmed** first | Heading is **not** stabilised for bank **70–80°**. If **pitch < 40°**, roll is **zeroed** and heading held. Heading adjustment via aileron/stabilizer trim |
| **Baro altitude hold** (ALT HOLD) | Holds barometric altitude | Requires **vertical velocity ≈ 0** (flight-path angle **< 5°**); requires ATT HOLD active (`DCS-FM p.35`) | If **roll < 7°** → roll zeroed + heading held. If **roll 7–50°** → roll *stabilised* (turn held). Cancelled by RESET or by Level-to-horizon |
| **Ground Collision Avoidance / AUTO RECOVER** | Pulls up from the preset dangerous altitude, then levels | **bank ≤ 30°**, **flight-path angle ≤ 8°**, **altitude 300–500 m**, **weight-off-wheels** | Below the set minimum radar altitude → automatic climb at **+8° flight-path angle, zero bank**; above the danger altitude → auto transition to level flight; then at **bank < 7° and pitch < 5°**, after **4–5 s**, engages **baro alt hold**. **Pilot input cancels the mode**; when the input stops, the level-off resumes |
| **Level-to-horizon** ("Приведение к горизонту") | Recovery to level flight from any attitude | none stated (the one mode that does **not** require prior trim) | From bank **> 80°** it first reduces bank to **80°**, *then* raises pitch. At **bank 7° / pitch 5°** → baro alt + heading hold. **Angular rate 40–45 °/s**, acceleration envelope **−1 g … +4.5 g**. Trim can bias the result |
| **APPROACH** | Director control on the landing approach | see `navigation.md` §4 | Directors on ADI/HSI |
| **MISSED APPROACH** | Traffic re-entry logic | — | **Not implemented** in DCS-EA (`DCS-EA p.64`); the *navigation-computer* side of it **is** (`navigation.md`) |

All mode logic above: `DCS-FM p.34–35`.

#### 5.2 Engage/disengage protocol
- **Trim before engaging** — mandatory for every mode except Level-to-horizon (`DCS-FM p.34, p.35`).
- **RESET / СБРОС button** (stick-mounted "AFCS MODES OFF", `DCS-EA p.68` item 2): short press cancels
  the currently active mode; **held > 3 s it also disables DAMPER and Ground Collision Avoidance**, and
  it resets an AFCS failure (`DCS-FM p.35`).
- The stick also carries a dedicated **"Autopilot cut-off button"** (`DCS-EA p.69` item 9) and a
  **"Levelling button"** (item 6 → Level-to-horizon).
- **BIT**: on cold start, with electrical power and hydraulic pressure present, the AFCS runs a
  **3-minute** self-test. "DAMPER DISABLED" (ДЕМПФЕР ВЫКЛ) is lit throughout; in the second half the
  DAMPER push-light **flashes** and **the stick moves by itself**. **No AFCS mode can be engaged during
  BIT** (`DCS-FM p.35`, `DCS-EA p.73`).
  - Failure recovery (`DCS-EA p.74`): centre the stick with trim until pitch and aileron neutral-trim
    lamps illuminate, then short-press AFCS MODES OFF → BIT restarts.
  - ⚠️ *"Do not move the flight stick until the end of BIT — it may cause fail of AFCS BIT"* — this is
    a real interlock, worth reproducing as a precondition/rejection case.

#### 5.3 Trim
`DCS-FM p.33`:
| Axis | Trim authority |
|---|---|
| Pitch | **38 % of aft stick travel, 50 % of forward stick travel** (asymmetric) |
| Roll | **50 % each side** |
| Rudder | **50 % each side** |

Three neutral-trim annunciators on the lower main panel: **ТРИММЕР СТАБИЛ. / ТРИММЕР ЭЛЕРОН /
ТРИММЕР РП** (stabilizer / aileron / rudder). Rudder trim is a **three-position switch on the left
console** (`DCS-EA p.64`); pitch/roll trim is a **four-position hat on the stick** (`DCS-EA p.68`).

---

### 7. Secondary controls (flaps, slats, speedbrakes) — logic worth reproducing

`DCS-EA p.57`, verbatim logic:
- Three pushbuttons on the left console: two **FLAPS DOWN** (**TAKEOFF** and **LANDING** positions) and
  one **FLAPS UP**. Either DOWN button extends **flaps *and* slats**.
- **FLAPS UP** retracts flaps and slats **only if the gear is up**.
- Slats retract manually via UP **when WoW is active**, or **automatically when the gear retracts in
  the air**.
- Takeoff with UP previously selected: **slats extend automatically the moment the right main gear
  loses ground contact**.
- **After the nose gear retracts the slats become fully automatic**, scheduled on **angle of attack and
  Mach number**.
- **Speedbrakes**: spring-loaded switch on the right throttle grip, auto-returns to IN. Full extension
  ≈ **3 s**. **Blow-back above 540 kts.** **Inoperative with the centreline tank fitted or gear down.**
  **Retract automatically on general electrical failure.**

---

### 8. Variant notes (no parallel description)
- **9-13 (MiG-29S/Fulcrum-C)**: same RD-33, same 9 g, same airframe geometry; **empty weight
  10,900 → 11,200 kg**, **max TOW 18,100 → 18,480 kg** (`DCS-FM p.14`); enlarged dorsal spine adds
  internal fuel (see `engines-fuel.md`). SOS-3M threshold **26°** applies to both 9-12A and 9-13 (§6.3).
- **9-12 first series** carried **ventral tail fins** and **20 % smaller vertical fins**
  (`DCS-FM p.10`) — a genuine aerodynamic difference if an early-series model is ever built.
- **SMT / MiG-35**: digital FBW and a different control law entirely; **out of scope** — anything in
  this file about the mechanical run stops applying there.

---

---

## State

**Nothing in this file is implemented.** FlightBox has no MiG-29 module, no
`sim/src/modules/mig29/` and no JSBSim MiG-29 model. The airframe exists only as a **spec-first
contract** — [`../flightbox/aircraft/mig29.md`](../flightbox/aircraft/mig29.md), whose own status
line reads *"spec only. Nothing is built."* Everything below is therefore a **forward commitment**,
not a description of code.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for the control system; it is complete as far as the two DCS manuals reach |
| **R6** — asymmetric weapons | nothing directly; the g-limit and AoA ceiling here bound what a launch envelope may assume |
| **R7** — enemy units at BVR scale | the SAU-451 mode set is the shape of the future `FBAutopilot` override; the SOS-3M limiter is the ceiling the pilot may command |
| **R8** — JSBSim model | `<flight_control>` is built from §2 (surface areas, deflection limits), §3 (the ARU gearing schedule — the number-one open item), §4 (SOS-3M) and §7 (flaps/slats/speedbrake logic). **No `fcs/fbw-override` counterpart exists**: the gearing schedule *is* the model |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../flightbox/roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — the itemised list below is the file's own; it is unchanged and keeps its
section number so existing cross-references stay valid. Beyond it: the **GAF T.O. 1F-MIG29-1**
(German Air Force MiG-29G flight manual, ~454 pp, English, USAF format) is the one acquisition that
would close most of §9 at T1 level — it was **not available to this pass** (see `PROGRESS.md`).

**Implementation gaps** — none can be stated yet: nothing is built (see State).

### 9. Open gaps (honest)
1. **ARU gear-ratio schedule** (ratio, and feel-spring rate, vs dynamic pressure/altitude) — not found
   in any public source. Blocks a faithful pitch-response model.
2. **Stick force gradients** (N/g, N/cm) in either ARU regime — not found.
3. **Rudder deflection limit** and **aileron/stabilizer roll-mixing ratio** — the FM gives aileron and
   stabilizer deflections separately but never the differential-stabilizer roll authority or how the
   two are blended.
4. **SAU-451 damper gains / authority limits** (series-actuator authority in % of surface travel) —
   not found. `DCS-FM` describes *what* each mode does, never *how hard* it can push.
5. **Whether the 15°/35° vs 5°45′/17°45′ stabilizer pair is TE-up or TE-down referenced** (§2 flag).
6. **SOS-3M override mechanics**: force threshold or displacement threshold? Sources say only "pull
   harder". A modelled soft limiter needs one of the two.
7. **Roll rate, sustained/instantaneous turn rate, corner speed** — deliberately *not* recorded here;
   they belong to `flight-model-spec.md` (parallel agent) and must come from measurement, not from
   a marketing figure.

---

## Knowledge

### 6. Technical depth (researched — outside the two DCS manuals)

#### 6.1 Hydraulics and actuators
- **Two independent hydraulic systems**, **NP-103A** variable-displacement pumps on the engine
  accessory gearboxes, **207 bar (3,000 psi)**, **80 litres** of fluid.
  - **Main system**: one chamber of *each* control-surface actuator, **leading- and trailing-edge
    flaps**, **stick pusher**, **artificial feel unit**, landing-gear extension/retraction, nosewheel
    steering, APU exhaust door.
  - **Back-up system**: the **second chamber** of each control-surface actuator **and the stick
    pusher**; can be driven by an emergency **NS-58** pump.
  - Source: Jane's-derived entry mirrored at `janes.migavia.com/rus/mig/mig-29.html` — **T3**.
  - **Rebuild consequence**: loss of one hydraulic system halves actuator authority but does not remove
    it — matching FlightBox's existing degraded-FLCS convention (`CLAUDE.md`: FLCS authority ×0.5 for
    one of two hydraulic systems). The MiG-29 justifies that same ×0.5 *from its own architecture*.
- **Booster designations**: **RP-260A** (pitch), **RP-280** (roll), **RP-270** (yaw); irreversible.
  Nose/main struts are twin-chamber oleos. Nosewheel steering **±8° takeoff/landing, ±31° taxi**.
  Source: `military.wikireading.ru/3814` (Техническое описание самолёта МиГ-29, изделие 9-12А) — **T4**.
- Control run described as **"жёсткая"** (rigid/push-rod), with irreversible hydraulic actuators —
  `ot-a-do-ya.org` — **T4**. Two independent T4 sources agree; no T1–T3 contradiction found.

#### 6.2 ARU-29-2
Function, composition and the pressure-sensor dependency as stated in §3. Source: a Russian technical
document indexed as "MiG 29, АРУ-29-2" (scribd) plus the wikireading technical description — **T4**.
No public gear-ratio *schedule* (ratio vs q) was found. **TODO** — this is the single most valuable
missing number for a faithful MiG-29 flight model.

#### 6.3 SOS-3M
- **26° AoA trip** on MiG-29/MiG-29UB; stick-pusher actuation; LEF control; pilot-information function.
  Sources: Russian-language technical summaries — **T4**, three independent sites in agreement.
- Threshold history (22° → 24° + override → 26° in SOS-3M) and the "override by pulling harder"
  behaviour: secretprojects.co.uk thread "MiG-29 Supermanoeuvrability" — **T4** (forum expert,
  uncorroborated; the *behaviour* is corroborated by ED's own DCS "stick deflection limiter override"
  feature threads).
- "The MiG-29 has been cleared to an AoA of up to **45°**" (vs F-16A's 25°) — toad-design MiG Alley
  comparison article — **T4**, and almost certainly a *demonstrated/flight-test* figure, not an
  operational limit. Do **not** use as a modelling limit.

#### 6.4 Envelope numbers
| Quantity | Value | Source / tier |
|---|---|---|
| Max operational g | **9** | `DCS-FM p.14` (spec table) |
| g vs Mach | **9 g below M 0.85**, **7.5 g above** | toad-design — **T4** |
| Cockpit g red mark | **7 g** | `DCS-EA p.19` |
| SOS-3M AoA trip | **26°** | **T4** (§6.3) |
| Max speed, sea level, clean | **1,500 km/h** | `DCS-FM p.14` |
| Max speed, altitude | **2,450 km/h** (M ≈ 2.35) | `DCS-FM p.14`; M 2.35 from wikireading **T4** |
| Service ceiling | **18,000 m** | `DCS-FM p.14` |
| Max climb rate | **330 m/s** | `DCS-FM p.14` (9-12 and 9-13 alike) |
| Speedbrake blow-back | retracts above **540 kts** | `DCS-EA p.57` |

#### 6.5 SAU-451 block variants
SAU-451-**02** (`DCS-FM p.34`) / -**03** (wikireading, T4, "functions above 50–60 m altitude") /
-**04** (the block integrated with the Lazur GCI datalink, see `datalink-gci.md`). The **50–60 m floor**
is a useful rebuild constraint: the AFCS is not a terrain-following system and refuses to work below
that height.

---
