# F-16C Defence — RWR & Countermeasures

**Sources** (kept distinguishable — cite the tag):
- **Chuck** = `doc/DCS F-16C Viper Guide.pdf`, Part 12 — Defence: RWR & Countermeasures, pp. 574–599.
- **ED EA Guide** = `doc/DCS F-16C Early Access Guide EN.pdf`, official module manual. Pages used this
  pass: 633–649 (Defensive Systems — RWR, CMDS, ECM), 680–683 (Appendix B — ALIC codes & RWR symbols).
  Precise on **operating-mode logic and parameter ranges** (what each CMDS mode actually does, what
  values a program parameter can take) — the primary source for rebuild-grade CMDS/RWR logic.

This file was **SHALLOW**; per task priority this pass raises it to the **same depth as `weapons.md`**
(CMDS is now in-scope, not "nice to have" — task explicitly elevates it).

## Spec

### 1. Chuck's Guide distillation (unchanged from previous pass)

#### RWR — AN/ALR-56M Radar Warning Receiver
Threat Warning Azimuth (TWA) indicator; symbols also on the HMD. Powered via the **RWR power button**
(runs a BIT). A status-change tone sounds when a new emitter symbol appears.

| Symbol decoration | Meaning | Tone |
|---|---|---|
| No circle | Radar in acquisition/search | New-threat tone |
| **Square** around symbol | Tracking / locked on you | Radar-lock tone |
| **Flashing circle** around symbol | Radar supporting a missile launched at you | Launch warning |
| **Diamond** | Highest threat level | — |
| "U" / "UKN" | Unknown emitter | — |

Position on the scope = relative bearing (own nose = top); distance from center ≈ threat proximity.

