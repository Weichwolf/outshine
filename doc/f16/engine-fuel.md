# F-16C Engine & Fuel Management

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 7 — Engine & Fuel Management, pp. 134–152.

## Spec

### Engine — General Electric F110-GE-129

Afterburning turbofan; powers >70% of USAF F-16C/D. (Early F-16 used P&W F100; AFE program 1984 →
GE F110. F110-GE-100 = 28,000 lbf; **F110-GE-129 = 29,000 lbf / 129 kN**.)

- Twin-spool: 3-stage fan, 9 HP-compressor stages, 1 HP-turbine stage, 2 LP-turbine stages, annular
  combustor, augmentor (afterburner).
- Max-power pressure ratio **30.7**; thrust-to-weight **7.29**.
- **No auto-throttle** on the F-16.

### Engine instruments & warning lights

| Indicator / Light | Meaning |
|---|---|
| Fuel Flow (lb/hr) | Fuel consumption |
| Engine RPM (%) | Core RPM |
| FTIT (×100 °C) | Fan Turbine Inlet Temperature |
| Nozzle Position (% open) | Exhaust nozzle |
| Oil Pressure (psi) | Engine oil |
| **HYD/OIL PRESS** light | Oil < ~10 psi for 30 s (out > 20 psi); or hydraulic A **or** B < 1000 psi |
| **ENGINE** light | RPM below IDLE, or ~2 s after FTIT > 1100 °C (overtemp/flameout) |
| **ENG FIRE** light | Engine fire detected |

### Engine limits

**On ground:**
| Condition | FTIT (°C) | RPM (%) | Oil (psi) |
|---|---|---|---|
| Engine start | 935 | — | — |
| Idle | 650 | — | ≥15 |
| MIL/AB | 980 | 108 | 25–65 |
| Transient | 980 | 109 | 25–65 |
| Fluctuation | ±10 | ±1 | ±5 (nozzle ±2%) |

- At MIL+, oil pressure must rise ≥10 psi above idle oil pressure. Cold start: oil may read 100 psi up to 2 min.

**In flight:** same FTIT/RPM/oil limits. Zero oil pressure allowable up to **1 minute at < +1 G**.

### Throttle quadrant (detents, aft → fwd)
OFF → IDLE → MIL (Military) → AB (afterburner on) → MAX AB.
- Cutoff release: `RSHIFT+HOME` → IDLE, `RSHIFT+END` → OFF.
- Afterburner engaged by throttling **past the MIL gate**. No cockpit "AB" light — monitor **fuel flow +
  nozzle position** (fuel flow rises dramatically).

### Engine control modes (ENG CONT switch)
- **PRI (Primary)**: unrestricted operation across the envelope.
- **SEC (Secondary)**: 70–80% of normal MIL thrust; manual or auto (DEEC-detected failure). Protects
  against exceeding limits; **closes nozzle, inhibits afterburner**.
- **AB RESET** switch: attempts to clear DEEC faults (NORM = de-energized; ENG DATA = record to EDU).
- **MAX POWER / VMax switch: inoperative** on the F110-GE-129 (was a P&W "Hail Mary").

### EPU — Emergency Power Unit
Hydrazine-powered; supplies emergency hydraulic + electrical power to flight controls when bleed air is
insufficient. Runs **~10–15 min** on fuel. (FBW is not mechanically linked to the stick → EPU keeps the
jet flyable after engine loss.) Safety pin removed by ground crew (not modelled in DCS).

**EPU switch:**
- **NORM**: armed for auto operation; runs automatically on flameout. With Weight-on-Wheels + throttle
  OFF, will not activate when generators drop.
- **ON**: commanded to run regardless of failures.
- **OFF**: on ground prevents/terminates EPU; in flight (if OFF since takeoff) terminates/inhibits —
  except main+standby generator failure if the switch was ever cycled to NORM since takeoff.

Lights: HYDRAZN (commanding hydrazine / primary speed-control failure), AIR (commanded to run with pin
removed), RUN (turbine in range + EPU hydraulic > 2000 psi). Fuel (hydrazine) quantity in %.

### Engine relight

**Windmilling** (enough altitude/airspeed):
1. Flameout → EPU auto-runs (NORM), ~10 min. 2. ENGINE FEED — NORM. 3. Throttle IDLE → CUTOFF
(`RSHIFT+END`). 4. Nose down to windmill compressor **> 20–25% RPM**. 5. Throttle OFF → IDLE at
20–25% RPM. 6. Confirm RPM + FTIT rise. 7. Above **60% RPM**, throttle up, resume.

**JFS-assisted** (insufficient altitude/airspeed): steps 1–3 as above, then set within JFS envelope
(**altitude < 20,000 ft, airspeed < 400 kts**) → JFS switch AFT to **START2** → JFS RUN light within 30 s
→ engine spools → at 20% RPM move throttle OFF → IDLE → light-off within 10 s → above 60% RPM resume.

