# MiG-29 — the first opponent

**Status: stages 1, 2a, 2b, 2c, 3, 4 and 5 built, plus the value round that closes gaps 4g and 4h(a) —
the airframe, the module, the sensor suite (N019 / SPO-15 / KOLS), the GCI loop, the WEAPONS (R-27R /
R-73 / GSh-301) plus the radar cross-section, the DUEL ([`../../duels.md`](../../duels.md)), and now the
two pieces the merge needed: the N019's broad ACM auto-lock volume (the MiG ACQUIRES in a turning merge,
`duel-merge` lock_s 0→14.2) and the BVP-30-26 dispensers (flares seduce the AIM-9 by the same
deterministic model that seduces the R-73 — the defensive asymmetry is now two-sided).** This is the
first file written the new way round — the contract exists before the code, and the build rounds are
measured against it instead of describing themselves afterwards.

**Neighbours:** `doc/modules/mig29/` (the knowledge base about the real aircraft, from the two DCS manuals plus
research — roadmap R3, in progress), [`../f16/module.md`](../f16/module.md) (the module pattern this one will follow),
[`../sim/sensors.md`](../../sensors.md) and [`../../weapons.md`](../../weapons.md)
(the systems it plugs into), [`../roadmap.md`](../../roadmap.md) (R3 → R6 → R7 → R8, the chain this file
depends on).

## Spec

### Scale — the deliberate one

The project scale is **staggered**: the F-16 exactly (it is the product), sensors, weapon envelopes
and the course of an engagement believable (this is where the game happens), enemy aircraft **hit
their envelope and nothing more**. The MiG-29 is explicitly a **BVR-scale** opponent: you usually do
not see him, so what must be right is what he can reach, how fast he gets there, what he can see and
what he can shoot — not how his flaps behave.

| Contract | Acceptance / measurement anchor |
|---|---|
| The flight model hits its documented envelope | every anchor of `doc/modules/mig29/flight-model-spec.md` (max speed by altitude, sustained/instantaneous turn, climb rate, service ceiling, fuel flow) measured in the gym against the documented number, deviation stated |
| Turn-fight fidelity is explicitly NOT a criterion | a failing knife-fight comparison is not a defect of this model; a wrong envelope is |
| It is a module like any other | `FBModule` derivation under `sim/src/modules/mig29/`, registered by name through `FBModuleRegistry`, composed from the `systems/` defaults; no second architecture, no special case in the runner |
| Its model lives under the single model root and obeys the delta rule | `mods/f16/src/aircraft/mig29`, declared in `mods/f16/src/aircraft/MODEL-DELTAS.md`, `make -C sim verify-models` green |
| It perceives only through simulated sensors | same boundary as the F-16: registry reaches sensor slots only, contacts are anonymous, identity only through IFF ([`../sim/sensors.md`](../../sensors.md)) |
| Its weapons are units like ours | R-27R/T, R-73, R-60M, GSh-301 — each with its own module, own FDM where it is guided, own telemetry file ([`../../weapons.md`](../../weapons.md)) |
| Its doctrine is GCI-led, not lone-wolf | the pilot flies a briefed vector under ground control: cued search, late own-radar emission, commit and disengage rules from the ground picture rather than from his own scope |
| Every number carries its provenance | derived / measured / `[SET]`, exactly as in `../conventions.md`; the manuals are cited through `doc/modules/mig29/`, never restated here |

### Why it exists at all

Because the F-16 duel is symmetric, and symmetry produces stalemates: every long shot is defeated in
the notch, and the outcome is a coin toss rather than a decision
([`../../pilot.md`](../../pilot.md), gap 2.3). An opponent with a different radar, a different
missile envelope and a different doctrine turns the coin toss into a choice. That is the reason the
asymmetric weapon round (R6) comes before the airframe round (R8).

### Build order (the contract with the roadmap)

| Step | Depends on | Done when |
|---|---|---|
| Knowledge base `doc/modules/mig29/` incl. `flight-model-spec.md` with documented envelope anchors | — | R3 lands |
| Weapon family (R-27R/T, R-73, R-60M class) + RCS as a unit property | R3 | R6 lands; envelopes measured, not asserted |
| JSBSim model built along the spec's build order, each step measured against one documented anchor | R3 | R8 lands; `MODEL-DELTAS` bookkeeping complete |
| Module + GCI-led pilot behaviour | R6, R8 | flies a mission against the F-16 in the gym with a decided outcome |

### Stage 2c — the weapons and the signature (the contract for this round)

