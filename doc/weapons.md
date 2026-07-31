# FlightBox — Weapon employment & damage

**What this file describes:** the complete chain from release to system consequence — who clears a
weapon, how it flies, who decides that it hit, what a hit does in the health register and how that
propagates through the avionics bus, HUD, warning set, command bus and flight physics.

**Sources** (all primary sources in the tree, nothing external):

| Area | Files |
|---|---|
| Release paths | `sim/src/weapons/FBStoresSystem.{h,cpp}`, `sim/src/weapons/FBGunSystem.{h,cpp}` |
| Ballistics | `sim/src/core/FBGunBallistics.{h,cpp}`, `sim/src/core/FBBallistics.{h,cpp}`, `sim/src/core/FBGunProjectiles.{h,cpp}` |
| Resolution | `sim/src/missions/FBMissionRunner.cpp` (`ClosestApproach`, `ResolveBurst`, `ResolveGunHit`, `ResolveGroundBurst`, `GroundCrossing`) |
| Damage | `sim/src/core/FBDamageModel.{h,cpp}`, `sim/src/core/FBSystemHealth.{h,cpp}`, `sim/src/units/FBSimUnit.{h,cpp}` |
| Module data | `sim/src/modules/f16/FBF16Damage.{h,cpp}`, `sim/src/modules/f16/FBF16Sms.h`, `sim/src/modules/f16/FBF16Gun.h`, `sim/src/modules/ground/FBGroundTarget.h` |
| Weapon modules | `sim/src/modules/stores/`, `sim/src/modules/missile/`, `sim/src/modules/ground/` |
| Weapon model | `sim/assets/aircraft/aim120/` (`aim120.xml`, `engine/WPU-6.xml`) |
| Catalogues (TYPES) | `sim/src/core/FBStore.h`, `sim/src/core/FBGun.h` — **documented as types in `core.md`**; here their BEHAVIOUR |
| Mission side | `doc/missions/weapons.md` (sections "Weapons", "The gun", "Air-to-ground attack", "Ground targets") |

**Marking of numbers** (taken from the source code): `[SET]` = a FlightBox setting without a quotable
source, justification named; `[DERIVED]` = computed from a named number by a named formula;
`[T3]`/`[T4]` = confidence levels from `doc/modules/f16/weapons.md`; without a mark = read from the pinned
JSBSim model itself.

---

## Spec

The full chain from release to system consequence: who clears a weapon, how it flies, who decides it
hit, what a hit does in the health register, and how that propagates through avionics bus, HUD,
warning set, command bus and flight physics.

| Contract | Acceptance / measurement anchor |
|---|---|
| A weapon is structurally a unit | own FDM instance on its own pinned model, own module from the same registry, own telemetry file, the same two judges |
| Release happens only through the command bus, and is therefore refusable | Master Arm not ARM / weight on wheels → `hardware_precedence`; empty drum → `depleted` |
| A weapon never scores its own hit | the system queues an event, the OWNER resolves it against published poses — the same boundary for proximity fuze, gun burst and ground burst |
| Ballistics is shared between fire control and the round | the same arithmetic answers "where will it land" and "when must I release" — pipper and countdown cannot diverge |
| Damage is deterministic geometry, no randomness, no time, no internal state | same geometry → same damage picture; measured thread-independent |
| A fragment warhead is ONE event; a gun stream SUMS per zone | otherwise damage would depend on tick rate |
| Consequences run through JSBSim, not bookkeeping | cutoff, control authority, drag area — a shot-up jet keeps flying as long as physics allows |
| Failure → block `Invalid` → visible everywhere | HUD dashes, warning reports INHIBITED, command into the dead box is rejected — none of that was written for damage |

### The seeker decides what a round IS (the asymmetric-weapon round)

A guided round is one module (`modules/missile`) and N catalogue entries; what makes an AIM-120, an
AIM-9 and an R-27R three different weapons is not three classes but the **kind of seeker** the
catalogue entry declares. Each kind is a derivation of a sensor slot that already exists, and each
one's tactical property falls out of the sensor's own limits rather than out of a flag.

| Contract | Acceptance / measurement anchor |
|---|---|
| **Active radar** (`FBSeekerKind::ActiveRadar`, AIM-120) — unchanged | seeker activates at its own range, the shooter's obligation ends there (`Pitbull`), midcourse over the uplink |
| **Infrared** (`FBSeekerKind::Infrared`, R-73 / AIM-9) — an `sensors/FBIrstSystem` derivation. **Its launch zone is bounded by its own head**: lock-before-launch means the round cannot be fired further than it can see, so `weapons/FBLaunchZone` caps `Raero`/`Rtr` at `SeekerRangeM` for this kind and for no other | **angles only.** The law is PURE proportional navigation on the measured line-of-sight rate (`a = N·V_own·(Ω × û)`), because an angle-only head has neither range nor closure to put into `N·Vc·Ω`. No uplink, no midcourse: lock before launch, then fire-and-forget. Measured: a rear-quarter shot hits; the same shot against a target dispensing flares head-on misses |
| **Semi-active radar** (`FBSeekerKind::SemiActiveRadar`, R-27R) — an `sensors/FBRadarSystem` derivation that **does not radiate** | its seeker is alive exactly while the SHOOTER illuminates. Illumination lost = seeker dead, and it **never comes back** (there is no transmitter of its own to switch on) — the round flies out its last information like an uplink loss, but without the second chance. Measured on both branches: an unbroken chain hits, a lock broken in mid-flight misses |
| The support obligation is the shooter's, and it is a DOCTRINE hook, not a timer | a SARH round never goes `Pitbull`, so `pilot/FBEngagement`'s `Support` state runs to predicted IMPACT instead of to seeker activation; and for an airframe whose weapon needs it, `Support` and `Defend` become **mutually exclusive** — beaming breaks the illumination the round lives on. One state-transition guard, one module hook |
| The crank ceiling is the ANTENNA, not a preference | the shooter may only turn as far as its own gimbal still tracks: the F-16's 45° comes off a 60° gimbal, the MiG-29's comes off the N019's 67° |
| A gun row is a gun row | the GSh-301 enters `core/FBGun.h` field for field beside the M61A1, and the SAME kinetic path carries it — 30 mm changes the numbers (2.7× the energy per round, 0.68× the flux) and not one line of `core/FBDamageModel` |
| Twin engines are two ids, appended | `core/FBSystemHealth` gains `Engine2` at the END of the enum (the `dmg_*` bitmask ordinals are telemetry); a single-engine airframe declares only `Engine` and every existing column keeps its meaning and its position |

## State

Built: AIM-120, Mk-82, M61A1, ground targets, damage model.

| Piece | Status | Anchor |
|---|---|---|
| Weapon-as-unit foundation | built | `b62c769` |
| AIM-120 with seeker, guidance, uplink | built | `5c68fc5` |
| Damage model: hits become failures, failures become invalidity | built | `6d84647` |
| M61A1: derived ballistics, EEGS funnel, kinetic damage | built | `a1a8fbf` |
| Air-to-ground: ground targets without FDM, CCIP/CCRP from one integration | built | `1eeff72` |
| **The seeker as the thing that distinguishes weapons**: `FBSeekerKind` + the three catalogue entries AIM-9M / R-73 / R-27R, each with its own FlightBox-own JSBSim deck | built | MiG-29 stage 2c |
| **Infrared seeker** (`modules/missile/FBMissileIrSeeker`) + the angle-only pure-PN law | built | MiG-29 stage 2c |
| **Semi-active seeker**: illumination gate, no reactivation, `TimeToActiveS = -1` reaching the shooter's Support state | built | MiG-29 stage 2c |
| GSh-301 catalogue row + `FBMig29Gun`/`FBMig29Sms`/`FBMig29FireControl` | built | MiG-29 stage 2c |
| `FBSystemId::Engine2` + `PropulsionOut()` + the one-engine-out throttle ceiling | built | MiG-29 stage 2c |
| `weapons/FBLaunchZone` — the DLZ arithmetic moved out of the F-16's box, so two fire controls share one | built | MiG-29 stage 2c |
| **The infrared launch zone is bounded by the round's own head** (`FBSolveLaunchZone`) — an IR round must *"lock before launch, then fire-and-forget"* (§Spec), so it cannot be launched further than its seeker reaches; `RaeroM`/`RtrM` are capped at `perf.SeekerRangeM` for `FBSeekerKind::Infrared` only. An ACTIVE round is unaffected (the uplink carries it to the activation ring) and a SEMI-ACTIVE one is unaffected (the shooter illuminates), which is why it hangs on the seeker kind | built | this round |
| **`Phase::Bfm` employs the IR round** — the shooter side of the same weapon, specified and built in [`pilot.md`](pilot.md) §5.11 | built | this round |

**Measured, stage 2c** (`mig29-r73`, `f16-aim9`, `mig29-r27`, `mig29-gun`, `duel-asym-probe`,
`mig29-intercept`; every one deterministic over `--threads 1/2/4` × 3 repeats, one fingerprint each):

| What | Measured |
|---|---|
| infrared round, rear quarter | R-73 `DETONATION missM=0.138` (fuze 3.5 m, tof 8.19 s); AIM-9 `missM=0.0196` (fuze 6.0 m, tof 7.72 s) |
| infrared round against flares, head-on | both decoyed at `tgtIntensity=0.16`; R-73 misses by **22.80 m**, AIM-9 by **25.96 m** |
| semi-active, unbroken chain | `ttaS=-1 ttiS=27.5` at launch → `DETONATION missM=0.442` after **28.56 s of unbroken illumination** |
| semi-active, lock broken in flight | `ILLUMINATION_LOST tofS=11.7 rangeM=15499` → the round flies the last 15 km blind and misses by **27.04 m**. The AIM-120 with the same lock loss still hits (`intercept-lostlock`, 0.755 m) — that difference IS the semi-active penalty |
| 30 mm kinetic path | at 294 m of round path: σ 1.77 m, ~0.8 rounds per 2-3 round bundle, 7,290 J/m² for the first, **kill at t = 28.7 s on 67 of 150 rounds**. At 571 m of path: σ 3.42 m, ~0.3-0.46 rounds/bundle, the FULL drum wipes the avionics (`dmg_failed 4016`) **without** downing the aircraft — the documented 200-790 m band emerging from the dispersion model rather than from a range limit |
| the MiG-29 intercept, end to end | `RADAR_DESIGNATE` at t = 58.4 s → shot at 60.9 s → support to impact → `damage KILL` at 87.7 s. `eng_state`: search → closing → attack → support |

**Measured, this round — what the CLOSE fight asked of this file.** Not one constant of
`core/FBDamageModel` moved, and the third row is the reason why not:

| What | Measured |
|---|---|
| **How many 30 mm hits a `damage KILL` costs, as arithmetic instead of an impression** | `kFlcsFail` = 1.5·10⁵ J/m² against 0.5·0.39·795² / 14.0 m² = **8,803 J/m² per landed round** ⇒ **17.0 rounds** into one zone [HERL]. `mig29-gun`'s kill lands **17.15** — the harness sits exactly on the threshold it was written before anybody computed. The merge delivered **6.37** and now delivers **9.53** |
| **Where the shortfall is, split** | at the σ the merge is actually fought at (3.78 m, 630 m of path) a PERFECTLY aimed drum lands **20.2** rounds and kills, so the EFFECT is not the ceiling; the drum kills at a mean miss ≤ **2.38 m** and the measured median is **6.41 m** (was 8.72). The whole remaining gap is aim, and `missM ≈ RangeM·tan(GunAimErrorDeg)` on the far bundles proves the aim error at the MUZZLE is the miss ([`pilot.md`](pilot.md) §5.8) |
| **The fragility ladder was NOT touched**, and this is the entry that says why | nothing in `doc/modules/mig29/weapons.md` §4 states how many 30 mm hits down a fighter — the ladder is four `[SET]` numbers that also carry every warhead in the tree, so moving them to make a gunfight lethal would re-price the AIM-120 and the R-27R to buy one outcome. The measured statement is that the ladder is REACHABLE at the range the doctrine is fought at |
| **Two infrared rounds against the same airframe, at the geometry that decides** | AIM-9M (9.4 kg / 6.0 m fuze) at 1.93 m ⇒ **218,781 J/m²** ⇒ flight controls FAIL ⇒ kill. R-73 (7.4 kg / 3.5 m) at 2.62 m ⇒ **94,388 J/m²** ⇒ flight controls DEGRADED. 1/r² puts the two kill radii at **2.32 m** and **2.08 m** — a 12 % geometric difference out of a 27 % warhead difference, and it is what decides the head-on merge |
| **The IR launch zone against its own head** | capping `RaeroM`/`RtrM` at `SeekerRangeM` refuses exactly one shot in the whole tree: `duel-emcon`'s third AIM-9 at **12,807 m** against a 12,000 m head. In the old build that round flew its full 60 s, lost its track twice and expired **2,251 m** wide. Verdict and exit code unchanged |

Measured: CCIP/CCRP total error 22 m (10.6 m lateral) **against our own ballistic table** — see the
Mk-82 fidelity caveat under Gaps before quoting that number.

## Gaps

**Two things changed under this file on 2026-07-28** ([`air-to-ground.md`](air-to-ground.md)), and both
are named here because this file owns the boundary they moved:

