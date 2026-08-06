# Stores — the model side

**Contributes:** `simulated` `instances`

**Subject:** the JSBSim models a store flies on, and the discipline that keeps a copy honest against
the pinned upstream. The *behaviour* of stores — release path, guidance, ballistics, hit resolution,
damage — is not here: it is in
[`../../weapons.md`](../../weapons.md). A weapon on a pylon has no behaviour
at all, only mass, drag and a model name (`core/FBStore.h`); its behaviour **is** the FDM it becomes
when it is released.

**Neighbours:** `doc/modules/f16/flight-model.md` §10 (what the pinned f16 model declares about carriage),
`mods/f16/src/aircraft/MODEL-DELTAS.md` (the delta list and its gate), [`f16/module.md`](f16/module.md) (the carrier),
[`../sim/fdm.md`](../fdm.md) (how carriage and damage reach JSBSim).

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| Every model FlightBox flies lives under ONE root | `mods/f16/src/aircraft/<model>/`, self-contained with its own `engine/` and `Systems/` subdirectories (JSBSim's own per-aircraft layout) |
| The pinned submodule is the **base**, not a load path | `sim/vendor/jsbsim` is never loaded from; it is what a copy is diffed against |
| A copy may deviate — but only as a **named, evidenced entry** | `mods/f16/src/aircraft/MODEL-DELTAS.md`: file, change, reason, evidence, plus the canonical unified diff itself |
| A better mission outcome is explicitly **not** evidence | evidence is a published source, a demonstrable error in the model, or a missing element an extension needs |
| The gate is exact text comparison, in both directions | `make -C sim verify-models` fails on an unexplained byte **and** on a declared-but-absent change; deliberately not `patch`/`git apply`, which could swallow a deviation with fuzz |
| Every directory under `mods/f16/src/aircraft` must be declared | an undeclared model is an unverified model and fails the gate |
| A store's catalogue numbers come from its own model or a named formula | Mk-82 mass = its `<emptywt>`, CdA = its own CDmin table × its own wing area (`core/FBStore.h`) |
| An unguided store's module does nothing but integrate | `modules/stores/FBStoreModule`: no control channel is ever written, so the trajectory is the model's aerodynamics plus gravity and nothing else |
| A guided round is a different module, not a flag | `modules/missile/`; which one a catalogue entry is, is decided by its `Guided` flag at exactly one place per registration file |

### The external fuel tank (`C5`, first half)

The tank is the one store whose subject is not what it does to a target but what it does to the
carrier's **reach**. It is a KIND of store, not a second mechanism: it hangs on a pylon, weighs, drags
and can be let go exactly like a bomb — and additionally OWNS one of the airframe's own JSBSim fuel
tanks for as long as it hangs there.

| Contract | Acceptance / measurement anchor |
|---|---|
| A fuel tank is a catalogue row with one more number | `FBStoreSpec::FuelLbs > 0` — appended, defaulting to 0, so every row written before it is *not* a tank and is byte-identical |
| There is exactly ONE fuel account, and it is the ENGINE's | the tank's fuel IS `FGTank` contents in the model's own external tank; the tank's DRY mass is the station point mass. FlightBox adds no quantity of its own, integrates no burn and copies no total — **Prinzip 2**. Acceptance: `sms_lbs` (the SMS's books) and `fuelLbs` (the propulsion's books) never describe the same pound |
| A fuel tank needs a **plumbed** station | `FBStoresSystem::DeclareFuelPlumbing(station, tankIndex)` — the module states which pylon is connected to which of ITS model's tanks. Loading a tank on an unplumbed station is refused (`SET_REJECTED`), because a pylon without a fuel line is a pylon without a fuel line |
| The plumbing is the MODEL's statement, not FlightBox's | `f16.xml` declares four tanks and names two of them `External Tank number 0 (station 4)` / `1 (station 6)`; `modules/f16/FBF16Sms` carries exactly that mapping and nothing invented |
| Loading a tank ASSERTS OWNERSHIP of the airframe's external tanks | the plumbed tank of a loaded station is filled to the store's quantity (clamped to the container's own capacity); **every other plumbed tank is emptied and deselected** — there is no tank hanging there, so there can be no fuel in it. Nothing happens on a jet that carries no fuel store: byte-identity by construction |
| The transfer ORDER is JSBSim's own, declared and not implemented | `propulsion/tank[i]/priority`: loaded external tanks → 1, internal tanks → 2. `FGPropulsion::ConsumeFuel` then empties every priority-1 tank that has fuel before it touches priority 2, and several tanks of one priority drain equally. That IS the F-16's NORM transfer sequence (external before internal, wing tanks together), and FlightBox writes ONE integer per tank to say so |
| The order is restored when the last tank goes | each tank's initial priority is read at attach and written back once no fuel store remains, so a jet that dropped its tanks is in exactly the fuel configuration its model declares |
| **Jettison changes three things, and the third is what makes it not a bomb** | mass (the point mass goes to 0), drag (the CdA leaves the `fb-stores` sum) **and the fuel state**: the plumbed tank is emptied and deselected in the same call. A tank dropped with fuel in it takes that fuel out of the aircraft — it is not transferred, not credited, and the falling body carries its deck's one dry mass |
| Jettison travels the ONE release path | `FBStoresSystem::Release` — the same master-arm-gated pickle every store leaves by, the same `FBStoreRelease`, the same spawned unit with its own FDM and telemetry. No second exit from a pylon exists (see Gaps: the real jet's selective/emergency jettison is not master-arm gated, and that is a stated simplification) |
| A jettisoned tank is inert | `WarheadKg` 0 — it ends in `UNIT_RESULT … IMPACT` like every store and damages nothing |
| **Air-to-air refuelling is NOT built, deliberately** | it is the expensive half (a tanker unit, a boom, a contact/latch state machine, a transfer rate and a formation task) and W2 does not need it if tanks carry. What stays unmeasurable without it is named in Gaps rather than approximated |

## State

| Model | Origin | Used by |
|---|---|---|
| `f16` (incl. `Systems/` and the two referenced engine XML as `f16/engine/`) | copy of the pinned upstream | the product |
| `mk82` | copy of the pinned upstream | `modules/stores`, registry name `mk82` |
| `aim120` | **FlightBox's own** — the pinned submodule has no AMRAAM | `modules/missile`, registry name `aim120` |
| `tank370` | **FlightBox's own** — the 370-gal wing tank as a falling body, built by the `mk84` recipe | `modules/stores`, registry name `tank370` |

### The external fuel tank — built 2026-07-30, and what it measured

`C5`'s first half is closed. `set store 4 tank370` / `set store 6 tank370` on the F-16, plus the new
`set fuel_int_pct` that makes a clean jet declarable at all. **Not** built: air-to-air refuelling
(§Gaps).

| Measurement | Clean | 2 tanks | 2 tanks, jettisoned when dry |
|---|---|---|---|
| fuel aboard at t = 0 | 6,971.9 lb | **11,523.9 lb** | 11,523.9 lb |
| range at flameout | **2,627.4 km** | **3,862.9 km** | **4,047.7 km** |
| specific consumption | 2.6535 lb/km | 2.9832 lb/km | 2.8470 lb/km |
| against clean | — | **+1,235.5 km (+47.0 %)** on +65.3 % fuel | +1,420.2 km (+54.1 %) |

Missions: `tank-radius-clean.fbm`, `tank-radius-tanks.fbm`, `tank-jettison.fbm` — one identical 5,200 km
leg at 8,000 m, the same commanded speed, flown to fuel exhaustion.

**The draw order is JSBSim's, and the proof is one number.** In the loaded run the total fuel reaches
**6,972.0 lb — the model's own internal capacity, to the digit — at t = 7,285.8 s**: both external tanks
empty, not one pound out of an internal one. FlightBox wrote one integer per tank
(`propulsion/tank[i]/priority`, external 1 / internal 2) and `FGPropulsion::ConsumeFuel` did the rest.

**What an EMPTY tank costs while it hangs there**, measured in a common window (t = 7,400–12,000 s,
identical 941.7 km, identical 271.18 kt CAS and Mach 0.6679 in both runs, so only the jet differs):

| | carried | jettisoned | difference |
|---|---|---|---|
| fuel burnt | 2,755.0 lb | 2,559.1 lb | **−195.9 lb (−7.1 %)** |
| per kilometre | 2.9257 lb/km | 2.7177 lb/km | −0.2080 lb/km |
| per hour | 2,156.1 lb/h | 2,002.8 lb/h | −153.3 lb/h |
| what left the jet | — | 2 × 240 lb, 2 × 0.475 ft² CdA, 2 × 0 lb of fuel | |

That is the whole reason the historical profile drops them, and it is now a number rather than a
sentence: **184.7 km of range, 4.8 %.**

**Byte-identity.** A jet that loads no fuel tank never enters any of this: the priority write, the tank
fill and the ownership rule are all behind "is any station carrying a fuel store". Measured over all 238
stock missions before/after: **no mission moved because of the tank** — the 42 that moved are the
pilot's branch order ([`../pilot.md`](../pilot.md) §7.4a) and are listed there.

The delta list carries **one** entry (D1, the flaperon mixer): four upstream-covered paths, one delta,
36 FlightBox-own models. Gate green (`verify-models`, 2026-07-30). Measured when the single root landed: 121/121 telemetry files byte-identical over
50 missions, all exit codes unchanged, all seven harnesses rc=0, corner speed unchanged
(380 KCAS / 16.2214 °/s), WASM builds and trims from the embedded root — see
[`../journal.md`](../journal.md).

## Gaps

### Fidelity

| # | Thing |
|---|---|
| 1 | **The Mk-82 model carries no documented aerodynamics.** Its own `<note>` calls itself a possibly crude approximation whose only similarity to the real object is the name. Consequence: the measured CCIP/CCRP accuracy (22 m total, 10.6 m lateral) is a statement about fidelity **to the model**, not about a real release. The error-budget split stays valid — it measures our guidance against our own ballistic table — but the absolute number must not be quoted as a fidelity result. Sourcing or building a Mk-82 model with documented aerodynamics is open. |
| 2 | No enemy store family exists at all (R-27/R-73/R-60M class) — see [`mig29/module.md`](mig29/module.md) and roadmap R6. |
| 3 | **`set fuel_pct` / `set fuel_lbs` fill the model's EXTERNAL tanks even on a jet that carries no tank.** `FBFdm::SetFuelTotalLbs` distributes over every declared tank in proportion to its capacity, and the pinned f16 declares four (2 × 3,486 lb internal + 2 × 2,991 lb external, total 12,954 lb). [MESS, `cmd-avionics.fbm`, `set fuel_pct 60`] `fuelLbs` at t = 0.1 s is **7,772.27 lb** = 0.6 × 12,954 — of which **3,589 lb sit in containers the aircraft is not carrying**. Every fuel figure ever quoted for an F-16 in this tree is therefore a figure for a jet with two phantom tanks, and the "combat radius on internal fuel" W2 wants to measure is not what any existing mission measured. **NOT fixed here, and the reason is stated rather than hidden:** the correction removes up to 46 % of the fuel of every one of the 238 missions and all nine campaigns, i.e. it is a full re-baseline of the tree and a round of its own. What this round adds instead is the DECLARATION that makes the clean case expressible for the first time (`set fuel_int_pct`, which fills only the tanks no station is plumbed to) and the tank that makes the external fuel legitimate. |

### `C5`, second half — what stays unmeasurable without air-to-air refuelling

Deliberately not built (§Spec). Naming the consequences is the price of that decision:

| Question | Why the tank cannot answer it |
|---|---|
| **A profile longer than one full fuel load** | a tank raises the reach ONCE. Anything beyond internal + external is a transfer in flight and has no expression at all |
| **A tanker that is late, weathered in, or at the wrong track** — Package Q's failure mode 1 ([`../campaigns/w3-desert-storm.md`](../campaigns/w3-desert-storm.md)) | the failure is a rendezvous that does not happen. Without a rendezvous there is nothing to fail; W3 sortie 06 therefore measures the EFFECT of a fuel state and never its cause |
| **The cost of the join-up itself** | time, the drop out of the package, the fuel burnt getting to the boom and the fuel burnt holding position |
| **A jet that goes home because it could not take fuel** | the only fuel decision in the tree is the minimum-fuel break-off ([`../pilot.md`](../pilot.md) §7.4a); "could not take fuel" is not a state anything can enter |
| **Everything downstream of a boom**: a receiver's FLCS gain change with the refuelling door open, the tanker as a unit with a track, a boom operator | all of it is the expensive half |

What the tank DOES make measurable is the whole of W2's constraint as the anchor states it: a jet that
leaves with more fuel than it can hold internally, runs the external tanks dry en route, drops them,
and reports what is left at the last waypoint.

### Open decisions (from the retired `TODO.md` §6)

| Question | State |
|---|---|
| **The first real model delta.** The rule and its gate stand, the list is empty. Untested is exactly one thing: whether the entry format carries a MULTI-FILE delta or a NEW file (diff against `/dev/null`) in daily use. The verifier can do both; it has not been measured. | open |
| **Jettison is master-arm gated, and the real jet's is not.** A tank leaves by the ONE release path, so a mission that drops tanks must brief `master_arm arm` first. The F-16 has a selective/emergency stores jettison that is a separate control and answers with the master arm SAFE. Adding it is a second command target and a second exit from a pylon; it was not built because nothing in W2 needs the difference, and stating it beats a jettison that quietly pretends to be a weapon release. | open |
| **Nobody drops an empty tank on their own.** The jettison moment is briefed (`brief_release_s`), not decided: the pilot has no rule "the tank is dry, therefore it goes". That is one instrument reading away — the SMS knows the plumbed tank's contents — but it is a pilot decision and belongs with the minimum-fuel decision in [`../pilot.md`](../pilot.md), which built one of the two this round and named this one. | open |

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
| F-16 station geometry as the model itself gives it (tank butt line ±65 in, half span 180 in, CG station longitudinally) — and why it is longitudinally collapsed | [`f16/module.md`](f16/module.md), [`../../weapons.md`](../../weapons.md) |

### The 370-gallon wing tank — every number and where it comes from

The type W2's anchor describes: the F-16's 370-gal wing tank, hung on stations 4 and 6, run dry en
route and jettisoned. One published figure exists, and the derivation says so.

| Quantity | Value | Provenance |
|---|---|---|
| Capacity | **370 US gal** | [T4] DCS F-16C Viper Guide Part 7 fuel-system table, distilled in [`f16/engine-fuel.md`](f16/engine-fuel.md) |
| **Full carriage weight** | **2,516 lb** | [T4] same table. This is the only number the source states, and it is the only one the airframe actually feels |
| Dry (empty) mass — `MassLbs` | **240 lb** | **[SET]** the published empty-weight class of this tank; the guide gives the loaded figure only |
| Fuel carried — `FuelLbs` | **2,276 lb** | **[DERIVED]** 2,516 − 240 |
| *(the tension, stated)* | implied density 6.15 lb/gal | below the 6.7 lb/gal JP-8 planning value, so the SPLIT is where the uncertainty lives. It moves neither gross weight (2,516 lb either way) nor CG (in this model the pylon and the external tank sit at the same butt line), only how much of the 2,516 lb burns away. Taking the density as certain instead would leave a 111 lb empty tank, i.e. an empty tank not worth jettisoning — which is the assumption that would silently answer W2's question |
| Container bound | model tank capacity **2,991 lb** | `f16.xml`, `External Tank number 0/1`. 2,276 < 2,991, so the store fits and the container is never the binding number; a store that declared more would be clamped to the container and the clamp logged |
| Frontal area | **3.95 ft²** (d = 26.9 in) | **[DERIVED]** 370 gal = 49.47 ft³; a fineness-ratio-8 body of revolution encloses ≈ 0.65 · S · L, and with L = 19.25 ft **[SET**, the tank's pylon envelope**]** → S = 49.47/(0.65 · 19.25) |
| `DragAreaFt2` | **0.475 ft²** | **[DERIVED]** CD 0.12 × 3.95 ft². CD **[SET]**: a streamlined body of revolution of this fineness is a 0.05–0.06 body on frontal area (Hoerner class), and the accepted rule for a pylon-mounted installation with interference is about double. Scale check against this tree's own measured number: two tanks = 0.95 ft² of CdA, against four Mk-82 whose 1.46 ft² measurably cost Mach 1.416 → 1.364 |
| `MaxFlightS` | **120 s** | **[SET]** a jettisoned tank falls; 120 s covers a drop from any altitude this tree flies |
| `WarheadKg` | **0** | it is a tank |
| Falling deck | `assets/aircraft/tank370` | FlightBox's own, built by the same recipe as `mk84` (the Mk-82 aerodynamic deck at this body's reference area), and carrying that deck's fidelity caveat in full. Nothing in this tree measures a falling tank's trajectory |