| Contract | Acceptance / measurement anchor |
|---|---|
| The R-27R's **support obligation is a mechanic, not a note** | its seeker sees only the reflection of this jet's own illumination: lock broken → seeker dead, no reactivation. Two measured branches on `mig29-r27.fbm`: unbroken chain = hit, lock broken mid-flight = miss |
| Support and Defend are **mutually exclusive on this airframe** | while a round of this jet still needs illumination the `Defend` transition is inhibited; the F-16's `crank-and-defend` is not available here. One guard, one module hook, both named |
| The crank ceiling comes off the **N019's own 67°**, not the F-16's 60° | `FBMig29Pilot::InterceptCrankAtaDeg` derived from 67° minus the same manoeuvre reserve the F-16 takes off 60° |
| The R-73 is the first IR round, and it is the SAME class as the F-16's AIM-9 | one `modules/missile` seeker kind, two catalogue entries, different numbers; the helmet-sight cueing bound (±60° az) is the module hook that decides whether a shot is offered, not the 75° seeker gimbal |
| **Flares become effective** the day this round lands | deterministic seduction on measured irradiance; measured on both branches (tail/afterburner = not deceived, head-on/dry = deceived), for the MiG's R-73 and for the F-16's AIM-9 alike |
| The GSh-301 is a catalogue row, not a second gun system | 30×165 mm, 150 rounds, 1,500 rd/min, 860 m/s [T3]; `weapons/FBGunSystem` unchanged, `FBMig29Gun` supplies only where it is bolted. The kinetic damage path carries the heavier round without a new constant |
| **RCS is a unit property**, and it is what makes the duel asymmetric | `FBUnitSignature::RcsM2` from the module; radar reach scales with `σ^¼`. Reference = the F-16's, so every F-16-against-F-16 measurement in the tree stays byte-identical and the asymmetry appears only across the two types |
| Twin-engine damage gets its state | `core/FBSystemHealth::Engine2`, appended; `FBMig29Damage` wires both nacelles, the F-16 keeps one, and the mapping is written down |
| `mig29-intercept` gets past `RADAR_DESIGNATE` | the rig's own reading rule changes: the intercept now designates, shoots and supports instead of aborting on first contact for want of a weapon |

## State

**EMISSION DISCIPLINE BUILT (2026-08-04, `E20`) — this jet manages its own PUR-31, and it is the first
piece of doctrine in this tree that the MiG-29 owns and the F-16 does not.** Three overrides on
`FBMig29Pilot` and nothing else: `EmissionControl()` names the SWITCH (`RadarEmission`, DUMMY ↔ ILLUM)
instead of the mode selector — which on this aircraft already carries RAD-against-ACM, so an EMCON that
went through it would drop the close-combat pattern every time the jet left a merge; `EmconRadiateNm()`
is the N019's own 50 km = **27.0 nm** (`FBMig29Radar::kSearchRangeM`, T4 §7.1) against the F-16's 40.0;
and `BriefedPictureRangeM()` makes the CONTROLLER'S last call the picture that permits the silence,
because this jet has no cooperative terminal and the generic rule would otherwise read *"an aircraft
without a datalink may never be quiet"* — the inverse of `datalink-gci.md` §5.2's *radiate as late as
possible*. The call expires at the controller's own briefed cadence, so a jet whose controller falls
silent radiates instead of going dark for the rest of the mission.

**What it costs, and the cost is this airframe's own:** the throw is DED-class on the command bus
(`core/FBCommandBus.cpp`, and no other airframe ever posts this target), so going quiet is head-down
seconds and is refused above `kDedMaxG` — this jet cannot change its emission state in a hard turn,
where an F-16 flicks a mode knob. [MESS, `sat-07-dry-merge`] `ma1` flies three ILLUM and four DUMMY
spells; every state entered is left.

**One defect fell out of it and is repaired here:** `FBRadarSystem::Powered_` defaults to `true` while
`FBMig29Radar::Emit_` defaults to `OFF`, and `SetEmission` returns early on an unchanged state — so the
block advertised a live set behind a dead switch from spawn. Nobody read `Radar.Powered` on this
aircraft before its pilot did, which is why it could sit there. The constructor now reconciles the two.

**Stage 2a and 3 built: the module and the solo end-to-end proof** (merged as part of `11722e5`-follow-up).
`sim/src/modules/mig29/` is an `FBModule` like any other — registered under `"mig29"` through
`FBModuleRegistry`, composed from the `systems/` defaults, no second architecture and no special case
in the runner. It adds exactly three things of its own: this aircraft's pilot numbers (`FBMig29Pilot`,
hooks only), its FBW gain preset (`FBFlightControl::Mig29()`) and its damage zones (`FBMig29Damage`,
every boundary a station the deck itself states). Four missions fly it: `mig29-takeoff`,
`mig29-landing` (glideslope capture from below), `mig29-full` (the acceptance: takeoff, three
waypoints, approach, flare, rollout to a stop on the Payerne threshold) and `mig29-pair` (an F-16 and
a MiG-29 in one formation — the multi-module proof `payerne-mixed` makes for f16+f16). All four exit 0.

