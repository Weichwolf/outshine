# Mission data — weapons, ground targets, air-to-ground attack

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Waffen — Zuladung, Abwurf, Aufschlag", "Bodenziele (`module target_soft` | `target_hard`)" and
"Luft-Boden-Angriff (`set task attack`, `set attack_mode ccip|ccrp`)". Translated 1:1 from the German
original; no revision of content.

This file is the **mission-author's view**: how a load-out is declared, what a release does, how a
ground target is put into a mission and what the air-to-ground numbers are. The implementation
contract is in [`../weapons.md`](../weapons.md); the real jet's stores data is in
[`../modules/f16/weapons.md`](../modules/f16/weapons.md).

---

## Spec

### A fired weapon is a UNIT like any other

No special path, no separate ballistics formula: a store that leaves the station is a
`units/FBSimUnit` with its own JSBSim instance on its own model, its own `FBModule`, its own telemetry
file and the same two monitors. Its trajectory is the aerodynamics of the model plus gravity (and, for
a guided missile, its own thrust and its own fins), nothing else.

Two store classes, one mechanism — which class a catalogue entry is, its `Guided` flag says
(`core/FBStore.h`), and its module registers itself accordingly:

| Class | Model | Module | Behaviour in flight |
|---|---|---|---|
| unguided (`mk82`) | `sim/assets/aircraft/mk82` (copy of the pinned upstream) | `modules/stores/FBStoreModule` (all slots default/NoOp) | integrate, nothing else |
| guided, active radar (`aim120`) | `sim/assets/aircraft/aim120` — FlightBox's OWN, the pinned submodule has no AMRAAM | `modules/missile/FBMissileModule` (seeker + guidance + uplink receiver) | seeker acquires, guidance law commands the simulated fins |
| guided, infrared (`aim9`, `r73`) | `sim/assets/aircraft/{aim9,r73}` — FlightBox's OWN | the SAME module; the seeker slot is `FBMissileIrSeeker`, an `sensors/FBIrstSystem` | ANGLES only, pure PN on the measured line-of-sight rate, deceivable by the target's flares |
| guided, semi-active (`r27r`) | `sim/assets/aircraft/r27r` — FlightBox's OWN | the SAME module; the radar seeker, gated by the shooter's illumination | dies with the shooter's lock and never comes back |

### Declaring a load-out

```
unit lead
  module f16
  spawn 46.60000 6.60000 1524 90.0 450
  set store 3 mk82            # one line per pylon
  set store 7 mk82
  set brief_master_arm arm    # without ARM the SMS refuses the release
  set brief_release_s 30      # repeatable: one pickle per line
  set brief_release_s 60
  set brief_gun_s 20 1.0      # repeatable: "<mission seconds> <burst seconds>" — a MOMENT and a
                              # DURATION, the same shape brief_release_s has and for the same reason.
                              # It exists so a gun can be measured without also having to win a
                              # dogfight; the value is the trigger-squeeze duration the command bus
                              # already carries, so every interlock and every rejection is unchanged.
```

`store <station> <type>`: station = the pylon number OF THIS TYPE (F-16: 1..9, 1/9 wingtip,
5 centreline — `modules/f16/FBF16Sms`; MiG-29: 1..6 wing plus 7 centreline —
`modules/mig29/FBMig29Sms`, whose numbering is a stated CONVENTION because no source establishes one),
type = a catalogue key (`core/FBStore.h`; today `mk82`, `aim120`, `aim9`, `r73`, `r27r`). Unknown station, doubly occupied station or unknown type = runtime FAIL at spawn, no silent
empty flight.

