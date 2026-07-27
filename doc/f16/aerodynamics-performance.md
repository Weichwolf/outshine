# F-16C Aerodynamics, Envelope & Limits

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 8 — Flight & Aerodynamics, pp. 153–157.
Cross-refs: AoA/G limiter behavior lives in the FLCS (Part 15) → see `flight-controls-flcs.md`.

> Note on scope: this guide's Part 8 is limits + the ALOW/VMS advisory systems. It does **not** tabulate
> corner speed, drag polar, or deep-stall aero curves. The envelope-protection numbers (max AoA, G-vs-AoA)
> are enforced by the FLCS and are captured in `flight-controls-flcs.md`. For FlightBox fidelity the
> reference remains the vanilla JSBSim F-16 aero itself (per project convention).

## Spec

### Airspeed limits

| Condition | Limit |
|---|---|
| Maximum airspeed (VNE) | **800 kts** at sea level, or **Mach 2.05** above 30,000 ft |
| Canopy open / in transit | 70 kts (incl. ground wind velocity) |
| Landing gear extended / in transit | 300 kts / Mach 0.65 (whichever is less) |
| Air-refuel door opening/closing | 400 kts / Mach 0.85 (whichever is less) |
| Air-refuel door open | 400 kts / Mach 0.95 (whichever is less) |
| Flight in severe turbulence (+3 G) | 500 kts |
| Crosswind limit | 25 kts |

### G limits

- **Structural: +9 G / −3 G.**
- Takeoff & landing: **+4 G / 0 G** symmetric; **+2.0 G / 0 G** asymmetric loadout.
- Landing-gear retraction & extension: **+4 G / 0 G** symmetric; **+2.0 G / 0 G** asymmetric.
  - Raising gear near 2 G approaching 300 kts: actuator power may be insufficient to fully retract until
    G is reduced.
- **Negative-G duration** (both reservoir tanks full):
  - Afterburner: **10 s**.
  - MIL power or below: **30 s**.
  - Engine flameout risk: AB engaged **> 5 s** under negative G, or **> 12 s** in zero G.
- G-vs-AoA envelope protection is a CAT-config function — see `flight-controls-flcs.md`
  (CAT I: −3…+9 G to 15° AoA, +7.3 G @ 20°, +1 G @ 25°, AoA-limited to 25°).

### Weight & envelope

- Maximum takeoff weight: **44,000 lb**.
- Service ceiling: **59,000+ ft**.

### ALOW — Altitude-Low advisory system

DED ALOW page (ICP ALOW(2) button; CNI page shown via Dobber LEFT/RTN).

- **RDR ALT** switch must be **ON (FWD)** for ALOW to be active.
- Altitude restrictions attach to a chosen steerpoint (DED inc/dec select).
- **CARA ALOW** (Combined Altitude Radar Altimeter – Altitude Low): low-altitude warning. Below it, the
  **"AL"** notation flashes and VMS calls **"ALTITUDE"**.
- **MSL FLOOR**: minimum-safe-level floor for approaches. Descending below triggers VMS **"ALTITUDE"**.
  Typical 18,000 ft as a reminder near transition altitude (switch to local QNH below it).
- Edit fields: Dobber UP/DOWN selects (asterisks mark active field) → ICP keypad → ENTR.
- HUD shows **Pull-Up cues (X)** when below CARA ALOW.

### VMS — Voice Message System ("Bitching Betty")

VOICE MESSAGE switch: **FWD = ON**, **AFT = INHIBIT (muted)**.

| Priority | Message | Trigger |
|---|---|---|
| 1  | PULLUP | Ground-proximity warning active |
| 2  | ALTITUDE | Descent after takeoff, or radar alt below CARA ALOW, or baro alt below MSL ALOW |
| 3  | WARNING | Any glareshield warning light illuminated |
| 4  | JAMMER | Threat should be jammed; pilot consent required |
| 5  | COUNTER | Dispense command should be initiated (CMDS semi-auto only) |
| 6  | CHAFF-FLARE | CMDS has initiated a dispense program |
| 7  | LOW | Expendable (countermeasure) low quantity |
| 8  | OUT | Expendable type completely spent |
| 9  | LOCK | Radar locked onto target |
| 10 | CAUTION | Any caution-panel light (except IFF caution) |
| 11 | BINGO | Bingo fuel warning activated |
| 12 | DATA | Datalink markpoint received |
| 13 | IFF | IFF not operable in flight (heard during ground test) |

