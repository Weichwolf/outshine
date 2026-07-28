# Stores — the model side

**Subject:** the JSBSim models a store flies on, and the discipline that keeps a copy honest against
the pinned upstream. The *behaviour* of stores — release path, guidance, ballistics, hit resolution,
damage — is not here: it is in
[`../weapons.md`](../weapons.md). A weapon on a pylon has no behaviour
at all, only mass, drag and a model name (`core/FBStore.h`); its behaviour **is** the FDM it becomes
when it is released.

**Neighbours:** `doc/modules/f16/flight-model.md` §10 (what the pinned f16 model declares about carriage),
`sim/assets/MODEL-DELTAS.md` (the delta list and its gate), [`f16/module.md`](f16/module.md) (the carrier),
[`../sim/fdm.md`](../fdm.md) (how carriage and damage reach JSBSim).

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| Every model FlightBox flies lives under ONE root | `sim/assets/aircraft/<model>/`, self-contained with its own `engine/` and `Systems/` subdirectories (JSBSim's own per-aircraft layout) |
| The pinned submodule is the **base**, not a load path | `sim/vendor/jsbsim` is never loaded from; it is what a copy is diffed against |
| A copy may deviate — but only as a **named, evidenced entry** | `sim/assets/MODEL-DELTAS.md`: file, change, reason, evidence, plus the canonical unified diff itself |
| A better mission outcome is explicitly **not** evidence | evidence is a published source, a demonstrable error in the model, or a missing element an extension needs |
| The gate is exact text comparison, in both directions | `make -C sim verify-models` fails on an unexplained byte **and** on a declared-but-absent change; deliberately not `patch`/`git apply`, which could swallow a deviation with fuzz |
| Every directory under `sim/assets/aircraft` must be declared | an undeclared model is an unverified model and fails the gate |
| A store's catalogue numbers come from its own model or a named formula | Mk-82 mass = its `<emptywt>`, CdA = its own CDmin table × its own wing area (`core/FBStore.h`) |
| An unguided store's module does nothing but integrate | `modules/stores/FBStoreModule`: no control channel is ever written, so the trajectory is the model's aerodynamics plus gravity and nothing else |
| A guided round is a different module, not a flag | `modules/missile/`; which one a catalogue entry is, is decided by its `Guided` flag at exactly one place per registration file |

## State

| Model | Origin | Used by |
|---|---|---|
| `f16` (incl. `Systems/` and the two referenced engine XML as `f16/engine/`) | copy of the pinned upstream | the product |
| `mk82` | copy of the pinned upstream | `modules/stores`, registry name `mk82` |
| `aim120` | **FlightBox's own** — the pinned submodule has no AMRAAM | `modules/missile`, registry name `aim120` |

The delta list is currently **empty**: four upstream-covered paths, zero deltas, one FlightBox-own
model. Gate green. Measured when the single root landed: 121/121 telemetry files byte-identical over
50 missions, all exit codes unchanged, all seven harnesses rc=0, corner speed unchanged
(380 KCAS / 16.2214 °/s), WASM builds and trims from the embedded root — see
[`../journal.md`](../journal.md).

## Gaps

### Fidelity

| # | Thing |
|---|---|
| 1 | **The Mk-82 model carries no documented aerodynamics.** Its own `<note>` calls itself a possibly crude approximation whose only similarity to the real object is the name. Consequence: the measured CCIP/CCRP accuracy (22 m total, 10.6 m lateral) is a statement about fidelity **to the model**, not about a real release. The error-budget split stays valid — it measures our guidance against our own ballistic table — but the absolute number must not be quoted as a fidelity result. Sourcing or building a Mk-82 model with documented aerodynamics is open. |
| 2 | No enemy store family exists at all (R-27/R-73/R-60M class) — see [`mig29/module.md`](mig29/module.md) and roadmap R6. |

### Open decisions (from the retired `TODO.md` §6)

| Question | State |
|---|---|
| **The first real model delta.** The rule and its gate stand, the list is empty. Untested is exactly one thing: whether the entry format carries a MULTI-FILE delta or a NEW file (diff against `/dev/null`) in daily use. The verifier can do both; it has not been measured. | open |

The other open decision of that section — the flaperon mixer of the f16 model, the first candidate for
a real delta — lives with the carrier: [`f16/module.md`](f16/module.md), Gaps, **awaiting owner decision**.

## Knowledge

| Fact | Source |
|---|---|
| Carriage is physics, not bookkeeping: an occupied station is a JSBSim **point mass**, the sum of drag areas a named `fb-stores` **external force** — mass, CG, inertia and added drag come from the engine | [`../sim/fdm.md`](../fdm.md) |
| Both mechanisms are populated at runtime through the model's own APIs; no model XML is patched for them | [`../sim/fdm.md`](../fdm.md) |
| Without any load, neither is ever created — a clean jet computes bit-identically to one that never heard of stores | [`../sim/fdm.md`](../fdm.md) |
| A released store is structurally a full unit: own FDM instance, own module from the same registry, own telemetry file, the same two judges; its end is a detonation, not a crash (`UNIT_RESULT … IMPACT`) | [`../missions/runtime.md`](../missions/runtime.md) |
| Its initial condition comes from the carrier state (position + station offset, carrier attitude, carrier velocity at that station including ω × r) — no trim, no invented ejector impulse | [`../missions/runtime.md`](../missions/runtime.md) |
| Aircraft XML carries its OWN licence (F-16 = GPL, most LGPL); the `<fileheader>` of every copy stays unchanged | [`../architecture.md`](../architecture.md) |
| F-16 station geometry as the model itself gives it (tank butt line ±65 in, half span 180 in, CG station longitudinally) — and why it is longitudinally collapsed | [`f16/module.md`](f16/module.md), [`../weapons.md`](../weapons.md) |
