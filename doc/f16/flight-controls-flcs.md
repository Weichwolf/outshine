# F-16C Flight Control System (FLCS) & Autopilot

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 15 — Flight Controls & Autopilot, pp. 662–672.
Cross-refs: limiter G/AoA numbers also appear in Part 8 (pp. 153–157, see `aerodynamics-performance.md`).

**This file has two clearly separated layers:** the guide distillation (below), and a researched
**Technical depth** section at the end citing public engineering literature for rebuilding the FLCS.

> Relevance to FlightBox: our FBW layer (`fcs/fbw-override`) commands the JSBSim F-16's own FLCS.
> This file is the spec that FBW must reproduce or bypass cleanly. The `*-cmd-norm` outputs are
> rate setpoints into this inner loop.

## Spec

### Architecture

- Digital **four-channel** fly-by-wire; nicknamed "Flickiss". Hydraulically positions surfaces.
- Pilot commands intent; **control laws** compute surface deflections. Computer can also inject
  automatic signals (stabilization, envelope protection) without pilot input.
- Axis → surface mapping:
  | Axis | Surface actuation |
  |---|---|
  | Pitch | **Symmetric** horizontal tails (stabilators) |
  | Roll  | **Differential** flaperons **and** horizontal tails |
  | Yaw   | Rudder |
- **ARI** (Aileron–Rudder Interconnection) provides roll coordination.
  - ARI **disabled** when main-gear wheel speed > 60 kts **or** AoA > 35°.

#### Control surfaces
- Rudder
- Leading-Edge Flap (LEF)
- Horizontal Tail Stabilator (stabilizer/elevator)
- Trailing-Edge Flaperon (TEF) — combined flap + aileron

#### LEF / TEF scheduling
- **LEF**: scheduled by the FLCS as a function of **Mach number and AoA**.
- **TEF**: scheduled as a function of **landing-gear handle position, ALT FLAPS switch position, and airspeed**.

### FLCC inputs (Flight Control Computer)

Gains are scheduled by air-data; sideslip angle & rate computed from INS.

| Channel | Inputs |
|---|---|
| Pitch | Pitch trim (→ mechanical limit), horizontal-tail stick force, AoA, pitch rate, normal acceleration, impact pressure |
| Roll  | Roll trim (→ mechanical limit), aileron stick force, roll rate |
| Yaw   | Yaw trim (→ mechanical limit), rudder-pedal force, yaw rate, lateral acceleration |
| Gun comp | Gun fire signal |
| INS   | Angles and velocities (for sideslip calc) |

Outputs: L/R horizontal tail (stabilator), L/R flaperon, rudder.

### Operational modes (gains)

Three gain sets modify how the FBW moves surfaces:

| Gain set | Active when |
|---|---|
| **Cruise Gains** (normal) | Normal flight: gear up, no FLCS failure |
| **Takeoff & Landing Gains** | Below **400 kts** AND (gear down OR ALT FLAPS = EXTEND OR air-refuel trap door open) |
| **Standby Gains** | FLCC has detected an FLCS failure |

#### Gear-down pitch behavior (Note 2)
- Gear deployed → FLCS is a **pitch-rate command** system up to 10° AoA.
- Above 10° AoA → **pitch-rate / AoA command** system.

### Configuration modes & limiters (CAT I / CAT III)

Aircraft auto-detects required CONFIG from weapon + external-tank loadout. **STORES CONFIG** caution =
the CAT switch (gear panel) does not match the loadout.

The CAT switch is **not** a direct G-limiter — it limits **AoA**, which in turn limits attainable G as a
function of AoA and airspeed.

| | CAT I (Air-to-Air) | CAT III (Air-to-Ground) |
|---|---|---|
| Use | A-A weapons + centerline tank | A-G weapons + external **wing** tanks (heavy/gas-heavy) |
| G envelope to 15° AoA | −3 G … +9 G | −3 G … +9 G |
| Above 15° AoA | max G = f(AoA, airspeed): **+7.3 G @ 20° AoA**, **+1 G @ 25° AoA** | — |
| Max AoA limit | **25°** | **15.5°–15.8°** |
| Roll rate | Full | **−40%** vs CAT I (roll-coupled departure resistance) |
| Rudder deflection fade | Starts ~**14°** AoA, zero at **26°** AoA | Starts **3°** AoA, zero at **15°** AoA |

