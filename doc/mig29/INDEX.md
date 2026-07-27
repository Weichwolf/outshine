# MiG-29A (9-12) Systems Reference — Index

The MiG-29 counterpart to [`doc/f16/`](../f16/INDEX.md). Purpose: enough distilled, cited system
knowledge to build **a JSBSim model and a FlightBox `FBModule`** for the Fulcrum — not a player guide.

**Variant baseline: MiG-29 izdeliye 9-12 (Fulcrum-A).** This is the DCS full-fidelity module's variant
and the simplest form of the type. Later variants (9-13 / MiG-29S, SMT, MiG-35) get a **short variant
note at the end of each file** — never a parallel description.

---

## Source situation (read this before trusting any number)

Two source documents, kept distinguishable in every file. **Cite the tag, never a bare page number.**

| Tag | Document | Pages | Character |
|---|---|---|---|
| **DCS-EA** | `doc/DCS MiG-29A Early Access Manual EN.pdf` (Eagle Dynamics, 2025) | 115 | The **full-fidelity 9-12 module** manual. Real switch names, real system designations, precise procedures and — in the SPO-15 chapter — an unusually deep, physically argued description of an analogue device. **Printed page == PDF page.** |
| **DCS-FM** | `doc/DCS MIG-29 Flight Manual EN.pdf` (Eagle Dynamics, 2018) | 116 | The **Flaming Cliffs 3** manual for the simplified MiG-29A/S. Rich on *concepts* (radar physics, notch, HUD symbol meaning, voice warnings) but its avionics model is deliberately abstracted, and several of its numbers are **ED design decisions**. **Printed page = PDF page − 6.** |

> ⚠️ **Page-numbering trap inside this directory.** DCS-FM has six roman-numbered front-matter pages,
> so its printed and PDF page numbers differ by 6. The two files were written in parallel by different
> agents and **do not use the same convention**:
> - `flight-controls.md`, `engines-fuel.md`, `cockpit-displays.md`, `radar-sensors.md`,
>   `defence-rwr-cm.md`, `navigation.md`, `procedures.md`, `datalink-gci.md`, `PROGRESS.md` →
>   **printed page** (`DCS-FM p.14` = the TTD table).
> - `weapons.md`, `flight-model-spec.md` → **PDF page** (`DCS-FM p.20` = the same TTD table).
>
> **DCS-EA has no front matter offset — printed == PDF — so it is unambiguous everywhere.**
> Reconciling the two conventions is an open item in [PROGRESS.md](PROGRESS.md).

**Three standing cautions:**
1. **The two manuals contradict each other in at least four places** — the SPO-15 threat-type letters
   (`defence-rwr-cm.md` §2.4), waypoint auto-sequencing (`navigation.md` §1), the tachometer's 100 %
   meaning (`engines-fuel.md` §2), and radar-altimeter range units (`navigation.md` §3.2). Every one is
   flagged in place and **never silently resolved**.
2. **Both manuals describe a game module.** Where a statement smells like an ED modelling decision rather
   than documented jet behaviour (the FC3 "MLWS" missile-approach warner, the 1,400 L drop tank, the
   Stock/Automatic RWR threat-programme selector, the "Fuel 1500/800/500 pounds/litres" thresholds), it
   is marked ⚠️ as such.