Numbers from `mig29-full` against `procedures.md` and the stage-1 anchors: rotation 130.1 kt
(documented 125–135), liftoff 144.2 kt (140–150), ground run 346 m (B8 324 m), cruise altitude hold
within 5 m, approach 146.5 kt at 11.54° AoA, flare at 28.2 ft AGL (documented 20–30), touchdown
143.4 kt at 11.66° AoA / 8.84° pitch / 3.59 m/s sink (documented ~140 kt @ 11°, never past 13°; the
deck's nozzle-strike geometry is 13.5°, the monitor's contact-pitch KO 15°), rollout 919 m at 13.6 t.
The lighter `mig29-landing` touches at 141.3 kt / 9.76° with 1.84 m/s and rolls 471 m at 12.2 t — the
AoA difference between the two landings is 1.4 t of fuel, not a different regulator.

**Stage 2b built: the sensors and the guidance.** Three real derivations, one new generic slot, and the
GCI loop as mission data. Numbers, sources and what each one deliberately does NOT model are in
[`../../sensors.md`](../../sensors.md) §4.9 (N019), §5.6 (SPO-15) and §6 (the IRST slot + KOLS); the
`set` keys are in [`../../missions/sensors.md`](../../missions/sensors.md). The short form:

| Piece | What it is | The thing that makes it a different sensor, not different numbers |
|---|---|---|
| `FBMig29Radar` | N019 "Rubin": `off`/`rad`/`cc`/`vs`/`bore` + STT over `ActiveVolume()`, ILLUM/DUMMY/OFF emission switch, ZONE thirds, antenna-elevation knob | the **quantified, range-dependent Doppler envelope that REJECTS** (81 kt beyond 8 nm, 27 kt inside, 32.4 kt inside 5.4 nm) and **6 s of inertial tracking** through it |
| `FBMig29Rwr` | SPO-15LM "Beryoza": ±30° elevation, eight logical azimuth channels, bearings reported as channel CENTRES | **the own radar blanks the forward hemisphere**; the priority row is chosen against a hard-assumed 26-55 kft; track is a property of the CHANNEL |
| `FBMig29Irst` | OEPS-29/KOLS on the **new generic `sensors/FBIrstSystem`** | it sees HEAT: aspect-dependent reach (25 km astern, 10 km head-on), afterburner ×1.5, **no range unless the 6 km laser fires**, **no IFF at all**, and a cloud deck ends the line of sight |
| `FBMig29Pilot::BriefGci` | the controller's BRAA as `set brief_gci` lines | not a track and not knowledge: three typed entries over the command bus (scan elevation by the documented range-angle method, ZONE, then ILLUM), each charged its own latency |

`set task intercept` is unlocked by this stage — the BVR phase machine needs a radar and this jet now
has one. `bfm` and `attack` stay closed: both need a WEAPON.

**Stage 2c built: the weapons and the signature.** Three real slots (`FBMig29Sms`, `FBMig29Gun`,
`FBMig29FireControl`), three catalogue rounds, one unit property, and one behavioural hardness. What
each piece is and what it measured:

| Piece | What it is | The thing that makes it a different weapon, not a different number |
|---|---|---|
| **R-27R** | semi-active, `FBSeekerKind::SemiActiveRadar` | its seeker lives on THIS jet's illumination and dies with it, once and for good. `ttaS=-1` reaches the shooter's Support state, which then runs to IMPACT: **28.56 s of unbroken pointing for one shot** (measured), against the AIM-120's 5-15 s |
| **`SupportInhibitsDefend`** | one module hook, one state-transition guard | while a launched round still needs illumination the pilot does not turn away. That is `DCS-FM p.65`'s "jousting", as a rule the phase machine obeys |
| **R-73** | the first IR round, shared with the F-16's AIM-9 | ONE seeker class, two catalogue entries: gimbal ±75° [DOC] against the Sidewinder's ±30° [T4], a DOCUMENTED 3.5 m fuze radius against a derived 6.0 m |
| **`BfmWvrCueDeg` = 60** | one module hook, read by the close-combat shot ([`../../pilot.md`](../../pilot.md) §5.11) | it is the HELMET and not the missile: `DCS-EA p.92` cues ±60° in azimuth while the R-73's own gimbal is 75°, and [`weapons.md`](weapons.md) §3.3 says in as many words that the cueing limit is what decides whether a shot is OFFERED. The F-16 leaves the default (< 0 = the round's own gimbal, ±30° for the AIM-9M) because no HMCS is modelled |
| **GSh-301** | a catalogue row beside the M61A1 | 2.7× the energy per round, 0.68× the flux, a 6 mrad cone against 2.23 — the documented 200-790 m effective band falls out of the DISPERSION rather than out of a range limit (measured, both ends) |
| **`RadarCrossSectionM2` = 4.0** | a unit property, published like the afterburner bit | `σ^¼` scaling against the F-16's 1.2 m² reference: **1.351× the range one way, 0.740× the other**. The reference is the F-16's own, so every F-16-against-F-16 measurement in the tree is byte-identical |
| **`FBSystemId::Engine2`** | appended, with `PropulsionOut()` | "one RD-33 out and one running" finally has a state: `CombatEffective` stays true, the throttle is capped at half the installed thrust. Both ids in the AFT zone, mapping written down |

