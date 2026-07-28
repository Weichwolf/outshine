# F-16C Start-Up Procedure

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 4 — Start-Up Procedure, pp. 84–118.
81 numbered steps in phases A–I. Condensed to the operationally load-bearing steps + parameters.

## Spec

### Pre-power (battery)
1. Ejection Seat Lever — DOWN & ARMED (usually done just before takeoff).
2. Test FLCS functions on battery power only.
3. MAIN PWR switch — MAIN PWR/FWD.

### A — Provide aircraft power (steps 4–5)
- MAIN PWR readies engine-mounted generator (no electrical power yet until engine running).
- On EPU panel, verify **EPU GEN** and **EPU PMG** indications.

### B — Pre-start setup (steps 6–9)
- Parking Brake / Anti-Skid — **PARKING BRAKE (UP)**.
- Close & lock canopy (CANOPY light extinguished).

### C — Engine start (steps 10–22)
1. Verify throttle at **OFF** detent.
2. JFS (Jet Fuel Starter) switch — **START2**.
3. JFS reaches IDLE RPM within ~30 s → JFS RUN light on, FLCS RLY/PMG lights out, TO FLCS light on.
4. Clutch engages (ADG/PTO shaft drives F110); engine spools to **20–25% RPM**.
5. At **20–25% RPM**, move throttle **OFF → IDLE**. (Too early = hot start or hung start.)
6. Light-off within **10 s**; RPM & FTIT rise.
7. Milestones: SEC caution off @ 20% RPM; standby gen @ ~60% RPM (ENGINE + STBY GEN lights out);
   main gen ~5–10 s later (MAIN GEN light out); JFS auto-off @ ~55% RPM.

**Idle engine parameters (step 22):**
| Parameter | Value |
|---|---|
| HYD/OIL PRESS warning | Off |
| Fuel flow | 700–1700 pph |
| Oil pressure | ≥ 15 psi |
| Nozzle position | > 94% |
| Engine RPM | 62–80% |
| FTIT | ≤ 650 °C |
| Hydraulic pressure (A & B) | 2850–3250 psi |

### D — Set up avionics (steps 23–38)
- Power ON: MMC, ST STA (store stations), MFD, UFC, GPS receiver. (DL/MAP left OFF — no function on F-16C.)
- BITs run as systems power up.
- LEFT/RIGHT HDPT power ON if HTS/targeting pod fitted.
- FCR power — **FCR/ON (FWD)** (enters BIT).
- RDR ALT power — **STBY (MIDDLE)**.
- COMM1 UHF / COMM2 VHF — ON, mode SQL; UHF backup — BOTH/MAIN.
- Turn on HUD via SYM intensity wheel.
- C&I switch — **UFC** (enables upfront control of comms).

### E — INS alignment (steps 39–43)
- INS selector — **ALIGN NORM** (normal) or stored-heading.
- Alignment status on DED; enter/confirm aircraft coordinates.
- Complete when status reaches **"10 RDY"**.
- Then INS selector — **NAV**.

### F — Datalink (steps 44–45) — only after INS aligned. G — IFF (steps 46–48) — IFF Master → NORM.

### H — Complete aircraft setup (steps 49–70)
- Uncage SAI (standby attitude indicator).
- Set FBW control mode via **STORES CONFIG** switch (CAT I / CAT III per loadout).
- Oxygen: Supply ON, Emergency NORMAL, Diluter NORMAL; pressure green; flow blinks.
- RWR/TWA power on; CMDS RWR/JMR ON; CMDS CH/FL ON; CM mode + program selected.
- Equip HMCS/NVGs; perform **HMCS alignment** (coarse then fine DX/DY + ROLL) against the HUD
  reference cross (steps 66–68 — see `hud-symbology.md`).
- Load DTC via DTE page; clear avionic faults on TEST page.

### I — Post-start checks (steps 71–81) — all optional
Pitot heater, fire/overheat detection, malfunction lights, SEC engine control, FLCS op check, fuel
quantity, **DBU**, trim, **MPO**, EPU system.

---

### ED EA Guide addendum — official procedure detail (pp.132–139)

The ED EA Guide's own start sequence (Aircraft Start → Before/After Engine Start) **cross-validates
every quantitative milestone in Chuck's guide above exactly**: JFS spins the core to 20–25% RPM before
IDLE fuel introduction, SEC caution off at 20% RPM, standby generator ~60% RPM, idle parameters
identical (fuel flow 700–1700 pph, oil ≥15 psi, NOZ POS >94%, RPM 62–80%, FTIT ≤650°C, HYD A&B
2850–3250 psi). No discrepancy found in this section — both sources agree.

ED adds **procedural steps Chuck's tutorial-style guide compresses or omits**, each directly
load-bearing for a start-state machine:

- **FLCS PWR TEST** (before engine start, battery power only): hold FLCS PWR TEST switch → verify
  `FLCS PMG` on, `TO FLCS` on, `FLCS RLY` off, and FLCS PWR A/B/C/D lights on the TEST panel — a
  pre-engine-start check that the flight-control-relay chain is alive on battery power alone, before
  any hydraulics exist.
- **FLCS BIT**: cycle flight controls to full deflection, then initiate BIT — **`FLCS RUN` light stays
  on for ~45 seconds** until complete, auto-returns switch to OFF; verify `FLCS FAIL` not illuminated.
- **DBU check**: `DIGITAL BACKUP → BACKUP` (verify `DBU ON` warning lights, verify all control surfaces
  still respond normally) → `OFF` (verify `DBU ON` extinguishes) — validates the FLCS standby/backup
  gain set (`flight-controls-flcs.md`'s DBU switch) actually drives surfaces before flight.
- **TRIM check**: `TRIM/AP DISC → DISC` (verify SSC trim inputs produce **no** surface/trim-wheel
  movement) → `NORM` (verify trim inputs **do** move surfaces/trim wheel) — the on-ground functional
  test of the same DISC/NORM switch that gates autopilot engagement (`flight-controls-flcs.md`).
- **MPO check**: full-forward stick, then `MPO → OVRD` held (verify stabilator trailing edges deflect
  **further down** beyond the stick-alone position), release (verify return) — ground-test of the Manual
  Pitch Override deep-stall-recovery authority (`flight-controls-flcs.md`).
- **Air-refuel system check**: `AIR REFUEL → OPEN` (verify `RDY` light, `DISC` light stays off) → `CLOSE`
  (verify `RDY` extinguishes). **ED states explicitly: "FLCS gains are set to takeoff/landing
  configuration when [AIR REFUEL is] OPEN"** — this directly confirms one of the three FLCS
  Takeoff&Landing-gain triggers already listed in `flight-controls-flcs.md` ("gear down OR ALT FLAPS
  EXTEND OR air-refuel trap door open"), now with an explicit causal statement rather than an inferred
  table entry.
- **EPU check**: Oxygen Diluter → 100%, throttle +10% RPM above idle, `EPU/GEN TEST → EPU/GEN` held →
  verify `AIR` light on, **EPU Run light on for ≥5 seconds**, `EPU GEN`/`EPU PMG` momentarily flicker
  then off, FLCS PWR A/B/C/D remain on — ground-validates the emergency-power reversion path
  (`engine-fuel.md`'s EPU).

#### INS alignment — exact types, timing, and status/CEP scale (ED EA Guide p.165–176)
Chuck's guide states only "complete when status reaches 10 RDY" (§E above); ED gives the full mechanism:

**Three alignment types** (selected on the INS knob, AVIONICS POWER panel):
| Type | Time to full alignment | Requirement |
|---|---|---|
| **Normal Gyrocompass** (`ALIGN NORM`) | **~8 minutes** to full position confidence | Aircraft must not move/reconfigure (incl. arming stations) during alignment |
| **Stored Heading** (`ALIGN STOR HDG`) | **~90 seconds** | Requires the aircraft was *not moved* since the prior Normal alignment's INS power-off — reuses stored alignment data |
| **In-Flight** (`IN FLT ALIGN`) | variable | Requires level flight, constant speed & heading; **GPS data required** for reliable quality; without GPS, pilot manually inputs magnetic heading |

**Alignment status/CEP scale** (the value right of the slash on the INS DED page — a single decrementing
number is the entire alignment-progress signal available to the pilot, cf. hud-symbology.md's "what the
pilot actually sees"):
| Status value | Meaning |
|---|---|
| 99 | No alignment performed |
| 98–91 | Platform leveling + true-heading acquisition |
| 90 | Accurate attitude data attained (coarse align begins) — typically **~30 s** after init |
| 79–63 | Coarse alignment in progress |
| 62 | Coarse-usable state (navigable, much less accurate than full) |
| 60–11 | Fine alignment; value = **CEP multiplier** (60 → 6.0× the fully-aligned error, decrementing toward 1×) |
| 10 | Fully aligned, CEP factor **1.0** |

- **RDY** (INS DED page) + **"ALIGN"** on the HUD (in place of Max-G field) appear once the *minimum
  navigable* (coarse) state is reached; both **flash** once the alignment reaches the fully-aligned
  state (10); both clear once the INS knob is set to NAV (or AUTO-NAV engages).
- A start-state machine for FlightBox's mission-boot spawn (`FBMissionBoot.h`) doesn't need to simulate
  this progression today (spawn is instantaneous IC application, not a start sequence) — but if a
  future "cold start" mission phase is added, this is the exact state table to encode.
- **DCS mission-boot defaults** (ED explicit notes, useful cross-reference for what "engine already
  running" spawn states should look like): a DCS mission starting with engine running has INS
  pre-aligned (stored-heading alignment available), IFF STBY, RWR powered off, CMDS OFF/STBY depending
  on start point (parking vs. runway), ECM OFF (needs 3-min warm-up before use). A mission starting
  cold requires the full `ALIGN NORM → NAV` sequence before taxi.

## State

**FlightBox has no start-up procedure — and does not need one to be honest about it.** A mission spawns a
trimmed aircraft through the single guarded IC path (`FBFdmBoot::Spawn`, the only writer of JSBSim initial
conditions); everything this file's phases D–H set by hand is *declared* instead, as `set` lines in the
`.fbm` mission file.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Phases A–C (power, JFS, engine start) | **not implemented** — the spawn starts with the engine running; `FBAirframeControls` does carry an engine start/cutoff channel, nobody sequences it | [`../flightbox/sim/systems.md`](../../systems.md) §9 |
| Phases D, F, G (avionics / datalink / IFF set-up) | **replaced by mission data** — `set fcr_mode`, `datalink`, `datalink_xmt`, `datalink_filter`, `iff_xpdr`, `iff_interrogator`, `cmds_mode`, `cmds_program`, … each interpreted by the module's own `ApplySetup` | [`module.md`](module.md) §2.6 |
| Phase E (INS alignment, stored heading, CEP) | **not implemented** — navigation has no alignment state and no drift; position is exact | [`../flightbox/sim/systems.md`](../../systems.md) §7 |
| BIT / FLCS PWR TEST / DBU / MPO / EPU ground checks | **not implemented** | — |
| Pre-flight avionics entered by the pilot in flight (ALOW, BNGO, master arm, weapon select) | **built, but as a flown brief** — `brief_*` mission lines, issued input by input over the command bus with its latency classes and rejection reasons | [`../../pilot.md`](../../pilot.md) §2 |
| A pilot phase named `Preflight` | **exists** in the phase machine; it is a hold-and-configure state, not the 81-step checklist | [`../../pilot.md`](../../pilot.md) §3 |

**The ED "already-running spawn state" note above is the directly usable part:** it is the reference
against which FlightBox's declarative spawn defaults (radar on, RWR powered, CMDS mode) can be judged.

## Gaps

**Source gaps** (this file vs. its sources)
- The `## Knowledge` section is an explicitly **SHALLOW** research pass (LRUs + a short principle),
  marked "deepen when in scope".
- Chuck Part 4 (pp.84–118) and ED pp.132–139 + 165–176 are processed; the remaining ED cockpit
  chapters that a full cold start would need (Left/Right Console, p.43–81 remainder) are **not
  processed** — see PROGRESS.md.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* nothing of the sequence itself.
- *Partially:* the **end state** of the procedure is modelled — as declarative mission data plus an
  in-flight pilot brief over the command bus, which is a deliberate substitution, not a start-up.
- *Not at all:* electrical/JFS/engine-start chain, INS alignment and its CEP/timing, all BITs, EPU and
  DBU checks, canopy/seat/ground-crew interactions.

## Knowledge

**Technical depth (researched — shallow pass — deepen when in scope)**

*Researched engineering depth. Kept separate from the guide distillation in `## Spec`; sources cited at
the end. This pass is explicitly **shallow** — deepen when the subsystem is in scope.*

### Components (LRUs)
- **JFS** (Jet Fuel Starter): a small **hydraulic accumulator-driven turbine** in the Accessory Drive
  Gearbox; spins the engine core via the PTO shaft for start.
- **EPU** (Emergency Power Unit): **hydrazine (H-70) monopropellant** turbine — emergency hydraulic +
  electrical power (`engine-fuel.md`).
- **INS/EGI**: ring-laser-gyro INS (LN-39/93 / H-423) or EGI (see `navigation-ils.md`); needs a
  gyrocompass alignment before NAV.

### Functional principle
Start sequence is a dependency chain: battery/MAIN PWR → JFS (accumulator energy spins the core to
~20–25% RPM) → introduce fuel at IDLE → light-off → generators come online as RPM rises → avionics BIT →
INS alignment (RLG gyrocompassing to "10 RDY") → sensors/EW/HMCS. Each numbered step in the guide gates the
next on a measured parameter (RPM, FTIT, gen lights, align status), which is exactly what a start-state
machine in the sim would encode. The EPU and DBU checks at the end validate the emergency-reversion paths.

### Sources
- Wikipedia *General Dynamics F-16* (JFS, EPU hydrazine); airforce-technology.com F-16 — INS types.
- DCS guide Part 4 (81-step sequence) — cross-referenced above.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Aircraft Start/Before/After
  Engine Start p.132–139 (FLCS PWR TEST, FLCS BIT, DBU/TRIM/MPO/air-refuel/EPU checks); Inertial
  Navigation System p.165–176 (alignment types, timing, status/CEP scale).