3. **Everything researched is confined to a "Technical depth" section per file** and tiered:
   **T1** official/declassified military documents · **T2** manufacturer data · **T3** established
   literature (Jane's, Butowski) · **T4** community/encyclopaedic consensus. Public MiG-29 sourcing is
   markedly thinner than the F-16's — **most researched numbers here are T4**, and that is stated rather
   than dressed up.

---

## Files

| File | Content | Depth | Sources |
|---|---|---|---|
| [flight-controls.md](flight-controls.md) | **Conventional (non-FBW) control system**: mechanical run + irreversible hydraulic boosters; **ARU-29-2** gear-ratio changer; **SOS-3M** AoA/g limiter (the cockpit's "COC"); surface areas and deflection limits; trim authorities; the complete **SAU-451 AFCS** mode logic with engage/disengage conditions; flap/slat/speedbrake logic | **FULL** on AFCS, **MEDIUM** on architecture, **SHALLOW** on gains/schedules | DCS-FM 9–14, 33–36; DCS-EA 19, 52, 55, 57, 60, 64, 68–69, 73–74, 77 + research |
| [engines-fuel.md](engines-fuel.md) | Two **RD-33**: thrust, cycle data, cockpit instrumentation and its limits; the **twin-path intake system** (main ramps + dorsal louvres, the ~108 kt changeover); start/shutdown/relight; **tank-by-tank fuel capacities**, ISTR4 gauge logic, BINGO/voice thresholds | **FULL** on procedure+instruments, **MEDIUM** on cycle data, **SHALLOW** on spool/AB limits/lapse | DCS-FM 9–11, 14, 28–29, 81–82; DCS-EA 25–30, 50, 58, 72–78 + research |
| [cockpit-displays.md](cockpit-displays.md) | Every instrument with its **designation, range and graduation**; the **TLP → MASTER CAUTION → AEKRAN → VIWAS** warning chain (a genuine priority queue with pilot acknowledgement); **HUD/HDD (SEI-31)** FOV and symbol sets; the HUD cue vocabulary (А/ПР/ОТВ/Г/РЛ/ТП/ПП/АП); full left/right console inventory; **HOTAS** switch list; R-862 radio | **FULL** on inventory + warnings, **SHALLOW on HUD geometry** (biggest gap in the set) | DCS-EA 7–69; DCS-FM 21–36, 40–61, 104–105 |
| [radar-sensors.md](radar-sensors.md) | **N019 "Rubin"**: mode taxonomy (RAD/CC/HEAD-ON/P/AUTO), the **quantified Doppler-notch thresholds**, scan geometry, range-scale switching, ECM/AOJ/burn-through, RCS-dot symbology; **and the OLS story** — **OEPS-29 / KOLS IRST + laser rangefinder + Shchel-3UM helmet sight** as a first-class *passive* channel, with the doctrine consequences spelled out | **FULL** on modes + IRST doctrine, **MEDIUM** on scan geometry | DCS-EA 12–13, 37, 59, 63, 69, 86–97; DCS-FM 12–13, 37–39, 43–57, 83–90 + research |
| [defence-rwr-cm.md](defence-rwr-cm.md) | **SPO-15LM "Beryoza"** in full: antenna geometry and the resulting azimuth resolution, threat-type classification, the **six-level priority logic** and its hard-coded 26,000–55,000 ft altitude assumption, and **eleven documented analogue limitations** — including *"when the onboard FCR is radiating, the forward hemisphere is completely disabled"*; **BVP-30-26** dispensers; Gardeniya ECM (9-13 only) | **FULL** on the RWR, **MEDIUM** on dispensers | DCS-EA 24, 33–34, 69, 105–112; DCS-FM 13, 30–32, 62, 104 + research |
| [navigation.md](navigation.md) | **SN-29 / A-323 "PION"** RSBN navigation: the 9-point store, the complete panel, **manual** waypoint sequencing, the CORR (radio-correction) dependency; **ADF (ARK-19)**; **A037 radio altimeter**; the **RETURN / LANDING / MISSED-APPROACH** state machine with all engage conditions, the descent profile table and the altitude plateaus; PRMG landing beacons | **FULL** on modes, **MEDIUM** on infrastructure, **SHALLOW** on INS/drift | DCS-EA 16–18, 23–25, 41–43, 45, 63, 81–84; DCS-FM 12–13, 23, 26, 41–42 + research |
| [procedures.md](procedures.md) | Preflight (25 steps) · start · post-start (17 steps) · taxi · normal/AB/crosswind takeoff · normal/crosswind landing · **brake chute rules**. Opens with a **one-table quick reference** of every phase number a pilot module needs | **FULL** on the procedure chapter, **SHALLOW** on performance tables | DCS-EA 60, 71–80; DCS-FM 81–82 |
| [datalink-gci.md](datalink-gci.md) | **The Lazur/Biryuza GCI story** and why it is the anti-Link-16: hardware evidence (all of it "not implemented"), the **voice-BRAA → manual-radar-cueing loop** that *is* documented and *is* implementable, IFF's identity monopoly, and a separated **design argument** for a `FBDatalinkSystem` override whose payload is a *vector*, not a track list | **SHALLOW on hardware** (honestly — the module doesn't implement it), **MEDIUM on doctrine** | DCS-EA 38, 48–49, 51, 59, 113; DCS-FM 12–13, 30–31, 43–44, 83–86, 99–100 + research |
| **[weapons.md](weapons.md)** | *(owned by the parallel agent)* GSh-301 gun, R-27/R-73/R-60 family, unguided rockets and bombs, station/pylon data, employment and release computation | — | — |
| **[flight-model-spec.md](flight-model-spec.md)** | *(owned by the parallel agent)* The JSBSim-model specification: geometry, mass, aero, engine tables, the envelope to be reproduced and measured | — | — |

Coverage ledger: **[PROGRESS.md](PROGRESS.md)**.

---

## Priority for a MiG-29 build (highest first)

1. **[flight-model-spec.md](flight-model-spec.md)** + **[flight-controls.md](flight-controls.md)** —
   nothing else can be validated until the aircraft flies. The critical unknowns are named in
   `flight-controls.md` §9: the **ARU gear-ratio schedule** and the **stick-force gradients**.
2. **[procedures.md](procedures.md)** — its §1 quick-reference table is directly consumable by an
   `FBPilot` phase machine, and its numbers differ enough from the F-16's that reusing the F-16 schedule
   would be simply wrong.
3. **[engines-fuel.md](engines-fuel.md)** — the intake changeover and the idle/climb RPM points are
   procedure-visible; the **spool times are the blocking gap**.
4. **[radar-sensors.md](radar-sensors.md)** — the sensor pair (active radar + passive IRST/helmet) is
   the type's defining tactical feature and drives the whole pilot doctrine.
5. **[defence-rwr-cm.md](defence-rwr-cm.md)** — the SPO-15's limitations are *modelling requirements*,
   not colour; several of them (own-radar blanking, per-channel track marking, CW/HPRF confusion)
   create exploitable, deterministic behaviours.
6. **[navigation.md](navigation.md)** and **[cockpit-displays.md](cockpit-displays.md)** — needed for a
   complete module, but neither blocks first flight.
7. **[datalink-gci.md](datalink-gci.md)** — the doctrine is settled, the hardware is not sourced; the
   voice/manual-entry loop (§2.2) is implementable now and the datalink can wait.

---

## FlightBox relevance — what this airframe changes

- **No fly-by-wire.** The F-16 module's central abstraction (`fcs/fbw-override`, an FLCS that *is* the
  control system) has no counterpart. The MiG-29's pilot commands **surface position through a scheduled
  gear ratio**; the automation (ARU, SOS-3M, SAU-451) sits *beside* the mechanical run and can be
  switched off. See `flight-controls.md` §1.