`set task bfm` is unlocked by this stage; `set task attack` stays closed, and now for a stated reason
rather than a missing weapon — the 9-12 carries no guided air-to-ground store at all and its unguided
delivery is a director the pilot flies rather than a release moment he reacts to
([weapons.md](weapons.md) §5.3), so `FBMig29FireControl` publishes no CCIP/CCRP block.

**Three defects this stage found by measuring**, each fixed where it belonged:

1. **The gun did not know who fired it.** `FBGunSystem::SetUnitId` was never called by this module, so
   its bursts carried `LauncherId 0`, the runner's shooter exclusion never matched, and the MiG shot
   ITSELF down at the muzzle (measured: 6 hits, `damage KILL` on the shooter). One line.
2. **The alpha limiter did not act on the hand stick.** `FBFlightControl::Run` returned early in
   `Manual`, so every manual-stick phase on an airframe whose DECK has no limiter was unbounded. It
   never showed because Takeoff/Flare/Rollout do not pull; BFM is the first phase that does, and the
   aircraft departed at 32° of incidence and 11.8 g after 8.9 s. The limiter is a STICK FORCE and now
   acts in both branches — at the hand stick it may only forbid PULL, never force a push, which is what
   the class comment always said. `AlphaLimitDeg = 0` (the F-16) leaves the branch dead.
3. **The BFM roll-rate cap inverted the wrong plant.** The cap solves `p[n+1] = a·p[n] + K(1−a)·u[n]`
   for the stick, so with another aircraft's `a`/`K` it is not a cap but an oscillator. Identified for
   this airframe from its own BFM samples: **a = 0.819 (τ = 0.50 s), K = 201 °/s per full deflection**
   against the F-16's 0.734 / 78.7 — it rolls 2.6× harder for the same stick. Now a hook with the
   F-16's numbers as the default.

**Stage 4 built: it fights.** The asymmetric duel campaign — nine `mods/f16/src/missions/duel-*.fbm`, the
geometry × outcome table, both sides' `eng_*` debriefing, the EMCON timeline and a mixed tournament —
lives in [`../../duels.md`](../../duels.md), because it is a property of the PAIRING and not of this
aircraft. What belongs here is what it said about the MiG-29:

| Finding | Number |
|---|---|
| **It can win.** `duel-doctrine-mig`: R-27R away at 14.41 nm (t=141.5), 25.8 s of unbroken illumination, detonation 9.35 m from the F-16, `damage KILL`. The F-16 never fired — its receiver lit 1.5 s before its own trigger would have | exit 0, `fulcrum result=SUCCESS` |
| **What it needs to win is its OWN launch doctrine**, not a better weapon: the R-27R carries the LONGER Raero (39.2 nm against the AIM-120's 31.1) and a Rtr within half a mile of it, and it has to illuminate to impact either way — so `InterceptShotRtrFactor` = 1.0, the F-16's rule, is close to the worst rule available for it. It stays the default because nothing in `doc/modules/mig29/` states a launch doctrine; the alternative is mission text (`set pilot_shot_rtr`) | [MESS] 1.2–1.6 × Rtr wins, 1.0 and ≥1.8 draw |
| **Two defects of its own pilot, both found by measuring and both fixed** | see below |
| **The SPO-15's forward blanking is a tactical cost, not a note.** A head-on MiG that illuminates cannot hear the radar working it: `eng_threat_s` = −1 for the whole of `duel-headon`. Dark, it hears the APG-68 at t=0.1, defends, and survives an AIM-120 (`duel-emcon`) | `duel-headon` vs `duel-emcon` |
| **Going loud and defending are mutually exclusive**, and nobody wrote that rule: `RadarEmission` is a DED-class command and the bus locks head-down inputs while the jet is manoeuvring | `duel-emcon`, t=174.2 `sequence_precondition` |

**Two pilot defects this stage found by measuring:**

1. **The GCI entry chain did not retry a refused entry.** Every other briefed input in the tree retries
   on a bus rejection (`FBPilot::EnterBriefedItems`, `kBriefRetryS` = 2 s); this one advanced on the
   POST rather than on the acknowledgement, so the single entry that makes this radar exist could be
   lost to one g-loaded tick — measured on `duel-emcon`, where ILLUM was refused at t=174.2 and the MiG
   then flew 400 s of the duel blind and never fired. One conditional in `FBMig29Pilot.cpp`; **all 69
   stock missions byte-identical**, because a run in which nothing was refused does not reach it.
2. **`InterceptSpeedKt` was a unit error, and an expensive one.** The AP speed loop controls TRUE
   airspeed; the old derivation compared this jet's corner in CAS against the F-16's ROUTE speed (300)
   instead of its intercept speed (550) and then fed the CAS answer to a TAS command. [MESS] the MiG
   cruised to every BVR merge at **217 KCAS / M 0.54** — 40 % below its own `BfmMinSpeedKt` of 380 KCAS,
   with the smaller launch zone a slower launcher gets. Corrected to **600** by the F-16's own
   documented rule (the TAS whose CAS at the 8,000 m band IS the measured corner): [MESS] 422.3 KCAS /
   M 1.00 on a mean throttle of 0.45, against the measured corner of 420. It moves exactly one stock
   mission, `mig29-intercept`, in the direction its own reading rule wants — designate 58.4 → 52.4 s,
   shot 60.9 → 54.9 s, kill 87.7 → 78.1 s, miss 1.13 → 0.34 m, same exit code and same verdict. **And
   it is the campaign's second-largest finding:** with the 330 kt hook the F-16 won four of the five BVR
   geometries outright; correcting it turned all four into draws.

**Stage 5 built: it flies in a PAIR — and the pair is where its doctrine hurts most.** The formation
round ([`../../formation.md`](../../formation.md)) gave every module a flight identity, a wingman
station, a target sort and a cover rule. What it said about THIS aircraft is a single sentence: the
airframe whose weapon makes mutual cover most valuable is the one that cannot organise it.

| Finding | Number |
|---|---|
| **The SARH obligation, per shot, side by side on one run** (`pair-2v2-asym.fbm`) | R-27R **17.3 s** bound with `eng_pitbull` 0 — it never goes autonomous — against the AIM-120's **0.3 s** with `eng_pitbull` 1. A factor of **58** |
| **The cover rule is the same rule on both aircraft; only its price differs** | a member does not fire a round that would bind it while a mate is already bound. Measured F-16 deferral **7.8 s** (`pair-cover.fbm`), and the flight then never had nobody free (`flt_both_s` 0.0) |
| **This aircraft cannot apply it.** "My leader is bound" travels in a PPLI, and the 9-12 has no cooperative terminal | so its `flt_defer_s` is 0 by construction rather than by choice, and both members of a pair can be bound — and therefore forbidden to defend — at the same time |
| **Its sort is the briefed CONTRACT and nothing else** (`set brief_sort left`) — the controller's split, agreed on the ground, applied by each pilot to the picture he personally has | measured against the F-16's computed sort on the same 4v4 geometry: distinct targets per engaged member **0.750** against **0.962**. One in four contract shooters is prosecuting a target its own flight already has |
| **The contract is still worth having** | in the flight tournament `mig_pair` (contract) beats `mig_solo` (nothing) on both geometries — `mirror` −480.6 against −534.2, `split` −1302.2 against −1400.4 |
| `set task formation` is accepted and does nothing on this jet | deliberately: with no lead report to hold, the station keeping falls straight through to its own route. A module declares what it has |

Nothing about the MiG's sensors, weapons or pilot numbers changed; the whole stage is generic
machinery plus one `set` key (`brief_sort`) and one accepted `task` value.

**Deliberately absent** (next stages): countermeasures (BVP-30-26), and any cooperative
datalink — this aircraft has none, and its GCI is a voice channel, not a track picture. Those slots hold
the NoOp/generic defaults, are not cycled, and their blocks stay `Invalid` — a module declares what it
has. The F-16 is provably untouched: all **56** stock missions byte-identical on every column they ever
had, `test-corner` unchanged (380 kt / 16.18 °/s / 5.44 g).

**Measured, stage 2b** (the four rigs are `mig29-radar-notch`, `mig29-rwr-blind`, `mig29-irst`,
`mig29-intercept`, all exit 3 = TIMEOUT by construction — measurement rigs, not tasks):

| Anchor | Documented | Measured |
|---|---|---|
| detection latency, RAD | "up to six seconds … several scanning cycles" | first firm contact **t = 6.0 s** (2 looks × 3.0 s frame) |
| Doppler notch, > 8 nm | closure must exceed **81 kt** = 41.67 m/s | `NOTCH_LOST` at 18.05 nm with target radial **7.94 m/s** vs `notchMs=41.67`; regained at 50.62 m/s |
| Doppler notch, < 5.4 nm | "not guaranteed" below **32.4 kt** = 16.67 m/s | `NOTCH_LOST` at 3.14 nm with radial **4.34 m/s** vs `notchMs=16.668`; regained at 49.53 m/s |
| inertial tracking | **up to 6 s** | `RADAR_DROP … coastS=6` — the limit is 6.0 s and the file drops at the first antenna frame at or after it (6-9 s observed on a 3 s frame) |
| SPO-15 forward blanking | own FCR radiating → forward hemisphere disabled | ILLUM accepted t=47.9 → `FORWARD_BLANK on=1` and `THREAT_BLIND` in the SAME tick, `rwr_threats` 1→0 at t=49.8 (one 2 s hold later) while the emitter's `fcr_on` never changed |
| SPO-15 azimuth reporting | resolution is the antenna pattern, not an angle | threat bearing reported as **−10.0°** (a channel centre) where the F-16's receiver reports 0.045° on the same geometry |
| IRST aspect law | 25 km astern / 10 km head-on | tail-on target (aspect 5.7°, law reach 24 963 m) detected at **19 562 m**; the 102.7°-aspect target (law reach 15 845 m) first detected at **15 222 m** — 9.7 km closer, same field, same target type |
| IRST laser | **6 km** | `LASER_RANGE … rangeM=5925 maxM=6000`; `irst_lock_nm` steps from **−1** to 3.199 nm at t=140.0 |
| IRST cloud masking | (FlightBox setting: broken deck = lid) | `irst_masked=1` for the target at 5 000 m above the ~2 950-3 900 m GFS deck while the MiG is at 2 000 m below it, from t=2.5 s until it leaves the field at t=114 s |
| GCI entry chain | voice → pilot → manual entry → scan solution | BRAA t=19.9 → elevation entry (range-angle, from the controller's two numbers and the pilot's OWN altimeter) → ZONE → ILLUM accepted **t=47.9**, i.e. **8.0 s** from call to radiating radar |
| the price of radiating | — | the opposing RWR's `THREAT_NEW` lands **0.1 s** after ILLUM; before it there is nothing to see |

**Stage 1 built: the airframe.** `mods/f16/src/aircraft/mig29/` holds a FlightBox-own JSBSim deck
(`mig29.xml`, `engine/RD-33.xml` ×2 instantiation, `engine/RD-33-nozzle.xml`, `reset00.xml`,
`release="ALPHA"`), declared as FlightBox-own in `mods/f16/src/aircraft/MODEL-DELTAS.md`, and
`make -C sim test-mig29` measures it against all 22 anchors of
[`../../mig29/flight-model-spec.md`](flight-model-spec.md) §8. **The anchor table with
IST/SOLL per row, the two consistency probes, the build-order gate status and the four missed anchors
with their diagnoses live in that file's `## State` and `## Gaps`** — they belong next to the spec they
answer, and are not restated here.

The one-line summary: **A1/A2 (Vmax at altitude and at sea level) within 2 %, the whole takeoff and
landing set in band, and the two misses that matter (Ps −24.8 %, ceiling +8.7 %) both traced to the
thrust side** — which is what the spec's own "freeze the thrust analogy, absorb residuals in drag" rule
exists to make diagnosable.

**Nothing loads this model.** No `sim/src/modules/mig29/`, no `FBModuleRegistry` name, no `.fbm`
mission; the only consumer is its own harness, so no existing measurement in the tree moved. The
53 stock missions are untouched (spot-checked after the build: exit codes unchanged, each matching the
reading rule in its own file header).

## Gaps

| done (2a) | ~~Module (`FBModule` derivation + registry name) and the systems it composes~~ | stage 2a |
| done (2a) | ~~SOS α limiter and the ARU-aware gains in `systems/FBFlightControl`~~ — `AlphaLimitDeg` is one preset number; 0 means the airframe has its own limiter and is what the F-16 sets | stage 2a |
| done (2b) | ~~Radar (N019), RWR (SPO-15), IRST (KOLS) and the GCI loop~~ | stage 2b |
| 4b | Own HUD symbology — the module flies the generic MIL-STD-1787 default | open |
| 5a | **The cover channel this aircraft might actually have was not tried.** The SPO-15 has no IFF and warns of every radar, friendly included (`datalink-gci.md` §3), and a leader illuminating for an R-27R publishes a Guidance-mode emission — so a wingman could infer "my leader is bound" from its own receiver, with the documented ambiguity that it cannot tell that emitter from a hostile one. It is the only sourced candidate for MiG mutual support | open, [`../../formation.md`](../../formation.md) F3 |
| ~~4g~~ | ~~**No dispensers.**~~ — **done** this round. The BVP-30-26 is `modules/mig29/FBMig29Cmds`, an override of `sensors/FBCountermeasureSystem`: 60 combined cartridges (2×30 PPI-26, [DOC] §3), a [SET] 30/30 chaff/flare split (the split is a named source gap), a 5/5 BINGO and the three geometry programmes mapped onto the generic slot machine with [SET] values (schema from the source, values set, exactly as the F-16's ALE-47). Wired into the module (cycled RWR→CMDS at 10 Hz, `CmDispense`/`CmConsent`/`CmdsMode` routed, `cmds_*`/`brief_flare_s` keys), gated on the `Countermeasures` health id the damage layout already zoned. Flares seduce the AIM-9 through the SAME deterministic model that seduces the R-73 (`sensors/FBIrstSystem::SelectFlare`): `mig29-defend.fbm` measures `irst FLARE_SEDUCED tgtIntensity=0.16` (head-on/dry) and the round expiring 16.0 m wide, against the astern control that detonates 0.04 m out. The defensive asymmetry ([`../../duels.md`](../../duels.md) D5) is now TWO-SIDED | done |
| ~~**4h**~~ | **CLOSED.** (b) DEPARTURE and the rate damper: [`../../pilot.md`](../../pilot.md) §5.10/§5.10a. (a) ACQUISITION: `FBMig29Radar::kAcm*` (±37° az [T4 §7.1], a [SET] ±30° vertical band, Doppler-exempt like CC, frame 0.75 s [DERIVED]) selected by the pilot in the fight phase through `BfmRadarModeOrdinal`; `duel-merge` `n019 MODE acm` t = 0.5, `RADAR_LOCK` t = 3.8 at 3.32 nm. (c) the F-16's roll law: §5.7.3. (d) — the last one — **the WEAPON**: `Phase::Bfm` had no missile shot at all, and now has one ([`../../pilot.md`](../../pilot.md) §5.11) with this module's own cueing hook. `duel-merge` **exit 3 → 0** and `duel-merge-stern` **exit 0**, i.e. the R-73 thesis is finally measured. **Its answer, and it is not the one this file guessed:** head-on the MiG loses even when it shoots FIRST, because the two warheads' kill radii against an F-16 are 2.32 m (AIM-9M, 9.4 kg) and **2.08 m** (R-73, 7.4 kg) and a 1,050 m/s pass leaves the terminal loop no room; astern at 590 m/s of closure the same round arrives 1.86 m out and kills. Successor gap: neither ACM box re-acquires after the first pass ([`../../pilot.md`](../../pilot.md) 2.9) | — |
| **4i** | **The merge CFIT is CLOSED, and it was the DAMPER.** `systems/FBFlightControl` applied this airframe's own rate damper (`KqDamp`/`KpDampRoll` — the SAU-451 DAMPER, [`flight-model-spec.md`](flight-model-spec.md) §7.4 maps it to *"FBFlightControl's inner rate loop"*) only on its FLCS path, while `Phase::Bfm` commands `Manual`: the jet fought every close engagement with the damper OFF. It is the same omission as §5.10's screws 1–2 one device further ([`../../pilot.md`](../../pilot.md) §5.10a). Reproducer without an opponent — one MiG on `set task bfm`, cold search, 300 s: mean `bfm_gcmd` **4.57 → 1.11**, mean bank **76° → 24°**, p95 |VS| **183 → 4 m/s**, CFIT from all three start altitudes → none; the F-16 flies the identical search at 1.22 / 40° / 9 m/s. `duel-merge` exit 2 → 3 with no KO on either side; `mig29-bfm` `bfm_ctrl_s` **0.0 → 287.6 s** of 300 (closest approach 1.70 → 0.65 nm) at an unchanged 60 °/s roll cap — the control position §5.7.3 had booked as the roll bound's price was never the roll bound's price. The YAW axis of the three-axis damper stays out: this airframe's rudder branch is measured OFF (`KNy = KNyi = 0`, departure at t = 28 s), so a yaw gain here would be invented | done |
| 4e | No `FBSystemId` for an optical station, so the IRST is the one sensor slot that is not damage-gated. Travels with 4d: both are a health-register change, and both move the `dmg_*` telemetry columns | needs a health-register change |
| ~~4f~~ | ~~The intercept ends in `Abort` on first contact~~ — **done** (2c). `mig29-intercept` now runs the whole chain: `RADAR_DESIGNATE` at t = 58.4 s, shot at 60.9 s, support to impact, `damage KILL` at 87.7 s; `eng_state` search → closing → attack → support | done |
| ~~4c~~ | **SETTLED, and the formula was right.** The measurement window hypothesis is REFUTED and the harness was comparing two different quantities — see Knowledge below | closed, stage 2c |
| ~~4d~~ | ~~Twin-engine damage~~ | **done** (2c): `FBSystemId::Engine2` + `PropulsionOut()` |
| 4e' | The IRST is STILL not damage-gated: `Engine2` was the appended id this stage needed, and an optical-station id is a second append that would move the `dmg_*` bitmask meaning again for no measured benefit yet | open |

In roadmap terms the remaining chain is **R6 → R8 stage 2/3 → R7**:

| # | Missing | Blocked by |
|---|---|---|
| 1 | ~~`doc/modules/mig29/flight-model-spec.md` with documented envelope anchors~~ | **done** (R3) |
| 2 | Enemy weapon family + RCS as a unit property | R3 → R6 |
| 3 | ~~JSBSim MiG-29 model measured against its anchors~~ | **done** — deck built and measured; four anchors missed with named causes, model stays `ALPHA` |
| 3b | The four missed anchors closed, or their `[DERIVED]`/`[SET]` levers re-tagged and fitted | a real RD-33 thrust deck, or GAF T.O. 1F-MIG29-1 |
| 4 | ~~Module (`FBModule` derivation + registry name) and the systems it composes~~ | **done** (2a) |
| 5 | SOS α limiter and the ARU-aware gains in `systems/FBFlightControl` | R8 stage 2; the deck deliberately carries no limiter |
| 6 | Stores integration: point masses, `fb-stores` drag, gun recoil | R6 |
| 7 | ~~GCI doctrine~~ — the voice-BRAA → manual-entry loop of `datalink-gci.md` §2.2 | **done** (2b); the Lazur/Biryuza HARDWARE link stays out, as both manuals describe it as not implemented |

Note on ordering, from the roadmap: the first opponents that are actually buildable are the one-way
drone and the cruise missile (R7) — they are real F-16 tasking, they stress the Doppler notch and the
gun, and `modules/drone` tests the module architecture at a fraction of this file's cost. The MiG-29 is
the first *manned* opponent, not the first opponent.

## Knowledge

The derivations of stage 1 live **inside the deck**, next to the numbers they produce — that is the
provenance rule of `../conventions.md` applied literally, and it is why `mig29.xml` is mostly comment.
Three findings from the build are worth having here, because they are about FlightBox rather than about
the MiG-29:

- **JSBSim's longitudinal trim drives `fcs/pitch-trim-cmd-norm`, not the stick.** A pitch channel that
  does not sum that input cannot be trimmed at all: `FGTrim` reports *"qdot doesn't appear to be
  trimmable"* because its control has no effect on the state it is solving. Costly to find, one summer
  to fix, and the pinned F-16 deck has the same summer for the same reason.
- **JSBSim interpolates tables linearly**, so a quadratic drag rise first sampled at α = 5° is
  overstated ~4.5× at the 1° incidence a fighter dashes at. Adding one ±2.5° breakpoint moved Vmax at
  11 km from M 2.07 to M 2.35.
- **A deck with no limiter is not measurable with full stick.** Without an FLCS, full aft stick is 35°
  of stabilator against a 5 % static margin and the answer is a tumble to 180° of α. The envelope
  harness therefore carries a throwaway SOS sketch; the real one belongs in `systems/FBFlightControl`
  (stage 2), which is what [`../../mig29/flight-model-spec.md`](flight-model-spec.md) §7.3
  specified before any of this was built.

### The corner-formula discrepancy (gap 4c), settled by measurement

The hooks fed `CornerTurnRateDegS` = `g·√(n²−1)/V` a corner of 420 KCAS and 7.8 g and got 20.2 °/s
against a harness reading of 24.18 °/s — −16 %, where the F-16's gap is −2 %. The standing hypothesis
was the measurement window: this airframe loses altitude inside the 4 s pull, so the "speed" in the
formula is not the entry speed. `test/modules/mig29/FBTestMig29Envelope.cpp` now logs the window's own state and
four readings of the same four seconds, and the answer is **neither the altitude nor the averaging**:

| Quantity, at the corner point (entry 420 KCAS) | Value |
|---|---|
| harness `turnRateDegS` — rate of the body's EULER HEADING | **24.18 °/s** |
| **`trackRateDegS` — rate of the VELOCITY VECTOR's ground track** | **20.49 °/s** |
| formula on the entry CAS (what the pilot hooks feed it) | 20.20 °/s |
| formula on the mean TAS actually flown (336 KCAS, 216 m/s) | 20.21 °/s |
| formula evaluated instantaneously and averaged over the same samples | 20.29 °/s |
| formula with the `1/cos γ` correction (γ = −10.4°, 154 m of loss) | 20.54 °/s |

**The formula is right to 1.4 %** — better than the F-16's own −2 %. The altitude loss is worth +1.7 %,
an order of magnitude too little; the convexity of `√(n²−1)` is worth +0.4 %. What the −16 % was is the
harness measuring the rate of change of the body's Euler heading while the formula predicts the turn
rate of the velocity vector, and at 22.7° of incidence in an 85°-banked pull those differ by 18 %. The
correction is therefore to the ANCHOR's label and to what the harness reports, not to the formula or to
the hooks: `BfmCornerG`/`BfmCornerSpeedKt` stay as measured, and `CORNER_POINT` now carries
`trackRateDegS` beside `turnRateDegS` so the two are never confused again.

The source split, worth stating once:

| Question | Where it is answered |
|---|---|
| What the real MiG-29 does | `doc/modules/mig29/` (from the two DCS manuals + research), the same relationship `doc/modules/f16/` has to [`../f16/module.md`](../f16/module.md) |
| How a module is composed and registered | [`../f16/module.md`](../f16/module.md), [`../architecture.md`](../../architecture.md) |
| What a weapon must be to exist here | [`../../weapons.md`](../../weapons.md) |
| What a model copy may deviate in | `mods/f16/src/aircraft/MODEL-DELTAS.md`, [`stores.md`](../stores.md) |