**What the load-out does to the aircraft** (`weapons/FBStoresSystem`): every occupied station is a
JSBSim **point mass** on the carrier aircraft (mass, centre of gravity and moments of inertia
therefore come from `FGMassBalance`, not from a computation of our own), and the sum of the drag areas
acts as a JSBSim **external force** (`CdA·qbar`, body-fixed −x direction) at the centre of gravity of
the occupied stations. On release the point mass goes to 0 in the same tick — the aircraft immediately
becomes lighter and cleaner, as physics. Measured (`missions/mk82-carriage-{loaded,clean}.fbm`,
identical but for four `set store` lines, both level at full thrust): 4× Mk-82 = **+2,000 lb** take-off
mass, time to Mach 1.0 **25.8 s instead of 22.6 s**, to Mach 1.2 **51.5 s instead of 41.1 s**, peak
Mach **1.364 instead of 1.416**.

### Release

The release runs over the **command bus** like every other control action (`WeaponRelease`, HOTAS
class, 0.5 s), not over a direct call, and can be rejected. The SMS decides:

| Condition | Outcome | Reason |
|---|---|---|
| master arm not ARM | `rejected` | `hardware_precedence` (§6.2: a physical switch blocks the software path) |
| weight on wheels | `rejected` | `hardware_precedence` (ground interlock) |
| no occupied station selected | `rejected` | `out_of_context` (§6.5: valid command, wrong context) |
| otherwise | `accepted` | — |

After each release the SMS steps to the next occupied station by itself (station step), as a real SMS
does. `missions/mk82-safe.fbm` is the reference run for the rejection (master arm never armed),
`missions/mk82-drop.fbm` runs both cases: four releases and a pickle on an empty jet.

### The gun

The M61A1 is not a store: it hangs on no pylon, never leaves the aircraft and fires a STREAM. It
therefore has its own system slot (`weapons/FBGunSystem`, F-16 installation `modules/f16/FBF16Gun`),
its own bus block (`FBGunBlock`) and its own mission line:

```
unit viper
  module f16
  set brief_master_arm arm    # without ARM the gun refuses the trigger
  set gun_rounds 40           # optional: fewer than the full 510 (e.g. for the empty-drum proof)
  set task bfm                # shooting happens in the combat phase
  set pilot_gun_burst_s 0.3   # optional: shorter bursts
```

**The trigger is a command** (`GunTrigger`, HOTAS class), its VALUE the duration of the press in
seconds (capped at `MaxBurstS` = 1.0 s, reported as `clamped`). The latency is exceptionally NOT the
0.5 s of the other HOTAS commands but 0.1 s: the delay between finger press and first round is the
barrel spin-up, and that is already in the weapon model (`SpoolUpS` 0.3 s) — counting both would be
counting twice (`core/FBCommandBus::kTriggerLatencyS`). Rejections carry a reason:
`hardware_precedence` (master arm SAFE or wheels on the ground), `depleted` (empty drum),
`system_failed` (gun shot to pieces).

