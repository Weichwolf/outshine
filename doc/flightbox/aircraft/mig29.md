# MiG-29 — the first opponent

**Status: spec only. Nothing is built.** This is the first file written the new way round — the
contract exists before the code, and the build rounds are measured against it instead of describing
themselves afterwards.

**Neighbours:** `doc/mig29/` (the knowledge base about the real aircraft, from the two DCS manuals plus
research — roadmap R3, in progress), [`f16.md`](f16.md) (the module pattern this one will follow),
[`../sim/sensors.md`](../sim/sensors.md) and [`../sim/weapons-and-damage.md`](../sim/weapons-and-damage.md)
(the systems it plugs into), [`../roadmap.md`](../roadmap.md) (R3 → R6 → R7 → R8, the chain this file
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
| The flight model hits its documented envelope | every anchor of `doc/mig29/flight-model-spec.md` (max speed by altitude, sustained/instantaneous turn, climb rate, service ceiling, fuel flow) measured in the gym against the documented number, deviation stated |
| Turn-fight fidelity is explicitly NOT a criterion | a failing knife-fight comparison is not a defect of this model; a wrong envelope is |
| It is a module like any other | `FBModule` derivation under `sim/src/modules/mig29/`, registered by name through `FBModuleRegistry`, composed from the `systems/` defaults; no second architecture, no special case in the runner |
| Its model lives under the single model root and obeys the delta rule | `sim/assets/aircraft/mig29`, declared in `sim/assets/MODEL-DELTAS.md`, `make -C sim verify-models` green |
| It perceives only through simulated sensors | same boundary as the F-16: registry reaches sensor slots only, contacts are anonymous, identity only through IFF ([`../sim/sensors.md`](../sim/sensors.md)) |
| Its weapons are units like ours | R-27R/T, R-73, R-60M, GSh-301 — each with its own module, own FDM where it is guided, own telemetry file ([`../sim/weapons-and-damage.md`](../sim/weapons-and-damage.md)) |
| Its doctrine is GCI-led, not lone-wolf | the pilot flies a briefed vector under ground control: cued search, late own-radar emission, commit and disengage rules from the ground picture rather than from his own scope |
| Every number carries its provenance | derived / measured / `[SET]`, exactly as in `../conventions.md`; the manuals are cited through `doc/mig29/`, never restated here |

### Why it exists at all

Because the F-16 duel is symmetric, and symmetry produces stalemates: every long shot is defeated in
the notch, and the outcome is a coin toss rather than a decision
([`../sim/pilot-ai.md`](../sim/pilot-ai.md), gap 2.3). An opponent with a different radar, a different
missile envelope and a different doctrine turns the coin toss into a choice. That is the reason the
asymmetric weapon round (R6) comes before the airframe round (R8).

### Build order (the contract with the roadmap)

| Step | Depends on | Done when |
|---|---|---|
| Knowledge base `doc/mig29/` incl. `flight-model-spec.md` with documented envelope anchors | — | R3 lands |
| Weapon family (R-27R/T, R-73, R-60M class) + RCS as a unit property | R3 | R6 lands; envelopes measured, not asserted |
| JSBSim model built along the spec's build order, each step measured against one documented anchor | R3 | R8 lands; `MODEL-DELTAS` bookkeeping complete |
| Module + GCI-led pilot behaviour | R6, R8 | flies a mission against the F-16 in the gym with a decided outcome |

## State

**Nothing built.** No module, no model, no weapons, no doctrine. `doc/mig29/` currently holds
cockpit-displays, defence/RWR/CM, engines/fuel, flight controls, radar/sensors and weapons; the
`flight-model-spec.md` that this file's acceptance criteria point at is still being written (R3).

## Gaps

The gap is the whole spec. In roadmap terms it is the chain **R3 → R6 → R8 → R7**:

| # | Missing | Blocked by |
|---|---|---|
| 1 | `doc/mig29/flight-model-spec.md` with documented envelope anchors | R3, in progress |
| 2 | Enemy weapon family + RCS as a unit property | R3 |
| 3 | JSBSim MiG-29 model measured against its anchors | R3 |
| 4 | Module + GCI doctrine | R6, R8 |

Note on ordering, from the roadmap: the first opponents that are actually buildable are the one-way
drone and the cruise missile (R7) — they are real F-16 tasking, they stress the Doppler notch and the
gun, and `modules/drone` tests the module architecture at a fraction of this file's cost. The MiG-29 is
the first *manned* opponent, not the first opponent.

## Knowledge

Nothing derived yet. What exists is the source split, and it is worth stating once:

| Question | Where it is answered |
|---|---|
| What the real MiG-29 does | `doc/mig29/` (from the two DCS manuals + research), the same relationship `doc/f16/` has to [`f16.md`](f16.md) |
| How a module is composed and registered | [`f16.md`](f16.md), [`../architecture.md`](../architecture.md) |
| What a weapon must be to exist here | [`../sim/weapons-and-damage.md`](../sim/weapons-and-damage.md) |
| What a model copy may deviate in | `sim/assets/MODEL-DELTAS.md`, [`stores.md`](stores.md) |