| What | Before | Now |
|---|---|---|
| **The third resolution boundary** (§5.1/§5.3) | a proximity fuze resolved against `FBUnitKind::Aircraft` only, and a store only where it CROSSED the surface — so an AIR-BURSTING air-to-ground weapon had no resolution path at all | a store with `FuzeRadiusM > 0` also resolves its burst against `Ground` units at closest approach, the LAUNCHER excluded (a round does not fuze on the rail it left — the gun path's own rule). **§5.4's refusal is untouched:** that one forbids a GROUND burst against an AIRCRAFT, for want of a fragment-against-airframe geometry, and this is its mirror image and not its exception. Conservation: measured byte-identical over all 113 pre-existing missions |
| **The damage model's second input** (§6.3, J/m² shared between warhead fragments and 20 mm impacts) | two mechanisms on one currency, declared as this simulator's CHOICE and not a statement of equivalence | **three.** A cluster canister is an AREAL ENERGY DENSITY over a declared rectangle and takes the identical `ApplyKinetic` path. It is the COARSEST use of that currency in the tree — a BLU-97 is a shaped charge *and* a fragmentation case *and* an incendiary ring — and the verdict against `target_soft` sits 12 % above the failure threshold (measured 3 109 J/m² against 2 800), so `kCaseFraction` and `kFragSpeedMs` decide it. Booked as `air-to-ground.md` N3 |

**Two seeker kinds were appended** to `FBSeekerKind`: `SemiActiveLaser` (whose `FBSeekerHandoverS` is
−1, the R-27R's obligation verbatim, but which REACQUIRES a spot that comes back) and `AntiRadiation`
(whose handover is 0 — the target IS the transmitter, so the shooter owes it nothing). Neither adds a
resolution path; both add a detector, and each detector is a derivation of a sensor that already exists.


### `C1` — the active surface-to-air threat: **BUILT 2026-07-28**, and its home moved

**The gap entry that stood here was a placeholder with a boundary and five open questions. All five are
answered, and the contract now lives where the class will:
[`modules/ground/module.md`](modules/ground/module.md)** — with the nine sourced catalogue rows beside it
in [`modules/ground/catalogue.md`](modules/ground/catalogue.md) and the rest of the campaign cast in
[`modules/ground/cast.md`](modules/ground/cast.md). Nothing is built.

**BUILT, and what this file gained by it:** `FBSeekerKind::CommandGuided` — ONE enum value, no new
architecture. The round powers no detector at all, so `FBMissileGuidance::UpdateTarget`'s existing strict
priority (own seeker > uplink > last known) degenerates to its middle branch for the whole flight;
`FBSeekerHandoverS` answers −1 as for a semi-active round, and `msl_seeker` is 0 from launch to impact
(measured on `sam-sa2-command`). Six new rows in `core/FBStore.h` (`v750` `v601` `3m9` `9m33` `strela2`
`igla`) with one new field, `GatherS` — guidance inhibited after launch, because a round leaving a rail
at zero airspeed has no fin authority — and `FBStoreRelease` gained the RAIL attitude a launcher aims
with (`HaveRail` false for every air-launched store, so the separation path is unchanged).
**`GatherS` was not READ by anything until 2026-07-29** — declared, filled, specified twice, never built;
it is now the early return in `FBMissileGuidance::FlyCommand` (§10.2 "The gathering phase"). `HaveRail`
gained a second consumer in the same round: it is what tells a rail launch from an air launch for
`FBFdmSpawn::MotorRunning`. Two new gun
rows (`azp23`, `zu23`). The two collisions this file's contracts had with a ground launcher resolved:
the weight-on-wheels interlock is not RELAXED but declared inapplicable behind a private one-friend write
gate (`FBStoresSystem::DeclareGroundLauncher`, refused outright once an airframe was ever bound, with
`AttachFdm` asserting the converse), and the second collision turned out not to exist — `PublishLoadout`
and `Release` have carried their `Fdm_` guards since they were written.

**Why the home moved.** This file owns the *weapon* half of `C1` and still does: every launched round is a
unit (§1), released through `FBStoresSystem` (§2), resolved by the three boundaries (§5) and applied by
`FBDamageModel` (§6) — all unchanged. But the thing itself is a **module in `modules/ground/`**, and a
contract belongs with the class it constrains. What stays here is this entry and the two collisions below,
which are defects of *this* file's contracts.

**The five answers, in one line each** (the argument is in `modules/ground/module.md` §Knowledge 1):

| Open question | Answer |
|---|---|
| Does a surface emitter need fields an airborne one lacks? | **No** — `FBEmitterSignature` unchanged. Two things change around it: the signature carries **two** beams (a battery is two antennas), and `FBEmitterKind` gains `SurfaceEarlyWarning`/`SurfaceFireControl`, appended. That second value is also the discriminator [`sensors.md`](sensors.md) gap 25 was waiting for |
| Is a SAM a store module with a seeker, or a new kind? | **A store with a seeker.** Three of four guidance families are already built; radio command is **one** new `FBSeekerKind::CommandGuided` — the uplink branch of the existing phase machine, forever, with `TERMINAL` unreachable |
| What launches it? | **A fire-control state machine**, `FBSiteFireControl : pilot/FBPilot`, six states, commanding through the command bus. Not a scripted release: that would be a world write path outside the simulation |
| Does it acquire through a sensor slot, and does `RESTRICTED` grow? | **Yes, and no.** Four detectors, all derivations of bases that already hold the include (`FBRadarSystem` ×2, `FBVisualSystem`, `FBRwrSystem`). The gate stays at **six** files, and printing *6 restricted header(s) respected* is an acceptance criterion of the round |
| AAA as a gun, against §5.4's refusal | **AAA is a gun, and §5.4 does not bite.** That refusal forbids resolving a *ground burst* (a bomb detonating on the ground) against aircraft. A gun **bundle** fired from the ground is a different object and runs through `ResolveGunHit` with the shooter's own velocity simply zero. What *does* bite is `FBGunProjectiles`' `3 s / 3000 m` cap: it admits 23 mm AAA and excludes every 57/100 mm gun, whose employment is a fuzed bursting shell and therefore wants the store path |

**Two collisions with this file's own contracts**, found while writing that spec. Both are places where a
rule written for a pilot meets a machine that has none:

| Collision | Detail | Proposed resolution |
|---|---|---|
| **The weight-on-wheels interlock refuses every ground launch** | §2.4 check #2 rejects a release on weight on wheels with `hardware_precedence`, and a unit without an airframe reports `AnyWow = true` by definition ([`missions/runtime.md`](missions/runtime.md) §3). A launcher is permanently on the ground | a virtual `FBStoresSystem::GroundInterlockApplies()`, default `true`, `false` for a launcher. The interlock stays first in the order for everything that flies |
| **The SMS assumes an airframe** | §2.1/§2.2: `AttachFdm` declares one JSBSim point mass per station and `PublishLoadout` pushes mass and drag into the deck. A site has no `FBFdm` | `AttachFdm` becomes optional; with no airframe no station masses are declared and `PublishLoadout` is a no-op. A rail's mass is not a flight-mechanical fact for a thing that does not fly |

**What `C1` is not:** not `C21` (no declarable initial damage), not `C14` (no moving ground units, no
ships), not `C17` (a runway with state). Those are named separately and stay separate.

### `C8` — the air-to-ground store family: **SPECIFIED 2026-07-28, and its home is elsewhere**

The store catalogue's air-to-ground half (anti-radiation round, Mk-84, laser-guided bomb, cluster, the
FAB rows, the rocket pod) is specified in [`air-to-ground.md`](air-to-ground.md) §§2–3, for the same
reason `C1`'s contract moved to `modules/ground/`: the subject cuts through `core/`, `sensors/`,
`missions/` and `pilot/` as well as this file, and it belongs in one argument. **This file owns the
weapon half and still does** — every new round is a unit (§1), released through `FBStoresSystem` (§2),
resolved by the three boundaries (§5) and applied by `FBDamageModel` (§6). What that spec asks of THIS
file is two things, and both are defects of contracts written here:

| Collision | Detail | Proposed resolution |
|---|---|---|
| **A proximity fuze is resolved against `Aircraft` only, so no air-to-ground weapon can burst above its target** | §5.1's gate is `FBUnitKind::Aircraft` and §5.3 resolves a store only where it crosses the ground. A round with a laser proximity fuze, a cluster functioning at altitude and every future air-burst weapon therefore have **no resolution path at all** | widen `ResolveGroundBurst`'s trigger: a store with `FuzeRadiusM > 0` also resolves against `Ground` units on closest approach, with the identical CPA machinery. **§5.4's refusal is untouched** — that one forbids a ground burst against *aircraft* for want of a fragment-against-airframe geometry, and this is its mirror image, not its exception |
| **The release interlocks ask the FIRE CONTROL; an anti-radiation round's precondition is a SEEKER** | §2.4 checks 5–7 read the cached `FBFireControlBlock`. That weapon needs none of the three and needs one thing nothing here has: a **pre-launch seeker state on a store still on the rail** — the identical defect [`modules/ground/module.md`](modules/ground/module.md) B1 measured on MANPADS | `RequiresLock = false`, and the angular cue travels on `FBStoreRelease` (three fields) from the shooter's OWN receiver — the same construction a guided round's `Target` uses. The general fix stays open and is now a defect of two weapon families |

Also booked there: the two constructors in `modules/ground/` and `modules/stores/` that leave their
radar slot powered, so **a bunker and a falling Mk-82 each radiate an `AirborneFireControl` beam**
([`air-to-ground.md`](air-to-ground.md) §6, with the two missions it breaks predicted in advance).

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| No AIM-120 lofting | midcourse flies flat, range stays below what the round could do; every measured `Raero` is that of a flat-flying round |
| **The Mk-82 model carries no documented aerodynamics** — its own `<note>` calls itself a possibly crude approximation whose only similarity to the real object is the name | the CCIP/CCRP accuracy (22 m total, 10.6 m lateral) is a statement about fidelity to the MODEL, not about a real release. The error-budget split stays valid (our guidance against our own table); the absolute number must not be quoted as a fidelity result. Sourcing or building a model with documented aerodynamics is open — see `../aircraft/stores.md`. |
| **No strafing.** The cause is *not* the zero area of ground targets but `FBGunProjectiles` giving up after 3 s / 3000 m — the rounds never reach the ground | air-to-ground with the gun is impossible |
| Ground burst is not resolved against aircraft | deliberate: the fragment geometry against an airframe does not exist; an invented radius would be a number posing as physics |
| No fragment directivity, no fuze failure, no round mass, no gun installation angle | — |

### Inventory (from the previous `Offene Punkte` section)

**Known gaps (named in the code, not hidden):**

- ~~**No IR seeker.**~~ **CLOSED (stage 2c).** What remains, and it is a different gap: the MiG-29 has no
  DISPENSERS, so it cannot answer an infrared shot at all.
- **No lofting of the AIM-120.** The guidance law is pure PN plus a 1 g gravity bias; a climbing
  midcourse (which gives a BVR shot real range) does not exist. Every measured `Raero` is therefore that
  of a flat-flying round.
- **Weapons are invisible in the renderer.** `render/stages/FBUnitsStage` and `FBSpritesStage` are NoOps
  (wired into the encode order but without content) — a missile, a store and a ground target exist
  completely in the simulation and not at all in the picture.
- ~~**WASM has neither a release nor a damage path.**~~ **CLOSED in the player-control round.** The whole
  chapter 5 apparatus moved out of `missions/FBMissionRunner.cpp` into **`missions/FBOrdnance`** and both
  clients drive the identical object: `Resolve` (fly what is already in the air and resolve what it
  reached) → `Launch` (this tick's `TakeRelease`/`TakeBurst` become units) → `SnapPoses` (the segment the
  next closest-approach is measured over). The runner keeps exactly one thing of its own, a telemetry CSV
  per released store, through the `OnStoreSpawned` hook — the browser has no file system. The move is
  behaviour-neutral: 12 missions across gun/missile/CCIP/CCRP/cluster/ARM/net/duel, every `events.log`
  and every `telemetry*.csv` byte-identical, exit codes unchanged.
- **But the browser's TICK is not the runner's, and weapons made that visible.** One bundle per `Run()`
  means one bundle per FRAME: 60/s in the browser against 10/s in the runner, so a held trigger reaches
  `kMaxBundles = 64` in about a second and the log fills with `gun BURST_DROPPED … live=64`. Rounds are
  conserved (the gun integrates its rate); BUNDLES are not, and a bundle is the unit of hit resolution.
  Measured, booked as [`clients/clients.md`](clients/clients.md) 5.5, deliberately not worked around.
- **No strafing.** Gun bursts are only resolved against `Aircraft`, and ground targets declare area/extent
  0. Fixing that would mean: tracking projectiles down to the ground (expressly not done today, via
  `kMaxAgeS`/`kMaxPathM`) AND setting a presented area for ground targets.
- **A ground burst against aircraft is deliberately missing** (§5.4) — a jet flying low over its own bomb
  stays unharmed.
- **No fragment directivity.** The isotropy assumption is the one geometric setting of the fragment
  model; a real warhead has a focused band whose position would depend on the angle to the missile axis.
- **No cross-section, no shielding, no fragment counting.** The airframe is an axial segment.
- **The proximity fuze always hits when the geometry is right.** There is no fuze failure, no dud rate and
  no fuzing logic beyond radius + arming time.
- **No ammunition weight** (§4.1) and no ammunition mix; a homogeneous round.
- **No gun installation angle** (`FBF16Gun`: bore 0°/0°, because `doc/modules/f16/` names none).
- **Station geometry collapses longitudinally**: all nine F-16 pylons share the CG station (FS −193 in),
  because `weapons.md` §4.5 marks the station data itself as T4. A loadout therefore produces no pitching
  moment; the LATERAL offsets are modelled.

**Numbers that are pure settings and carry every statement hanging on them** (each to be measured as
soon as a source turns up):

| Setting | Value | What hangs on it |
|---|---|---|
| `kCaseFraction` | 0.5 | EVERY warhead damage, linearly |
| `kFragSpeedMs` | 1800 | ditto, quadratically (dominates `v_closure` at ~850 m/s clearly) |
| M61A1 `RoundMassKg` | 0.100 | EVERY kinetic damage number, linearly |
| M61A1 `DragCoef` | 0.30 | time of flight, impact energy |
| Fragility classes F-16 | 6 values | every kill/degrade verdict |
| Fragility classes ground target | 4 values | ditto |
| Presented areas F-16 | 4.0 / 14.0 m² | expected hit count, linearly |
| AIM-120 `FuzeRadiusM` | 10 m | whether a shot is a hit |
| AIM-120 `SeekerRangeM`/`ActivationRangeM` | 14.8 / 18.5 km | terminal handover |
| AIM-120 seeker FOV/gimbal | ±10° / ±45° | whether a midcourse was "good enough" |
| Mk-82 `ArmingS` | 2.0 s | the dud threshold of the pull-up cue |

**Unresolved questions / contradictions:**

- **The CCIP/CCRP accuracy has no absolute significance.** The 22 m total error (of which 10.6 m lateral,
  §4.2) is measured against a bomb model that describes itself as a possibly gross approximation whose
  only similarity to the real object is the name. The error budget split stays valid (guidance against our
  own ballistic table); the absolute number is no fidelity evidence. Sourcing or building an Mk-82 model
  with documented aerodynamics is open.

- **Drum content 510 vs. 512** — the same guide names both (§3 specification table vs. §2.5 text).
  FlightBox takes 510 (the specification table wins) and notes the difference instead of averaging.
- **The shared currency J/m² for fragments and 20 mm impacts** is a declared modelling decision and
  expressly NOT a statement of physical equivalence (§6.3). Whether a separate threshold table per
  mechanism would be better is open — it would be uncalibratable today.
- **`kMinReportedHits` 0.1** is a reporting threshold, not a physical one: below it nothing is resolved at
  all, so the transition from "hit" to "near miss" is not quite continuous.
- **The anchor `IntBriefHdgDeg_` = 000** for a unit without a flight plan (expressly documented in
  `damage-amraam.fbm` as a "wart worth fixing in the pilot"): the intercept pilot anchors the heading its
  state carried at the FIRST decision tick — and that lies before the first FDM step, so 000 instead of
  the spawn bearing.
- **`FBReleaseSolution::StampS` delay** (§2.5): the stores command group is serviced BEFORE the fire
  control, so a round carries the solution of the previous sweep. Logged (`solAgeS`), but not fixed — the
  bus order would be the adjusting screw.
- **`gun MISS` and `stores MISS` report the closest approach**, but a burst retired after a hit produces
  no miss line at all — the statistic "how many bursts missed" therefore has to be formed from `gun BURST`
  minus `gun HIT`, not from `gun MISS` alone.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. The basic decision: a fired weapon IS a unit

There is no weapon special path. A store that leaves the station becomes a `units/FBSimUnit` — with the
same mechanics as a jet:

| Property | Jet | released store / missile | static ground target |
|---|---|---|---|
| own `FBFdm` instance | yes | yes (own pinned model) | **no** (no model) |
| `FBModule` from `FBModuleRegistry` | yes (`f16`) | yes (`mk82` / `aim120`) | yes (`target_soft`/`target_hard`) |
| own telemetry file | yes | yes (`telemetry_<callsign>.csv`) | yes |
| `FBFlightMonitor` (physics judge) | yes | yes | **no** (kind Ground) |
| `FBMissionMonitor` | if objectives declared | no | if objectives declared |
| `FBSystemHealth` register | yes | yes | yes |
| damage model applicable | yes | yes (empty layout → no damage) | yes |
| in `FBUnitRegistry` | yes | yes | yes |
| snapshot pose (`PublishPose`) | yes | yes | yes |

#### What `FBUnitKind` does — and what NOT

`FBUnitKind` (`sim/src/units/FBUnit.h`) distinguishes `Aircraft` / `Weapon` / `Ground`. The distinction
exists for exactly those things that belong to the OWNER of the simulation:

| Rule | Aircraft | Weapon | Ground |
|---|---|---|---|
| physical knockout ends the run (`FirstFlightKo`) | **yes** | no — it is the detonation | no (does not fly) |
| searched for by air-to-air sensors | yes | no | no |
| gun bursts resolved against it | yes | no | **no** (no strafing) |
| proximity fuze resolved against it | yes | no | no |
| ground burst resolved against it | **no** (see §5.4) | no | yes |
| in the roster for `objective kill …` | yes | **no** | yes |
| ticked by JSBSim | yes | yes | no (`Airframe` optional) |
| ground elevation for the FDM | real elevation | `FBFdm::kNoGroundElevM` = −100 000 m, **from the initial condition onwards** | — |

What the distinction does NOT do: it is not a second code path, not a behaviour flag and not an exception
in the tick. A missile is computed by the same loop, judged by the same judges and documented with the
same log lines as a jet; `UNIT_RESULT` calls it `IMPACT` instead of `CRASH`, and that is the whole
difference in the verdict.

The last point of the table is a deliberate decision with a justification: JSBSim's ground reactions
describe a RESTING object. The spring/damper values of the mk82 model (10,000 lbf/ft, 200,000 lbf/ft/s)
diverge within one step at a 150 m/s impact — no impact state would be left to report. A bomb does not
bounce, it detonates. The store therefore flies ballistically through the ground, and WHERE the impact was
is reconstructed sub-tick by the runner (§5.3).

**AMENDED 2026-07-29 — the rule was one call too late, and where the constant lives says so.**

| Before | Now |
|---|---|
| `constexpr double kWeaponNoGroundElevM` in an anonymous namespace in `units/FBSimUnit.cpp`, plus a second copy in `clients/FBTestMissileAirframe.cpp` | **one** `static constexpr double FBFdm::kNoGroundElevM` in `fdm/FBFdm.h`. Three callers must say the same number — the unit's per-tick `UpdateGroundAsl`, the store spawn, and the airframe harness — so it belongs to the class that hands the number to JSBSim |
| applied only per tick, i.e. AFTER `FBFdm::LoadUnguarded` had already run `RunIC()` | applied **inside** `LoadUnguarded`, before the IC, through the new `FBFdmSpawn::TerrainElevM` field |

`FGLGear` resolves its contacts **inside** `RunIC()`. A store spawned nose-up on a rail therefore had a
structure point metres below a ground it was never supposed to have, the contact spring answered with an
angular impulse, and the integrator carried that impulse out of step 1 — see the measured probe in
[`modules/ground/module.md`](modules/ground/module.md) §4. `TerrainElevM` is a **separate** field from
`GroundElevM` (which only *places* the spawn) and defaults to JSBSim's own datum, so every spawn that ever
ran in this tree keeps running its IC against exactly what it ran against before; only a released store
sets it.

---

### 2. `FBStoresSystem` — the SMS

`sim/src/weapons/FBStoresSystem.{h,cpp}`. The airframe-agnostic DEFAULT (interface + implementation in
one class, the `FBSystemSlots.h` pattern); a module supplies only its PYLON GEOMETRY
(`modules/f16/FBF16Sms`) and may override `Run()`.

#### 2.1 Contract

| Phase | Call | Condition |
|---|---|---|
| Setup | `DeclareStation(number, xIn, yIn, zIn)` | before `AttachFdm`, station number 1-based the way the jet counts, position in the STRUCTURAL FRAME of the model (inches) |
| Setup | `AttachFdm(FBFdm&)` | creates ONE JSBSim point mass `station<N>` per declared station (weight 0) |
| Mission data | `Load(station, FBStoreSpec)` | false on an unknown or occupied station |
| Operation | `SetMasterArm`, `SelectStation` | Master Arm is on `Sim` (SAFE) at power-up |
| Operation | `Release(nowS, outcome, reason)` | reachable **only through the command bus** |
| Owner | `TakeRelease(FBStoreRelease&)` | FIFO drain, false when empty |
| Tick | `Run(FBState&, dt)` | publishes the `FBStoresBlock`, caches the fire control answer |

#### 2.2 Carriage effect — physics of the engine, not a computation of this class

`PublishLoadout()` (only on load and on release, never per frame — nothing about a loadout changes
between those two events):

- **Mass**: one JSBSim point mass per station (`FBFdm::SetStorePointMassLbs`). Mass, centre of gravity
  AND inertia tensor therefore come from `FGMassBalance`, not from a computation of our own.
- **Drag**: the SUM of the `DragAreaFt2` as the JSBSim `<external_reactions>` force `fb-stores`
  (`FBFdm::SetStoresDrag`), `CdA·qbar` opposing the body x axis, acting at the **weight-weighted centroid
  of the occupied stations**. That is not a detail: an asymmetric loadout thereby produces its yaw/roll
  moment from JSBSim's own force model and not from a term invented here.
- **On release** the point mass goes to 0 in the SAME tick — the aircraft immediately becomes lighter,
  better trimmed and cleaner, as physics.

Measured (`missions/mk82-carriage-loaded.fbm` vs. `mk82-carriage-clean.fbm`, identical apart from four
`set store` lines, both level at full thrust):

| Quantity | 4× Mk-82 | clean |
|---|---|---|
| Initial mass | +2,000 lb | — |
| Time to Mach 1.0 | 25.8 s | 22.6 s |
| Time to Mach 1.2 | 51.5 s | 41.1 s |
| Peak Mach | 1.364 | 1.416 |

The log line `sms RELEASE` deliberately names `gwLbs` as the weight **before** it takes effect:
`FGMassBalance` sums the point masses in its own `Run`, so the new weight exists one step later and is
read from the telemetry column `sms_gw_lbs`. A "before" that the next step contradicts would be a
measurement of nothing.

#### 2.3 The class SPAWNS nothing — and that is the anti-cheat structure

`Release()` creates an `FBStoreRelease` record (`core/FBStore.h`) in a queue of fixed capacity
(`kMaxPendingReleases == kMaxStations` — a full ripple of all stations fits in, so the queue can never be
the reason a store is lost). The OWNER of the simulation drains it.

The reason is structural: creating an `FBFdm` requires `fdm/FBFdmBoot.h`, and this header may not be
included by any file under `systems/` or `modules/`. A module that could put a unit into the world itself
would have a world write path — exactly what CLAUDE.md's "no cheating" excludes. The release path
therefore ends in a VALUE, not in a new airframe.

The runner (`missions/FBMissionRunner.cpp`) drains the queues **at the end of the tick**, in actor order, FIFO
per actor; the new store is computed only in the NEXT tick. That is the determinism condition: the step
phase distributes actor indices over threads, and an actor appearing in the middle of the phase would make
the result depend on the order.

#### 2.4 The rejection reasons

`Release()` checks in exactly this order (`doc/modules/f16/controls-commands.md` §6.2/§6.5):

| # | Condition | Outcome | Reason | Detail |
|---|---|---|---|---|
| 1 | Master Arm ≠ ARM | Rejected | `hardware_precedence` | "master arm not in ARM" — the pilot's safety |
| 2 | Weight on wheels | Rejected | `hardware_precedence` | "weight on wheels" — the airframe's safety |
| 3 | no occupied station selected | Rejected | `out_of_context` | valid command, wrong context |
| 4 | release queue full | Rejected | `channel_busy` | "release queue not drained" |
| 5 | `RequiresLock` and no lock/target | Rejected | `out_of_context` | "no fire-control lock" |
| 6 | `RequiresLock` and no DLZ | Rejected | `out_of_context` | "no launch-zone solution" |
| 7 | `RequiresLock` and outside the DLZ | Rejected | `out_of_context` | "target beyond Raero" / "target inside Rmin", additionally `sms LAUNCH_OUT_OF_ZONE` |
| — | otherwise | Accepted | `none` | + station step to the next occupied station |

The two hardware interlocks come FIRST, because a physical switch locks the software path out and no
software state may talk its way past it. The weapon-specific checks (5–7) come AFTER them and BEFORE
leaving the rail; they are the answer of the FIRE CONTROL (cached in `Run()` from the
`FBFireControlBlock`), not the opinion of the SMS — `Release()` is called by the command bus between two
ticks and must not reach for the bus itself.

#### 2.5 What a round is given at launch

Two handovers, one principle: **the prediction must leave the jet WITH the weapon.**

| Round | Field in `FBStoreRelease` | Content | Source |
|---|---|---|---|
| guided (`Guided`) | `LauncherId` + `Target` (`FBWeaponTargetState`) | where the shooter's fire control last saw the target, and whose uplink to listen to | `SetTargetState()` per tick from the module (`FBF16FireControl`) |
| unguided | `Solution` (`FBReleaseSolution`) | predicted impact point, computation plane, time of flight, aim point, miss distance, arming margin, timestamp | `SetReleaseSolution()` per tick from the module |

Why: the error between what the computer said and what was flown is a REAL property of every release (the
FCC carries a coarse stored table, the weapon flies its full aero model). If the prediction stayed in the
jet, the owner of the simulation could not lay it beside the measured impact. That is why it travels along
on the round and is set against reality on impact as `stores DELIVERY` (§5.3).

`FBReleaseSolution::StampS` is part of the honesty: the stores command group is serviced BEFORE the fire
control tick in the `FBF16Module` rate plan, so a round's solution is necessarily that of the previous
sweep. At fighter speed this delay is worth tens of metres — it is logged (`solAgeS`), not hidden.

#### 2.6 The guidance link (uplink)

`Uplink()` delivers what this aircraft RADIATES to a round it launched — published in the unit's
`FBUnitSignature`, like XMT and the IFF transponder:

```
Uplink_.Active = GuidedInFlight_ > 0 && Target_.Valid
```

Active therefore only as long as (a) a lock-requiring round has been launched AND (b) the fire control
still has a target. If the shooter loses the lock, the transmission stops mid-flight — that is the whole
tactical point of the midcourse phase. It expressly does NOT stop when the round's flight ends: nobody
reports that to the shooter, and in the real jet nobody does either — the transmitter falls silent when
the pilot no longer supports the shot.

#### 2.7 Telemetry & events

`sms_arm`, `sms_station`, `sms_loaded`, `sms_lbs`, `sms_released`, `sms_gw_lbs` (the last column is the
gross weight of the AIRFRAME in the same row as the SMS's own books — only that way is the carriage effect
measurable instead of claimable).

Events: `sms RELEASE`, `sms RELEASE_REJECTED`, `sms LAUNCH_OUT_OF_ZONE`, `sms LAUNCH_SOLUTION` (guided:
the complete DLZ at the moment of launch), `sms RELEASE_SOLUTION` (unguided: the release solution).

---

### 3. `FBGunSystem` — the gun

`sim/src/weapons/FBGunSystem.{h,cpp}`. The SMS's sibling slot. The same where the structure is the same;
different where the weapons differ.

#### 3.1 How it differs from the SMS

| | SMS | Gun |
|---|---|---|
| Product | ONE store per trigger event | a STREAM of projectiles |
| Boundary object | `FBStoreRelease` → becomes a UNIT | `FBGunBurst` → becomes a BUNDLE in the client's pool |
| Queue capacity | one per station | `kMaxPendingBursts` = 4 (the owner drains every tick) |
| Command value | 1.0 (pickle) | **duration** of the trigger squeeze in seconds |
| Bus latency | HOTAS 0.5 s | 0.1 s (`FBCommandBus::kTriggerLatencyS`) |
| Trigger rate | one action per store | one action, N ticks of fire |

The latency exception is justified: the 0.5 s of the other HOTAS commands is a key duration; the delay
between finger press and first round is the barrel spin-up, and that is already in the weapon model
(`SpoolUpS` 0.3 s). Counting both would be double counting. The SPACING of two presses stays
`kHotasLatencyS`.

Why a bundle is not a unit (`core/FBGunProjectiles.h`): 6,000 rounds/min against a 0.1 s tick is ten
rounds per tick and aircraft; a sustained fight would produce thousands, each with a JSBSim instance, a
telemetry file and a monitor. What the projectiles physically ARE does not justify that: unguided lumps
without systems and without decisions, of which only one question is asked — where are they and what did
they hit. So they live as arithmetic.

#### 3.2 The integrated rate — and why it MUST be rate-independent

`RoundsBetween(a, b, ratePerS, spool)` is the integral of the rate of fire between two moments `a`,`b`
after the trigger press, with a linear spin-up over `spool`:

```
I(x) = 0                              x <= 0
I(x) = rate · x²/(2·spool)            0 < x < spool     (ramp)
I(x) = rate · (spool/2 + (x−spool))   x >= spool        (full)
n    = I(b) − I(a)
```

`Run()` adds `n` onto `Fraction_`, takes `floor` off as whole rounds and carries the remainder. With that
the drum is empty in exactly `Capacity/rate` seconds, no matter with which `dt` the slot is ticked — and a
0.1 s tick at 6,000 rd/min is ten rounds, not "a burst".

Precisely for that reason the gun in the F-16 module is **not throttled** but entered once per `Run()`
with the FULL `dt` (`FBF16Module.cpp`): a different entry rate would invent or swallow rounds. A gun that
is not firing costs one comparison.

#### 3.3 Interlocks

| Condition | Outcome | Reason |
|---|---|---|
| no gun installed | Rejected | `not_implemented` |
| Master Arm ≠ ARM | Rejected | `hardware_precedence` |
| weight on wheels | Rejected | `hardware_precedence` |
| drum empty | Rejected | `depleted` — the one rejection that is a fact about the AIRCRAFT instead of about the request |
| `seconds <= 0` | Rejected | `out_of_range` |
| `seconds > MaxBurstS` | **Clamped** | `value_clamped` (reported, not silently shortened) |
| gun shot up | Rejected | `system_failed` (from the module router, §8.3) |

A second press during a running burst EXTENDS it to the later end; the spin-up is not restarted, because
the barrels never stopped.

#### 3.4 What a bundle carries

On reaching a whole round, `Run()` creates an `FBGunBurst`:

- **Location**: the MUZZLE, not the CG. `FBBodyVecToEnu(roll,pitch,yaw, fwd,right,down)` applied to the
  installation offset, then added geodetically. At gun range this offset is the difference between a hit
  and a miss.
- **Velocity**: the jet's own velocity PLUS the muzzle velocity along the bore direction
  (`FBBodyLosToEnu` on `BoreRightDeg`/`−BoreDownDeg`). Both halves are physics and this class knows both —
  what crosses the boundary is therefore a plain projectile state.
- **Count** and `LauncherId`, `Kind`, `SimTimeS`.

#### 3.5 It NEVER scores a hit of its own

The same boundary as with the SMS: the owner of the simulation drains the queue, flies the rounds
(`core/FBGunProjectiles`) and decides on the PUBLISHED poses what they hit. A gun that scores itself
would be the same cheat as a missile that scores itself.

#### 3.6 F-16 installation

`modules/f16/FBF16Gun.h`: M61A1, muzzle in the left fuselage-root strake.

| Axis | Value | Origin |
|---|---|---|
| forward | +4.6 m | from the pinned `f16.xml`: CG at FS −193 in, port ~180 in ahead of it [SET within the model geometry] |
| right | −0.9 m | port installation, half the fuselage width |
| down | −0.3 m | upper edge of the strake fillet |
| bore | 0°/0° | doc/modules/f16/ names NO installation angle — zero is the honest choice and stands as such in the header |

#### 3.7 Telemetry & events

`blk_gun` (first — the gun block was added long after `FBStateBusTelemetry`'s list, and a name there would
have shifted every column to the right of it), `gun_rounds`, `gun_fired`, `gun_firing`, `gun_triggers`,
`gun_refused`, `gun_burst`, plus the aiming solution as READ: `gun_sol_rng`, `gun_sol_err`,
`gun_sol_span`, `gun_in_funnel` (a read of foreign blocks, which every system is allowed; they sit on the
gun source because "where did the gun point" and "what came out" are ONE measurement).

Events: `gun TRIGGER`, `gun BURST`, `gun BURST_DROPPED`, `gun DRY`, `gun HIT`, `gun MISS`.

---

### 4. Ballistics

#### 4.1 `FBGunBallistics` — the SHARED arithmetic

`sim/src/core/FBGunBallistics.{h,cpp}`. Pure functions on values, no state, no allocation. Three consumers
use literally the same code:

| Consumer | When | Question |
|---|---|---|
| `modules/f16/FBF16FireControl` | BEFORE the shot | where must the barrel point (EEGS solution) |
| `core/FBGunProjectiles` | AFTER the shot | where are the projectiles |
| `clients/FBTestGun` (`make -C sim test-gun`) | verification | do both agree with `doc/modules/f16/weapons.md` |

That fire control and the flown trajectory share identical code would normally be a cheat (which is why
`FBWeaponPerf` is a deliberately COARSER separate copy of the missile aerodynamics). Here it is not, and
the difference is the point: an AMRAAM flies as a guided airframe with its own autopilot and its own
energy management — no table predicts that exactly. A 20 mm projectile is an unguided lump on a ballistic
arc, and that is exactly what the FCC solves. Modelling a deviation between the two would mean INVENTING
an error instead of measuring one.

**The trajectory model** — point mass under gravity and quadratic drag, `dv/dt = −k·v²` along the
velocity, with

```
k = 0.5 · rho · Cd · A / m         [1/m]     (rho = ISA density at firing altitude)
A = pi/4 · RoundDiaM²
```

The one named SIMPLIFICATION: drag acts on the MAGNITUDE, gravity on the vertical — not on the vector sum.
Over the whole usable lifetime of a round (under 2 s, under 2 km) the drop is metres against a path of
kilometres; the angle between the two is small, the decoupling costs centimetres. What it buys is a
CLOSED FORM:

```
v(t) = v0 / (1 + k·v0·t)
s(t) = ln(1 + k·v0·t) / k
t(s) = (exp(k·s) − 1) / (k·v0)          — the exact, iteration-free inverse
```

The inverse is the reason why the lead solution below converges in a fixed six passes without a search and
without allocation. `t(s)` returns −1 if `k·s > 20` — a guard against nonsense inputs, so that no infinity
propagates into a pose.

**The lead solution** (`FBGunSolveLead`) — the fixed point of an equation: the path length `s(t)` must
equal the distance to the place where the target will be at time `t`:

```
D(t) = rel + v_target·t + up·(0.5·g·t²)
```

(current offset, target motion during the time of flight, and the drop that has to be aimed OVER).
Direction → `t` exactly from `FBGunTimeToPath`; `t` → direction from `D(t)`. Six passes settle this well
below one metre, at every range at which the gun is used.

**The own-velocity correction** is the step a naive lead computation gets wrong: the round leaves the
barrel with the OWN VELOCITY plus the muzzle velocity, so the round's direction of flight is NOT the bore
direction. Solvable in closed form — decompose the own velocity into the components along and across the
desired direction of flight; the barrel has to lead across by exactly as much as makes the muzzle velocity
cancel the across component:

```
v_along  = v_own · flightdir
v_across = v_own − v_along·flightdir
mu       = sqrt(v_muzzle² − |v_across|²)
bore     = (mu·flightdir − v_across) / v_muzzle          (then normalised)
v0       = v_along + mu
```

This lead angle is the physical origin of the EEGS funnel shape: that is why the projectiles of a hard
turning fighter go where its nose is not. `Valid=false` means: there is no solution — the target runs away
faster than the round closes, or the jet crosses faster than the muzzle velocity can compensate
(impossible at 1,030 m/s against an aircraft, but checked instead of assumed).

Output `FBGunAim`: `TofS`, `RangeM`, bore unit vector (ENU), `SpreadM = DispersionSigmaRad · dist`,
`ImpactSpeedMs` = remaining velocity minus the target's along-component (i.e. the velocity the TARGET
sees — a head-on shot arrives harder than a tail chase shot, without that having to be stated anywhere
extra; clamped to ≥0).

