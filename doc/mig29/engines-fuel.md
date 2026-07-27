# MiG-29A (9-12) — Powerplant (2× RD-33), Intakes, Fuel System

**Variant scope**: izdeliye **9-12** (Fulcrum-A). Deltas in §8.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 25–27, 29–30, 50, 52, 58, 72–78. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf`
(**printed page = PDF page − 6**); pages used: 9–11, 14, 28–29, 81–82, 105.
Researched material is in **§6 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **FULL** on cockpit engine instrumentation, start/shutdown/restart procedure and
the fuel-gauge logic (both manuals cover it). **MEDIUM** on engine cycle data and fuel-tank capacities
(researched, multiple T4 sources in agreement). **SHALLOW** on spool times, AB time limits, altitude
thrust lapse, and the *actual* tank feed sequence — see §7.

---

## Spec

### 1. Powerplant at a glance

Two **Klimov RD-33** afterburning turbofans, each with **its own turbine starter (VK-100)** — so
**individual or simultaneous start of both engines is possible** (`DCS-FM p.81`).

| Parameter | Value | Source |
|---|---|---|
| Static thrust, **full afterburner**, per engine | **8,300 kgf** (81.4 kN / 18,300 lbf) | `DCS-FM p.14`; T4 cross-check |
| Static thrust, **maximum military**, per engine | **5,040 kgf** (49.4 kN / 11,110 lbf) | `DCS-FM p.14`; T4 cross-check |
| Combined military / AB thrust | **10,080 kgf / 16,600 kgf** | derived from the above |
| Dry mass, per engine | **1,055 kg** | T3/T4 (§6.1) |
| Bypass ratio | **0.49** | T4 (§6.1) |
| Overall pressure ratio | **21 : 1** | T4 (§6.1) |
| Air mass flow | **75.5 kg/s** | T4 (§6.1) |
| Turbine inlet temperature | **1,407 °C (1,680 K)** | T4 (§6.1) |
| Configuration | 4-stage LP fan + 9-stage HP compressor, annular combustor (24 injectors), 1-stage HP + 1-stage LP turbine, afterburner | T4 (§6.1) |

**Thrust-to-weight**, useful as an FDM sanity check (T4, §6.4): ≈ **1.5** empty, ≈ **1.16** at full
internal fuel, ≈ **1.0** with a typical air-to-air load. Normal takeoff weight **15,300 kg**, maximum
**18,100 kg**, empty **10,900 kg** (`DCS-FM p.14`).

---

### 2. Cockpit engine instrumentation (**FULL**)

| Instrument | Designation | Range / marks | Source |
|---|---|---|---|
| RPM tachometer, both engines on one dial | **ITE-2TB** | **0–110 %**, two pointers (L and R), **1 % accuracy** | `DCS-EA p.25` |
| Exhaust gas temperature, one per engine | **ITG-1** (magnetoelectric millivoltmeter) | **200–1,100 °C**, digitised at 200/400/600/800/1000, tick 100°. **Readings ×100.** Rim carries a **yellow** and a **red** mark | `DCS-EA p.27`, `DCS-FM p.29` |
| EGT rim marks meaning | — | (a) **max permissible temperature during engine start**, ground *and* in flight; (b) **max permissible at maximum, afterburner and transient regimes** | `DCS-EA p.27` |
| Afterburner lamps | "**ФОРСАЖ**" (one per engine) | Lit at full AB | `DCS-FM p.28` |
| Fuel flow / range computer | **ISTR4** | see §4 | `DCS-EA p.26` |
| Intake ramp position | Ramp position indicator, L and R marker areas + manual setting handles | see §3 | `DCS-EA p.30` |
| Combined pressure indicator | **IKG-1** | Hydraulics **0–300 kp/cm²**, pneumatics **0–260 kp/cm²**; scales read **bottom-up** | `DCS-EA p.29` |

⚠️ **Conflict**: `DCS-FM p.28` states *"Full afterburner power (reheat) is shown as 100 %"* on the
tachometer, while `DCS-EA p.25` gives the scale as **0–110 %** and `DCS-EA p.73` reports **idle at
58–72 %**. The FC3 manual is the simplified module; treat **DCS-EA** as authoritative for the 9-12.
Model RPM as a *shaft-speed percentage*, not a power setting: max mil and max AB share the same
physical RPM, the AB adds fuel downstream.

Hydraulic pressure scale detail worth reproducing (`DCS-EA p.29`): red 0–100, yellow 100–150, brown
150–220, yellow 220–240, red 240–300 kp/cm², with labelled indices **Pак** (accumulator pressure),
**Qм** (max pump delivery), **Q0** (zero pump delivery).

---

### 3. Air intakes — a genuinely unusual system

The MiG-29 has **two intake paths per side** (`DCS-FM p.11`):
- **Main intakes**: Soviet-type supersonic, **external compression**, **adjustable** (horizontal ramp
  wedge), with a **boundary-layer bleed system**.
- **Upper (dorsal) louvred intakes** in the LEX: **used while the aircraft is moving on the ground**,
  operating **at speeds up to 200 km/h**. The main intakes are closed by FOD doors on the ground.

Operationally documented behaviour:
| Event | Value | Source |
|---|---|---|
| Ramps close during start | at **35 % RPM** | `DCS-EA p.73` |
| Ramp function check | push a throttle to **80–90 % RPM** → "**LH INLET CHECK**" / "**RH INLET CHECK**" lamps illuminate; back to idle → out | `DCS-EA p.74` |
| Intake changeover on the takeoff roll | **thrust increase observed when the intake system opens, at about 108 kts** | `DCS-EA p.78` |
| Handling consequence | **nose-lowering tendency during intake-duct opening** — *delay rotation until it has occurred* | `DCS-EA p.78` |
| Emergency | **emergency ramp retraction switches, separate for L and R** (`DCS-EA p.58`, not implemented in DCS) | `DCS-EA p.58` |
| Mechanical-devices indicator | shows the **square-hole grids** (FOD screens) L and R alongside gear/flaps/airbrakes | `DCS-FM p.24` |

**Rebuild note**: the ~108 kt intake changeover is a **discrete thrust step plus a pitching-moment
step** on every takeoff. It is a real, procedure-visible event and belongs in the takeoff phase of a
MiG-29 pilot module, not in an aero table.

---

### 4. Fuel system

#### 4.1 Tanks (**researched — T4, three sources in agreement**)
| Tank | Capacity | Position |
|---|---|---|
| **№1** | **650 L** | fuselage, forward (behind the equipment bay) |
| **№2** | **870 L** | fuselage |
| **№3** (integral caisson) | **1,810 L** | fuselage, **the aircraft's main fuel source** (`DCS-FM p.9`) |
| **№3A** ×2 | **155 L each** (310 L) | flanking the engine bays |
| **Wing integral** ×2 | **330 L each** (660 L) | wing inter-spar |
| **Total internal** | **4,300 L** | — |
| **Centreline drop tank** | **1,500–1,520 L** | see conflict below |
| Underwing ferry tanks | **1,150 L each** | later variants / ferry fit only |

Sources: `military.wikireading.ru/3814` (per-tank), `ot-a-do-ya.org`, `ru.wikipedia.org` (total 4,300 L,
"five fuselage and two wing tanks") — all **T4**, mutually consistent; per-tank sum **= 4,300 L**
exactly, which is a good internal-consistency check. `DCS-FM p.9` corroborates the *layout* (tanks 1,
2, integral 3, two 3A) without numbers.

⚠️ **Conflicts to keep visible**:
- **Internal fuel mass**: **3,200 kg** (sirviper, T4) vs **≈3,375 kg** (flyandwire DCS measurement,
  T4) vs the DCS-EA preflight check **"fuel quantity 3,100…3,700 kg"** (`DCS-EA p.72`). All three are
  compatible with 4,300 L at kerosene densities 0.75–0.80 kg/L. **Use volume, derive mass.**
- **Drop tank**: 1,520 L (wikireading), 1,500 L (ruwiki), **1,400 L in the DCS module** (flyandwire).
  The 1,400 L value is likely an **ED modelling choice**, not the real tank.

#### 4.2 Cockpit fuel indication — ISTR4 (`DCS-EA p.26`)
| Element | Behaviour |
|---|---|
| Total fuel gauge | reads in **hundreds of kilograms** |
| **Minimum fuel marker** | **550 kg** |
| Empty lights | "**CL**" external tanks · "**WING**" wing tanks · "**3**" tank 3 · "**1**" tank 1 |
| Switch **T – R** | selects **direct in-tank measurement** vs **flow-computed** remaining fuel |
| Switch **TAC – OPT** | estimated-range computation for **actual** flight regime (TAC) vs **max-conservation** regime (OPT) |
| Estimated distance indicator | in **nautical miles** in the English cockpit |

**Feed sequence — inference, not documented**: the *existence and order* of the four empty lights
(CL → WING → 3 → 1) is the only feed-order evidence in either manual. The Russian technical description
adds that the **navigation reserve of 550 (units) sits in tank №2, which is used last** (T4, and note
that source says **litres** while the cockpit marker says **kilograms** — ⚠️ unit conflict, same
concept). A defensible model: **externals → wings → 3/3A → 1 → 2 (reserve)**. Flagged as inference;
do not present as documented.

#### 4.3 Fuel-related warnings
| Trigger | Annunciation | Source |
|---|---|---|
| Enough fuel only to reach the nearest friendly airbase | voice "**Bingo fuel**" | `DCS-FM p.104` |
| Fuel at 1,500 / 800 / 500 (pounds or litres) | voice "**Fuel 1500 / 800 / 500**" | `DCS-FM p.104` |
| VIWAS self-test response | answers "**BINGO BINGO**" if healthy | `DCS-EA p.73` |
| Preflight fuel check | **3,100…3,700 kg** | `DCS-EA p.72` |

⚠️ The FC3 "Fuel 1500/800/500" thresholds are stated in *"pounds/litres"* — an ED localisation artefact.
Treat as **module behaviour**, not jet behaviour.

#### 4.4 Emergency fuel/engine controls (`DCS-EA p.58`, panel exists, *not available* in DCS)
Emergency fuel shut-off valve toggles (per engine, under guards) · **KSA fire-extinguisher switch** ·
**afterburner emergency shutdown switch** · generator-drive emergency shutdown · **air-to-air engine
start switches, separate per engine** · emergency ramp retraction (§3).

---

### 5. Start, shutdown, restart (**FULL**)

#### 5.1 Ground start (`DCS-EA p.72–73`)
Preconditions: ground electric power on; **RECORD** switch on; canopy locked; ejection handle ARMED;
**Start-Up Mode Switch = "START BOTH"**; both throttles at **IDLE**.

Engine start control panel (`DCS-EA p.50`):
- **APU MODE switch**, three positions: **START** (normal, under a guard) · **engine cold crank**
  (motoring without ignition) · **APU cold crank**.
- **GND START button** — executes whatever the APU MODE switch selected.
- **Three-position engine selector**: left / right / **both in sequence**.

Sequence to monitor after pressing **GND START**:
1. **RH/LH ENG START lamps light in sequence — the right engine leads.**
2. Engine RPM rises.
3. **BOTH HYDRO FAILURE light extinguishes**; hydraulic pressure rises.
4. **Ramps close at 35 % RPM.**
5. RH/LH ENG START lamps extinguish.
6. **EGT within the yellow sector limits.**
7. **RPM settles between 58…72 % at idle.**

FC3 equivalent (`DCS-FM p.81`): electric power on → throttle IDLE → per-engine start; "**ЗАПУСК**"
lamp lights. **After start, with power and hydraulic pressure present, the AFCS begins its 3-minute
BIT** (see `flight-controls.md` §5.2).

#### 5.2 Shutdown (`DCS-FM p.82`)
Throttle to the **IDLE stop**, then the per-engine cutoff command.

#### 5.3 In-flight restart (`DCS-FM p.82`)
Throttle to **IDLE**, then to the **"СТОП" (STOP)** position (cutoff); then move the throttle **off
STOP** and command start. ⚠️ No relight envelope (altitude/airspeed window, windmill vs starter-assist)
is given in either manual — §7.

---

### 8. Variant notes
- **9-13 (MiG-29S)**: same RD-33 and the same thrust ratings (`DCS-FM p.14`). **Enlarged dorsal spine
  raises internal fuel to ≈4,540 L** (T4); empty weight **+300 kg**, max TOW **+380 kg**
  (`DCS-FM p.14`).
- **RD-33 series 3 / RD-33MK**: higher thrust and much longer life — **out of scope** for a 9-12 build.
- **Smoke**: the early RD-33's visible exhaust smoke is a well-known type trait but no quantitative
  source was found; it is a *rendering* concern, noted here so it is not forgotten.

---

## State

**Nothing in this file is implemented.** FlightBox has no MiG-29 module, no
`sim/src/modules/mig29/` and no JSBSim MiG-29 model. The airframe exists only as a **spec-first
contract** — [`../flightbox/aircraft/mig29.md`](../flightbox/aircraft/mig29.md), whose own status
line reads *"spec only. Nothing is built."* Everything below is therefore a **forward commitment**,
not a description of code.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for powerplant and fuel |
| **R6** — asymmetric weapons | nothing directly |
| **R7** — enemy units at BVR scale | the intake changeover and the idle/climb RPM points are procedure-visible, so a pilot phase machine needs them before it can taxi or take off |
| **R8** — JSBSim model | two `<turbine_engine>` blocks from §1 (thrust, cycle data) and §4 (tank-by-tank capacities, feed order); the **spool times are the blocking gap** for any throttle loop |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../flightbox/roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — the file's own itemised list follows, section number unchanged. The
**GAF T.O. 1F-MIG29-1** would close most of it at T1 level and was not available to this pass
(`PROGRESS.md`).

**Implementation gaps** — none statable yet: nothing is built (see State).

### 7. Open gaps (honest)
1. **Spool times** (idle→mil, mil→max AB, and the reverse) — **not in either manual, not found in
   research**. This is the single most important missing engine number for a pilot module: every
   throttle decision loop depends on it.
2. **Afterburner time limits** (continuous AB minutes at SL / at altitude) — not found.
3. **Altitude/Mach thrust lapse tables** — not found; only static sea-level ratings exist publicly.
4. **In-flight relight envelope** (altitude band, minimum IAS, windmill vs starter-assisted) — the
   restart *procedure* is documented, the *envelope* is not.
5. **Idle RPM in flight** vs on the ground — only the ground figure (58–72 %) is documented.
6. **EGT numeric limits**: the manuals only ever say "within the yellow sector" / "within the brown
   sector" (`DCS-EA p.73, p.78`). The actual °C thresholds for start, max, AB and transient are **not
   printed anywhere in either manual**. ⚠️ A JSBSim engine model needs them; treat as TODO, do not
   invent.
7. **Tank feed sequence** — inferred only (§4.2).
8. **Fuel-flow rates** at defined throttle settings — the charts in the flyandwire article are images;
   no numeric table was recoverable.

---

---

## Knowledge

### 6. Technical depth (researched)

#### 6.1 RD-33 cycle data
- **49.43 kN dry / 81.40 kN AB**; **SFC 75 kg/(kN·h) dry, 188.1 kg/(kN·h) AB**; **OPR 21:1**; **BPR
  0.49**; **airflow 75.5 kg/s**; **TIT 1,407 °C**; twin-spool, 4-stage LP fan on a single-stage LP
  turbine, 9-stage HP compressor on a single-stage HP turbine, annular combustor.
  Sources: HandWiki *Klimov RD-33*, Military-History Fandom, toad-design MiG Alley — **T4**, mutually
  consistent, all ultimately tracing to Klimov published data (so effectively **T2-derived**).
- **Conflicting SFC set**: **78.5 kg/(kN·h) dry, 209 kg/(kN·h) AB** and **max thrust 50.4 kN / AB
  81 kN** — `military.wikireading.ru` — **T4**. The two sets differ ~5 % dry and ~10 % AB. ⚠️ Pick one
  and record which; do not average.
- Physical: **length 4,229 mm, diameter 1,040 mm, dry mass 1,055 kg** — **T4**.
- **Time between overhaul ≈ 350 h** on early RD-33 (sirviper, **T4**); later RD-33MK life **4,000 h**
  (**T4**). Not a flight-model input, but it explains the type's operational reputation.

#### 6.2 Endurance (T4, DCS-measured — flag as *module* data)
- Full afterburner endurance ≈ **20–30 min** depending on altitude and configuration.
- Military power at **30,000 ft with the centreline tank: > 1 hour**.
- Source: flyandwire.com "MiG-29 9.12A – TWR, Fuel & Performance" (Oct 2025) — **T4**, and explicitly
  measured *inside DCS*, i.e. it validates the ED module, not the jet.

#### 6.3 Range/ferry (`DCS-FM p.14`, spec table)
| Quantity | 9-12 | 9-13 |
|---|---|---|
| Flight range, no external tanks | **1,430 km** | 1,500 km |
| Ferry range | **2,100 km** | 2,900 km |
Low-altitude range **700 km** (`ot-a-do-ya.org`, **T4**).

#### 6.4 Thrust-to-weight (T4, flyandwire, DCS-measured)
≈1.5 empty · ≈1.16 full internal fuel · ≈1.0 typical A-A load (2× R-27R + 4× R-60M ≈ 680 kg ordnance).

---