- **TWP (Threat Warning Prime) panel**: HANDOFF (mode select, not simulated in Chuck's era), **T
  (Target Separation)**, MISSILE LAUNCH light, UNKNOWN SHIP toggle, System Test, Mode Selector.
  RWR display modes: **PRIORITY** (5 highest threats) / **OPEN** (16 highest).
- **TWA auxiliary panel**: RWR source switch (enables RWR data for CMDS SEMI/AUTO), Jammer source
  switch, **SEARCH** button ('S' search-radar symbols), **LOW ALTITUDE** button, ACT/PWR indicator.

#### CMDS — AN/ALE-47 Countermeasures Dispenser System
- **Chaff**: passive radar-reflective. **Flares**: heat decoys. FCDs in the body fairing; ground crew
  sets loadout, **max 120 combined** (typical 60 chaff / 60 flare).
- **6 programs**: PRGM knob selects 1–4; **PRG 5** = slap switch (left sidewall, always); **PRG 6** =
  Bypass program. 1–5 programmable via DTC/UFC only when CMDS mode = STBY.
- **CMDS mode knob**: OFF / **STANDBY** (reprogram only) / **MAN** (CMS fwd dispenses selected program)
  / **SEMI** (system picks program by threat, CMS aft = consent) / **AUTO** (auto pick+dispense, CMS
  aft = enable, CMS right = disable) / **BYP** (manual 1 chaff + 1 flare on failure).
- **CMS switch (stick)**: FWD = dispense manual program, AFT = consent/enable SEMI/AUTO, RIGHT =
  disable AUTO, slap button = PRG 5. **Jettison** switch dumps CM even with CMDS OFF.
- CH/FL counters, **LO** = low quantity. **MWS not functional on Block 50** (per Chuck).

---

### 2. ED EA Guide — official system logic (primary source for rebuild)

#### 2.1 RWR — AN/ALR-56M (ED EA Guide p.634–637)

**Antenna geometry & coverage** (ED, precise): 2× high-band antennas + a dual-blade low-band antenna
per side (4 high-band total), giving **360° azimuth coverage but only ±45° in elevation**. This creates
a genuine **RWR blind spot directly above/below the fuselage centerline** — high-pitch or high-bank
defensive maneuvers can rotate a hostile radar into that blind spot, **silently dropping lock/launch
warnings**. In SEMI/AUTO CMDS modes this **also stops the ECM pod from emitting** for the duration the
threat sits in the blind spot (§2.2/2.3 below) — a compounding, not independent, vulnerability window.
**Rebuild-relevant**: the RWR is explicitly NOT omniscient — it only reports *detected emissions*, never
whether the threat radar can actually see ownship, nor whether it's tracking ownship vs. another
aircraft on the same bearing. A faithful simulation must model this as a **geometry-gated emission
detector**, not a ground-truth threat oracle.

**Threat Warning Azimuth Indicator** — a circular top-down azimuth-only display, own-ship at center,
symbol distance from center = **relative lethality** (not physical range):
| Radar state | Symbol position/decoration |
|---|---|
| Search/Acquisition | outer ring, plain symbol |
| Tracking | just outside the solid white circle, boxed |
| Missile guidance | inside the solid white circle, flashing circle |
| Airborne platform | chevron overlay on symbol |
| Highest-priority threat | diamond overlay (continuously re-evaluated) |

Symbol *type* is looked up from a threat library (see Appendix B, §3 below); a new-threat detection
plays an audio tone over the THREAT channel; track/missile-guidance transitions get distinctive tones.

**Threat Warning Prime panel** (ED, precise button semantics):
- **MODE** button: **OPEN** (16 highest-priority threats shown) vs **PRIORITY** (5 highest) — this is
  a hard display cap, not a detection-range limit; more threats can exist than are shown.
- **LAUNCH** button flashes "MISSILE LAUNCH" when any threat radar enters missile-guidance mode.
- **TGT SEP**: radially separates visually-overlapping symbols for **5 seconds**, then symbols snap
  back — a fixed, documented timer.
- **UNKNOWN** button: toggles display of unclassifiable emitters as "U"; when disabled, the button's
  "U" indicator **flashes** (rather than showing nothing) if unknown signals are present but hidden —
  i.e. suppression is visually distinguishable from absence.
- **SYS TEST**: hold 1 s → full panel self-test (all lights + diagnostic symbols/tones), abort by
  pressing again.

**Threat Warning Auxiliary panel**:
- **SEARCH** toggle: shows/hides early-warning/surveillance/non-lethal-acquisition radar symbols
  (a separate class from the lethal/priority set); when both SEARCH and LOW ALTITUDE modes are active,
  their center-screen "S"/"L" indicator glyphs **alternate ("nipple") rather than overlap** — a
  documented shared-display-slot mechanism.
- **ALTITUDE** toggle: **High-Altitude mode** prioritizes long-range/high-altitude SAM+fighter threats;
  **Low-Altitude mode** prioritizes short-range/low-altitude threats. This changes *threat prioritization
  logic*, not detection capability — i.e. it's a re-ranking function over the same detected-emitter set.
- **POWER** toggles the whole ALR-56M; **ACT/PWR** button shows POWER (powered) vs ACTIVITY (any threat
  currently in track/missile-guidance mode) as two independently-lit states on one button.

#### 2.2 CMDS — AN/ALE-47 (ED EA Guide p.638–643)

**CMDS Control Panel** (Left Auxiliary Console):
- **Status display** 3-state: **NO GO** (powered, malfunctioned) / **GO** (ready) / **DISPENSE RDY**
  (ready + awaiting pilot consent in SEMI, with an optional "Counter" voice prompt).
- **Quantity display**: per-type remaining count; **"LO"** appears once a type is at/below its
  **BINGO threshold** (pilot-set per type on the CMDS BINGO DED page, valid range **0–99**, with an
  optional "Low"/"Out" voice message).
- **O1/O2/CH/FL switches**: only **CH** (chaff) and **FL** (flare) are functional on this airframe —
  O1/O2/JMR/MWS switches are present but **no function** (matches Chuck's Block-50 MWS note).
- **JETT switch**: dumps **all** onboard countermeasures simultaneously, **regardless of CMDS MODE
  knob position** — an unconditional override, not gated by mode.
- **PRGM knob**: selects which of Manual Programs 1–4 is dispensed by CMS Forward. Programs 5 (slap
  switch) and 6 (CMS Left) are **always** available regardless of PRGM knob position.

**CMDS mode knob — precise per-mode CMS/ECM interaction** (this is the load-bearing state machine ED
documents in full, and the deepest gap in the previous SHALLOW pass):

| Mode | Dispensing | ECM interaction |
|---|---|---|
| **OFF** | none (JETT switch still works) | ECM pod emissions **disabled** |
| **STBY** | none (JETT switch still works); DED reprogramming **only** allowed here | ECM pod emissions **disabled** |
| **MAN** | CMS Fwd = selected Manual Program (1–4 per PRGM knob, or 5/6 directly); no Automatic programs | CMS Aft **activates** ECM noise jamming if XMIT switch = position 3; CMS Right deactivates |
| **SEMI** | CMDS auto-selects the appropriate Automatic program per threat, but **requires CMS Aft consent per dispense**; re-prompts after each program completes; Automatic dispensing **withheld if chaff is LO** | CMS Aft **enables** ECM deception jamming (XMIT 1/2), *only* while actively locked by a hostile radar; CMS Right disables |
| **AUTO** | CMDS auto-selects **and** repeats the selected Automatic program **continuously** while locked, once consent (CMS Aft) is given; **consent defaults to granted every time the knob is turned to AUTO**; Automatic dispensing withheld if chaff is LO | ECM deception jamming (XMIT 1/2) fires **without per-instance consent** any time locked, provided ECM power = OPR; CMS Right revokes consent and interrupts any in-progress program |
| **BYP** | CMS Forward → exactly **1 chaff + 1 flare**; Programs 1–6 and other CMS functions unavailable | in-progress deception jamming continues until the current threat clears, then goes standby |

**Key rebuild facts**: (1) SEMI requires **per-dispense** consent, AUTO requires consent only **once
per mode-entry** then free-runs while locked — these are meaningfully different state machines, not a
one-bit "auto vs manual" toggle. (2) Automatic (not Manual) programs are the only ones gated by chaff-
LO status. (3) ECM jamming activation is **layered on top of** CMDS mode+consent, gated additionally
by the ECM XMIT switch position (see §2.3) — three independent gates (CMDS mode, consent, XMIT
position) must all align for jamming to fire.

**CMDS BINGO DED page** (ED, precise): Chaff/Flare Bingo quantities **0–99** each; independent
Feedback ("Chaff flare" on any dispense), Request-Countermeasures ("Counter" — SEMI consent prompt),
and Bingo ("Low"/"Out") voice-message toggles. Editable **only** with CMDS MODE = STBY.

**CMDS CHAFF/FLARE DED pages** (ED, precise — the actual per-program parameter schema): each of the 6
Manual Programs independently defines, **per countermeasure type**:
| Parameter | Range | Meaning |
|---|---|---|
| **Burst Quantity (BQ)** | 0–99 | cartridges dispensed within one salvo |
| **Burst Interval** | 0.020–10.000 s (0.001 s steps) | time between cartridges within a salvo |
| **Salvo Quantity (SQ)** | 0–99 | number of salvos in the program |
| **Salvo Interval** | 0.50–150.00 s (0.01 s steps) | time between salvos |

Setting BQ or SQ to 0 for a type **removes that type from the program entirely** — e.g. a program can
be flare-only or chaff-only by zeroing the other type's burst/salvo quantity. This is the precise
schema a FlightBox CMDS model needs to reproduce a "program" as data, not hand-tuned behavior.
Program 5 = CHAFF/FLARE Dispense button (left cockpit wall, independent of the SSC CMS switch).
OTHER1/OTHER2 DED pages exist but **have no function** — chaff/flare are the only expendables modeled.

#### 2.3 Electronic Countermeasures (ED EA Guide p.644–649)

**Physical principle** (ED's own explanation, consistent with public EW literature — high confidence):
a radar depends on receiving its own reflected pulse strongly enough to process range/position against
noise/clutter. **Noise jamming**: saturate the victim's receive band with matching-frequency energy to
deny ranging — effective but **self-reveals** the jammer's presence/bearing even before native
detection ("barrage jamming"). **Deception jamming**: analyze the victim signal and retransmit a
precisely-matched, stronger signal to inject **false range/position data** or corrupt automatic
tracking — can be used intermittently to stay covert until needed. **Burnthrough**: once the true
radar return (which strengthens as range closes, inverse-square-ish) exceeds the jammer's signal
strength at the victim receiver, the radar "sees through" the jamming — burnthrough range is
radar-system-specific, not a fixed number.

**ECM Control Panel**:
- **ECM Power switch**: OFF / **STBY** (powered, ~3-minute warm-up, no processing/jamming) / **OPR**
  (powered + jamming per XMIT setting; if warm-up wasn't complete when switched from STBY→OPR, jamming
  is **withheld until warm-up finishes**, even though the switch already reads OPR).
- **XMIT switch — three modes, each with a DIFFERENT required CMDS-mode gate**:
  | XMIT | Behavior | FCR side-effect | Required CMDS mode |
  |---|---|---|---|
  | **1** (Deception, Avionics Priority) | reactive deception jamming when tracked/engaged | FCR keeps operating, but detection/lock range **reduced** | SEMI or AUTO |
  | **2** (Deception, ECM Priority) | reactive deception jamming when tracked/engaged | FCR forced to **standby**, unless current weapon profile = AIM-120 (then behaves like mode 1) | SEMI or AUTO |
  | **3** (Noise, ECM Priority) | **continuous, preemptive** noise jamming | FCR forced to **standby** | **MAN only** |
  This is a genuinely intricate cross-system gate: the ECM mode determines whether the *own* FCR stays
  usable, and which CMDS mode is a hard prerequisite for the pod to emit at all — three interacting
  state machines (ECM XMIT, CMDS mode, FCR availability), not one.
- **Manual Band Control buttons** (1–5 + growth slot): per-module enable/disable, 4-state status light
  per module (**S**tandby / **A**ctive / **F**ailed / **T**ransmitting). ED notes explicitly that in
  DCS **all band-module selections and both pod types (ALQ-131/ALQ-184) behave identically** —
  i.e. this is documented as a **simulation simplification**, not modeled per-real-pod fidelity; worth
  knowing if FlightBox ever wants to differentiate pod behavior beyond what DCS itself models.

#### 2.4 Hands-On Controls (ED EA Guide p.649)
CMS (4-way, SSC) is the sole stick-mounted defensive control; its FWD/AFT/RIGHT/Left-slap semantics
are fully mode-dependent per the CMDS-mode table in §2.2 (MAN/SEMI/AUTO each remap the same 4
directions to different actions — already tabulated above, not repeated). The **CHAFF/FLARE Dispense
button** (left cockpit wall, above throttle) is a **hardware-independent** path to Program 5, usable in
MAN/SEMI/AUTO regardless of SSC CMS state.

### 3. Appendix B — ALIC codes & RWR symbols (ED EA Guide p.680–683)

Structural note only (the full table is large — dozens of emitter entries — and belongs in a future
threat-database extraction pass, not hand-transcribed here): ED provides a lookup table mapping
**ID** (ALIC numeric/alpha code, used to program HARM/HTS threat tables, §weapons.md 2.5) ↔ **RWR**
(the symbol shown on the ALR-56M/HAD/WPN displays) ↔ **NATO reporting name** ↔ **system** ↔ **radar
designation** ↔ **Type** (functional class: CWAR/EWR/FCR/RR/SR/STR/TAR/TI/TTR — Continuous-Wave
Acquisition / Early Warning / Fire Control / Ranging / Surveillance / Search-and-Track / Target
Acquisition / Target Illumination / Target Tracking Radar respectively), covering air-defense radars
(SA-2 through SA-19 families + associated EWR/SR sets), naval radar systems, airborne radar systems,
and other threat symbols (Appendix B tables — pp. 680–683). **TODO (future pass)**: transcribe the
full ID↔RWR↔system table into a structured reference if/when the RWR/threat-classification system is
actually implemented — not needed for the current flight+rendering scope.

## State

Both halves are built: the RWR as the **exact mirror image of the radar** (it asks "who is looking at
me", reading only published emissions), and the CMDS as a real program state machine.

| Item of this reference | FlightBox | Where |
|---|---|---|
| RWR as a passive receiver on published emissions | **built** — `FBRwrSystem` reads the emitter signature other units publish and checks two geometries: does the sender's beam cover this jet, and can this jet's own antenna hear from that direction | [`../flightbox/sim/sensors.md`](../../sensors.md) §5 |
| **No range on the RWR** | **built as a property**: a threat carries relative bearing, mode, estimated emitter class, a "new" window and lethality — never a distance, because a receiver measures power, not range | same |
| AN/ALR-56M antenna geometry | **built** — 360° azimuth but only ±45° elevation, i.e. a real blind zone that own manoeuvring opens up | [`module.md`](module.md) §6 |
| PRIORITY / OPEN display caps | **built** as an explicit *display* limit over a detection that keeps running | same |
| ALE-47: magazine, programs, salvo/burst schema | **built** — 60/60 magazine (120 combined max), BINGO 10/10, six programs whose *schema* comes from §2.2 and whose *values* are marked `[SET]` | [`../flightbox/sim/sensors.md`](../../sensors.md) §6 |
| CMDS mode state machine OFF/STBY/MAN/SEMI/AUTO/BYP | **built**, including consent semantics: SEMI asks per dispense, AUTO once per mode change and then repeats itself; automatic dispensing stops at chaff BINGO | same |
| Dispensing over the command bus (CMS) | **built** — `CmDispense` / `CmConsent` / `CmdsMode`, rejectable (`depleted` on an empty magazine) | same |
| Chaff that actually works | **built** — each cartridge leaves a cloud with an ageing curve at the release position; whether it *defeats* anything is decided by the opposing radar's Doppler notch, never by a probability roll | same, §4 |
| SEMI/AUTO triggering on the **warning**, not on the truth | **built** — what stands in the RWR blind zone gets no answer | same |
| Flares | **counted, ineffective** — there is no IR seeker for them to defeat, and the code says so | same, Gaps 2 |
| ECM / jammer, ECM-XMIT interaction with CMDS modes, burnthrough | **not implemented** — the whole §2.3 interaction is absent | same, Gaps 7 |
| MWS / missile approach warning | **not implemented**; a launch is noticed only through a supporting radar or an active seeker in the beam — which matches the source (MWS not functional on Block 50) | same, Gaps 8 |
| Threat library (ALIC codes, symbol table) | **not implemented** — classification passes the emitter class through, so the estimate is always right; Appendix B is not transcribed by the source either | same, Gaps 6 |

## Gaps

**Source gaps** (this file vs. its sources)
- §4.3 above ("What remains a genuine gap") is the itemised list, left in place under Knowledge so its
  numbering stays citable.
- **Appendix B (ALIC codes & RWR symbols, ED pp.680–683) is summarised structurally, not transcribed** —
  §3 above says so explicitly; no symbol/code correlation table exists in this file.
- ED pp.633–649 and Chuck Part 12 are processed.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* RWR geometry and its blind zone, threat presentation without range, the display caps, the
  ALE-47 magazine/program/mode machine, chaff as a physical cloud judged by the opposing radar.
- *Partially:* the RWR — azimuth blanking by the own airframe is not modelled (360° really is 360°), the
  "new"-window is derived from a continuously radiating volume rather than pulsed illumination, and one
  receiver-sensitivity constant serves every emitter class.
- *Not at all:* ECM/jammer and its interaction with CMDS modes, MWS, the threat/ALIC library, flare
  effectiveness, expendable types beyond chaff/flare.

## Knowledge

### 4. Technical depth (researched + derived — deepened this pass, no longer SHALLOW)

*Section number kept for cross-reference stability (`§2.1`, `§2.2` and `§4.x` are cited from the code.)*

Confidence tiers: **T1** official/declassified mil docs · **T2** manufacturer datasheets · **T3**
established literature/databases (FAS, GlobalSecurity, DTIC-adjacent) · **T4** community/wiki
(cross-check only, flagged).

#### 4.1 AN/ALR-56M — components & coverage
- **Architecture** (T3, FAS/man.fas.org "AN/ALR-56M Radar Warning Receiver"): fast-scanning
  superheterodyne receiver + superhet controller + analysis processor + low-band receiver/power supply
  + **four quadrant (high-band) receivers**. Matches ED's own antenna diagram (§2.1) exactly — 2 high-
  band antennas per side + 1 dual-blade low-band antenna, consistent with a 4-quadrant high-band
  architecture. High confidence (T3 + ED agree).
- **Frequency coverage**: **2–20 GHz** superheterodyne (T3, FAS). This is a broadband coverage figure,
  not a per-quadrant beamwidth; the ED-documented **±45° elevation limit** (§2.1) is a antenna/geometry
  fact, not a frequency-coverage fact — the two are independent constraints and both apply.
  - **Platform lineage**: ALR-56M is a form/fit-compatible, miniaturized derivative of the F-15's
    ALR-56C, developed as a **drop-in replacement for the ALR-69** on F-16 Block 40+ (T3, FAS/
    Microwave Journal). Not independently load-bearing for FlightBox simulation, but useful context
    for why the antenna/coverage figures resemble the F-15 family's.

#### 4.2 AN/ALE-47 — capacity & cartridge facts
- **Physical capacity**: T3/T4 sources (GlobalSecurity AN/ALE-47 page, Wikipedia) describe payload
  modules divided into **3 zones of 10 cartridges each per dispenser module** (≈30/module), with total
  aircraft-installed capacity commonly cited in the **32–120 round range depending on airframe/
  cartridge-type configuration**. This is **consistent with, but not more precise than**, ED's/Chuck's
  own **"max 120 combined (typical 60 chaff / 60 flare)"** figure already carried in this file (§1) —
  keep ED/Chuck's number as primary since it's F-16C-Block-50-specific, treat the general ALE-47
  capacity range as T3/T4 cross-check context only.
- **Cartridge types**: T3/T4 (TARA Aerospace, GlobalSecurity) name **RR-129/144/170/180/188/196**
  chaff-cartridge form factors and **M206/XM211/XM212** flare types as the ALE-47's expendable family;
  **exact F-16C Block-50 cartridge type/mix is not independently confirmed this pass** — treat as
  general ALE-47-family context, not a confirmed F-16C loadout spec. **TODO**: an AFTO/-21 loading
  manual excerpt would settle this; not found in this pass's budget.
- **Program-parameter ranges** (burst/salvo quantity/interval, §2.2 table) are **ED-primary and
  precise** — no independent re-verification needed or attempted; these are DCS's own simulated
  values and, per the task's source-hierarchy guidance, a design/doctrine decision (not physically
  derivable), so a gap here would be marked, but ED already provides the complete, load-bearing schema.

#### 4.3 What remains a genuine gap (not guessed)
- Full ALIC/RWR/threat-system correlation table (Appendix B) — structurally described (§3) but not
  transcribed; needed only once threat classification is actually implemented.
- Confirmed F-16C Block-50 CMDS cartridge type/mix (RR-1xx chaff variant, M206 vs XM21x flare variant)
  and per-dispenser physical zone layout — general ALE-47-family data found, airframe-specific loadout
  not confirmed.
- Burnthrough-range formula/threshold per specific threat-radar class — ED explains the *concept*
  precisely (§2.3) but (correctly) does not commit to per-threat numbers, since burnthrough range is a
  function of the emitting jammer's ERP vs. the victim radar's receiver sensitivity and antenna gain,
  neither of which is public per-threat-system data.
- **Gym note**: once RWR/CMDS/ECM are implemented as actual systems (currently out of FlightBox's
  flight+rendering scope per CLAUDE.md), several of the above (e.g. actual chaff/flare ballistic
  dispersal, seduction-probability curves) become measurable in `fb-gym` telemetry rather than
  needing a literature citation — noting this for the later combat-systems phase, not the current one.

### Sources
- **ED EA Guide** (primary this pass): pp. 633–649 (§2 above), pp. 680–683 (§3 above).
- **Chuck's Guide**: Part 12, pp. 574–599 (§1 above, unchanged from previous pass).
- T3 web research (§4): man.fas.org / GlobalSecurity.org "AN/ALR-56M Radar Warning Receiver",
  Microwave Journal "AN/ALR-56M Radar Warning Receiver Safeguards F-16...", GlobalSecurity
  AN/ALE-47 page, Wikipedia AN/ALE-47, TARA Aerospace chaff/flare cartridge pages (T4, cross-check
  only, flagged inline).