**The hit density/energy model.** No randomness, nowhere. The rounds are a circular normal pattern of
width `sigma` about the bundle axis, the TARGET is a disc of the area it presents, and the expected hits
are the overlap of the two. Writing the target disc as its own equivalent normal (`sigma_t² = A/(2π)` —
the width whose central density equals that of a disc of area A) makes the overlap closed-form:

```
hits = N · A/(A + 2π·sigma²) · exp( −d² / (2·(sigma² + A/(2π))) )
```

with `d` = miss distance to the target centre. Energy per round `E = ½·m·v_rel²`; the flux density is
`hits·E` spread over the SMALLER of the two areas (pattern `2π·sigma²` or target area):

```
FBGunFluxJm2 = hits · 0.5·m·v_rel² / min(2π·sigma², A)        [J/m²]
```

Every limiting case falls out of the same formula:

| Case | Result | Consequence |
|---|---|---|
| Pattern ≫ target | the target catches `N·A/(2π·sigma²)` | flux ∝ 1/range² (sigma grows linearly) — **that** is why a gun is a short-range weapon, derived instead of decreed by a range limit |
| Pattern ≪ target | every round hits, over `2π·sigma²` | flux saturates at what a point-blank burst does |
| Bundle a few metres off | the EXTENT of the target catches a part | a point-target model gets exactly that wrong — an aircraft is metres wide |