**What a burst IS** (`core/FBGun.h`): one ballistic BUNDLE per sim tick — at 6,000 rounds/min and a
0.1 s tick, ten rounds with a common muzzle velocity (muzzle velocity along the bore direction PLUS
the jet's own velocity) and a dispersion cone. The bundle is flown by the client
(`core/FBGunProjectiles`, gravity plus quadratic drag against the ISA density), and hits are resolved
against the PUBLISHED poses (`missions/FBMissionRunner`) — the same boundary as with the proximity
fuze: the weapon never evaluates its own hit.

Events in `events.log`: `gun TRIGGER` (every press), `gun BURST` (every bundle), `gun HIT` (expected
hits, dispersion, impact velocity, energy density, zone), `gun MISS` (the closest approach of a bundle
that hit nothing), `gun DRY` (drum empty), `pilot GUN_TRACK` (the pilot now flies the funnel instead of
the pursuit curve). Telemetry: the `gun_*` columns at the right-hand edge (drum contents, consumption,
triggers, rejections, solution plus funnel verdict).

**What the hits do** (`core/FBDamageModel::ApplyKinetic`): the same register, zone and threshold logic
as a warhead, only the arriving areal energy is computed from the number of hits, the impact velocity
and the dispersion instead of from a fragment mass — and it SUMS per zone, because a burst is a
continuous stream that the tick merely cuts into bundles (otherwise the damage would depend on the
tick rate). A warhead does not sum: a detonation is ONE event.

### Store initial condition

Position, attitude and velocity vector come from the CARRIER, through the one declarative IC
application that also spawns every jet (`fdm/FBFdmBoot` with `FBFdmSpawn::Ballistic`): position =
carrier position + station offset (body-fixed, rotated with the carrier attitude), attitude = carrier
attitude, velocity = carrier velocity **at that station** (CG velocity + ω × r, so that a release in a
roll is right). There is deliberately NO ejector impulse — no citable source exists for its magnitude
([`../modules/f16/weapons.md`](../modules/f16/weapons.md) §4.5), so the store inherits the aircraft's
motion and nothing invented. Nothing is trimmed: a bomb has no rudder.

### Life cycle, tick semantics, determinism

- **Owner** is the runner, as for every actor (`FBActorList`). The module cannot create a unit (the IC
  lies behind `fdm/FBFdmBoot.h`, which includes no `systems/` or `modules/` file); the SMS only puts an
  `FBStoreRelease` record into a queue which the runner drains.
- **Creation**: at the END of the tick in which the release was commanded — the store is therefore only
  computed in the NEXT tick. That is the determinism reason: the step phase distributes actor indices
  over threads, and an actor appearing in the middle of the phase would make the result depend on the
  order. The capacity of the actor list is pre-reserved (one row per occupied station), nothing is
  allocated in the tick path.
- **End**: the impact, decided by `core/FBFlightMonitor` — the same judge as for every jet, against the
  same elevation source. For a weapon that is the detonation instead of a crash: the runner does not end
  the RUN for it (`UNIT_RESULT` calls it `IMPACT`) but retires the unit. A store that neither hits nor
  diverges is discarded after the lifetime of its catalogue entry (Mk-82: 300 s).
- The store FDM deliberately gets NO ground (`units/FBSimUnit`): JSBSim's ground reactions describe a
  RESTING object — the spring/damper values of the mk82 model (10,000 lbf/ft, 200,000 lbf/ft/s) diverge
  at a 150 m/s impact within one step, and no impact state would be left to report. A bomb does not
  bounce, it detonates; where the impact is remains the judge's decision.

### The guided missile (AIM-120)

A guided store has three things a bomb does not, and all three are simulated systems, not formulas:

- **Seeker** (`modules/missile/FBMissileSeeker`, a `sensors/FBRadarSystem`): own active radar, ±10°
  field of view, slewed onto the current target estimate, range and activation range from the
  catalogue. It is OFF until the guidance switches it on, and it acquires like any other radar (several
  looks to "firm", then gimbal tracking ±45°). No IFF: a missile cannot ask who that is.
- **Guidance law** (`modules/missile/FBMissileGuidance`, a `pilot/FBPilot` derivation): proportional
  navigation `a = N · Vc · (Ω × r̂)` with N = 4, gravity compensation, underneath it two lateral
  acceleration loops (accelerometer + gyro, gain scheduled by dynamic pressure) → **fin commands** via
  `FBAutopilot`(Manual) → `FBFlightControl` → `FBFdm::SetControls`. Nothing sets position, heading or
  attitude.
- **Uplink receiver** (`modules/missile/FBMissileUplink`, a `sensors/FBDatalinkSystem`): listens to the
  guidance transmission of ITS shooter (its `units/FBUnit` signature, an observable emission like XMT
  and IFF) and publishes it as the one datalink track on its own bus.

**Three guidance phases** (`msl_phase`): `INERTIAL` (0) — the launch programming, extrapolated
constantly; `MIDCOURSE` (1) — the shooter corrects over the uplink as long as he holds his lock;
`TERMINAL` (2) — its own seeker has acquired. The transition is an EVENT, not a timer: the seeker comes
on at the activation range, the phase changes only when it really acquires — which only succeeds if the
midcourse guidance has aimed it close enough. **If the shooter loses his lock the uplink stops**, the
phase falls back to `INERTIAL` and the missile flies on with its last information
(`missions/intercept-lostlock.fbm` still hits with that, `missions/intercept-defeated.fbm` no longer
does).

**Launch zone (DLZ)**, computed in the fire control system (`modules/f16/FBF16FireControl`) from a
forward integration of the weapon performance table against the current radar geometry: `Raero`
(maximum kinematic range), `Rtr` (a hit even if the target turns away at launch), `Rmin`
(`interlock time · arming time + turn allowance`), plus the time marks to seeker activation and to
impact. The SMS refuses a launch **without a lock**, **without a solution** or **outside** the zone —
in addition to the hardware interlocks (master arm, ground contact). `missions/intercept-dlz.fbm` runs
all three answers in one run.

**Hit**: a store with a proximity fuze (`FuzeRadiusM`, AIM-120: 10 m) detonates when it passes a unit
closer than that radius. Measured is the **closest approach within the tick** (segment CPA between two
poses — at 1,500 m/s closure, two 10 Hz samples lie 150 m apart, and a pure distance test would miss
every real hit), on the TRUTH (published poses), not on the missile's estimate. Nothing fuzes before the
arming time has elapsed — which is why a missile does not detonate on its own carrier.

### Ground targets (`module target_soft` | `target_hard`)

A **static ground target is an entirely ordinary `unit`** — no new keyword, no second declaration
syntax. It differs in exactly one line:

```
unit bunker
  module target_soft            # FBModuleRegistry key like 'f16' — the KIND of target
  team hostile                  # the team a 'kill unit/team' is checked against
  spawn 46.90000 7.05000 ground 90.0 0    # position; 'ground' = elevation from the provider, hdg = longitudinal axis
```

**It has no flight dynamics and therefore no JSBSim model.** That is the design decision, and it sits
at exactly one place: `FBModule::FdmModelName()` returns an EMPTY name, `FBModule::UnitKind()` returns
`FBUnitKind::Ground`, and the spawn path (`missions/FBMissionBoot.h`) therefore builds a
`units/FBSimUnit` WITHOUT an `FBFdm`. Everything else about the unit is unchanged the same mechanics as
for a jet: identity, team, published pose, **health register**, **damage model**, roster, telemetry
file, unit registry. The alternative — giving a bunker a trivial JSBSim model — would have integrated
an invented aerodynamic object at 100 Hz just to reproduce the position it was spawned at.

Consequences that fall directly out of `UnitKind::Ground`: the physics monitor never sees a ground
target (it does not fly, it cannot have a crash), air-to-air sensors (radar/datalink) do not find it,
gun bundles are not resolved against it (**no strafing**, see below), and it stands in the roster
against which `objective kill unit` is checked. A `set` key is ALWAYS unknown on a target and therefore
a mission FAIL — what a target is, its module name says; where it stands, its `spawn` line.

| Module | Structure fails at | …degrades at | intended as |
|---|---|---|---|
| `target_soft` | 2.8e3 J/m² (Mk-82: ~45 m) | 1.2e3 J/m² (~69 m) | unprotected installation, vehicle park, position |
| `target_hard` | 9.0e4 J/m² (Mk-82: ~8 m) | 2.5e4 J/m² (~15 m) | bunker, hardened structure |

Only `Structure` is declared: `FBSystemHealth::CombatEffective` asks about engine, controls and
structure, and a building has exactly one of them. Destroyed = `Structure` failed = combat-ineffective
= `objective kill unit <name>` fulfilled. The thresholds are [SET]
([`../modules/f16/weapons.md`](../modules/f16/weapons.md) §4.7 names warhead internals as a real gap),
anchored to the openly cited order of magnitude of a 50–60 m effect radius of a 500-lb bomb against
unprotected targets.

**Ground impact as a detonation:** if a store hits the ground, the runner — not the module — resolves
its warhead against every ground target nearby, through the same `core/FBDamageModel` as a missile
beside a jet. The impact point is reconstructed **sub-tick** (the store flies on without ground contact
forces, so `depth / sink rate` is projected back): at the 0.1 s tick of the run that is ~20 m of
horizontal travel, one fifth of the entire delivery error. Against AIRCRAFT a ground burst is
deliberately NOT resolved — a jet over one's own detonation is really endangered, but the fragment
geometry against an airframe does not exist here, and an invented radius would be a number pretending
to be physics.

### Air-to-ground attack (`set task attack`, `set attack_mode ccip|ccrp`)

A unit with `set task attack` flies a **bombing attack on the active steerpoint** (`pilot/FBPilot`'s
attack phase). Three parts, one decision:

1. **Approach** — `FBAutopilot::Direct` onto the active waypoint, at ITS altitude and speed, i.e. a
   **level laydown approach**. Deliberately level: `Direct` holds an altitude and flies exactly that
   path exactly and repeatably, whereas a 20–30° dive would make the pilot fight his own altitude hold
   along the whole path — and then every metre of miss distance would be an argument about the flying
   instead of a measurement of the release computation. The approach is a **PATH, not a bearing**: at
   the moment the approach begins the pilot anchors its origin and commands
   `FBAutopilot::SetDirectLeg` onto the line origin→target — exactly what an attack run is (one turns
   onto the attack heading and HOLDS it). Regulated onto a bearing, the jet drifts laterally away from
   its own path over the approach length (measured: 31 m over 19 km), and the bomb inherits it.
2. **Release** — ONE pickle over the command bus, on the fire control's cue and on nothing else. The
   pilot computes no ballistics; he reads the `FBFireControlBlock` like any other instrument.
3. **Egress** — a 135° avoidance turn with a climb (F-16 numbers: `AttackEgressTurnDeg` 135,
   `AttackEgressClimbM` 600, `AttackEgressS` 30), then back into the route phase.

**CCIP and CCRP are ONE computation, two questions** (`core/FBBallistics`, shared primitive): a forward
integration of the store's ballistics table (`core/FBStore.h`'s `FBWeaponPerf` — mass, ONE Cd,
reference area, arming time) against a flat impact plane. The plane is the **steerpoint elevation**,
i.e. the `FBElevationProvider` value the radar altimeter also reads and against which the monitor
judges the impact. From the same integration:

| Mode | Cue the pilot releases on |
|---|---|
| `ccrp` | `AgTimeToReleaseS <= 0` — the solution cue runs down the steering line and passes the FPM |
| `ccip` | the same moment AND `\|AgCrossErrM\|` within the pipper tolerance (F-16: 45 m) — "the pipper lies LATERALLY on the target", the judgement a countdown cannot make |

On a cleanly flown approach both release in the same tick; on a badly tracked one CCRP releases and
misses, CCIP does not release at all. Both are the documented behaviour
([`../modules/f16/weapons.md`](../modules/f16/weapons.md) §2.5).

**The pickle is led by one's own actuation latency.** A command reaches the box one class latency later
(`core/FBCommandBus`, HOTAS 0.5 s); pressing exactly on the cue would release the store 0.5 s too late
— at 231 m/s that is 115 m, more than the whole computation is worth. The real jet solves the same
problem the other way round: in CCRP the pilot HOLDS the button and the AIRCRAFT releases when the cue
passes the FPM. Measured: without lead 123 m long, with lead 8 m.

Mission lines:

```
  set task attack                 # starts in the attack phase (like 'bfm'/'intercept')
  set attack_mode ccip|ccrp       # which cue the pilot takes; also sets the mode in the release protocol
  set pilot_attack_bias_s <s>     # variant: release s seconds AFTER the cue (+ = late, − = early)
  set pilot_attack_ccip_m <m>     # variant: the CCIP pipper tolerance
```