#### Anti-spin / departure logic
- **Note 3**: Above **35° AoA**, the yaw-rate limiter provides roll & yaw anti-spin inputs and **cuts out
  stick roll commands**.
- **Note 4**: Below **−5° AoA and < 170 kts**, the yaw-rate limiter provides anti-spin rudder inputs.

#### Gun compensation
- FLCS auto-compensates off-center gun firing + gun-gas effects by moving rudder & flaperons.
- Optimized for **0.7–0.9 Mach**; firing outside this band can cause adverse effects.

### Deep-stall recovery — Manual Pitch Override (MPO)

- **MPO switch → OVRD** (held): commands greater stabilator authority to pitch the nose down out of a
  deep-stall departure, to regain airspeed for controlled flight.
- **DBU** (Digital Backup) switch: pilot manually selects a backup FLCS software state.

### Autopilot ("relief modes")

Pitch and roll modes are independent and **combinable** (e.g. STRG SEL + ALT HOLD = follow steerpoint at
held altitude).

#### Switches
- **Roll Mode switch**: HDG SEL / ATT HOLD / STRG SEL
- **Pitch Mode switch**: ALT HOLD / A/P OFF / ATT HOLD
- A **pitch mode must be active** for any roll mode to engage.

#### Modes
| Mode | Behavior |
|---|---|
| PITCH ATT HOLD | Holds current pitch attitude; will not capture beyond **±60°** pitch |
| ROLL ATT HOLD | Holds current roll attitude; will not capture beyond **±60°** roll |
| ALT HOLD | Holds current **barometric** altitude |
| HDG SEL | Turns to & flies the EHSI heading bug; **bank limited to 45°** |
| STRG SEL | Turns to & flies the active steerpoint; **bank limited to 45°** |

#### ALT HOLD detail
- Engaging captures current altitude as the **reference altitude**; holds ±100 ft.
- Vertical velocity **> +2000 ft/min or < −2000 ft/min prevents capture**.
- Reset reference in flight: hold **Paddle** (disengages while held), fly to new altitude, release →
  new actual altitude becomes reference.

#### ATT HOLD detail (pitch & roll)
- Any stick input while engaged **re-captures** the new attitude and holds it.

#### STRG SEL detail
- Select steerpoint on CNI page (Dobber LEFT = RTN) via DED inc/dec, or STPT(4) → number → ENTR.
- **AUTOMATIC** sequencing: on reaching a steerpoint, steers to the next automatically.
- **MANUAL** sequencing: on reaching a steerpoint, **circles it at 30° bank**.
- Toggle MAN/AUTO: Dobber DOWN to MAN/AUTO field, M-SEL(0) to toggle.

#### Autopilot auto-disengage conditions
Any one of:
- Paddle switch (stick) pressed *
- TRIM A/P Disc switch → DISC
- Landing gear DOWN
- Air-refuel trap door open
- ALT FLAPS switch → EXTEND
- **AoA > 15°**
- DBU engaged
- MPO switch held in OVRD
- Autopilot or FLCS failure
- Stall horn active

\* Paddle **overrides** (while held) in ATT/ALT modes but **does not disengage** autopilot while in
**HDG SEL or STRG SEL** — use Pitch Mode → A/P OFF to disengage those.

#### TRIM / A/P Disc switch
- **NORM**: stick trims energized, autopilot possible.
- **DISC**: stick trims and autopilot inhibited.

#### ED EA Guide addendum — autopilot (official, pp.128–131) — includes a genuine numeric discrepancy
`doc/DCS F-16C Early Access Guide EN.pdf` p.128–131 documents the same 2×2 PITCH/ROLL switch autopilot
(ALT HOLD/ATT HOLD × HDG SEL/ATT HOLD/STRG SEL, a pitch mode required for any roll mode — matches Chuck
exactly) but adds **quantified command-authority and disengage-condition figures Chuck does not give**,
and **one figure that conflicts with Chuck's** — flagged explicitly, not silently resolved:

- **Command authority while engaged** (ED, new numbers not in Chuck): pitch command rate **+3.0 G to
  −1.0 G**, roll rate **≤ 20°/s**, **bank angle limited to ±30°**.
  ⚠️ **Discrepancy**: Chuck (§ above, "Modes" table) states HDG SEL/STRG SEL are **"bank limited to
  45°"**; ED states the autopilot's overall bank-angle limit is **±30°**. Both values are stated as
  hard limits in their respective source, for what reads as the same mechanism (autopilot roll-axis
  authority) — **ED EA Guide is the more belastbar (authoritative) source** per this task's source
  hierarchy (official ED module manual vs. Chuck's third-party tutorial guide), so **treat ±30° as
  primary and 45° as Chuck's figure to be re-verified**, not silently overwritten. If a FlightBox
  autopilot implementation needs one number, use ED's ±30°; if behavior diverges from either, that's
  worth a targeted DCS-behavior check before trusting either guide over the vanilla-JSBSim reference
  model (CLAUDE.md Prinzip 5 — the model, not either guide, is ground truth for FlightBox).
- **Auto-disengage conditions** — ED's list (verbatim sense) vs. Chuck's list (§ above) overlap almost
  entirely but each has items the other doesn't:
  | Condition | Chuck | ED |
  |---|---|---|
  | Paddle switch pressed | listed (but see Chuck's own asterisk: doesn't actually disengage in HDG SEL/STRG SEL) | **not listed** |
  | TRIM/AP DISC → DISC | yes | yes |
  | Gear down/locked | yes ("DOWN") | yes ("extended **and locked**" — more precise: transit ≠ disengage trigger) |
  | Air-refuel door open | yes | yes ("AIR REFUEL switch → OPEN") |
  | ALT FLAPS → EXTEND | yes (unconditional) | yes, **qualified: "below 400 knots"** — ED is more precise here |
  | AoA > 15° | yes | yes (matches exactly) |
  | Bank angle > 60° | **not listed** (Chuck instead caps ATT-HOLD *capture* at ±60°, a different mechanism) | **yes, explicit disengage trigger** |
  | DBU engaged | yes ("DBU engaged") | yes ("FLCS operating on Digital Backup software") |
  | MPO switch OVRD | yes | yes ("MANUAL PITCH switch → OVRD") |
  | AP/FLCS failure | yes | yes |
  | Stall horn / low-speed warning | yes ("Stall horn active") | yes ("Low speed audio warning is triggered") |

  Net effect: ED's **gear-lock qualifier** and **ALT-FLAPS speed qualifier** are genuine added precision
  (not contradictions); the **bank>60° disengage** is new information Chuck doesn't carry at all
  (distinct from Chuck's ATT-HOLD ±60° *capture-limit* concept, which ED doesn't restate); the
  **paddle-switch** discrepancy is likely not a real contradiction — Chuck's own asterisked footnote
  already says paddle **doesn't** disengage in HDG SEL/STRG SEL, so ED simply omitting it from the
  unconditional-disengage list is consistent with Chuck's own caveat, not opposed to it.

## State

**FlightBox does not rebuild this FLCS — it commands the one inside the pinned model.** The model's own
`<flight_control>` XML *is* the flight control system (11 channels, 58 components, every gain converted
in [`flight-model.md`](flight-model.md) §7); FlightBox adds an outer loop on top of `fcs/*-cmd-norm`.

| Item of this reference | FlightBox | Where |
|---|---|---|
| The FLCS itself (pitch/roll/yaw laws, LEF/TEF schedule, gear-down gains) | **the model's**, documented channel by channel | [`flight-model.md`](flight-model.md) §7.1–§7.8 |
| FBW outer loop (rate/attitude PIDs onto `*-cmd-norm`), F-16 gain preset | **built** — `FBFlightControl` + `FBFlightControl::F16()` | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §3 |
| `fcs/fbw-override` | **built, and honest about its reach**: it bypasses the **pitch** channel only | [`flight-model.md`](flight-model.md) §7.10 |
| Autopilot "relief modes" (ALT HOLD, ATT HOLD, STRG SEL, HDG SEL) | **not implemented as these modes**. FlightBox has its own guidance primitives — `Direct` (point-to-point course/altitude/speed), `Course` (a flown, measured localizer-style law) and `Manual` — which the pilot phase machine commands | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §2 |
| Autopilot auto-disengage conditions, TRIM/AP DISC switch | **not implemented** — there is no engage state to lose | — |
| CAT I / CAT III limiter switching, ARI, anti-spin logic, gun compensation, MPO | **not implemented** in FlightBox; whatever the model's XML does is what happens | [`flight-model.md`](flight-model.md) §7.11 (the model-vs-real-FLCS deviation table) |
| Damage on the FLCS | **built as authority, not as law**: a degraded FLCS scales the commanded deflections (one of two hydraulic systems), a failed one removes them — the FLCS keeps commanding, the aircraft stops answering | [`../flightbox/sim/weapons-and-damage.md`](../flightbox/sim/weapons-and-damage.md) §8 |

**The deviation table is the load-bearing link:** [`flight-model.md`](flight-model.md) §7.11 compares
the model's FLCS against the real one described here, item by item. Read it before treating any number
in this file as something FlightBox should reproduce.

## Gaps

**Source gaps** (this file vs. its sources)
- **The ED/Chuck autopilot bank-limit discrepancy stays open**: Chuck "bank limited to 45°" vs. ED's
  command-authority numbers (pitch +3.0/−1.0 G, roll ≤20 °/s, bank ±30°) for HDG SEL/STRG SEL. Both are
  kept in the addendum above; ED is marked the more reliable source, and the JSBSim model remains the
  actual ground truth per Prinzip 5.
- Only the ED **autopilot** chapter (pp.128–131) was folded in; the rest of this file is Chuck plus
  research.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the inner loop (as the model's FLCS) plus an outer FBW loop with an F-16 gain preset;
  damage as loss of control authority.
- *Partially:* the autopilot — FlightBox has guidance modes, but not *these* modes, and none of the
  engagement/disengagement logic. `FBFlightControl::Run` also hard-wires `0.01` instead of `dt`, which
  silently binds the outer loop to 100 Hz ([`../flightbox/sim/systems.md`](../flightbox/sim/systems.md)
  Gaps).
- *Not at all:* CAT I/III switching, ARI, anti-spin and deep-stall logic, MPO, gun compensation,
  FLCC channel voting/BIT, hydraulic system modelling, trim.

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

Everything above is the pilot-guide view. This section is engineering reference for reconstructing the
FLCS control laws. Sources cited inline; confidence flagged where the public record is indirect.

> FlightBox note: the operative rebuild artifact is the repo's JSBSim F-16 `flight_control` XML — the
> real-world values here are the **design targets** that model (and our FBW bypass) should match. Our FBW
> commands rate/`*-cmd-norm` setpoints into this inner loop, so the command-shaping and limiter structure
> below is exactly what FBW must reproduce or cleanly override.

### Control-law architecture (signal flow)

The FLCS is a **command-augmentation** system, not a rate damper bolted onto a stable airframe: the F-16A
was built with **relaxed static stability** (~ −5% MAC subsonic), so the pitch loop is load-bearing —
without it the aircraft is divergent and unflyable in pitch. This is why there is no mechanical pitch
backup (Droste & Walker, *GD Case Study on the F-16 FBW FCS*, AIAA Professional Study Series, 1980).

Per-axis response type (Kim, Ji & Kim, *Adv. Mech. Eng.* 2020; ryanporto FLCS notes; VT F-16 study):
| Axis | Command variable | Feedbacks |
|---|---|---|
| Pitch (up-and-away) | **Normal acceleration Nz** (g-command), blended to an **AoA command** near the limit | AoA, pitch rate q, Nz (accelerometer **15 ft ahead of CG** — AeroBench `xa=15.0`), impact pressure |
| Pitch (power approach, gear down) | **Pitch-rate command** to 10° AoA, then pitch-rate/AoA blend | same — matches guide "Note 2" |
| Roll | **Stability-axis roll rate (Ps) command** | roll rate p |
| Yaw | Sideslip/lateral-accel regulation + **ARI** | yaw rate r, lateral accel Ny, INS sideslip |

- **Inner/outer structure**: AoA control is the inner loop, Nz the outer loop; the Nz path injects a
  `+(1−Nz)·g` gravity-compensation term so 0 stick = 1 g level (Kim et al. 2020).
- **Gain scheduling**: controller gains scheduled on **air data (dynamic pressure q̄ / Mach / altitude)**
  for full-envelope validity (ryanporto; VT study: "gains are scheduled using air data").
- **ARI** (Aileron–Rudder Interconnect): feeds roll command into rudder for turn coordination; guide
  gives the cutouts (gear wheel-speed > 60 kt, AoA > 35°).

### Limiter logic (the envelope protection)

The CAT-I numbers in the guide are the limiter outputs. Structurally (falcon-bms FLCS reverse-engineering;
ryanporto; guide Part 15):
- Pitch command is a **blend**: commands g up to a break, then transitions to an **AoA command** so the
  jet rides the AoA limit rather than the g limit at low speed. CAT I: **−3…+9 g to ~15° AoA**, then max g
  falls with AoA (+7.3 g @ 20°, +1 g @ 25°), hard **AoA limit ≈ 25°**. CAT III: AoA limited **15.5–15.8°**,
  commanded roll rate **−40%**.
- **Roll**: commanded roll rate is reduced as a function of AoA and in CAT III (departure resistance).
- **Anti-spin/departure**: yaw-rate limiter takes over roll+yaw above 35° AoA (cuts roll stick), and adds
  anti-spin rudder below −5° AoA / < 170 kt (guide Notes 3–4).
- **Confidence**: the *shape* (g→AoA blend, scheduled limiters) is well established; exact break points and
  gain tables for the real jet are not fully public — treat the guide numbers as the authority and the
  JSBSim model as the implementation.

### Canonical model constants (Stevens & Lewis / NASA)

The standard open F-16 model (Nguyen et al., **NASA TP-1538**, 1979; Stevens, Lewis & Johnson, *Aircraft
Control and Simulation*; coded in Bak's **AeroBenchVVPython** and JSBSim) — the reference our FDM inherits:

**Control-surface position limits** (AeroBench `low_level_controller.py`, confirmed):
| Surface | Position limit | Rate limit† |
|---|---|---|
| Elevator / stabilator | **±25°** | 60°/s |
| Aileron / flaperon | **±21.5°** | 80°/s |
| Rudder | **±30°** | 120°/s |
| Leading-edge flap | 0…25° | 25°/s |
| Throttle | 0…1 | first-order lag (engine `pdot`) |

† Rate limits are the classic Stevens-Lewis actuator-model values (first-order lag + rate/position
saturation); **medium confidence** — not re-confirmed from the primary code this pass. Position limits and
scalings **high confidence**: model uses `dail = ail/20`, `drdr = rdr/30` normalizations.

**Leading-edge flap**: commanded by the FLCS as a function of **AoA and Mach** (guide) — in the NASA model
an α-driven schedule with a lag; keeps the wing at favorable camber and delays LE separation.

**Low-level LQR controller** (AeroBench — the linearized realization of the inner loop):
- **Longitudinal** gain `K_long = [−156.88, −31.04, −38.73]` acting on `[AoA, pitch-rate q, ∫(Nz−Nz_cmd)]`
  → elevator command. **3 integrators** track Nz, Ps, Ny+r errors.
- **Lateral-directional** `K_lat` (2×5) acting on `[β, p, r, ∫Ps_err, ∫(Ny+r)_err]` → `[aileron, rudder]`:
  ```
  [ 37.84, −25.41,  −6.83, −332.88, −17.16 ]   # → aileron
  [−23.91,   5.70, −21.63,   64.49, −88.36 ]   # → rudder
  ```
- Commanded quantities (the FBW-relevant setpoints): **Nz, stability-axis roll rate Ps, and Ny+r**
  (coordinated-turn sideslip term). This is the exact triple our FBW should drive.

### FLCC computer architecture (analog → digital)

- **F-16A/B (Block 1–15/25)**: **analog** FLCS — a quad-redundant analog stability-augmentation computer.
  The pitch/roll/yaw laws are hardwired analog networks; gains scheduled by an air-data gain-changer.
- **F-16C Block 40+**: **DFLCS** — digital FLCC replaces the analog boxes with a **four-channel digital
  computer**, computer-controlled and still hydraulically positioning surfaces (f-16.net; ryanporto).
  Digital enables the CAT-I/III switching, MPO, DBU, and gun-compensation logic in the guide.
- **Quad redundancy & voting**: **4 independent channels**, each with its own sensors/processing/power.
  Channels are compared; a channel whose commanded servo position errs **> 20% of stroke** vs the model
  is **centered / voted out** (fail-op / fail-op → fail-safe: survives two like failures) (ryanporto).
- **Digital Backup (DBU)**: a simplified backup software state selectable by the pilot (guide) — the
  digital analog of the analog system's reversionary path.
- **Frame rate**: FLCS/telemetry signals sampled at **64 Hz** in NASA AFTI/F-16 work (Mackall, *AFTI/F-16
  DFCS Experience*, NTRS 19840012524). **Medium confidence** as the production Block-40 FLCC frame rate —
  the AFTI is a research variant; production rate is commonly cited in the ~64–100 Hz band but not firmly
  public. For our fixed-step sim this is the design target for the inner-loop rate.

### Actuators & hydraulics

- **Integrated Servoactuators (ISA)**: each primary surface (2 stabilators, 2 flaperons, 1 rudder) is
  driven by a hydraulic ISA that fuses the electro-hydraulic servo-valve + ram + position feedback; the
  four FLCC channels drive redundant servo-valve coils (f-16.net; ryanporto).
- **Two independent hydraulic systems (A & B)**, ~3000 psi (guide gives 2850–3250 psi operating band,
  `engine-fuel.md`); surfaces stay controllable on a single system. EPU backs hydraulics on engine loss.
- **Leading-edge flaps**: driven by a separate rotary drive (flap drive system) — not an ISA — commanded
  on the α/Mach schedule.
- **Rates/limits (canonical model)**: stabilator ±25°, flaperon ±21.5°, rudder ±30° (position); rate
  limits ~60/80/120°/s per the Stevens-Lewis actuator model (medium confidence — see table above).

### Sensors feeding the FLCC
- Triaxial **rate gyros** (p, q, r).
- **Normal + lateral accelerometers** (Nz at the accelerometer station **15 ft ahead of CG**; Ny).
- **Angle-of-attack probes** (AoA cone/vane transducers, redundant) — primary for the AoA limiter & blend.
- **Air-data** (impact/static pressure → q̄, Mach, altitude) for **gain scheduling**.
- **Quadruplex sidestick force transducers**: the stick measures **force**, near-zero displacement (the
  original F-16 stick was rigid; small motion added later) — command is a force gradient, not a deflection.
- Sideslip angle/rate is **computed from INS**, not directly sensed (guide FLCC diagram).

### Sources
- Droste & Walker, *The General Dynamics Case Study on the F-16 Fly-By-Wire Flight Control System*, AIAA, 1980.
- Nguyen et al., *Simulator Study of Stall/Post-Stall Characteristics…* (relaxed static stability), NASA TP-1538, 1979.
- Stevens, Lewis & Johnson, *Aircraft Control and Simulation*, 3rd ed. — F-16 model.
- Bak, **AeroBenchVVPython** (`low_level_controller.py`, `subf16_model.py`) — confirmed surface limits, LQR gains, integrator set.
- Kim, Ji & Kim, *Control law to improve handling qualities for short-range air-to-air combat*, Adv. Mech. Eng. 2020 — Nz/pitch-rate response types, inner/outer loop.
- ryanporto.com F-16 FCS wiki; W.H. Mason, VT F-16 study (archive.aoe.vt.edu); falcon-bms *Revealing the Dark Side of the F-16 — FLCS*.
- Mackall, *AFTI/F-16 Digital Flight Control System Experience*, NASA (NTRS 19840012524) — 64 Hz sampling.