**Two scales** (`FBGunExpectedHits`), because a fighter has two: `targetAreaM2` = how much MATERIAL is
presented, `extentM` = how far this material REACHES from the centre. A single disc cannot do both: right
for a burst on the fuselage, "nothing at all" for one four metres beside it, where a real F-16 still has
wings. So the LARGER of two readings of the same pattern:

- **COMPACT**: the material as a disc of area A (exact for a burst on the centre — where a lethal burst
  lies);
- **EXTENT**: the same material spread thinly over the silhouette disc of radius `extentM`, so a round
  within it hits with probability `A/(π·extent²)`. Only evaluated if the silhouette is larger than A;
  `extentM = 0` switches the second reading off (the target counts as compact).

Clamped to `hits <= rounds`: a bundle cannot land more rounds than it holds.

**M61A1 numbers** (`core/FBGun.h`, derivations there in the original):

| Quantity | Value | Mark/source |
|---|---|---|
| Muzzle velocity | 1,030 m/s | [T4] 3,380 ft/s, source-consistent, no T1/T2 |
| Rate of fire | 6,000 rd/min | ED number |
| Drum | 510 | ED §3 (§2.5 of the same guide says 512 — the specification table wins, the difference is noted instead of averaged) |
| Spin-up | 0.3 s | [T4] — modelled, because omitting it made the larger error (~15 rounds per press) |
| Projectile mass | 0.100 kg | **[SET]** — §4.1 expressly refuses this number; EVERY kinetic damage number is linear in it, hence named once here |
| Calibre | 0.020 m | 20×102 mm |
| Cd | 0.30 | **[SET]**, checkable: yields ~1.3 s time of flight at 1,000 m (`make test-gun`) |
| Dispersion σ | 2.2295e−3 rad | **[DERIVED]** from MIL-DTL-45500/1A: "80 % of a 75-round burst within 8.0 in at 1,000 in" = 80 % within a 4 mil radius. For a circular normal pattern `P(r<R) = 1 − exp(−R²/2s²)`, hence `s = 4 mil / sqrt(2·ln5) = 2.2295 mil`. **Cross-check with the second, UNUSED number of the same source**: predicts 97.3 % inside the 12 mil circle, which the source calls "100 %". A uniform disc would have only 44 % inside the 8 mil circle — excluded by the source, not by taste |
| max. burst | 1.0 s | [SET] = 100 rounds; a trigger command is ONE action and needs a duration |

Expressly NOT modelled: barrel wear, round-to-round muzzle velocity dispersion, ammunition mix
(tracer/HEI/API — the drum is homogeneous) and the MASS of the ammunition (510 rounds ≈ 110 lb; the
empty-weight breakdown of the vanilla `f16.xml` cannot be decomposed, principle 1, so a drum point mass
would be as likely to double-count as to correct — under 0.5 % of the gross weight, named instead of
hidden).

#### 4.2 `FBBallistics` — the shared primitive of both air-to-ground procedures

`sim/src/core/FBBallistics.{h,cpp}`. ONE forward integration, two questions:

| Mode | Question | Function |
|---|---|---|
| CCIP | "if I release NOW, where does it hit?" | `FBSolveImpactPoint` |
| CCRP | "given this point — WHEN must I release?" | the same prediction, projected onto the current ground track (`FBSolveAim`) |

With that, pipper and release countdown cannot diverge.

**What is integrated** — and why it deliberately is NOT the aerodynamics the bomb then flies:

```
a = −g·u_up − (0.5·rho(h)·v²·Cd·S/m) · v_hat
```

Only four fields are read from `FBWeaponPerf`: `LaunchMassKg`, `DragCoefA`, `RefAreaM2`, `ArmingS`. A store
without a motor and a seeker has no more. The density is ISA at the CURRENT altitude of the falling round
and is re-evaluated every step — a bomb from 4 km falls through a third of the atmosphere, and a single
density would be wrong here by more than the measured effect.

Not modelled (omissions of the COMPUTER, not of the simulation): lift (the round is a point mass without
an angle of attack), wind (there is none), Coriolis force, Mach dependence of Cd.

**Why the difference is a real property of every release:** on leaving the pylon the round becomes its own
JSBSim instance with the full aero of the pinned model — Mach-dependent drag, lift at the trim alpha,
pitch damping. A real fire control computer has none of that; it carries a stored table and integrates a
point mass. That is exactly what this file does. The error between prediction and flown result is a real
property of EVERY delivery ever flown — feeding the computation with the weapon's aerodynamics would hide
it. The CCIP/CCRP missions measure exactly this error.

**Numerics:** Heun (predictor + corrector on the same acceleration law), `kStepS = 0.05 s`,
`kMaxTofS = 120 s` (leak guard). Justification in the header: an Mk-82 from 4 km at 450 kt falls ~30 s,
hence 600 steps — cheap enough for the 10 Hz fire control slot and fine enough that the step error lies
far below the MODEL ERROR that the whole prediction is meant to expose (measured: halving it shifts the
impact point by well under a metre). Plain Euler would be systematically off by metres with quadratic
drag — the same order of magnitude as the measured effect, so not acceptable for free. The crossing of
the plane is interpolated linearly WITHIN the crossing step (quantisation to 0.05 s would be ~15 m of
throw range at release speed).

**The impact plane is PASSED IN, never looked up** — this file knows no terrain. The F-16 caller hands in
the steerpoint elevation, hence the same `FBElevationProvider` value that the radar altimeter reads and
against which the monitor judges the impact. A flat plane at that altitude is exactly what a jet with a
barometric/steerpoint ranging solution has (provider letter `B`).

**Outputs:**

`FBImpactPrediction`: `LatDeg`/`LonDeg` (double, because 1e−5° ≈ 1 m IS the measured quantity), `ElevM`,
`TofS`, `RangeM`, `BearingDeg`, `ImpactSpeedMs` (the closure with which a ground burst is resolved),
`ArmMarginS = TofS − ArmingS` — the pull-up anticipation cue as what the computation really delivers: how
much fall remains after the arming time. Negative = the release arrives unarmed (the dud case of the
source). The real jet draws that as a screen position towards the FPM; a margin in seconds is the same
fact in the form in which the decision is made, and needs no second integration.

`FBAimSolution`: both points (predicted impact and designated aim point) projected onto the CURRENT ground
track (`FBTrackProjectM`) — the axis on which a release cue lives: along-track one moves the round by
WAITING, across-track by TURNING. One projection, both modes: CCIP reads `MissM`, CCRP reads
`AlongErrM`/`TimeToGoS`. Invalid without a ground track (>1 m/s) — without a direction of motion there is
no release point one could be short of; "no answer" is not "release now".

**Measured** (`missions/attack-ccrp.fbm` / `attack-ccip.fbm`, `--elev const`, 19 km run-in, 900 m,
450 KCAS, throw range 2,880 m):

| Quantity | CCRP | CCIP | 2 s late (`attack-late.fbm`) |
|---|---|---|---|
| `predErrM` (computer against model) | 57.1 m | 57.1 m | 57.1 m |
| `aimErrM` (bomb against target) | 22.2 m | 22.2 m | 481.5 m |
| of which long / lateral | 19.5 / 10.6 m | 19.5 / 10.6 m | 481.5 / 3.5 m |
| `tofErrS` | −0.097 s | −0.097 s | −0.097 s |
| Verdict | SUCCESS (0) | SUCCESS (0) | TIMEOUT (3), target standing |

The computer error is a systematic lead error towards SHORT: the real bomb flies further than the table
says, because the table carries ONE Cd for all Mach numbers and does not know the lift of the
weathercocking round at all. The delivery error is SMALLER than the computer error, because its
along-track component runs counter to the release moment. Error budget:

| Item | Amount | Belongs to |
|---|---|---|
| Computer (table against the model aero) | 57.1 m short | `core/FBBallistics` — a declared omission |
| Release moment (cue + lead) | 19.5 m long (net) | pilot/fire control |
| Cross-track (tracking error of the guidance) | 10.6 m | `systems/FBAutopilot` |

**Against what these numbers are measured.** The SPLIT above stays valid — it measures FlightBox's
guidance and fire control against FlightBox's own ballistic table, and both sides are ours. The ABSOLUTE
number (22 m, of which 10.6 m lateral) by contrast says nothing about a real release: the reference is the
aerodynamics of the Mk-82 model, whose own `<fileheader>` `<note>` concedes it could be "a gross
approximation, with the only similarity to an actual object being the name" and is "for educational and
entertainment purposes only". Principle 5 therefore applies especially strictly here — the number is
fidelity to the MODEL, not fidelity to reality, and must not be quoted as fidelity evidence.

#### 4.3 `FBGunProjectiles` — the pool

`sim/src/core/FBGunProjectiles.{h,cpp}`. Fixed capacity `kMaxBundles = 64` (enough for four continuously
firing aircraft over the full lifetime of a bundle, with reserve — **at the runner's 10 Hz tick; a client
that ticks faster makes proportionally more bundles and overruns it, measured in the browser at 60 fps**).
Owned by the CLIENT through `missions/FBOrdnance`, ticked by it, read by it. The structural sibling of `core/FBDamageModel`:

- no module can reach it or construct one — no aircraft flies its own projectiles and none decides what
  they did;
- nothing in it is random, time-dependent or hidden: the same bundle from the same geometry flies the same
  trajectory — that makes a gun engagement reproducible across thread counts;
- **it allocates nothing.** A bundle that cannot be taken up is COUNTED (`DroppedCount`) instead of
  silently lost — a pool quietly eating a burst would break the drum arithmetic.

`Bundle` carries the PREVIOUS and CURRENT position, because a hit is a closest-approach computation over
the SEGMENT of the tick (~100 m of path per 0.1 s — a distance test per tick would miss almost
everything).

`Step(dt)`: drag on the magnitude (closed form from §4.1), gravity on the vertical, position update
trapezoidal on the mean of the two velocities (second order in dt — necessary at 0.1 s and ~1,000 m/s).
`PathM` grows along (the lever arm of the dispersion pattern), `AgeS` likewise.

Lifetime: `kMaxAgeS = 3.0 s` OR `kMaxPathM = 3000 m`, whichever comes first — both far beyond the ranges
at which the gun is used (the funnel itself ends at 3,000 ft per `weapons.md` §2.5). **Deliberately not
modelled:** projectiles are NOT tracked down to the ground, there is no ballistic impact on terrain. The
pool is for the air-to-air gun; claiming a strafing footprint that nothing here computes would be worse
than the named absence.

`Retire(index)` is the caller's verdict: this bundle has been resolved against a target and is spent. A
bundle hits ONCE — the rounds it stood for went into the target.

---

### 5. The three resolution boundaries

