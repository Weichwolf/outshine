# MiG-29 — the first opponent

**Status: stages 1, 2a, 2b and 3 built — the airframe, the module, the sensor suite (N019 / SPO-15 /
KOLS) and the GCI loop. No weapons.** This is the first file written the new way round — the contract exists before the code,
and the build rounds are measured against it instead of describing themselves afterwards.

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
| Its model lives under the single model root and obeys the delta rule | `sim/assets/aircraft/mig29`, declared in `sim/assets/MODEL-DELTAS.md`, `make -C sim verify-models` green |
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

## State

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

**Deliberately absent** (stage 2c): countermeasures (BVP-30-26), stores, gun, and any cooperative
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

**Stage 1 built: the airframe.** `sim/assets/aircraft/mig29/` holds a FlightBox-own JSBSim deck
(`mig29.xml`, `engine/RD-33.xml` ×2 instantiation, `engine/RD-33-nozzle.xml`, `reset00.xml`,
`release="ALPHA"`), declared as FlightBox-own in `sim/assets/MODEL-DELTAS.md`, and
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
| 4b | Own HUD symbology — the module flies the generic MIL-STD-1787 default | stage 2c |
| 4e | No `FBSystemId` for an optical station, so the IRST is the one sensor slot that is not damage-gated. Travels with 4d: both are a health-register change, and both move the `dmg_*` telemetry columns | needs a health-register change |
| 4f | The intercept ends in `Abort` on first contact, because `pilot/FBPilot`'s own rule is "a target on the scope and nothing on the rails → disengage". Correct behaviour, but it means `RADAR_DESIGNATE` is unreachable until stage 2c gives this jet a weapon | R6 |
| 4c | The corner formula `g·√(n²−1)/V` reads 20.2 °/s from the hooks against a measured 24.18 °/s (−16 %; the F-16's gap is −2 %). Cause: altitude loss inside this airframe's measurement window | stage 2c |
| 4d | Twin-engine damage: `core/FBSystemHealth` carries ONE `Engine` id, so "one RD-33 out" — this airframe's most characteristic damage state — has no state to live in | needs a health-register change, not a module one |

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

The source split, worth stating once:

| Question | Where it is answered |
|---|---|
| What the real MiG-29 does | `doc/modules/mig29/` (from the two DCS manuals + research), the same relationship `doc/modules/f16/` has to [`../f16/module.md`](../f16/module.md) |
| How a module is composed and registered | [`../f16/module.md`](../f16/module.md), [`../architecture.md`](../../architecture.md) |
| What a weapon must be to exist here | [`../../weapons.md`](../../weapons.md) |
| What a model copy may deviate in | `sim/assets/MODEL-DELTAS.md`, [`stores.md`](../stores.md) |