- **A passive primary sensor.** KOLS gives search, track *and* (within 6 km) true range without
  radiating, and the helmet sight can hand a target to a missile seeker without pointing the aircraft.
  The cost is **range and identity**: the IRST has no IFF. This inverts the F-16 module's BVR cost
  function from "a lock warns the target" to "**radiating at all** warns the target". See
  `radar-sensors.md` §6.4.
- **An RWR that lies in documented ways.** The SPO-15 model is not "detector + priority sort" but "a set
  of analogue receivers whose physics leaks into the display" — including a **forward-hemisphere
  blackout whenever the own radar transmits**. See `defence-rwr-cm.md` §2.7.
- **Ground-controlled by design.** The compensating asset for a short-legged radar is a controller on the
  ground. FlightBox's `FBDatalinkSystem` slot fits it without new architecture, but the payload is a
  **steering vector with human entry latency**, not a track list. See `datalink-gci.md` §5.
- **Manual waypoint sequencing and no in-cockpit coordinate entry.** The route is loaded on the ground
  from a data cartridge; the pilot selects among nine stored points and presses "next" himself. That is
  a *pilot action over the command bus*, not a `FBNavSystem::AdvanceWaypoint` housekeeping step. See
  `navigation.md` §1.
- **A brake chute as a normal-procedure, decision-gated device** with a 175 kt separation speed and six
  mandatory-use cases. See `procedures.md` §7.3.
- **A warning system that is itself a command channel.** AEKRAN's priority queue requires per-message
  pilot acknowledgement — a natural fit for the existing command/acknowledge pattern. See
  `cockpit-displays.md` §3.

---

## Not distilled (and why)
- **DCS-FM pp. 1–8** (MiG-29 history) and **pp. 15–19** (Game Avionics Mode) — no rebuild value; the
  Game Avionics Mode is an arcade overlay, explicitly not the aircraft.
- **DCS-FM pp. 94–103** (radio command menus, wingman/ATC/tanker phraseology) — folded only where it
  carries data: the AWACS/GCI BRAA protocol is in `datalink-gci.md` §2.1 and the ATC glide-path calls in
  `procedures.md`. The wingman-command tables are DCS UX.
- **DCS-FM pp. 63–80** and **DCS-EA pp. 86–104 (weapons/employment)** — **owned by
  [weapons.md](weapons.md)** (parallel agent).
- **DCS-EA pp. 113–115** (abbreviations) — used as a lookup, not distilled.