All three run through the OWNER of the simulation (`missions/FBMissionRunner.cpp`), never through a module, and
all three measure on the PUBLISHED poses — hence on the truth, like `FBFlightMonitor`'s ground contact.
The reason is always the same: the weapon's seeker says where it SUSPECTS the target; letting the weapon
score itself on its own estimate would be the purest form of cheating.

#### 5.0 The shared primitive: `ClosestApproach`

Why not a distance check: the tick is 0.1 s, a head-on closure can exceed 1,500 m/s — successive samples
lie 150 m apart, and a distance test against a 10 m fuze would miss almost every real hit. So the minimum
over the SEGMENT between the relative position of the last tick and that of this tick, the standard CPA
formula on `p(t) = p0 + t·(p1−p0)`, `t∈[0,1]`:

```
t*      = −(p0·d) / (d·d),  d = p1 − p0,  clamped to [0,1]
MissM   = |p0 + t*·d|
Closure = |d| / dt
FracT   = t*                    (the SUB-TICK time of the event — logged)
RelE/N/U= the vector itself     (the damage resolution needs the direction)
```

The straight-line assumption within one tick is worth about a metre of curvature at 20 g — named in the
header, not hidden.

#### 5.1 Proximity fuze beside a jet

Conditions (all in the runner, in this order):

1. the store HAS a proximity fuze (`FuzeRadiusM > 0`; a bomb has none),
2. `simT − SpawnS >= Perf.ArmingS` — **the arming delay is what keeps a launch from detonating on its own
   carrier**: a round leaving the rail 3 m beside the jet is therefore not a hit on it,
3. the target is an `Aircraft`, active, not the round itself,
4. `MissM <= FuzeRadiusM`.

Then: `stores DETONATION` (target, miss distance, fuze radius, closure, sub-tick time of flight, aspect,
altitudes and speeds of both) → `ResolveBurst` → `store.Retire()`.

`ResolveBurst` rotates the CPA vector with `FBEnuToBodyVec` into the BODY FRAME of the target (from its
published attitude — the same snapshot against which everything else was measured in this tick), sets
`ClosureMs` and `WarheadKg` from the catalogue and calls `FBSimUnit::TakeBurst`. The weapon supplies ONE
number (its explosive mass), the TARGET's module supplies ONE table (where its systems sit), and neither
of them decides anything.

The closest approach to an aircraft other than one's own shooter is carried along as `MinMissM` and
reported at the end of the round's flight as `stores MISS`. The shooter is exempt from the REPORT, not
from the fuze: a round separating from a pylon passes its own carrier by tens of metres — a fact about
geometry, not about accuracy.

#### 5.2 Gun stream

The order in the tick is fixed: bundles fly (`Bullets.Step(dt)`) → resolve against every aircraft passed →
ONLY THEN take up the bundles fired in this tick. A bundle is therefore never resolved in the tick in
which it came into being — the same snapshot discipline as with the actor growth.

Per bundle × per aircraft (not the shooter itself):