### Observable

- `events.log`: `sms RELEASE` (station, type, mass balance), `sms RELEASE_REJECTED` (reason + detail),
  `stores SEPARATION` (the complete initial condition), `stores IMPACT` (`mode=ground|lost`, position,
  ground elevation, time of flight, velocity, **impact angle**, attitude), `stores EXPIRED`.
- For guided rounds additionally: `sms LAUNCH_SOLUTION` (the complete launch zone at the moment of
  launch), `sms LAUNCH_OUT_OF_ZONE`, `missile PROGRAMMED` (the launch programming), `missile PHASE`
  (every phase change with reason, time of flight, range), `missile SEEKER_ACTIVE`, `stores DETONATION`
  (target, **miss distance**, closure, time of flight, aspect) and `stores MISS`/`EXPIRED`
  (`closestM` = closest approach to a unit OTHER than the shooter).
- For air-to-ground: `pilot ATTACK_RELEASE` — the cue at the moment of pressing: `ttrS`, `leadS`,
  `biasS`, `alongErrM`, `crossErrM`, `missM`, `bombRangeM`, `tofS`, `armMarginS`.
  `sms RELEASE_SOLUTION` — what the computer PREDICTED as it leaves the jet with the round
  (`predLat`/`predLon`/`predTofS`/`aimLat`/`aimLon`/`aimMissM`/`armMarginS`/`solAgeS`). The counterpart
  to `LAUNCH_SOLUTION` of a guided round.
  `stores IMPACT` — additionally `crossLat`/`crossLon`/`crossBackS`/`crossTofS`: the sub-tick
  reconstructed penetration point, against which everything else is measured.
  `stores DELIVERY` — **prediction against reality**, measured by the owner of the simulation:
  `predErrM` (what the COMPUTER had wrong), `aimErrM` (what the DELIVERY had wrong),
  `aimLongM`/`aimAcrossM` (long/short and right/left in the approach direction), `tofErrS`, `planeM`
  against `groundAslM` (the elevation difference between computation plane and real ground at the
  impact point).
  `damage DAMAGE`/`SYSTEM`/`KILL` at the ground target — the same lines as for a hit jet.
  `mission UNIT_RESULT` of a ground target: `INTACT` or `DESTROYED` (instead of a flight verdict).
