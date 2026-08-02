# Missions — the `.fbm` format and its runtime

**Source of this collection:** the former `doc/mission-format.md` (German, split and translated in the
Phase-3 mirror rebuild) plus the former `doc/flightbox/sim/units-and-missions.md` (already English,
now [`runtime.md`](runtime.md)). This directory mirrors `sim/src/missions/`.

The mission format is the interface of the **mission control loop** (`../build-and-ops.md`): define a
mission → simulate headless to completion/error/crash → analyse the telemetry mechanically → correct →
loop. Everything a mission can declare is here; everything the runner does with it is in
[`runtime.md`](runtime.md).

---

## The leading rules

**0 — All four foundation contracts of this directory are BUILT.** The mission clock `time` (`C2`,
[syntax.md](syntax.md)), the four objective kinds (`C12`, [verdict.md](verdict.md)), the visual sensor
channel (`C3`, [`../sensors.md`](../sensors.md) §9 with its mission switches in [sensors.md](sensors.md))
and the campaign layer (`C0`, [campaign.md](campaign.md)). Each round left the missions below untouched:
a `.fbm` run singly judges exactly as it did before, which is measured every time.

**1 — A mission describes a FLIGHT, not a jet.** Mission-wide data (name, optional runway, timeout,
weather) plus a **list of actor blocks** (`unit <callsign>`). Every block is exactly one simulated unit
with its own module, team, initial state and objectives. A single flight is the special case "one
block" — no second dialect, no special path in the code.

**2 — The orchestrator knows no mission specifics.** Four steps: load the mission → set the world up
with its actors (resolve the elevation per actor, spawn the module) → run the actors (cycle every
module, feed both monitors per unit) → validate the world (the monitors decided long ago). `module
<name>` resolves through `FBModuleRegistry`; the runner never names a concrete module type.

**3 — The initial state is pure data declaration.** No ground/air special case in the code: `spawn`
carries position + altitude-or-ground + heading + speed, and a SINGLE IC application
(`missions/FBMissionBoot.h::FBMissionSpawnActor`) serves both.

**4 — The exit code is the verdict.** Termination SUCCESS/FAIL/CRASH/TIMEOUT → exit codes 0/1/2/3,
combined from the two incorruptible judges per unit ([`verdict.md`](verdict.md)):

| Exit | Verdict | Condition |
|---:|---|---|
| 0 | SUCCESS | every unit with objectives, whose loss was not another's declared objective, reached them |
| 1 | FAIL | a unit with objectives failed decisively (touchdown off the runway, or shot combat-ineffective without that being anybody's objective) |
| 2 | CRASH / LOC | an AIRCRAFT is no longer alive — `core/FBSystemHealth::Destroyed()`, set only through `FBDamageModel::ApplyPhysicalKo` out of `core/FBFlightMonitor`'s physical checks — and its loss was not another's declared objective. The loop asks the damage REGISTER, so a future unit kind inherits the rule; the RESULT line quotes the monitor's own reason (`missions/cfit-oberland.fbm` is the flown proof: exit 2, `CFIT`) |
| 3 | TIMEOUT | a unit with objectives did not reach them before the timeout |

Combat missions (`set task bfm`, `set task intercept`) end in TIMEOUT **by construction** — an
engagement has no waypoint objective. Their verdict is read out of the telemetry, not out of the exit
code ([`combat.md`](combat.md)).

**5 — A mission file's header comment is a binding reading rule.** Every `.fbm` in `sim/missions/`
states in its own header what it PROVES and why it is built the way it is (which geometry, which single
changed line against its sibling mission, which number it is the measurement for). That comment is part
of the mission, not decoration: a run whose result contradicts its header comment is a finding, and a
mission whose subject changes gets its header rewritten in the same round. `sim/web/missions/` holds
build artefacts — copies, never sources.

**6 — Column order is append-only.** New telemetry sources are always registered at the end
(`units/FBSimUnit::StartTelemetry`) so that no measured column ever loses its position
([`output.md`](output.md)).

**7 — A campaign never edits a mission.** `sim/campaigns/*.fbc` name `.fbm` files in `sim/missions/`
and carry three facts between them; the files themselves stay the statement of what was flown, and rule
5's header comment stays true. A `.fbc` carries its own binding header for the same reason
([campaign.md](campaign.md)).

## The files

| File | Content |
|---|---|
| [syntax.md](syntax.md) | the line syntax, the two scopes, the keyword table, parse errors versus runtime FAIL, the data model, the tick order and the snapshot rule |
| [verdict.md](verdict.md) | the two judges, the combination rule, when a waypoint counts as reached, combat objectives, the expected-loss rule, shoot-down as a mission verdict, the landing standstill rule |
| [sensors.md](sensors.md) | the `set` keys and rules for datalink, FCR/IFF, RWR, the KOLS, countermeasures and the EYE — scan volumes, net cycle, anonymity of a contact, the blind zone, the six CMDS programs, and the three `visual*` keys with their nine columns |
| [avionics.md](avionics.md) | three-state block validity, the command bus with its four outcomes and the rejection catalogue, and the `brief_*` lines the pilot enters in flight |
| [weapons.md](weapons.md) | load-out, release, the gun, the store initial condition and life cycle, the guided round, ground targets, the air-to-ground attack and its measured error budget |
| [combat.md](combat.md) | `set task bfm`, `set task intercept`, the engagement state machine, the `bfm_*`/`eng_*` columns, the sixteen `pilot_*` variant keys and the tournament runner |
| [weather.md](weather.md) | the `wx` line, the three providers, the precedence rule, how the wind acts and the measured crosswind and release cases |
| [output.md](output.md) | the files per run, damage events and columns, unit attribution, `UNIT_RESULT`, and the catalogue of example missions |
| [runtime.md](runtime.md) | the machinery behind the format: `FBUnit`/`FBSimUnit`/`FBUnitRegistry`, the snapshot barrier, the four-step orchestrator, the multi-unit stages incl. the thread pool and the honest scaling numbers, detonation and impact resolution |
| [campaign.md](campaign.md) | **the layer above a mission** (`C0`, built): the `.fbc` file, the three carried facts and the rule that rejected the rest, the overlay that may delete but never add, the campaign fingerprint that makes a campaign replayable, and `fb-gym --campaign` / `--state` with their two measured determinism proofs |

## Related

| Place | Relationship |
|---|---|
| [`../core.md`](../core.md) | the value types the format parses into: `FBMission`, `FBSpawn`, `FBFlightPlan`, `FBObjective`, the two monitors |
| [`../pilot.md`](../pilot.md) | what a `set task` phase actually flies |
| [`../sensors.md`](../sensors.md), [`../weapons.md`](../weapons.md) | the implementation contracts behind the mission-side switches |
| [`../build-and-ops.md`](../build-and-ops.md) | the gates and the mission control loop that consumes these files |
| [`../modules/f16/`](../modules/f16/INDEX.md) | the real jet's boxes a mission switch refers to |
