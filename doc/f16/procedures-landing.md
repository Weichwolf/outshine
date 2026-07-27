# F-16C Landing Procedure

Source: DCS F-16C Viper Guide (Chuck's Guide), Part 6 — Landing, pp. 126–133.
HUD element details in `hud-symbology.md`.

## Spec

### Overhead pattern (7 phases)
Initial Approach → Overhead Break → Downwind Leg → Base Turn → Final Turn → Short Final → Roll-Out.

#### 1. Initial Approach
- RADAR ALTIMETER — ON (FWD).
- Align with runway at **1500 ft AGL**, maintain **300 kts**.

#### 2. Overhead Break
- Break left/right over desired touchdown point.
- Throttle **80% RPM**; deploy **speedbrakes**.
- Break at ~**70° bank**, ~**3–4 G**.
- Align **FPM with the Horizon Line** to hold a level turn.

#### 3. Downwind Leg
- Roll out opposite landing heading, **200–220 kts**, **1500 ft AGL**.
- Extend landing gear; LANDING light UP.
- Control AoA with **throttle, not pitch trim** (FBW sets AoA). Target **11° AoA**.
- Monitor AoA via: AOA Indicator, AOA Indexer, HUD AOA Bracket (with FPM).

#### 4. Base Turn
- Begin abeam rollout point (estimate: wingtip at runway end).
- Lower nose to **8–10° pitch**, fly turn at **11° AoA**.

#### 5. Final Turn
- Throttle controls airspeed; stick maintains 8–10° nose-low + 11° AoA through the turn.
- Roll out on final, raise nose to hold glidepath: **300 ft AGL, 1 nm** from touchdown.
- Align **FPM + 2.5° pitch-ladder lines with the runway threshold** for glidepath; hold **11° AoA**.

#### 6. Short Final
- Over the overrun, shift FPM forward to a point **300–500 ft down the runway**.
- Gently flare to reduce descent rate — **do NOT level off**.
- Throttle to IDLE; touchdown at **≤ 13° AoA** (green circle).
- **> 15° AoA on rollout** risks speedbrake/nozzle striking the runway.

#### 7. Roll-Out
- Hold **13° nose-up** two-point aerodynamic braking until ~**100 kts** (F-16 wheel brakes are weak — this
  step matters).
- Reduce back stick, lower nosewheel.
- Speedbrakes fully open, full aft stick for max braking; apply moderate–heavy wheel braking.
- Engage nosewheel steering **below 30 kts**, taxi clear.

### ILS approach
See `navigation-ils.md` (Part 16 ILS tutorial): center localizer + glideslope bars on the FPM, capture,
gear down → "E" AoA bracket appears, LANDING light, speedbrake.

---

### ED EA Guide addendum — official procedure detail (pp.147–156)

ED's own overhead-pattern description cross-validates most of Chuck's numbers above, **refines the
AoA targets from single values to ranges**, and adds several technique/limit details Chuck's guide
doesn't carry. One genuine numeric discrepancy is flagged explicitly below (per this task's convention
— never silently resolved).

#### Cross-validated (both sources agree)
- Initial approach: 1,500 ft AGL, 300 KCAS (exact match).
- Break bank ~70°, throttle ~80% RPM, speedbrakes open (exact match).
- Downwind: 200–220 KCAS, 1,500 ft AGL, gear down, trim to 11° AoA (exact match).
- Short final: shift FPM 300–500 ft down the runway, flare without leveling off (exact match).
- Touchdown: max 13° AoA; **>15° AoA** risks structural strike on rollout (exact match, ED adds one
  more component at risk — see below).
- Two-point aerobraking to ~100 kt, NWS engage below 30 kt (exact match).
- Rollout on final target: ~1 nm, 300 ft AGL, FPM + 2.5° pitch-ladder marker on runway threshold (exact
  match).

#### ⚠️ Discrepancy: overhead-break G-loading
- **Chuck** (`procedures-landing.md` above, sourced from Chuck's guide): **"3–4 G"** during the level
  break.
- **ED EA Guide** (p.149, official): **"Pull into the break at approximately 3 G."**
  Both describe the same maneuver (70° bank, 80% RPM, speedbrakes out, FPM-on-horizon level turn).
  Treat **ED's "~3 G" as the primary figure** per this task's source-hierarchy convention (official ED
  module manual > third-party tutorial guide), keep Chuck's "3–4 G" as the wider envelope a real pilot
  might pull — not a contradiction severe enough to indicate a bug either way, but worth using ED's
  single number if a break-turn AI needs one commanded G value.

#### AoA targets refined from single values to ranges (ED more precise than Chuck)
Chuck's guide (and our `FBF16Pilot`'s current 11° single-value doctrine) states a flat **11° AoA**
target through downwind/base/final. ED gives the same 11° for **downwind trim**, but explicit **ranges**
for the turning/final segments:
| Phase | Chuck | ED EA Guide |
|---|---|---|
| Downwind (trim) | 11° | 11° (match) |
| Base turn | 11° ("fly the turn at 11° AoA") | **11°**, described as "the top of the AoA Bracket symbol" — same value, ED ties it to the visual cue |
| Final turn (base→final) | 8–10° pitch, 11° AoA | **11–13° AoA** (range, not fixed) |
| Rollout on final | 11° AoA | **11–13° AoA** (range) |
| Touchdown | ≤13° AoA | ≤13° AoA (match) |