- The telemetry file of a guided round has ITS OWN columns (the bus is built per unit, so a jet trace
  does not change by a single column): `msl_phase`, `msl_range`, `msl_closure`, `msl_losrate` (what
  proportional navigation drives to zero), `msl_los_az`/`msl_los_el`, `msl_nz_cmd`/`msl_ny_cmd`,
  `msl_fin_pitch`/`msl_fin_yaw`, `msl_seeker` (0 off / 1 active / 2 acquired), `msl_tgt_age` (age of
  the last real measurement — the number that makes the lock loss visible).
- `telemetry.csv`, six columns appended at the end (existing ones never shift): `sms_arm`,
  `sms_station` (selected station, −1 = none), `sms_loaded`, `sms_lbs` (carried store mass),
  `sms_released`, `sms_gw_lbs` (take-off mass of the aircraft — the mass jump at release therefore
  stands in the same line as the bookkeeping).
- One `telemetry_<callsign>_<type>_<n>.csv` per store: the same schema width as a jet, with the full
  trajectory (10 Hz) up to the impact line.

## State

| Item | State |
|---|---|
| Carriage | built; point mass + external force through model-owned JSBSim APIs, measured against a clean jet |
| Release | built; command bus, three rejection reasons, station step |
| Gun | built; stream cut into bundles, kinetic damage summed per zone, `make -C sim test-gun` |
| Guided round | built; seeker, PN guidance, uplink receiver, three phases, DLZ |
| Ground targets | built; two kinds, structure only, sub-tick impact reconstruction |
| Air-to-ground | built; CCIP and CCRP from one integration, lead by actuation latency |

### Measured — air-to-ground (`--elev const`, flat 0 m base)

`missions/attack-ccrp.fbm` / `attack-ccip.fbm`: 19 km approach, 900 m, 450 KCAS, throw range 2,880 m.