### Fuel system
6 internal tanks: left wing, right wing, aft fuselage (A1), aft-fuselage reservoir, forward fuselage
(F1/F2), forward-fuselage reservoir.

External drop tanks:
| Tank | Capacity | Weight |
|---|---|---|
| Centerline (fuselage) TK300 | 300 gal | 2,040 lb |
| Wing TK370 (×2) | 370 gal each | 2,516 lb each |

**Fuel Quantity Selector** knob: TEST (both pointers 2000, total 6000) · NORM (AL = aft-left reservoir +
A1; FR = fwd-right reservoir + F1/F2) · RSVR (reservoir tanks) · INT WING · EXT WING · EXT CTR.

- **External Fuel Transfer switch**: NORM (centerline empties first) / WING FIRST.
- **Engine Feed Selector**: fuel gravity-feeds (engine won't starve with pumps OFF); pumps prevent
  negative-G starvation + allow CG balance. OFF / **NORM** (all pumps, CG auto) / AFT (aft→engine, CG
  forward) / FWD (fwd→engine, CG aft).
- **Fuel Master switch**: guarded MASTER; OFF closes shutoff valve.
- **Tank Inerting switch**: pumps Halon 1301 to reduce tank pressure / fire risk (battle damage).
- **Air Refueling Door switch**: also sets FLCS gains to takeoff & landing mode.

### BINGO / JOKER fuel
- **BINGO**: fuel level that triggers immediate RTB (covers return leg + approach + alternate + emergency
  reserve). Set: LIST → ICP "2" (BNGO DED page) → enter value → ENTR → FUEL QTY SEL to NORM (compute on
  fuselage fuel). Below BINGO: **FUEL caution on HUD + VMS "BINGO, BINGO"**.
- **JOKER**: warning above BINGO (typically **+1000 lb** = ~1 min AB combat time).

## State

**Read the first line of this file against the model first:** this reference documents the
**F110-GE-129**; the pinned JSBSim F-16 flies an **F100-PW-229** (`f16/engine/F100-PW-229.xml`). Where a
number here disagrees with the model, the model is the reference (Prinzip 5) — the engine as actually
flown, with its thrust tables, spool law and the throttle→afterburner mapping, is written down in
[`flight-model.md`](flight-model.md) §5.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Thrust, spool dynamics, afterburner | **built — by JSBSim** (`FGTurbine`): three thrust tables, the N2 seek law, and the throttle-norm→AB detent mapping | [`flight-model.md`](flight-model.md) §5.2–§5.5 |
| Throttle as a pilot control | **built** — `fcs/throttle-cmd-norm` through `FBAirframeControls`; `TakeoffThrottleNorm` = 1.0 is the AB detent | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §3.1 |
| Fuel quantity, tank contents, running dry | **built** — `FBFdm::Get/SetFuel*` over `FGPropulsion`, per tank or as a total distributed proportionally to capacity; fuel starvation is JSBSim's own physics, the adapter only makes it observable and settable | [`../flightbox/sim/fdm.md`](../flightbox/sim/fdm.md) §11 |
| BINGO | **built** — in the warning block, evaluated against the *effective* threshold rather than the entered one | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §6 |
| Engine damage consequences | **built** — a degraded engine loses the afterburner (throttle cap), a failed one is cut off, both through `FBFdm` into the physics | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §8 |
| Engine instruments (FTIT, oil, nozzle, RPM), ENG CONT PRI/SEC, EPU, relight, JFS, fuel transfer/tank sequencing, JOKER | **not implemented** — `FBPropulsionSystem` is the reserved slot for exactly this and is still a NoOp | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §10 |

## Gaps

**Source gaps** (this file vs. its sources)
- **Engine mismatch, unresolved on purpose:** the guides describe the F110-GE-129 (Block 50), the pinned
  model carries an F100-PW-229. Both stand; neither is edited to match the other.
- ED's engine/fuel chapter was **not** cross-checked into this file (PROGRESS.md Pass 2, priority-4
  sweep) — this file is Chuck-only plus research.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* thrust and spool behaviour (as the model's engine), throttle control, fuel quantity and
  starvation, BINGO, damage-driven thrust loss.
- *Partially:* the fuel *system* — a total quantity exists and is distributed proportionally, but there
  is no transfer logic, no tank sequencing, no CG effect from fuel state beyond what the model does.
- *Not at all:* engine instrumentation and limits as monitored quantities, ENG CONT modes, EPU,
  relight, JFS, fuel-system switching, JOKER, engine fire/overheat handling.

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

Real F110-GE-129 engineering data beyond the guide, for a thrust/fuel-flow model. Sources cited inline.

### F110-GE-129 specifications
| Parameter | Value | Note |
|---|---|---|
| Intermediate (MIL) thrust | **17,155 lbf** (76.3 kN) | standard day (GE / Wikipedia) |
| Max afterburner thrust | **~29,500 lbf** (131 kN) | guide says 29,000 lbf / 129 kN — same class |
| Airflow | **270 lb/s** (122 kg/s) | |
| Bypass ratio | **0.76** | low-bypass turbofan |
| Overall pressure ratio | **30.7** | matches guide |
| Thrust-to-weight | **7.29** | matches guide |
| Dry weight | **~3,980 lb** | |
| Architecture | 2-spool: **3-stage fan, 9-stage HP compressor, annular combustor, 1-stage HP turbine, 2-stage LP turbine** | matches guide |
| Control | **FADEC / DEEC** (Digital Electronic Engine Control) | full-authority |
| Service entry | **1992, F-16C/D Block 50** | |

### Modeling implications (for a thrust/fuel model)
- **Two-spool, low-bypass, afterburning turbofan**: dry thrust ~17k lbf, AB roughly **+72%** to ~29.5k lbf.
  The augmentor step is large and non-linear in fuel flow — matches the guide's "fuel flow rises
  dramatically" and the fact there is no discrete AB gauge (monitor fuel flow + nozzle).
- **Nozzle (A8) scheduling**: the exhaust nozzle opens with AB and is scheduled by the DEEC; SEC mode
  closes the nozzle and inhibits AB (guide) — a nozzle-position state is needed to reproduce the gauges
  and the SEC-mode thrust ceiling (70–80% MIL).
- **DEEC/FADEC**: closed-loop control of fuel flow, nozzle, and variable stators; the PRI/SEC split and
  AB-reset logic in the guide are DEEC behaviors. A faithful model schedules thrust on throttle, Mach,
  and altitude (air density) with spool-up lag (the AeroBench/Stevens-Lewis F-16 uses a first-order power
  lag `pdot` with `tgear` throttle gearing — see `flight-controls-flcs.md` engine note).
- **Idle/limits** (guide): FTIT ≤ 650 °C idle / ≤ 980 °C MIL-AB / 935 °C start; RPM 62–80% idle, 108% MIL;
  these bound the gauge model.
- **Confidence**: F110-GE-129 numbers high (multiple public sources agree); exact thrust-vs-Mach/alt deck
  is not public — use the JSBSim F-16 propulsion tables as the implementation.

### Thrust map & spool dynamics (for the propulsion model)
- **Ratings are sea-level-static**: 17,155 lbf MIL / ~29,500 lbf AB. In flight, thrust follows the usual
  turbofan behavior a model must reproduce:
  - **Altitude lapse**: thrust falls roughly with ambient density/pressure ratio (δ) as altitude rises —
    the ~29.5k SLS AB figure is not available at altitude.
  - **Mach/ram effect**: at low altitude and increasing Mach, **ram recovery raises thrust** — GE cites
    "over 30% additional thrust at low-altitude combat" for the -129 vs baseline; a low-bypass augmented
    fan gains net thrust with speed in the transonic band before lapse dominates.
  - Detailed **thrust-vs-Mach/altitude decks are not public**; the JSBSim F-16 propulsion tables are the
    implementation — this section sets the qualitative shape and the SLS anchors. **Confidence: high on
    SLS ratings, qualitative on the map.**
- **Spool dynamics**: modeled in the canonical F-16 sim as a **first-order power-lag state** with a
  throttle "gearing" (`tgear`) and a rate limit `pdot` that is faster spooling up than down (Stevens-Lewis
  `pdot`/`tgear`; see `flight-controls-flcs.md`). Real DEEC schedules acceleration to stay within surge and
  FTIT limits, so idle→MIL takes several seconds. **FADEC/DEEC** is closed-loop on N2, FTIT, and nozzle.
- **Limit envelope for gauges** (guide, cross-checked): FTIT 935 °C start / 650 °C idle / 980 °C MIL-AB;
  RPM 62–80% idle, 108% MIL, 109% transient; nozzle > 94% at idle; oil 15 psi idle / 25–65 MIL.

### Sources
- Wikipedia *General Electric F110*; globalsecurity.org F110; GE Aviation F110-GE-129 catalog; HandWiki
  *General Electric F110* — thrust (17,155 / 29,500 lbf), airflow 270 lb/s, BPR 0.76, architecture, FADEC,
  "+30% low-altitude thrust", service entry 1992 Block 50.
- Stevens, Lewis & Johnson, *Aircraft Control and Simulation* — F-16 engine power-lag (`pdot`/`tgear`) model.
- DCS guide Part 7 (pressure ratio 30.7, T/W 7.29, spool architecture, limits) — cross-confirmed above.