**Implication for `FBF16Pilot`**: our measured on-speed CAS of 165 kt for 11° AoA is a **model
measurement**, not a documented number in either guide — neither ED nor Chuck states an approach CAS
by weight; the F-16's approach law is explicitly **AoA-referenced, not speed-referenced** (per
`flight-controls-flcs.md`'s takeoff/landing pitch-rate/AoA-command gains). Nothing here contradicts our
165 kt measurement; it simply confirms neither source gives an independent number to cross-check it
against — 165 kt @ 11° AoA remains a JSBSim-model fact (CLAUDE.md Prinzip 5), not a documentation fact.
What ED **does** newly support: our pilot could legitimately target an **11–13° AoA band** rather than
a single 11° setpoint through the base/final turn (tighter to 13° max approaching touchdown), which is
closer to real procedure than a flat 11° all the way to flare.

#### Base-turn geometry (new detail, not in Chuck)
- Lower the nose so the **FPM sits between the −2.5° and −5° pitch-ladder markers** (a pitch-ladder
  reference, not an absolute pitch-attitude number like Chuck's "8-10° pitch") — ED's convention is
  ladder-relative, Chuck's is absolute-attitude; both aim at the same physical nose-down attitude
  entering the turn.
- Bank angle: **30–45°** in the base and final turns (Chuck doesn't give a bank number for these turns
  at all — new information).

#### Touchdown strike-risk components (ED slightly more complete than Chuck)
ED: **">15° AoA during touchdown or aerodynamic braking may cause the speedbrakes, ventral fins, or
engine nozzle to contact the runway."** Chuck only lists speedbrake/nozzle — **ventral fins** is new
information from ED, worth adding to any geometric tail-strike check.

#### Aerobraking technique — Roll Indicator reference (new, not in Chuck)
ED gives a concrete instrument cross-check for holding the 13°-max aerobraking attitude: **"bisect the
Roll Indicator's lower curve with the Horizon Line, between the 0° tick mark and the pointer."** This
is a usable closed-loop pitch-attitude target for an aerobraking control law (equivalent to "hold pitch
attitude ≈ a few degrees below the nominal 13°, cross-checked against the Roll Indicator's lower arc"),
more concrete than Chuck's plain "hold 13° nose-up."

#### Crosswind landing technique (new section, not present in Chuck's landing file at all)
1. Approach wings-level, **crab into the wind** (don't decrab before touchdown).
2. At main-gear touchdown: immediately apply rudder to **remove the crab angle** and track the runway
   centerline (or use differential wheel braking).
3. Apply stick pressure **into the wind** to keep wings level through rollout.
4. Continue normal two-point aerobraking below 100 kt, using NWS only as needed to hold track.
Caution: a strong pedal input when first engaging NWS can produce an abrupt yaw — center the rudder
before engaging NWS if possible, then reapply gently.

#### Go-around rule (new, explicit trigger point)
**Go-around must be initiated before the flare**, not during/after it. Sequence: increase throttle →
arrest descent rate → establish climb → positive rate of climb confirmed → gear up → gear fully
retracted with doors closed before exceeding 300 KCAS → turn crosswind to re-enter the pattern.

#### Decision-height / missed-approach automation link (ILS-specific — see `navigation-ils.md` for the
full ILS geometry)
ED ties the CARA ALOW / MSL FLOOR advisory system (`aerodynamics-performance.md`) directly to ILS
decision-height alerting: setting **CARA ALOW = the Decision Height value** flashes "AL <n>" on the HUD
when radar altitude crosses it; setting **MSL FLOOR = Decision Height + runway threshold elevation MSL**
triggers the VMS "Altitude…altitude" voice call at the same point using barometric/system altitude
instead. Both are **existing systems already documented in `aerodynamics-performance.md`**, newly shown
here to be the mechanism a pilot uses to automate the ILS missed-approach trigger rather than relying on
watching the deviation bars alone. A useful pattern for a future FlightBox autonomous-ILS-approach
phase: use the (already-simulated) radar-altitude-floor advisory as the missed-approach decision gate.

## State

FlightBox lands autonomously — `payerne-full` flies gate-to-gate (`8cd3a74`) — but it flies a straight-in
approach, not the overhead pattern.

| Item of this reference | FlightBox | Where |
|---|---|---|
| Approach → flare → roll-out as a procedure | **built** — pilot phases `Approach`/`Flare`/`Rollout` | [`../flightbox/sim/pilot-ai.md`](../flightbox/sim/pilot-ai.md) §3 |
| Approach speed | **built, but as a speed, not an AoA**: `ApproachSpeedKt` = 165 kt `[MESS]` — the trimmed pinned model holds 11.0° AoA at 164.9 KCAS, gear down, ~40 % fuel | [`../flightbox/aircraft/f16.md`](../flightbox/aircraft/f16.md) §3.2 |
| 3° glidepath, speedbrake on approach, 50 ft flare gate, 12.5° flare attitude, 12°/100 kt aerobrake, 0.8 brake on roll-out | **built** as `FBF16Pilot` hooks (`[DOC]`/`[SET]` tagged) | same |
| Localizer-style lateral tracking | **built** — `FBAutopilot` COURSE guidance (a flown, measured approach law), not an ILS receiver | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §2 |
| Overhead pattern (initial → break → downwind → base → final) | **not implemented** — no pattern phases, no break turn | — |
| ILS approach (localizer/glideslope needles, DH, missed approach) | **not implemented** — no ILS receiver, no marker beacons; the runway is mission data | — |
| Crosswind landing technique | **not implemented** — and as of the state this table was written against, unreachable: no wind field, JSBSim's `FGWinds` unwired. The weather round (roadmap R4) is changing exactly that; **verify against `doc/flightbox/` before relying on this line** | [`../flightbox/roadmap.md`](../flightbox/roadmap.md) R4 |
| Go-around / ALOW-as-decision-height | **not implemented** — ALOW exists as a warning (`FBF16Ufc` + `FBWarningSystem`) but is not wired to any missed-approach logic | [`../flightbox/sim/systems.md`](../flightbox/sim/systems.md) §6 |

## Gaps

**Source gaps** (this file vs. its sources)
- The `## Knowledge` section is an explicitly **SHALLOW** research pass (LRUs + a short principle),
  marked "deepen when in scope".
- The **overhead-break G discrepancy stands unresolved on purpose**: Chuck 3–4 G vs. ED ~3 G — both
  values kept above, flagged ⚠️, never averaged.
- Chuck Part 6 (pp.126–133) and ED pp.147–156 are fully processed; no unprocessed pages for this topic.

**Implementation gaps** (this reference vs. FlightBox)
- *Modelled:* the descent-to-roll-out sequence, glidepath, flare gate and attitude, aerobraking,
  wheel braking.
- *Partially:* the approach is flown to a **fixed CAS**, not to the documented **AoA band 11–13°**;
  `ApproachSpeedKt` is a single number instead of a weight schedule, and a porpoise after touchdown is
  measured and open — [`../flightbox/sim/pilot-ai.md`](../flightbox/sim/pilot-ai.md) Gaps 2.5.
- *Not at all:* overhead pattern, ILS approach, decision height / missed approach, crosswind
  technique, hook/barrier and emergency variants.

## Knowledge

**Technical depth (researched — shallow pass — deepen when in scope)**

*Researched engineering depth. Kept separate from the guide distillation in `## Spec`; sources cited at
the end. This pass is explicitly **shallow** — deepen when the subsystem is in scope.*

### Components (LRUs)
- **Speedbrakes**: split panels on the aft fuselage (open ~60° each), used through the approach and
  rollout.
- **Wheel brakes**: carbon anti-skid brakes — **deliberately weak**, hence the two-point aerodynamic
  braking technique in the guide.
- **AoA sensing**: AoA probes drive the indexer/bracket and the FLCS approach law (`flight-controls-flcs.md`).

### Functional principle
The approach is flown to a constant **AoA (~11°, on-speed 13°)** rather than a fixed speed — the FLCS is in
takeoff/landing (pitch-rate) gains, and the pilot sets AoA with throttle while the FLCS holds the commanded
pitch response (`flight-controls-flcs.md`). The flare bleeds descent rate, touchdown at ≤13° AoA, then
**aerodynamic braking** (hold 13° nose-up) does most of the deceleration because the carbon wheel brakes
are ineffective until slow. Exceeding 15° AoA on rollout risks a speedbrake/nozzle strike (geometry limit).

### Sources
- Wikipedia *General Dynamics F-16* (speedbrake, brakes); DCS guide Part 6 — cross-referenced above.
- Approach AoA/FLCS gains: `flight-controls-flcs.md`, `hud-symbology.md`.
- `doc/DCS F-16C Early Access Guide EN.pdf` (ED EA Guide, official) — Descent/Before Landing, Overhead
  Break, Landing, Crosswind Landing p.147–156 (AoA ranges, break-G discrepancy, base-turn geometry,
  aerobraking Roll-Indicator technique, crosswind technique, go-around trigger, decision-height/ALOW
  link).