| Quantity | CCRP | CCIP | 2 s late (`attack-late.fbm`) |
|---|---|---|---|
| `predErrM` (computer against model) | 57.1 m | 57.1 m | 57.1 m |
| `aimErrM` (bomb against target) | **22.2 m** | **22.2 m** | **481.5 m** |
| of which long / lateral | 19.5 / 10.6 m | 19.5 / 10.6 m | 481.5 / 3.5 m |
| `tofErrS` | −0.097 s | −0.097 s | −0.097 s |
| Verdict | SUCCESS (exit 0) | SUCCESS (exit 0) | TIMEOUT (exit 3), target stands |

`missions/attack-hardened.fbm` flies the same release against `target_hard`: the same 22 m miss
distance, NO damage line (the arriving energy does not even reach the degrade threshold) — TIMEOUT,
`result=INTACT`. The fragility classes are therefore a model and not decoration.

### Measured — gun tracking

**Proof missions**: `missions/gun-bfm.fbm` (a tracking pass against an opponent flying STRAIGHT) and
`missions/gun-turning.fbm` (the same shooter against bfm-basic's PERMANENTLY TURNING defender — the
hard test, because there the funnel solution WANDERS through the funnel). Both end with a kill (exit 1
= FAIL for the one hit); the verdict stands in the events (`gun HIT`, `damage SYSTEM`, `damage KILL`)
and in `gun_sol_err`/`gun_in_funnel`. `gun-bfm`'s defender leg and clock grew with the capped approach
(50 → 115 km, 400 → 600 s): he flies a STRAIGHT line to a fix, and at 50 km he reached it at t=342 —
his own mission monitor ended the fight for a reason that has nothing to do with guns. The weapon's own
numbers (dispersion fit against MIL-DTL-45500/1A, time of flight, funnel geometry, lead solution
against the flown path, ammunition consumption, rejection on an empty drum) are checked by
`make -C sim test-gun`.

**The tracking is a control loop, not aiming** (`pilot/FBPilot`, section 3c): the law commands a
turn rate ∝ error, and against a turning opponent the demanded bore direction is a RAMP — a pure P
term then stays constant behind by (ramp rate × time constant) (measured: error never below 4.6° at a
~1° funnel tolerance, two bursts, 70 rounds, no hit). That is why ERROR RATE and integral are terms of
their own. Before/after over eight approaches per defender each: funnel time 3.2 s → 20.7 s (straight)
resp. 0.0 s → 21.6 s (turning), rounds on target 11.9 → 111.2 resp. 0.0 → 120.4, kills 0 → 5 resp.
0 → 7 out of eight runs each, mean tracking error 10.5° → 6.9° resp. 11.9° → 4.1°.

**The trigger reads a MAGNITUDE.** The published aiming solution is a magnitude, so its prediction over
actuation latency + time of flight must also be read as one: a solution that wanders THROUGH zero —
exactly what it does against a turning opponent — predicts −1.5° and that means "1.5° off", not
"perfect". The former clamp to 0 made the fastest-wandering solution the best there is in combat.
Measured over the same eight approaches each (only this one line changed): bursts 46 → 30 (straight)
resp. 59 → 38 (turning), rounds on target per burst 1.81 → 4.42 resp. 2.21 → 3.19, ammunition per kill
394 → 254 resp. 270 → 204 rounds.

**The control position is a question of braking authority, not of tracking.** The schedule (desired
closure ∝ remaining range) demands a deceleration of slope × closure; the airframe has 2.4 m/s²
(measured, `FBF16Pilot::BfmBrakeMs2`), so a/k is the largest closure that can still be shed before the
control range — previously a cap of 200 kt stood there and the pilot accelerated to 190 kt closure at
2 nm, only to spend 35 s failing to lose it again with idle and speedbrake (arrival in the band at
98 kt instead of the scheduled 5, then a fly-through at 61 m). Measured over eight approaches each:
peak closure in the stern approach 235 → 183 kt, kills 4/8 → 7/8 (straight) resp. 7/8 → 7/8 (turning),
ammunition per kill 394 → 125 resp. 270 → 201 rounds. The arrival ITSELF remains too fast (median
90 → 85 kt at the band edge, time in the band 21.4 % → 20.7 %): the cap removes the self-inflicted part
of the excess, not the rest — the throttle regulates a speed DIFFERENCE instead of the closure itself,
and both alternatives to that are measured and rejected (a geometric trail angle and a throttle on the
measured closure, see the comments in `pilot/FBPilot`).