| Step | Value |
|---|---|
| dispersion at this point | `sigmaM = DispersionSigmaRad · PathM`, lower bound 0.05 m |
| pre-check | `MissM > 3·sigma + kGunHitReachM (8 m)` → skip (beyond the airframe's own reach; the density computation could only deliver a number there that no report should carry) |
| presented area | `FBPresentedAreaM2(layout, fwd,right,down)` in the target's body frame; `<= 0` → no target at all (a store, a unit without a declared airframe) |
| presented extent | `FBPresentedExtentM(…)`, same interpolation |
| *(not this path)* | `FBDamageLayout` also declares a **plan extent**, read ONLY by `sensors/FBVisualSystem` ([`sensors.md`](sensors.md) §9.4). The gun's two-view area law is untouched by it: the eye and the gun read one table because they look at one aeroplane, but they ask it different questions — an AREA proxy against the LARGEST DIMENSION of a silhouette |
| expected hits | `FBGunExpectedHits(...)`; `< kMinReportedHits (0.1)` → **near miss**, nothing is resolved |
| energy density | `FBGunFluxJm2(...)` |
| application | `FBSimUnit::TakeKineticBurst` → `FBDamageModel::ApplyKinetic` |
| afterwards | `Bullets.Retire(bi)` — the rounds went into it |

A bundle whose life ends without a hit produces `gun MISS` with the CLOSEST approach it ever reached (not
the first tick in which it came anywhere near) — exactly that number says whether the aiming or the timing
was wrong. Reported only below `kGunNearMissM` (200 m): far enough that "that one went past him" gets
measured, tight enough that a bundle in the same sky does not count.

#### 5.3 Ground impact of a store

The store has no ground contact forces (§1) and therefore keeps flying ballistically until the physics
judge reports penetration. With the 0.1 s tick it is then already up to one tick BELOW the surface —
measured on an Mk-82 arriving at 216 m/s: 14 m of depth, hence ~20 m of horizontal travel beyond the true
impact point. That is a fifth of the ENTIRE delivery error that this mission set measures, and it is an
artefact of the sampling rate, not something the aircraft or the computer did.

`GroundCrossing(store, backS)` reconstructs the crossing from the observed sample:

```
depth = GroundAslM − elev            (only if > 0 and sink rate < −0.1 m/s)
backS = depth / (−vy)
lat  −= (−vz)·backS / kMPerDeg
lon  −= vx·backS / (kMPerDeg·cos lat)
elev  = GroundAslM
```

Over ~0.1 s the curvature of the arc is worth centimetres — hence a straight line and not a second
integration. Deliberately NOT interpolated between the last two published POSES: by the time the judge
concludes, the previous pose is already below the surface too (it takes a few metres of penetration for
that to be a verdict and not a rounding error) — there is no bracketing pair.

Everything further uses THIS point: `stores IMPACT` (`crossLat`/`crossLon`/`crossBackS`/`crossTofS`),
`stores DELIVERY` (the prediction from §2.5 against measured reality: `predErrM`, `aimErrM`,
`aimLongM`/`aimAcrossM` in the ARRIVAL DIRECTION of the round — a bomb weathercocks into its velocity, so
its heading at impact is the approach heading —, `tofErrS`, `planeM` against `groundAslM`, `armMarginS`,
`solAgeS`), and the warhead.

`ResolveGroundBurst` against every active `Ground` target. The proximity gate is DERIVED, not a chosen
radius: the LOWEST threshold that this target's layout declares is the least energy that can do anything
to it at all — a burst whose flux at that range lies below it could only produce a zero-effect line and a
wrong entry in its hit count. `ClosureMs` here is the pure arrival velocity of the round (the target does
not move).

#### 5.4 Why a ground burst is NOT resolved against aircraft

A jet flying low over its own detonation is really inside a fragment envelope. Modelling that would need a
frag-against-airframe geometry (and an evasion manoeuvre against which to measure it) that nothing here
has; resolving a burst against everything within an INVENTED cutoff would be a number posing as physics.
So: a ground burst hurts GROUND units, and the boundary stands in plain text in the code instead of behind
a radius constant.

The same rule the other way round with `target_soft`/`target_hard`: their presented area/extent are ZERO,
so the gun's hit density model puts no round onto them — **no strafing**. That too is not a gap to be
filled later by estimate: strafing would need projectiles tracked down to the ground, which the gun pool
expressly does not do (§4.3); a presented area here would claim a capability that the projectile side does
not have.

---

### 6. `FBDamageModel` — what a hit DOES

`sim/src/core/FBDamageModel.{h,cpp}`. The ONE writer of `core/FBSystemHealth` (its only `friend`), owned
by the client. A module never resolves its own damage, any more than it judges its own crash.

**It is a MODEL and says so.** Observed and checkable is the INPUT: the burst geometry (the runner's
closest-approach computation on the published poses), the closure and the explosive mass from the store
catalogue. MODELLED is the step from these three numbers to a system state, and it is built from the two
things that really are physics — isotropic fragment spread and kinetic energy — plus a threshold per
system, which is a setting.

#### 6.1 The energy of a warhead, in three steps

| # | Step | Formula | Assumption |
|---|---|---|---|
| 1 | fragment mass | `m_frag = WarheadKg · kCaseFraction` | `kCaseFraction = 0.5` **[SET]** — the usual order of magnitude for a fragmentation case; `doc/modules/f16/weapons.md` §4.7 lists warhead internals as a genuine gap, hence a setting instead of a citation |
| 2 | areal density | `rho_A = m_frag / (4π·r²)` [kg/m²] | ISOTROPY — the one geometric assumption. A real warhead sprays into a focused band; that would make the result depend on the angle to the missile axis, and nothing here claims to know that band |
| 3 | specific energy | `flux = ½·rho_A·(v_frag² + v_closure²)` [J/m²] | `kFragSpeedMs = 1800` **[SET]**. For a radially symmetric cloud, the MEAN magnitude of the vector sum of ejection velocity (radial) and closure is `sqrt(v_eject² + v_closure²)` — deliberately NOT `v_eject + v_closure`, which would only hold for the fragments thrown straight ahead |

Range lower bound `r >= 0.5 m`: not a physical statement but a protection — the 1/r² law diverges at zero,
and a burst INSIDE the airframe is no more instructive than one at its skin. 0.5 m is about half the
fuselage width of a fighter, hence the closest thing that can lie outside.

The result is a **1/r² law in the energy**: twice the miss distance = a quarter of the arriving energy.
That — and not any single threshold — is what makes the model behave sensibly at ranges at which nobody
has calibrated it.

`FBFragmentFluxJm2(warheadKg, rangeM, closureMs)` is PUBLIC, so that a report, a harness or a log line can
reproduce the exact number behind a damage verdict instead of trusting it.

#### 6.2 Zones: 1/r² falloff instead of a partition

An aircraft is not a point. The layout (module data) cuts the airframe along its LONGITUDINAL AXIS into
zones and names which systems sit in which. `FBDamageModel::Apply` computes a separate distance PER ZONE:

```
fwd_clamped = clamp(burst.FwdM, zone.AftM, zone.FwdM)
r           = |(burst.FwdM − fwd_clamped, burst.RightM, burst.DownM)|
```

A burst abeam the middle of a zone is therefore as close as its lateral distance; one in front of the nose
has to reach back along the axis as well.

**Every zone is evaluated, not just the nearest.** Fragments go everywhere, they just arrive thinner
further away — letting the 1/r² law say that is more honest than partitioning the airframe and giving one
partition everything. The airframe is modelled as this axial segment and as nothing else: no
cross-section, no shielding, no fragment counting.

Per zone and system: `flux >= FailJm2` → `Failed`, otherwise `flux >= DegradeJm2` → `Degraded`. A system
without DERIVABLE degraded behaviour sets `Degrade == Fail` and therefore never has one.

`FBDamageResult` reports: the zone with the highest flux, its distance, the peak flux, the bitmasks
`NewlyFailed`/`NewlyDegraded` (what THIS burst changed) and `WasEffective`/`NowEffective`.

#### 6.3 `ApplyKinetic` — the second input

Deliberately a DIFFERENT input type instead of a flag on the same one, because the two weapon effects are
KNOWN through different things:

| | Warhead | Gun burst |
|---|---|---|
| known through | a MASS, from which the model derives the energy | an **AREAL ENERGY DENSITY** that the owner of the simulation has already computed from hit count, impact velocity and dispersion (`FBGunFluxJm2`) |
| geometry | isotropic fragment rain → every zone sees something | narrow pattern → only the zones the footprint touches |
| summation | **no** — a detonation is ONE event | **yes**, per zone (`FBSystemHealth::AddKinetic`) |

Why this file does not derive the energy of a burst itself: it never sees a round and would have no right
to.

What the two SHARE, and why that is legitimate: the TARGET. Both express what arrives as J/m² at a place
on the airframe, both are judged against the same thresholds, so ONE damage register answers for both
without a second, uncalibrated set of numbers. That is a declared modelling decision and not a physical
claim: 20 mm impacts and warhead fragments do not damage structure through the same mechanism; the shared
currency is this simulator's choice, not a statement of equivalence.

**Why kinetic energy is SUMMED per zone** (`FBSystemHealth::AddKinetic`, the only piece of damage state
that belongs to no system): a gun is a continuous stream that this simulator necessarily cuts into tick
bundles. Judging every bundle on its own would make the damage a function of the TICK RATE — exactly what
CLAUDE.md's principle 4 forbids. Fifty rounds as five bundles do the damage of fifty rounds. A warhead has
no equivalent and does not use it.

**Footprint**: `half = max(SpreadM, 0.5 m)`; only zones overlapping `[FwdM−half, FwdM+half]` see anything.
The floor of 0.5 m makes sure that a point-blank burst — whose pattern is centimetres wide — lands on the
zone it went through, instead of on a mathematical point between two of them. `res.RangeM = 0` — a hit,
not a stand-off burst; there is no range.

#### 6.4 `FBDamageLayout` — two scales

```
FrontalAreaM2  seen head-on/from astern        FrontalExtentM  half the wingspan
LateralAreaM2  seen from the side/above        LateralExtentM  half the length
```

Interpolation for a stream from direction `(fwd,right,down)` in the target's body frame:

```
along  = |fwd| / |v|
across = sqrt(1 − along²)
area    = Frontal·along + Lateral·across
extent  = FrontalExtent·along + LateralExtent·across
```

"The simplest interpolation that is exact at both ends, and no claim whatsoever about the shape in
between." Two numbers instead of one, because the difference is a factor of three on every fighter and the
interpolation is free. A module that declares neither (the default, and every released store) presents
NOTHING and takes no gun damage — which is correct: nobody shoots at a bomb in free fall.

#### 6.5 Determinism — and what follows from it

No random generator anywhere in this file, no time dependence, no internal state. Same geometry + same
warhead + same closure → the same masks, always.

Consequences: an engagement is bit-identically reproducible across thread counts (measured, see CLAUDE.md
"stage 4"); a regression run can use damage pictures as a fingerprint; and a debriefing can recompute
every number behind a verdict, because `FBFragmentFluxJm2` and `FBGunFluxJm2` are public.

#### 6.6 The physical consequence constants

All together in `FBDamageModel.h`, so that the whole "how damage feels" model is readable at once. Applied
exclusively through JSBSim (`FBSimUnit::ApplyDamageToAirframe` → `fdm/FBFdm`), never through a second
parallel flight model.

| Constant | Value | Derivation |
|---|---|---|
| `kAuthorityDegraded` | 0.5 | **[SET, but with a structural reason]**: the F-16 has TWO independent hydraulic systems at its actuators — losing one is the natural meaning of "degraded". Scales the commanded deflections INSIDE `FBFdm::SetControls`: the FLCS keeps commanding unchanged, the aircraft merely no longer answers |
| `kAuthorityFailed` | 0.0 | no authority; the controls no longer answer, the aircraft flies on trim and inherent stability — exactly the departure that JSBSim then integrates itself |
| `kThrottleLimitDegraded` | 0.6 | **[DERIVED]** from the `throttle-cmd-norm` convention of the F-16 model: that is where the afterburner gate sits. Degraded = no afterburner |
| (engine failed) | — | JSBSim's own cutoff, no thrust term invented here |
| `kDamageDragFt2Degraded` | 1.5 ft² | **[SET]**; scale: the zero-lift drag area of a clean F-16 is on the order of 4 ft² — degraded = "noticeably dirty" |
| `kDamageDragFt2Failed` | 6.0 ft² | **[SET]**; = "flying with a hole". Applied through the SAME `<external_reactions>` mechanism as the carriage drag (`FBFdm::SetDamageDrag`), through the CG — NO pitching moment is claimed that nobody could evidence |
| `kRadarRangeDegraded` | 0.70710678… | **[DERIVED]** from the radar equation: `R⁴ ~ Pt·G²` with `G ~ A`, hence `R ~ sqrt(A)`; half the aperture = `1/sqrt(2)` of the range |

---

### 7. `FBSystemHealth` — the register

`sim/src/core/FBSystemHealth.{h,cpp}`. ONE register per `FBSimUnit`, the structural sibling of
`FBFlightMonitor`/`FBMissionMonitor`: owned by the CLIENT, fed only by a core-owned verdict, READ — never
written — by the module that flies the aircraft.

#### 7.1 The write gate is the TYPE, not a convention

Every mutator (`Worsen`, `NoteHit`, `AddKinetic`) is **private**, and there is exactly one `friend`:
`FBDamageModel`. So nowhere does an API exist — not on a const handle, not on a non-const one — with which
a system, a pilot or a module could mark itself (or anybody else) as damaged or repaired.
`grep -rn FBSystemHealth src/systems src/modules` finds only reads, and it CAN find nothing else:
everything else does not compile.

#### 7.2 Monotone

A state never gets better. There is no repair in flight, and a monotone register is what makes the damage
picture of a run a function of the bursts taken and of nothing else — no question of order, no healing
race between two observers.

#### 7.3 The inventory

`FBSystemId` (append only — the ordinal is telemetry-visible in the `dmg_*` bitmasks):

| Ordinal | Id | means |
|---|---|---|
| 0 | `Engine` | propulsion: thrust — on a twin, the LEFT one |
| 1 | `FlightControls` | FLCS/hydraulics: control authority |
| 2 | `Structure` | airframe: drag |
| 3 | `AirData` | ADC + probes |
| 4 | `RadarAlt` | radar altimeter (CARA) |
| 5 | `Nav` | INS/navigation |
| 6 | `Radar` | active air-to-air set |
| 7 | `FireControl` | launch envelope computer |
| 8 | `Stores` | SMS: racks and wiring |
| 9 | `Datalink` | net terminal |
| 10 | `Rwr` | warning receiver |
| 11 | `Countermeasures` | dispenser |
| 12 | `Gun` | the gun: drum, feed, barrels (**appended**, per the enum's rule) |
| 13 | `Engine2` | the SECOND engine of a twin (**appended**, same rule) — see below |

**The twin-engine case, and why it needed a second question rather than a second threshold.** A
single-engine airframe never declares `Engine2`, so the bit can never be set on one and every `dmg_*`
mask ever measured keeps its meaning. What changes is that `CombatEffective` now asks
`PropulsionOut()` instead of `Failed(Engine)`:

```
PropulsionOut() = Failed(Engine) && (!HasEngine2 || Failed(Engine2))
```

`HasEngine2` is learned from the only place that knows what the aircraft has — the MODULE's own damage
layout, walked whole by `FBDamageModel::NoteLayout` before anything else is asked. Before the first
burst the two readings are identical, so there is no window in which they can differ. The physical
consequence is likewise not a threshold: ALL engines out is JSBSim's cutoff as before, ONE of two out
is `kThrottleLimitOneEngineOut = 0.5` — half the installed thrust, expressed as half the commanded
range on a deck whose engines share one `throttle-cmd-norm`, which also takes the afterburner with it.
`FBMig29Damage` puts both ids in the AFT zone (the two RD-33s sit side by side; this model has no
lateral resolution and does not pretend to one) with the mapping written down: `Engine` = left,
`Engine2` = right.

The list is deliberately the module SLOT set plus the three physical things whose consequence JSBSim can
carry itself.

#### 7.4 The three states for a consumer

| State | Behaviour |
|---|---|
| `Intact` | system runs, publishes its output block normally |
| `Degraded` | keeps running and publishing, with reduced performance WHERE one is derivable (radar range, engine ceiling, FLCS authority). Where not, a system has no degraded behaviour and its layout entry never produces one |
| `Failed` | the system does NOT run and does NOT publish. Its block goes `Invalid`, and everything else follows by itself (§8) |

#### 7.5 `CombatEffective` — a MISSION verdict

```
CombatEffective() = !Failed(Engine) && !Failed(FlightControls) && !Failed(Structure)
```

A declared modelling decision: a unit is combat-ineffective as soon as the AIRFRAME can no longer fly its
sortie to the end. Avionics losses are expressly NOT part of it — a jet with a dead radar and dead racks
is out of the fight but flying; and what this predicate feeds (`core/FBMissionMonitor`) judges the SORTIE,
not the engagement.

**The unit is not "dead" when that goes false.** No freeze, no marking, no case for the physics monitor: it
keeps flying exactly as long as the physics allows, and crashes because its engine is out and its controls
are gone. In the `UNIT_RESULT` line the MISSION verdict takes precedence over the later CRASH for a shot
down unit: the shoot-down explains the impact, the impact explains nothing. Reference run
`missions/damage-amraam.fbm` — the target deliberately declares NO objectives, hence carries no
`FBMissionMonitor` at all and cannot end the run; the mission is about the ~340 s aftermath and ends with
exit 2 (CRASH), caused by nothing but its own damage.

#### 7.6 Telemetry

Its own source `dmg`, registered LAST by the unit (the append rule): `dmg_hits`, `dmg_failed`,
`dmg_degraded` (bitmasks over `FBSystemId`), `dmg_effective`.

---

### 8. THE COUPLING — the core of it all

A failed system is **no longer ticked** by the module, and its block goes `Invalid`. Everything else
follows from the avionics bus, which has been able to do this all along. **Nothing was newly written for
it.**

#### 8.1 The gate in the module

`modules/f16/FBF16Module.cpp` — one comparison per slot, always to the same pattern:

```cpp
if (SystemWorking(FBSystemId::X)) X_->Run(...);
else SharedState.X.H.Invalidate();
```

| Slot | Gate | Addition |
|---|---|---|
| FCR | `Radar` | beforehand `SetRangeFactor(Degraded ? kRadarRangeDegraded : 1.0)` |
| Air data | `AirData` | — |
| Radar altimeter | `RadarAlt` | — |
| Navigation | `Nav` | ALSO invalidates `Cruise` (the same box publishes both messages) |
| Fire control | `FireControl` | — |
| SMS | `Stores` | — |
| Gun | `Gun` | — |
| RWR | `Rwr` | — |
| Countermeasures | `Countermeasures` | — |
| Datalink | `Datalink` | — |

The warning set (`FBWarningSystem`) runs LAST of the group and is a pure consumer of everything published
above it — including the validity headers.

#### 8.2 What follows from that FOR FREE

| Consumer | Behaviour on `Invalid` | where it stands |
|---|---|---|
| HUD | **dashes** — every display queries `H.Readable()` | `modules/f16/displays/FBF16Hud` |
| Warning set | the affected warning reports itself as **INHIBITED** instead of as "no warning" | `systems/FBWarningSystem` |
| Pilot (shot) | cannot even get at `wantShot`: `zone = fc.H.Readable() && fc.DlzValid`, `weapons = state.Stores.H.Readable() && LoadedCount > 0` | `pilot/FBPilot.cpp` |
| Pilot (gun) | `if (!state.Gun.H.Readable() \|\| !state.Gun.Ready) return;` and `if (!fc.H.Readable() \|\| !fc.GunValid) return;` | `FBPilot::BfmGunfire` |
| Pilot (pressing on) | `CanPressOn` = weapons aboard ∧ no BINGO ∧ a radiating radar — all three read off the BUS, none known | `FBPilot::CanPressOn` |
| Command bus | a command into a destroyed box → `rejected/system_failed` | `FBF16Module::ApplyCommand` |

#### 8.3 The command gate

`FBF16Module::ApplyCommand` checks BEFORE every box:

```cpp
if (CommandOwner(c.Target, owner) && !SystemWorking(owner)) { Rejected; SystemFailed; return; }
```

Necessary because the release path does NOT run through `Run()` but through this router: without the gate
a shot-up SMS would still let a round off the rail. The pilot then behaves correctly for free — he reads
the rejection exactly like that of an empty magazine.

Mapping command target → owning system (extract): `MasterArm`/`StationSelect`/`WeaponSelect`/
`WeaponRelease` → `Stores`; `GunTrigger` → `Gun`; `CmDispense`/`CmConsent`/`CmdsMode` →
`Countermeasures`; `Datalink*` → `Datalink`; `Radar*`/`Iff*` → `Radar`.

#### 8.4 Where damage becomes PHYSICS

`FBSimUnit::ApplyDamageToAirframe()` — idempotent, called immediately after every `TakeBurst`/
`TakeKineticBurst`, the only place where damage becomes physics:

| State | Engine | Flight controls | Structure |
|---|---|---|---|
| Failed | JSBSim cutoff | `SetControlAuthority(0.0)` | `SetDamageDrag(6.0)` |
| Degraded | `SetThrottleLimit(0.6)` | `SetControlAuthority(0.5)` | `SetDamageDrag(1.5)` |
| Intact | — | — | — |

All three channels are neutral until something has been hit: an undamaged aircraft computes bit-identically
to one that never heard of damage (measured).

#### 8.5 Events and columns

| Event | When | Fields (extract) |
|---|---|---|
| `damage DAMAGE` | per hit | zone, distance to the airframe structure, `fluxJm2`, `warheadKg`, `closureMs`, body-frame coordinates, bitmasks, hit count |
| `damage SYSTEM` | per system changing state | `system=…`, `state=degraded\|failed` |
| `damage KILL` | exactly once, when `WasEffective && !NowEffective` | reason, `failed` mask, altitude/speed (air) resp. position (ground) |
| `gun HIT` | per resolved bundle | expected hits, bundle size, `missM`, `spreadM`, `impactMs`, `areaM2`, `extentM`, `fluxJm2`, zone |

Columns: `dmg_*` (§7.6) and — the same process from the other direction — the `blk_*` columns, which will
see every block of a failed box go `Invalid`.

---

### 9. Zones and fragility as MODULE DATA

#### 9.1 F-16 (`modules/f16/FBF16Damage.{h,cpp}`)

Every zone boundary is read from the pinned `f16.xml` (structural frame: x positive AFT, CG at FS
−193 in), converted into metres AHEAD of the CG — no number comes from a drawing or a manual:

| Reference | Station | m ahead of the CG |
|---|---|---|
| Radome contact point (nose tip) | FS −486.6 in | +7.46 |
| Eyepoint (cockpit) | FS −336.2 in | +3.64 |
| Nose gear | FS −299.6 in | +2.71 |
| CG | FS −193.0 in | 0.00 |
| Main gear | FS −158.6 in | −0.87 |
| Wingtips | FS −121.3 in | −1.82 |
| Ventral fins (start of the engine bay) | FS −97.6 in | −2.42 |
| Nozzle | FS 0.0 in | −4.90 |
| Arresting hook (aftmost extent) | FS +100.7 in | −7.46 |

(Nose + tail = 14.9 m — the F-16's own length of 15.03 m.)

| Zone | Range [m] | Systems |
|---|---|---|
| `Nose` | +3.64 … +7.46 | Radar (APG-68 antenna/transmitter), AirData (pitot/AoA probes), Structure |
| `Forward` | 0.00 … +3.64 | Nav (INS), **Gun** (left strake), FireControl (FCC), RadarAlt (CARA), Datalink (MIDS), Structure |
| `Center` | −2.42 … 0.00 | Stores (SMS + station wiring at the wing roots), FlightControls (hydraulics + actuator runs), Structure |
| `Aft` | −7.46 … −2.42 | Engine, Rwr (ALR-56M aft), Countermeasures (ALE-47), FlightControls (tail actuators), Structure |

The gun takes the STRUCTURE thresholds, not the avionics thresholds — not for effect: a gun is a
mechanical installation with mass and the cross-section of the airframe around it, not a black box in a
rack. What stops it is what perforates the structure around it.

**The four fragility classes** — the actual SETTING of the model, all `[SET]`, in J/m²:

| Class | Degrade | Fail | conceived as |
|---|---|---|---|
| Avionics | 1.2e4 | 3.0e4 | a box: thin skin, no redundancy |
| Engine | 5.0e4 | 1.5e5 | accessories/nozzle: military power only |
| FLCS | 5.0e4 | 1.5e5 | one of two hydraulic systems |
| Structure | 8.0e4 | 2.5e5 | skin and stringers: drag |

As a scale, against an AIM-120 warhead (20.5 kg) at ~850 m/s head-on closure:

| Threshold | Range | Threshold | Range |
|---|---|---|---|
| 1.2e4 | ~11.6 m | 8.0e4 | ~4.5 m |
| 3.0e4 | ~7.3 m | 1.5e5 | ~3.3 m |
| 5.0e4 | ~5.7 m | 2.5e5 | ~2.5 m |

Reading: **anything that triggers the proximity fuze (10 m) at all costs avionics; only a burst within
~3 m takes the engine or the flight controls with it.** Everything in between then follows from the 1/r²
law instead of from another number.

Apart from the radar, an avionics system has no derivable degraded behaviour (range via the radar
equation), so all the other boxes set `Degrade == Fail` and never enter the degraded state. Modelling "a
bit of noise" on an INS or an ADC would be an invented number.

**Presented areas/extents:**

| Quantity | Value | Origin |
|---|---|---|
| `FrontalAreaM2` | 4.0 | **[SET]**, an equivalent area: ~1×1.5 m fuselage cross-section plus the thin edge of a 27.9 m² wing and the fins. NOWHERE is a real shape projection computed |
| `LateralAreaM2` | 14.0 | **[SET]**: the model's own `<wingarea>` 27.9 m², `<wingspan>` 9.14 m over a 14.5 m length — the planform is in this order of magnitude, the side view below it; 14 m² is the middle, more than a single number for "across the axis" can honestly be |
| `FrontalExtentM` | 4.57 | half the model `<wingspan>` — model geometry, not a setting |
| `LateralExtentM` | 7.3 | half the model length — ditto |

The two areas scale the expected hit count LINEARLY, which is why they are named here once.

#### 9.2 Ground targets (`modules/ground/FBGroundTarget.h`)

One value-type line per target class, no behaviour — the same decision as `core/FBStore.h` for a store.

| Module | Zone | Structure degrades from | fails from | conceived as |
|---|---|---|---|---|
| `target_soft` | ±10 m (20 m installation) | 1.2e3 J/m² (Mk-82: ~69 m) | 2.8e3 J/m² (~45 m) | unprotected installation, vehicle park, position |
| `target_hard` | ±6 m (12 m block) | 2.5e4 J/m² (~15 m) | 9.0e4 J/m² (~8 m) | bunker, hardened structure |

The radii are the honest reading of the thresholds: an Mk-82 (87 kg, `kCaseFraction` 0.5, `kFragSpeedMs`
1800) arriving at ~245 m/s delivers `flux(r) = 5.71e6 / r²` J/m².

All four thresholds `[SET]` (`weapons.md` §4.7 names warhead internals as a genuine gap; no source in the
tree names an effective radius). They are ANCHORED to the openly and often quoted order of magnitude for a
500 lb general purpose bomb against unprotected targets — effect/failure radius on the order of 50–60 m —
so 45 m for "finished" and 69 m for "hurt" is a conservative rather than a generous reading. The hard class
then says exactly what the two classes are supposed to distinguish: the same weapon needs practically a
direct hit.

Only `Structure` is declared: `CombatEffective` asks about engine, controls and structure, and a structure
has exactly one of them. Giving a SAM site a "radar" system would mean inventing a consumer that does not
exist.

`missions/attack-hardened.fbm` flies the same release against `target_hard`: the same 22 m miss distance,
**no** damage line (the arriving energy does not even reach the degrade threshold) — TIMEOUT,
`result=INTACT`. With that the fragility classes are a model and not decoration.

---

### 10. The three weapon modules

#### 10.1 `modules/stores` — the unguided round

`FBStoreModule.{h,cpp}` + `FBStoreModuleRegistration.cpp`.

- **A full `FBModule`** whose system slots are ALL the airframe-agnostic default: an Mk-82 has neither
  autopilot nor pilot nor displays nor radar. The RWR is set unpowered in the constructor, so that nothing
  it holds can be mistaken for a picture.
- **`Run()` does exactly one thing**: integrate its own FDM in fixed 100 Hz substeps (the same accumulator
  and the same spiral protection as every other module, max. 12 substeps per frame). **No control channel
  is ever written** — the trajectory is the aerodynamics of the pinned model plus gravity and nothing
  else. That is the whole point of modelling a weapon as its own FDM instance instead of as a hand-written
  ballistic formula.
- **One class, N registry names**: `FBStoreModuleRegistration` walks `kStoreCatalogue`, skips every
  `Guided` entry and registers the same class under the store's key (today `mk82`). `FdmModelName()` comes
  from the spec.
- **`ApplySetup` ALWAYS returns false**: a released store accepts no mission configuration — it was
  configured by being loaded onto a pylon. A `set` on a store could only be a mission that thinks it is
  declaring an aircraft, and is a runtime FAIL.

A guided weapon is a DIFFERENT module, not a flag on this one. Which of the two a catalogue entry becomes
is said by its `Guided` flag, read at exactly one place each in the two registration files.

#### 10.2 `modules/missile` — the guided missile

`FBMissileModule.{h,cpp}`, `FBMissileSeeker.{h,cpp}`, `FBMissileGuidance.{h,cpp}`,
`FBMissileUplink.{h,cpp}`, `FBMissileModuleRegistration.cpp`. Today: AIM-120.

The exact counterpart to `FBStoreModule`: a bomb has no pilot and no sensors, its `Run()` only integrates;
a missile has both, its `Run()` ticks them — that is the whole difference.

**Three real slots** (not special cases but derivations of the generic systems):

| Category | Class | Base |
|---|---|---|
| Sensors | `FBMissileSeeker` | `sensors/FBRadarSystem` — the ONLY thing in the module that sees the registry |
| Comms | `FBMissileUplink` | `sensors/FBDatalinkSystem` |
| Pilot | `FBMissileGuidance` | `pilot/FBPilot` |

Everything else is default: guidance/FCS pass the control commands through in `Manual`, no displays, no
navigation, no warning set, no stores of its own.

##### Rates

Seeker and guidance run INSIDE the 100 Hz substep loop, next to the FCS: a round closing at 1.5 km/s
covers 15 m per 10 ms — a 10 Hz decision rate (right for a pilot) would be a 150 m guidance quantum. The
seeker's antenna keeps its own absolute-time frame raster (0.05 s), so it is entered often but LOOKS only
at its own rate. The uplink receiver runs once per `Run()`: the shooter's fire control cannot produce a
fresher estimate than its own radar frame anyway.

##### The guidance law: proportional navigation, with derivation

Let `r` be the vector missile→target and `v_rel = v_target − v_missile`. The line of sight rotates at

```
Omega = (r × v_rel) / |r|²        [rad/s, vector along the rotation axis of the LOS]
Vc    = −d|r|/dt = −(r · v_rel)/|r|
```

The **core geometric fact**: if two objects move in straight lines, they collide exactly when the line of
sight does NOT rotate (`Omega = 0`) while the range decreases. Constant bearing with decreasing range =
collision course (the same rule as at sea). The whole task of a guidance law is therefore to drive `Omega`
to zero, and PN does that with an acceleration PERPENDICULAR to the LOS, proportional to the rotation rate
and to the closure:

```
a_cmd = N · Vc · (Omega × r̂)
```

The `Vc` factor: the same LOS rate counts for more the less time is left, and `Vc/|r|` is the reciprocal of
that time — it is what makes PN converge instead of chase.

**The navigation constant `N`** — the intrinsic dynamics of the LOS rate under this law gives
`λ̈ ∝ −(N−2)·λ̇/t_go`:

| N | Behaviour |
|---|---|
| ≤ 2 | the LOS rate does not decay at all → a tail chase, never an intercept |
| 3 | the classical minimum: the miss distance from a step manoeuvre decays, the integral of commanded acceleration is minimal for a non-manoeuvring target |
| **4** | the standard for an air-to-air missile against a manoeuvring target: takes the LOS rate out faster, the round arrives with its turn already done instead of pulling hardest at the end, where it has the least energy |
| ≥ 5 | amplifies seeker noise and estimation error into fin activity — costs energy in drag long before it buys accuracy |

FlightBox: `kNavConstant = 4.0`, one constant at one place, so that a future experiment can MEASURE it
instead of arguing about it.

**Gravity**: PN says nothing about weight — an unbiased law would let the round sag, arrive low and pull
up at the end. A one-g upward bias is added to the command, exactly what the autopilot of a real missile
does with its own accelerometer.

##### The autopilot under the law

The commanded acceleration is resolved into the body frame and flown as **two independent lateral
acceleration loops** — SKID-TO-TURN, because a cruciform missile pitches and yaws without rolling; there
is no lift vector to roll:

```
fin = Ka·(a_commanded − a_accelerometer) − Kd·(body rate from the gyro)
```

The rate term is not optional: the airframe's aerodynamic pitch damping is nearly zero by design
(`aim120.xml`'s `Cm_q` banner, −500 /rad, expressly low in damping-ratio terms), because that is what a
short finned body has. The gyro feedback makes the loop stable, exactly as in the original. The roll
channel keeps the fin cross level.

**Dynamic pressure gain schedule** — the airframe's control authority is proportional to `q`.
`FBTestMissileAirframe` measures ~13.8 g per unit of fin command at Mach 2 / 6 km (`q = 119 kPa`; the same
airframe buys a fraction of that at launch speed and a multiple of it low and fast). A FIXED gain would
therefore be sluggish exactly where the round is slow (just after launch, where the first turn has to be
made) and twitchy where it is fast. So the fin-per-g gain is scaled with `qRef/q`:

| Constant | Value | Meaning |
|---|---|---|
| `kQRefPa` | 119,000 | the dynamic pressure at which the 13.8 g/unit was measured |
| `kFinPerG` | 1/13.8 | fin command per g of demand at `kQRefPa` (the measured reciprocal) |
| `kLoopP` / `kLoopI` | 1.2 / 1.5 | proportional and integral terms above it. `kLoopI` was 2.0 until the D1 re-tune round: past ~10 g the fins hit their stops and the integrator wound into a reversal (measured terminal tick-to-tick \|Δα\| 0.698°); integration is now CONDITIONAL (no wind-up against a saturated fin) and the gain sits on the stable side of the measured boundary (1.75 → 0.698°, 1.50 → 0.139°). Miss on `bvr-duel-decided` 7.09 → 2.36 m; collateral improvements `intercept-lostlock` 4.12 → 0.755 m, `damage-amraam` 1.90 → 1.49 m. |
| `kRateGain` | 0.35 | fin per rad/s of body rate, scheduled the same way |
| `kGainScaleMin/Max` | 0.15 / 20.0 | clamp of the scaling factor |
| `kIntegralClamp` | 1.0 | anti-windup — the integration is in FIN units and clamped there, so that the limit is the physical one (a fin cannot go past its stops) instead of one that would have to be re-derived per gain |
| `kRollGain`/`kRollRateGain` | 0.05 / 0.02 | roll holder |
| `kMaxCommandG` | 25 | **[SET]** command ceiling: no fin deflection buys more than the airframe's trim alpha allows at that altitude; 25 g is above what is achievable except low and fast, so it bounds the demand without ever being the binding limit |
| `kUplinkTimeoutS` | 1.5 | **[SET]** how old an uplink message may be before the round no longer calls itself supported: the fire control transmits at its radar frame rate (0.1–1 s), so a second without a message means "support ended", not "message missed" |

**Why the integral term is not optional**: a pure P acceleration loop leaves a steady-state error of
`1/(1+loop gain)`, and with the 1 g gravity bias this error is a PERMANENT SINK — the round arrives low.
Exactly that was shown by the first flown intercept before this term existed (measured: −18 m/s, ~900 m
too low at the merge).

##### The three phases — transition by ACQUISITION, not by timer

| Phase | Ordinal | Data source |
|---|---|---|
| `INERTIAL` | 0 | the launch programming (`FBStoreRelease::Target`) — position and velocity, extrapolated at constant rate. Also the state the round FALLS BACK to when the shooter stops supporting |
| `MIDCOURSE` | 1 | a fresh message over the shooter's uplink; the round re-aims at what his radar sees NOW |
| `TERMINAL` | 2 | the OWN seeker has a lock; from then on it stays terminal — a seeker that has the target does not ask any more |

Strict priority in `UpdateTarget`: own seeker > uplink > the last known. Every branch writes THE SAME four
fields, so the law below never asks where its numbers came from — only how old they are.

**The transition is an EVENT**: the seeker is switched on when the estimated range falls below the round's
activation range (a catalogue number); the phase changes only when it actually ACQUIRES — which it manages
only if the midcourse has aimed it close enough (field of view ±10°). A bad midcourse therefore produces a
miss, not a magical terminal phase.

**Uplink loss is survivable and observable**: if the shooter's lock drops, the transmission stops; the
phase falls back to `INERTIAL` and the round keeps flying the last information, extrapolated. Whether that
is enough depends on how long ago it was and what the target did since — the whole tactical point,
measured instead of claimed: `missions/intercept-lostlock.fbm` still hits with it,
`missions/intercept-defeated.fbm` no longer does.

##### The gathering phase — BUILT 2026-07-29, and it is orthogonal to those three phases

`FBStoreSpec::GatherS` was declared, filled for all six surface rounds and specified in two doc files
**and read by no line of code**. It is now read, in `FBMissileGuidance::FlyCommand`:

| | Rule |
|---|---|
| **Test** | `Spec_ && Spec_->GatherS > 0.0 && NowS_ - LaunchS_ < Spec_->GatherS` |
| **Effect** | `FinPitch_ = FinYaw_ = 0`, `ManualPitch = ManualYaw = ManualRoll = 0`, and `FlyCommand` returns **before** the two lateral-acceleration loops and the roll holder. The fins TRAIL; the round flies the rail direction on thrust alone |
| **What still runs** | the guidance law above it. `NzCmdG_`/`NyCmdG_` are written as always, so **`msl_nz_cmd` nonzero beside a zero `msl_fin_pitch` IS the phase** in the trace — the phase is observable without a new telemetry channel |
| **Why it sits below the law and not in `UpdateTarget`** | `INERTIAL`/`MIDCOURSE`/`TERMINAL` name the **data source**; the gathering phase names whether the **fins are connected**. The two are independent, so they are not one enum |
| **Second reason it must return early** | the two loops integrate. Against an airframe with no dynamic pressure to answer them the `kLoopI` integrators wind straight into `kIntegralClamp` — the same failure mode the D1 re-tune measured at the other end of the flight |
| **Air launches are untouched by construction** | `GatherS = 0.0` for every air-launched catalogue row, so the test is false for every store that ever left a pylon. Measured: **150 of 160 committed missions byte-identical**, the 10 that moved are exactly the ones with a ground launch |

**The six values are not a new number.** They already stood in the catalogue with their `[SET]`
provenance before this round; building an unread field introduces nothing. Beside them the burn time each
deck actually computes, `t = P·Isp/T`:

| Row | `GatherS` | burn time `P·Isp/T` | relation |
|---|---:|---:|---|
| `v750` | 3.0 | 4.499 s | gathering ends **during** the burn |
| `v601` | 2.5 | 2.498 s | the only row where the two **coincide** |
| `3m9` | 2.0 | 3.995 s | during |
| `9m33` | 1.5 | 1.992 s | during |
| `strela2` | 0.6 | 1.975 s | during |
| `igla` | 0.6 | 1.982 s | during |

**They are deliberately NOT aligned.** "Gathering ends at booster separation" is a plausible-sounding rule
and it is simply false for a shoulder-launched round, which has no booster to separate; `GatherS` is the
time the fins are useless, and burnout is the time the thrust stops. Two quantities, two numbers.

**Measured, V-601 on a 70° rail** (pre-fix against post-fix): pitch +70° → −41° in 1.6 s and an impact 7 m
under the ground, against **70.00 / 69.97 / 69.95°** held through the phase and **868.8 kt = 447 m/s** at
its end — i.e. the round reaches full fin authority *before* the first steering command is allowed
through, which is the entire point of the phase.

##### The seeker

`FBMissileSeeker` is structurally THE SAME class as the jet's FCR (`sensors/FBRadarSystem`) — not a
shortcut but the point: the missile is a world unit like any other and perceives the world only as is
allowed here, namely through a simulated sensor that scans a volume, needs several looks for a track and
writes ANONYMOUS geometry into its own `FBState`.

| Property | Value | Justification |
|---|---|---|
| Field of view | ±10° | **[SET]** — no public number (`weapons.md` §4.7 names exactly this class as a gap). Ten degrees is the order of magnitude for a 7 inch dish, and it has a MEASURABLE consequence: a midcourse handing over more than 10° off the nose does not acquire — which makes midcourse quality something that counts |
| Gimbal | ±45° | **[SET]** and expressly a DIFFERENT, much larger quantity than the instantaneous field of view: after the lock the dish points AT the target and only runs out at the mechanical stops. Without this distinction a locked seeker would lose its own target as soon as the geometry moved 10° — which the first flown lost-lock run showed literally (loss and reacquisition 25° off the nose, 3 s before impact). The same form as the FCR's search box/STT split |
| Frame time | 0.05 s | **[SET]** — a STARE, not a sweep: 20 looks per second, so that the terminal guidance flies on MEASUREMENTS instead of on extrapolation |
| Mode | `AutoAcquire` + `SingleTarget` | nobody designates for a missile; the first firm track becomes STT, after which it stares at it |
| Slaving | `SlewTo(losAz, losEl)` until its own lock | the "SLAVE" line-of-sight mode from `weapons.md` §2.5. A BORE launch is the same class with `SlewTo` at zero — which is why the mode is a pair of numbers and not a subclass |
| State before activation | OFF | a switched-off seeker is not merely silent — it REPORTS nothing, so that the guidance does not accidentally aim at a track from before activation |
| IFF | interrogator AND transponder off | a missile cannot ask who that is, and nobody answers for it |
| Emission | `FBEmitterKind::MissileSeeker` | the one thing a receiver cares about: behind this antenna sits a warhead. An RWR classifies this signal as the launch case, whatever it is currently scanning |

##### The uplink receiver

`FBMissileUplink` walks the registry, finds the ONE unit whose id is the programmed shooter, and takes its
`FBUnitSignature.Uplink` **only** if it is still actively transmitting. It reads nothing else about that
unit and nothing at all about the target: the content is the SHOOTER'S RADAR ESTIMATE, with his errors and
his age.

It is published as the datalink track `Tracks[0]` — because that is EXACTLY what the received message is
(a position, a velocity and the time of the measurement); the guidance reads it as an instrument like
everything else. No new bus block, no return channel, and the validity header answers the only question
the guidance really has: is anybody still telling me something?

The track is called `"UPLINK"` and carries NO unit id: the shooter's radar does not know whom it is looking
at either. A missile cannot learn an identity its shooter never had. The timestamp is the SHOOTER'S LOOK,
not the moment of reception — the estimate stands on his radar, and its age is what the missile has to fly
with.

Uplink loss is not an error path: `Active` goes false, this class stops publishing tracks (`H.Invalidate()`
— not an empty picture but NO picture), and the guidance sees the age growing. Nothing here decides
anything about the flight.

##### The model is FlightBox's OWN

`sim/assets/aircraft/aim120/` — in the same single model root as f16 and mk82, but as the only model
WITHOUT an upstream counterpart (`sim/assets/MODEL-DELTAS.md`, provenance table: `—`). The pinned JSBSim
submodule has no AMRAAM; so there is nothing to diff here, only a self-written model. Nothing under
`vendor/` is touched by its existence.

Modelled in it is the WHOLE flight mechanics: mass and its decrease over the burn, thrust over the burn
time, axial and normal force over Mach and angle of attack, static stability, pitch/yaw/roll damping and
the fin moments. `modules/missile/` writes fin commands and a throttle and reads back an accelerometer and
rate gyros — nothing else. It never sets a position, a velocity or an attitude.

Provenance scheme of the XML (in the file header): `[T-ED]` / `[T3]` / `[DERIVED]` / `[SET]`. The
aerodynamic coefficients are ALL `[SET]` or `[DERIVED]` — there is no public aero deck for this missile,
and inventing a citation for it would be worse than saying so. What makes them honest is that they are a
CONSISTENT slender-body set (trim relation, achievable g and drag deceleration are each named individually
and each measurable on this model's telemetry), not that they are evidenced. Extract:

| Quantity | Value | Mark |
|---|---|---|
| Diameter 7 in → `S = πd²/4` | 0.02482 m² = 0.2672 ft² | [T3] → [DERIVED] |
| Reference length = diameter | 0.5833 ft | [DERIVED] |
| Fin span 526 mm | 1.726 ft | [T3] (only for the roll damping) |
| Launch mass | 335 lb | [T3] |
| Propellant | 115 lb | [DERIVED] from the rocket equation against ED's "max. ~Mach 4": `Isp 235 s → ve = 2305 m/s`, `m0/m1 = exp(1000/2305) = 1.543`, `m_p = 335·(1 − 1/1.543) = 118 lb`, rounded |
| Empty mass `<emptywt>` | 220 lb | [DERIVED] |
| CG | station 69 in | [SET] — just ahead of the body midpoint, a classical layout |
| Pitch inertia | 105 slug·ft² | [SET] (a uniform rod overestimates a body with heavy sections) |
| Roll inertia | 0.44 slug·ft² | [DERIVED] `Ixx = m·r²/2` |
| Fin dynamics | lag `c1 = 60 1/s` (~17 ms), travel ±25° | [SET] |
| `Cm_alpha` | −12 /rad | [SET] — static stability margin, directly as a moment |
| `Cm_q` | −500 /rad | [SET] — deliberately LOW in damping-ratio terms; precisely why the autopilot needs the gyro feedback |
| `Cl_p` / `Cl_da` | −12 / 0.05 /rad | [SET] — together ~4 rad/s of sustained roll rate at full fin and Mach 2 |
| Motor start-up time | 0.06 s (sine ramp) | [SET] |

The motor (`engine/WPU-6.xml`, rocket engine, `Isp 235`) is a boost-sustain; the SPLIT between the two
phases is `[SET]` (the public source situation says "boost-sustain" and nothing else).

**No throttle for a solid-fuel motor**: the guidance commands `ManualThr = 1.0` every tick, and the
throttle slew in `FBFdm` (0.5 s from idle) IS the safety separation delay before ignition. It is commanded
every tick because that is the one channel there is, not because anything could switch it off again.
**A RAIL launch has no such delay** (2026-07-29): `FBFdmSpawn::MotorRunning`, set from `HaveRail`, lights
the motor at the initial condition, because a round leaves a rail *because* the motor pushed it off. The
0.5 s remains what an air-launched round drops through — see [`fdm.md`](fdm.md) §6 step 6 for the measured
1.48 m of sink the missing distinction used to cost a surface round.

**Its own telemetry columns** `msl_*` instead of the pilot channels (the bus is built PER UNIT, so no
column of a jet's trace changes): `msl_phase`, `msl_range` (to the ESTIMATE, never to the truth),
`msl_closure`, `msl_losrate` (what PN drives to zero), `msl_los_az`/`_el`, `msl_nz_cmd`/`msl_ny_cmd`,
`msl_fin_pitch`/`_yaw` (what really reached the fins), `msl_seeker` (0 off / 1 active / 2 locked),
`msl_tgt_age` (since the last real measurement).

Events: `missile PROGRAMMED`, `missile PHASE` (every change with reason, time of flight, range, LOS),
`missile SEEKER_ACTIVE`.

##### The three seeker kinds — one module, three weapons

`FBStoreSpec::Seeker` (`core/FBStore.h`) decides which SENSOR SLOT `FBMissileModule` cycles, and that
is the whole difference between the guided rounds in the tree. Exactly one detector is powered on any
round; the other publishes nothing, so a trace can never claim a sensor the weapon does not carry.

| | **ActiveRadar** (AIM-120) | **Infrared** (AIM-9M, R-73) | **SemiActiveRadar** (R-27R) |
|---|---|---|---|
| slot | `FBMissileSeeker` (`sensors/FBRadarSystem`) | `FBMissileIrSeeker` (`sensors/FBIrstSystem`) | `FBMissileSeeker`, but it never transmits |
| what it measures | range, bearing, closure | **angles only** | range, bearing, closure — of a REFLECTION |
| guidance law | true PN, `a = N·Vc·(Ω × û)` | **pure PN**, `a = N·V_own·(Ω × û)` | true PN, unchanged |
| gets on | activation range (a catalogue number) | uncaged at launch, slaved to the programming | the shooter's illumination |
| loses it | never (it has its own transmitter) | when the mark leaves the field | when the illumination stops — **and never comes back** |
| midcourse | uplink | none: lock-before-launch, then alone | uplink, which IS the illumination indicator |
| shooter's obligation (`FBSeekerHandoverS`) | until activation | **0** — free at launch | **-1** — until impact |
| deceived by | chaff (Doppler notch) | **flares** (irradiance, `doc/sensors.md` §6.6) | chaff, and by the shooter breaking off |

**The angle-only law, and why it is the PURE form.** True PN needs the closing speed; an infrared head
has none. So the line-of-sight rate is differentiated from consecutive LOOKS of the head — which is
literally what a real seeker's rate gyro puts out, without a position ever existing —

```
u     = unit vector along the LOS, from the head's reported bearing/elevation
Omega = (u_prev x u_now) / dt          [rad/s]
a     = N * V_own * (Omega x u) + 1 g up
```

and the round's own speed takes `Vc`'s place. The rate is HELD between looks (`kLosRateHoldS` = 0.1 s,
two head frames): the head reports at its frame time and the loop runs at 100 Hz, so differentiating
inside a frame would read zero four times out of five and then a step. `msl_range` is **-1** for such a
round throughout, and that is the point — the column says what the seeker cannot measure instead of
printing a truth it never had. **And this is where a flare wins:** the law flies at whatever the head
reports, with full authority and no idea anything happened.

**The semi-active mechanic, in four lines of guidance.** `ActivationRangeM = 0` declares "there is
nothing to activate"; the seeker is switched on while the shooter's uplink message is fresher than
`kUplinkTimeoutS` and switched OFF for good the moment it is not (`missile ILLUMINATION_LOST`). That is
the only asymmetry: an uplink loss on an AIM-120 is survivable because its own transmitter is still to
come, and on an R-27R it is terminal. Upstream of it, `FBSeekerHandoverS` puts `-1` into
`FBFireControlBlock::TimeToActiveS`, `pilot/FBEngagement::NoteSupport` therefore falls back to the
predicted time of flight for its support window and `Pitbull` can never become true, and
`FBPilot::SupportInhibitsDefend` (a module hook, `false` for everyone else) makes Support and Defend
mutually exclusive on an airframe whose doctrine says so. **No new architecture: three hooks and one
state-transition guard**, exactly as `doc/modules/mig29/weapons.md` §3.2 predicted.

**The three decks** are FlightBox's own (`sim/assets/aircraft/{aim9,r73,r27r}`, MODEL-DELTAS provenance
`—`), built from the AIM-120's slender-body set, which is NON-DIMENSIONAL and describes a finned
cylindrical body. Per round only what depends on its own proportions is recomputed — the reference area
`S = πd²/4`, the fin arm in calibres, and `Cm_de = -CN_de · arm` with `Cm_alpha` set to hold the same
~0.41 rad trim design point at full fin. Every motor is sized by the rocket equation against the
documented terminal Mach at `Isp` 235 s, so the round and the fire-control table that predicts it start
from the SAME motor. Two omissions are named in the decks rather than papered over: the **R-73's
thrust-vectoring vanes** (documented, but vane authority, gas-generator duration and blending law are
all unpublished — so the deck understates it exactly where the real weapon is exceptional), and the
fact that at equal dynamic pressure all three pull the same class of g the AIM-120 does, which is less
than a short-range round really has.

#### 10.3 `modules/ground` — the module that does not even integrate

`FBGroundModule.h` + `FBGroundModuleRegistration.cpp`. Structurally `FBStoreModule` MINUS one thing instead
of plus one: a released bomb has no pilot and no guidance but does integrate; this does not even have that.

- **`Run()` is EMPTY.** No FDM to tick, no system to cycle, no state to advance — its whole tick behaviour
  is that its pose is the one declared by the mission.
- **`FdmModelName()` returns an EMPTY string** — that is the SIGNAL to the spawn path
  (`missions/FBMissionBoot.h`): there is no airframe to load here, and `AttachFdm` is consequently never
  called. `UnitKind()` returns `FBUnitKind::Ground` and thereby says what kind of world entity it becomes.
- **The design question this class answers**: the two ways were to give a bunker a trivial JSBSim model (so
  that nothing else has to change) or to let a unit exist without one. The first would have meant an
  invented aerodynamic object — mass, contact springs, a trim state — for a thing that does not move, and
  100 Hz of integration only to reproduce the position at which it was spawned. So the AIRFRAME is
  OPTIONAL at unit level (`std::unique_ptr<FBFdm>` may be null): a unit with one is ticked, a unit without
  one holds its declared pose, and everything else is the same code.
- **The only non-default: `DamageLayout()`** — where its structure sits and how much it takes. The same
  table form that `FBF16Damage` supplies for the airframe, so that ONE damage model answers both. That is
  the accessor which turns a target into a real participant instead of a marker.
- **One class, N registry names**, as with the stores; `ApplySetup` always returns false (what a target IS
  is said by its module name, where it stands by its `spawn` line).
- A ground target's `UNIT_RESULT` reads `INTACT` or `DESTROYED` instead of a flight verdict.

---

### 11. Mission data and proof runs

#### 11.1 The keys (F-16, `FBF16Module::ApplySetup`)

| Line | Effect |
|---|---|
| `set store <station> <type>` | one line per pylon; station = the pylon number OF THIS TYPE (F-16: 1..9, 1/9 wingtip, 5 centreline), type = catalogue key (`mk82`, `aim120`). Unknown/duplicate station or unknown type = runtime FAIL at spawn |
| `set gun_rounds <n>` | drum content at start, 0..510; more than the capacity = FAIL |
| `set brief_master_arm arm\|sim` | the pilot sets Master Arm IN FLIGHT over the bus (HOTAS class) |
| `set brief_release_s <t>` | repeatable: when the pilot pickles |
| `set task attack` + `set attack_mode ccip\|ccrp` | attack phase and which cue |
| `set pilot_attack_bias_s <s>` / `set pilot_attack_ccip_m <m>` | variants of the release moment resp. of the pipper tolerance |
| `set pilot_gun_burst_s <s>` | burst length |

#### 11.2 The pilot's release moment

The pilot computes **no** ballistics: he reads the `FBFireControlBlock` like any instrument.

| Mode | Cue |
|---|---|
| `ccrp` | `AgTimeToReleaseS <= 0` — the solution cue passes the FPM |
| `ccip` | the same moment AND `|AgCrossErrM|` within the pipper tolerance (F-16: 45 m) — the lateral judgement a countdown cannot make |

**The pickle is LED by one's own actuation latency** (`FBCommandBus::LatencyS`, HOTAS 0.5 s): pressing
exactly on the cue released the store 0.5 s too late — at 231 m/s that is 115 m, more than the whole
computation is worth. The real jet solves the same problem the other way round: in CCRP the pilot HOLDS
the button and the AIRCRAFT releases. Measured: without the lead 123 m long, with the lead 8 m.

The gun trigger leads analogously — by `kTriggerLatencyS + fc.GunTofS` — and reads the solution as an
ABSOLUTE VALUE (a solution TRAVELLING through zero predicts −1.5°, and that means "1.5° off", not
"perfect"; the earlier clamp at 0 made the fastest travelling solution the best one in a fight). Measured
over eight approaches each, with only this one line changed: bursts 46 → 30 (straight) resp. 59 → 38
(turning), rounds on target per burst 1.81 → 4.42 resp. 2.21 → 3.19, ammunition consumption per kill
394 → 254 resp. 270 → 204 rounds.

The tracking itself (error rate + integrator, `pilot/FBPilot` section 3c) improved over eight approaches
each: funnel time 3.2 s → 20.7 s (straight) resp. 0.0 s → 21.6 s (turning), rounds on target 11.9 → 111.2
resp. 0.0 → 120.4, kills 0 → 5 resp. 0 → 7 out of eight runs each, mean tracking error 10.5° → 6.9° resp.
11.9° → 4.1°.

#### 11.3 Initial condition of a released store

`FBMissionBoot.h::FBMissionSpawnStore` — the same four-step spawn as for any jet, only that the IC comes
from the CARRIER STATE (`FBFdmSpawn::Ballistic`):

- position = carrier position + station offset (body-fixed, rotated with the carrier attitude),
- attitude = carrier attitude,
- velocity = carrier velocity **at this station**, including `ω × r` (so that a release in a roll is
  right),
- **no ejector impulse** — no evidenced source exists for its magnitude (`weapons.md` §4.5), so the store
  inherits the aircraft's motion and nothing invented,
- **no trim** — a bomb has no control surface.

The station offset is computed in the SMS (structural → body-fixed, relative to the CG, JSBSim's own
`FGMassBalance::StructuralToBody` convention), because only it knows its pylon geometry.

#### 11.4 Proof missions

| Mission | Subject |
|---|---|
| `mk82-carriage-loaded` / `-clean` | carriage effect, numerically (§2.2) |
| `mk82-safe` | rejection: Master Arm never armed |
| `mk82-drop` | four releases + one pickle on an empty jet |
| `attack-ccip` / `attack-ccrp` | release computation against reality (§4.2) |
| `attack-late` | the same run-in, released 2 s late |
| `attack-hardened` | the same release against `target_hard` — no damage, `INTACT` |
| `gun-bfm` | tracking pass against a straight-flying opponent |
| `gun-turning` | the same shooter against a CONTINUOUSLY TURNING defender (the hard test: the funnel solution TRAVELS) |
| `gun-dry` | empty drum, rejection `depleted` |
| `intercept-aim120` / `intercept-dlz` | guided shot, DLZ |
| `intercept-lostlock` | uplink loss — still hits |
| `intercept-defeated` | uplink loss + evasion — no longer hits |
| `damage-amraam` | the CONSEQUENCE: detonation, damage resolution, system failures, block invalidities and 340 s of aftermath until impact (exit 2) |
| `make -C sim test-gun` | dispersion fit against MIL-DTL-45500/1A, time of flight, funnel geometry, lead solution against the flown trajectory, ammunition consumption, rejection on an empty drum |
| `make -C sim test-missile` | the AIM-120 airframe open-loop (motor/drag/trim) |
| `mig29-r73` / `f16-aim9` | the infrared round, BOTH branches: a rear-quarter hit and a flare-decoyed miss, on both airframes |
| `mig29-r27` | the semi-active chain, BOTH branches: illumination held to impact, and illumination broken in flight |
| `mig29-gun` | the GSh-301: a briefed burst, the 30 mm kinetic path, and the range dependence of the density model |
| `duel-asym-probe` | F-16 vs MiG-29, one round each — a SMOKE TEST for the asymmetry, explicitly not a campaign |