## State

**The envelope is the pinned model's, not this file's** (Prinzip 5). Where the two differ, the model
wins and the difference is documented rather than corrected — [`flight-model.md`](flight-model.md) is
that documentation, and its §8 carries the measured envelope.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Corner speed / max instantaneous turn rate | **measured, not declared**: 380 KCAS, 5.6 g, 16.22 °/s (`make -C sim test-corner`) — inside this file's 330–440 KCAS corner plateau | [`flight-model.md`](flight-model.md) §8.1 |
| Max roll rate | **measured**: saturates at ~186 °/s (the model's 1/0.31821 command scale) | [`flight-model.md`](flight-model.md) §8.2 |
| 9 g structural limit | present only as a pilot hook (`BfmMaxG` = 9.0 `[DOC]`); the model's pitch channel is a *rate* command, so full aft stick buys ~5.6 g at corner — an accepted model property | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §3.3 |
| Airspeed / gear / store limits | **not enforced** — no limiter, no overspeed warning, no gear-speed lockout; JSBSim integrates and `FBFlightMonitor` judges the physical K.O. from the model's own structural and gear truth | [`../flightbox/sim/units-and-missions.md`](../flightbox/sim/units-and-missions.md) |
| Weight / stores effect on the envelope | **built as physics, not as a table** — a loaded station is a JSBSim point mass, the sum of the drag areas an external force, so mass, CG, inertia and drag come from the engine | [`../flightbox/sim/fdm.md`](../flightbox/sim/fdm.md) §9 |
| ALOW (CARA altitude-low advisory) | **built** — floor in `FBF16Ufc`, warning in `FBWarningSystem`, and it is the reference case for three-state block validity: with the radar altimeter unpowered the warning reports **INHIBITED**, not "no warning" | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §5–§6 |
| VMS ("Bitching Betty") voice messages | **not implemented** — no audio path; the same conditions exist as warning bits and `warn_*` telemetry columns | same |

## Gaps

**Source gaps** (this file vs. its sources)
- Chuck Part 8 does **not** tabulate corner speed, drag polar or deep-stall curves (stated in the scope
  note above) — that is why the envelope numbers had to be *measured* against the model.
- ED's corresponding chapter was **not** cross-checked into this file (PROGRESS.md Pass 2, priority-4
  sweep: "not cross-checked against their corresponding ED chapters this pass").

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the flight envelope itself (as the model's, measured), stores mass/drag effects, ALOW.
- *Partially:* G and AoA limiting — it exists as the model's FLCS behaviour, not as the documented
  CAT I/III limiter schedule; the 9 g structural number is a pilot hook, not an enforced limit.
- *Not at all:* airspeed/gear/store speed limits as enforced limits, VMS voice messages, deep-stall
  and departure procedures, CAT I/III switching.

## Knowledge

**Technical depth (researched — for rebuild)**

*Researched engineering depth (public engineering sources, cited at the end). Kept separate from the
guide distillation in `## Spec` — every fact here is researched, not taken from the DCS guides.*

Real-world envelope/aero facts beyond the guide, for FDM validation. Sources cited inline. Reminder: for
FlightBox the *implementation* reference is the vanilla JSBSim F-16 aero itself — these are cross-checks.

### Maneuvering envelope
- **No single corner speed** — the F-16 has a **corner *plateau*** of roughly **330–440 KCAS** (some
  sources 350–450 KIAS) over which max-rated g is available and turn rate is near-best (f-16.net;
  vnfa2 BFM notes). Corner speed = slowest speed at which max g is reachable.
- FBW limits: **+9 g / max 25° AoA** (CAT I, F-16A baseline) — same numbers the guide gives for CAT I.
- **Relaxed static stability**: subsonic CG aft of the neutral point (~ −5% static margin) → the pitch
  FLCS is mandatory for control (see `flight-controls-flcs.md`); this is what buys the agility.

### High-AoA / departure / deep stall
- **Deep stall**: the jet can hang up **between ~50° and 60° AoA** — a stable trim point where the
  stabilator lacks authority to pitch the nose down; caused by the aero moment balance at extreme AoA
  (airliners.net F-16 deep-stall thread; guide MPO description).
- **Recovery**: **MPO (Manual Pitch Override)** unlocks extra stabilator authority; pilot **rocks** the
  pitch oscillation in phase ("pitch rocking") to build amplitude and break the nose down (see
  `flight-controls-flcs.md`). The FLCS anti-spin logic (yaw-rate limiter > 35° AoA, rudder < −5° AoA /
  < 170 kt) normally prevents departure.
- **Confidence**: 50–60° hangup band is consistently reported in the public literature; exact aero
  break is model-dependent — trust the JSBSim aero tables.

### Airframe reference numbers (public)
- Wing area ≈ **300 ft²** (27.87 m²), span ≈ 32.8 ft (with wingtip launchers); cropped-delta with LERX.
- Empty weight ≈ **18,900 lb**; the guide's 44,000 lb MTOW and 59,000 ft ceiling match published figures.
- These bound the FDM mass/geometry; source: General Dynamics/USAF F-16 public data.

### Reference aerodynamic dataset — NASA TP-1538 (the FDM's origin)

**This is the dataset our JSBSim F-16 is built from** — the JSBSim project explicitly uploaded the
"nasa-tp-1538 tables from the wind-tunnel test of the F-16A" into its `f-16` directory (JSBSim devel
mailing list, 2003). So TP-1538 *is* the ground truth against which our aero is validated, not an
external cross-check.

Nguyen, Ogburn, Gilbert, Kibler, Brown & Deal, *Simulator Study of Stall/Post-Stall Characteristics of a
Fighter Airplane With Relaxed Longitudinal Static Stability*, **NASA TP-1538**, Dec 1979 (NTRS 19800005879):
| Property | Value |
|---|---|
| Aircraft | F-16A (subscale wind-tunnel model) |
| Coefficient form | **Nonlinear tabular** functions of **AoA and/or sideslip** (lookup tables, not linear derivatives) |
| AoA range | **−20° to +90°** (high-α / post-stall focus) |
| Sideslip range | **−30° to +30°** |
| Data type | Low-speed **static** force tests **+ dynamic forced-oscillation** tests (damping derivatives) |
| Test/validation rig | Langley **Differential Maneuvering Simulator** |
| Study variable | Effect of **relaxed longitudinal static stability** (varying static margin) — the F-16's defining trait |

Modeling implications:
- The wide **−20°…+90° AoA** table is exactly what makes the **deep-stall 50–60° hangup** representable —
  the pitching-moment table goes non-monotonic and traps a stable high-α trim (see high-AoA section above).
- **Relaxed static stability** shows up as a pitching-moment curve `Cm(α)` whose subsonic slope near
  trim is **destabilizing** (positive `∂Cm/∂α` region) — which is *why* the FLCS pitch loop is mandatory
  (`flight-controls-flcs.md`). A rebuild must not "fix" this into a statically stable curve.
- Forced-oscillation data supplies the **dynamic damping** derivatives (`Cmq`, `Clp`, `Cnr`, …) that set
  the short-period / dutch-roll / spin behavior — cross-check our `[flt]` structure-invariants against these.
- Note the dataset is **F-16A, subsonic, low-speed** — it does not carry transonic/supersonic compressibility
  or stores effects; that is a known bound of the open model (matches our mission-validator caveats).

### Sources
- Nguyen et al., **NASA TP-1538** (NTRS 19800005879) — F-16A aero dataset: AoA −20…+90°, β ±30°, static +
  forced-oscillation, relaxed static stability, Langley DMS.
- JSBSim devel mailing list (sourceforge msg 7557640) — TP-1538 tables uploaded as the JSBSim F-16 aero.
- W.H. Mason, VT *Real Aircraft Aerodynamic Data* (archive.aoe.vt.edu) — TP-1538 as the standard F-16 model reference.
- f-16.net forum (Sustained Turn Performance); vnfa2 BFM Mechanics — corner plateau 330–440 KCAS.
- airliners.net F-16 deep-stall discussion — 50–60° AoA hangup, rocking recovery.
- USAF/General Dynamics F-16 public specifications — weights, wing area, ceiling.