### Cross-check against the independent computation

`missions/mk82-drop.fbm` with `--elev const` (flat 0 m base, so release altitude = release AGL).
Measured against the drag-free derivation from
[`../modules/f16/weapons.md`](../modules/f16/weapons.md) §4.2 (`t=√(2h/g)`, range `v·t`):

| Quantity | JSBSim mk82 | drag-free | Difference |
|---|---|---|---|
| Fall time from 2,499 m | 24.10 s | 22.58 s | **+6.8 %** |
| Travel at 231 m/s | 4,600 m | 5,221 m | **−11.9 %** |
| Vertical velocity at impact | 190.8 m/s | 221.4 m/s | **−13.8 %** |

(all four stores of the same run identical to within 0.01 s / 8 m — the spread is the difference of
their release states, not noise)

Both signs are the expected ones: drag lengthens the fall and at the same time brakes the horizontal
component more than it lengthens the fall. The derivation is exactly what its own source claims, a
**lower bound** — it is not a target value and is not computed away.

## Gaps

| Gap | Detail |
|---|---|
| No ejector impulse | no citable source for its magnitude ([`../modules/f16/weapons.md`](../modules/f16/weapons.md) §4.5); the store inherits the carrier motion |
| No strafing | gun bundles are not resolved against ground targets |
| A ground burst is not resolved against aircraft | the fragment geometry against an airframe does not exist; an invented radius would be a number pretending to be physics |
| Ground-target thresholds are `[SET]` | anchored to the cited 50–60 m order of magnitude, but warhead internals are a real source gap |
| The ballistics table is one Cd | the release computer carries one Cd for all Mach numbers and does not know the weathercocking round's lift — a declared omission of `core/FBBallistics`, worth 57.1 m short in the reference run |
| Gun arrival speed still too fast | the throttle regulates a speed difference rather than the closure itself; two alternatives measured and rejected |
| Five store types | `mk82` unguided; `aim120` active-radar, `aim9`/`r73` infrared, `r27r` semi-active |

## Knowledge

- **Why a fired weapon is a full unit.** It gets its own FDM instance on its own pinned model, its own
  module from the same registry, its own telemetry file and the same two judges. That means there is no
  second code path in which a weapon's motion could be computed differently from an aircraft's — the
  trajectory is the model's aerodynamics plus gravity, and nothing else.
- **Why the store spawns at the END of the tick.** The step phase distributes actor indices over
  threads. An actor appearing in the middle of that phase would make the result depend on when it
  appeared, i.e. on the scheduler. Appending at the end and computing it from the next tick makes the
  growth of the actor list order-independent.
- **Why the weapon never evaluates its own hit.** The proximity fuze, the gun bundles and a ground
  burst are all resolved by the OWNER of the simulation against the PUBLISHED poses. The same boundary
  three times: the thing that shoots does not get to decide what it hit.
- **Why CCIP and CCRP share one integration.** They are two questions to the same forward integration —
  "where does it land if I release now" and "when must I release". Sharing it is what keeps the pipper
  and the release countdown from disagreeing.
- **Why the pickle is led and not held.** The real jet solves the actuation delay by having the pilot
  hold the button and the aircraft release on the cue. FlightBox's pilot issues one command, so the
  same effect has to come from leading the cue by the command bus's own latency (measured: 123 m → 8 m).
- **Why the impact point is reconstructed sub-tick.** The store has no ground contact forces, so it
  flies ballistically past the surface; `depth / sink rate` projected back gives the penetration point.
  At 0.1 s and ~200 m/s that is ~20 m — one fifth of the whole delivery error, so ignoring it would
  dominate the measurement it is supposed to support.
