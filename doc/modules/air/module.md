# FlightBox — the catalogue aircraft (`modules/air/`)

**Subject:** `C7` — **one flying unit that is not a module.** The contract for a data-driven aircraft
class with N catalogue rows: what it is, what flies it, what it may see, what it shoots, how much pilot
it gets, and how a campaign can tell whether it lost as *itself* or as a coarse model.

**Status: SPECIFIED, NOT BUILT.** No line of `sim/` was touched to write this file. It is the third
level below the module, and it exists because [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md)
names `C7` the gap that **degrades nine of ten campaigns**: every missing type is replaced by a present
one, every replacement changes the answer, and each is declared one at a time in a mission header.

**Delimitation, in the tree's own terms:** this file documents the **CODE that will be built**, the same
role [`../ground/module.md`](../ground/module.md) plays for the ground position and
[`../f16/module.md`](../f16/module.md) for the F-16. The real aircraft — radars, envelopes, armament,
performance, with source and confidence tier — is [`catalogue.md`](catalogue.md), in the schema of
[`../mig29/`](../mig29/INDEX.md). The single decision this contract rests on — **does a catalogue cell
fly on JSBSim, and on what deck** — is [`flight-model-recipe.md`](flight-model-recipe.md).

**Home migration:** [`../ground/cast.md`](../ground/cast.md) §"Air" carried the air rows at four
quantities apiece as a placeholder, and drew the unit/module line with the question *"does it have to
react to what the player does?"* That question is **kept and sharpened** in §Spec 1 and §Spec 4; the air
rows' home is now here, and `cast.md` points at it.

---

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| A catalogue aircraft is **one class with N catalogue rows**, never N classes | `modules/air/FBAirModule` + `core/FBAircraft.h`'s `kAircraftCatalogue`; the registration file walks the catalogue exactly as `FBSiteModuleRegistration` walks `kSiteCatalogue` and `FBStoreModuleRegistration` walks `kStoreCatalogue`. A second C++ airframe class in this directory is a defect, not a precedent |
| A row **flies on JSBSim** iff its own manoeuvre decides an outcome **and** its envelope is published; otherwise it moves kinematically | the two-part test of §Spec 4, applied per row in [`catalogue.md`](catalogue.md)'s `Motion` column. Nine rows get a deck, nine get a mover, and no row gets both |
| A deck is **generated from ONE recipe against published anchors**, never hand-built | [`flight-model-recipe.md`](flight-model-recipe.md): eight anchors per row, closed-form inversion, one turbojet and one turbofan thrust analogy, deviation bands **derived from the one existing generated deck's measured misses**. `make -C sim test-air <row>` measures every anchor a row has |
| The pilot is **staffled by row**, and a tier is a declared TASK SET — not a class | §Spec 5. `FBAirPilot : pilot/FBPilot` adds exactly two states (`Orbit`, `Drag`) for the two lowest tiers and otherwise only fills hooks. A tier a row's own MEASURED hooks cannot support is refused at boot, not flown badly |
| It sees through sensor slots **or not at all**, and the registry reader list does **not** grow | the detectors are configured instances / thin derivations of `FBRadarSystem`, `FBRwrSystem`, `FBIrstSystem`, `FBVisualSystem` — all four already hold the include. `tools/verify_layers.py` prints **6 registry reader(s) inside the perception boundary** after the round exactly as before |
| **An early-warning aircraft moves an ANTENNA. It never creates a TRACK** | [`../../air-defence-network.md`](../../air-defence-network.md) §3's rule, verbatim, with the sender airborne: `FBNetReport` carries a POINT with the sender's own look age, no id field and no team field. Acceptance: a cued fighter whose own `Radar` id is failed produces **zero** contacts however perfect the cue. §Spec 7, and it is the file's most dangerous line |
| The **one** interface price of that is named in advance | a receiving aircraft needs a **second comms slot** (`sensors/FBNetLinkSystem : FBDatalinkSystem`, its own `FBState` block), because a fighter's Datalink block already carries Link-16 PPLI. That is `air-defence-network.md` §2 **design B**, deferred there and **made due here** |
| Damage is the **existing** register, with no new type, no new friend and no new system id | `core/FBSystemHealth` unchanged; a row declares an `FBDamageLayout` like every module. A mover declares one zone and `Structure` alone, like `target_hard` |
| Doctrine is **mission data**, capability is **catalogue data** | **two** new author-facing `set` keys (`orbit`, `drag_threat_s`), each `[SET]` with a reason. Everything else about a row is said by its module name; everything about its tasking uses keys that already exist (`task`, `store`, `brief_gci`, `iff_xpdr`, `jam_comm_m`, `fuel_pct`, `pilot_*`) |
| A campaign can tell **model error from doctrine** | §Spec 11's attribution test: the row is admissible as an opponent iff `band_deck ≤ 0.25 × band_doctrine`, on the same tournament instrument [`../../duels.md`](../../duels.md) already runs. A row that fails prints both numbers instead of a result |
| Nothing about a catalogue aircraft is random | no die anywhere: acquisition is the radar's own volume/frame law, the mover's track is arithmetic, the drag reflex is an RWR bearing test. Determinism over `--threads 1/2/4` × 3 repeats, one fingerprint |
| A mission that declares no catalogue row behaves **byte-identically** | the conservation rule of `C2`/`C12`/`C22`: nothing new is written unless something new is declared |

### 1. The third level — module, catalogue cell, unit

[`../ground/module.md`](../ground/module.md) §Spec 1 drew one line with the test **"does anybody fly
it"**, and put a SAM battery on the far side. That test is right and it does not resolve this round: an
E-3 and a MiG-21 are both flown, and they are not the same kind of object. The line splits in two:

1. A **module** is a flown airframe FlightBox is *judged on*: a pinned or anchor-measured JSBSim deck,
   a full avionics bus, a pilot phase machine, ~20 C++ classes, and a **reference base directory of its
   own** distilled from manuals. Two exist and there will not be many more.
2. A **catalogue cell** is a flown airframe FlightBox is *not* judged on: one parametric class, one
   catalogue row, a **generated** deck or no deck at all, the sensor and weapon slots its row declares,
   and a pilot tier. It earns **one catalogue row**, never a directory.
3. A **unit** is everything that is perceived, shot at and shoots back without being flown: the ground
   position, the released store, the inert target.

| Property | Jet module (`f16`, `mig29`) | **Catalogue cell** (`mig21` … `mi8`) | Ground unit (`sa2` … `p18`) |
|---|---|---|---|
| JSBSim deck | full aero/mass/propulsion, pinned or anchor-measured, delta-gated | **generated from one recipe** against 8 published anchors — or **none**, for a mover row | none (`FdmModelName()` empty) |
| Provenance of the deck | NASA TP-1538 (F-16) / 22 measured anchors (MiG-29) | 8 anchors, closed-form inversion, two declared thrust analogies, bands derived from the MiG-29 deck's own measured misses | — |
| C++ classes | ~20 per airframe | **one**, plus one pilot derivation and thin sensor configurations | one, plus three sensor derivations and one fire control |
| Documentation | a reference base directory per type | **one catalogue row** in [`catalogue.md`](catalogue.md) | one catalogue row in `../ground/catalogue.md` |
| Avionics bus | 18 blocks, three-state validity | the blocks its row's sensors fill, and no others | what its telemetry needs |
| Pilot | the full `pilot/FBPilot` phase machine | **a declared tier**, §Spec 5 — five levels, from a track to the full machine | a five-state engagement machine |
| Flight verdict | `core/FBFlightMonitor` judges it | **judges it, if it has a deck**; a mover has no airframe and is never shown one | never (no airframe) |
| Health register | `core/FBSystemHealth`, unchanged | `core/FBSystemHealth`, **unchanged and identical** | ditto |
| Registry access | through the sensor slots | through the sensor slots — the same six files | ditto |
| What is accepted about it | nothing: a missed anchor is a defect | **the anchor bands of `flight-model-recipe.md` §5** — a row inside its band is *accepted*, and a campaign result is only publishable for rows that are | its catalogue row is accepted as data |

**Consequence for the tree:** `modules/` grows by one directory holding one class. Nothing in
`modules/f16/` or `modules/mig29/` moves, because the catalogue class composes the same `systems/`
defaults every module does.

### 2. Grob where it does not decide, treu where it does

The owner's rule for an airframe. Left column: modelled, and a mission result depends on it. Right
column: documentation only, or absent, and the file says so.

| **Modelled, because it decides** | Why it decides |
|---|---|
| Radar **search range and azimuth/elevation field** | the MiG-21's ±30° × ±10° nose-inlet field is half the F-16's, and it is *sourced*. It decides whether a cued interceptor finds anything at all |
| Radar **frame time**, derived from that field | the tree's own declared relation (`sensors.md` §4.2: *this relation, not the absolute seconds, is the model*). Acquisition costs `kHitsToFirm × FrameS`, and that time is half the intercept |
| Whether the set can **look down** | the Sapfir-23D-III cannot see below 1 000 m and rejects anything slower than 60 km/h; that is the whole low-ingress argument, sourced, per row |
| **Weapon envelope** `Rmin`/`Rmax` and the aspect restriction | the rear-aspect-only K-13 against the all-aspect AIM-9L is the difference between two campaigns |
| Whether the weapon **binds the shooter** | already fully modelled (`FBSeekerHandoverS` −1 / 0 / positive). Every catalogue row except the F-15C and Su-27 carries a SARH round, so almost the whole catalogue is bound to impact |
| **RWR presence** and what it hears | a MiG-17 has none. A row without a receiver cannot defend, cannot be `emcon`-cued, and cannot run the drag reflex |
| **Countermeasures presence** | the tree's chaff/flare models are built; a row either has a dispenser or it does not, and most eastern rows do not |
| **Visual extent** (span × length) | the eye's Johnson criteria (`sensors.md` §9.7) — and this is the **one channel the catalogue can source completely**, because span and length are published for every row |
| The eight flight-performance anchors | Vmax at two altitudes, ceiling, climb, g limit, masses, thrust, wing area — they set what a row can reach, how fast, and how long it can hold a turn |
| Engine count (twin/single) | `FBSystemId::Engine2` already exists; a twin that loses one is a different aircraft |

| **Coarse or absent, because it does not decide** | What is done instead |
|---|---|
| Post-stall and departure behaviour | **refused.** It needs a per-type high-α analogy; the MiG-29 needed the whole NASA HARV database for its ten post-linear curves. A generated deck carries the linear range and stops, and the α limiter binds before it |
| Variable geometry (MiG-23, Su-17/22, EF-111) | **one declared planform per row.** `<metrics>` carries one `wingarea` and the whole polar scales by it. Named in Gaps with the number it costs |
| Cockpit, displays, HOTAS, startup, navigation | absent; all `systems/` no-op defaults. A catalogue cell is never flown by a human |
| Fuel system detail, feed order, CG travel | one tank set from published internal fuel; JSBSim's own starvation model, as for every module |
| Radar modes beyond search and STT | one `ActiveVolume()` per state. No ACM box set, no raid assessment, no TWS — those are module-grade fire controls |
| Exact frequency, PRF, power | the emitter signature's range gate, exactly as for the ground positions (`core.md` §8.3). The band letter is documentation |
| Defensive turrets on a bomber | **named, not built.** A tail turret is `FBGunSystem` with a rearward boresight and it is one hook; the reason it is not in this round is that no campaign's verdict hangs on it yet |
| Air-to-air refuelling (the boom) | **absent, `C5`.** The tanker row exists and cannot give fuel; W2's subject stays unexpressible |
| Radar jamming | **absent, `C13` radar half.** The jammer rows exist and jam **comms only** (`set jam_comm_m`, `C24`) |
| Crew skill, doctrine variation | `set pilot_*` and `set brief_gci` — mission levers, not model |

### 3. Composition and layer placement

```
modules/air/FBAirModule                : FBModule            (rank 8)
  ├─ FBAirRadar      : sensors/FBRadarSystem  (rank 4)   one volume pair per row, from the catalogue
  ├─ FBAirRwr        : sensors/FBRwrSystem    (rank 4)   present only if the row has a receiver
  ├─ FBAirIrst       : sensors/FBIrstSystem   (rank 4)   present only if the row has an IRST
  ├─ FBAirEye        : sensors/FBVisualSystem (rank 4)   every row, verbatim — no derivation
  ├─ FBAirPilot      : pilot/FBPilot          (rank 7)   the tier: two extra states, the rest hooks
  ├─ FBAirMover                               (rank 8)   the kinematic pose integrator, mover rows only
  ├─ systems/FBFlightControl                  (rank 6)   the MiG-29's RAW-airframe path, per-row preset
  ├─ weapons/FBStoresSystem                   (rank 5)   used, not derived
  └─ weapons/FBGunSystem                      (rank 5)   used, not derived
core/FBAircraft.h                             (rank 1)   the catalogue: one row per registry key
sensors/FBNetLinkSystem : FBDatalinkSystem    (rank 4)   the controller feed — §Spec 7, the one new slot
```

**Why here and not elsewhere**, decided rather than assumed:

| Candidate home | Verdict |
|---|---|
| One `modules/<type>/` directory per row | **rejected.** It is `C7` restated as eighteen airframe projects, which is the reason `C7` has been open since the campaigns were written |
| `modules/ground/` beside `FBSiteModule` | **rejected.** That directory means *ground unit*; the class there has no FDM by design and its whole §Spec 1 rests on that |
| `units/` | **rejected** for the same reason the site was: `units/` is rank 3, below `sensors/`, and a class there could never hold a radar |
| **`modules/air/`** | **chosen.** It is resolved by `FBModuleRegistry` from a `.fbm` `module` line, which is what a module *is* mechanically; it is the symmetric counterpart of `modules/ground/`; and `modules/missile/` is the precedent of a module whose slots are sensor/pilot derivations |
| `core/FBAircraft.h` for the catalogue | **chosen**, beside `core/FBSite.h`, `core/FBStore.h` and `core/FBGun.h`, for the same reason those are there |

The documentation mirrors that: `sim/src/modules/air/` → `doc/modules/air/`.

### 4. The flight model — the central decision

**The question:** a SAM position has no flight model and needs none. An aircraft has one. Does a
catalogue cell fly on JSBSim with a generated deck trimmed to published envelope anchors, or on
something simpler?

| Design | What it is | Price |
|---|---|---|
| **A: every row gets a generated JSBSim deck** | one recipe, 18 decks | six rows (`e3` `e2c` `kc135` `tu95` `an26` `mi8`) publish **no** quantity from which a drag polar can be inverted — no Vmax pair, no Ps, no sustained turn. A deck for them would be **invented aerodynamics wearing a citation**, which is exactly what `flight-model-spec.md` §1.1 forbids. And `mi8` is a rotorcraft: the recipe's fixed-wing set does not apply at all |
| **B: every row is a kinematic mover** ([`../ground/cast.md`](../ground/cast.md) design A, extended to the air) | a pose integrated along a declared track | it deletes the exact quantity the campaigns exist to measure. The tree's own duel campaign found that **every long shot is defeated in the notch** ([`../../pilot.md`](../../pilot.md) gap 2.3, `duels.md`) — a defender that cannot beam, cannot bleed and cannot unload loses every BVR engagement by construction, and the campaign then measures our missile instead of the doctrine |
| **C: split on a two-part test** — a deck iff **(the row's own manoeuvre decides an outcome)** ∧ **(its envelope is published)** | nine decks, nine movers | two motion laws in the tree, and the seam between them. Both consequences are nameable and bounded: a mover is never shown to `FBFlightMonitor` (it has no airframe, the rule `../ground/module.md` §Spec 1 already states), and a mover reports weight-on-wheels by construction (§Gaps, collision 1) |

**RECOMMENDED: C.** The two parts of the test are not two conveniences; they are the same fact seen
twice. A row whose manoeuvre decides is a fighter, and a fighter's envelope is published because that is
what fighter data *is*. A row whose manoeuvre does not decide is a large aircraft, and a large
aircraft's polar is not published because nobody ever needed it. **The test never splits a row.**

**Applied**, with the result in [`catalogue.md`](catalogue.md)'s `Motion` column:

| Motion | Rows | Why |
|---|---|---|
| **deck** (generated JSBSim) | `f15c` `mig21` `mig23` `mig25` `mig17` `su7` `su22` `su27` `mirf1` `f5e` | ten fighters and fighter-bombers; each publishes at least two Vmax anchors plus a ceiling |
| **mover** (kinematic) | `e3` `e2c` `kc135` `tu95` `an26` `ef111` `mi8` `ah64` | eight large or rotary aircraft; the intercept geometry is set by their **speed and altitude**, both published, and by nothing they do |

The recipe, its eight anchors, its closed-form inversions, its two declared analogies, what it
deliberately cannot produce and the deviation bands every deck must meet: **[`flight-model-recipe.md`](flight-model-recipe.md).**

**The mover, in full, because it is the cheaper half and must not grow:**

| Property | Value |
|---|---|
| State | position (geodetic), altitude, heading, speed, and nothing else |
| Advance | great-circle leg to the next waypoint at the declared speed, with a **declared turn radius** (from the row's published cruise speed and a `[SET]` bank of 25°, so a track has a curve and an intercept has a geometry) and a **declared climb rate** |
| Kind | `FBUnitKind::Aircraft` — **the kind is not the motion.** [`../ground/cast.md`](../ground/cast.md)'s design A proposed `FBUnitKind::Vehicle` for a ground mover; that value stays a GROUND concept. An airborne mover must remain `Aircraft` or no radar in the tree can see it (`sensors.md` gap 3), which would delete the row's only reason to exist |
| Never | shown to `FBFlightMonitor`; given a control channel; given a trim state; given store carriage aerodynamics |
| Judged by | `FBMissionMonitor` alone: `INTACT` / `DESTROYED` and its objectives |

### 5. The pilot staffelung — five tiers, and a tier is a task set

A bomber that is escorted needs a route and nerve. An Su-27 needs a duel. An early-warning aircraft
needs an orbit and a radar that sees far. Building one pilot for all three would be building the worst
of the three.

**The insight that makes this cheap:** `pilot/FBPilot` is already generic, and every airframe number in
it is a **virtual hook** ([`../../pilot.md`](../../pilot.md) §Spec: *airframe numbers are hooks, pilot
numbers are not*). So a tier is not a class. **A tier is (a) which `set task` values the module accepts,
(b) which sensor slots it powers, and (c) which hooks its own measured deck fills** — the identical
mechanism by which `mig29` accepts `bfm` and refuses `attack` for a stated reason.

| Tier | Accepts `set task` | Sensors powered | Weapon | What it does | Rows |
|---|---|---|---|---|---|
| **T0 — Track** | `route` | eye only | none | flies its waypoints or an `orbit` and reacts to nothing. The cheapest thing that flies | `kc135` `an26` |
| **T1 — Track + one reflex** | `route` | eye + RWR (+ radar, `e3`/`e2c`) | none | T0 plus **one** state: on its own RWR reporting an `AirborneFireControl` threat, turn to the reciprocal of the reported bearing at max speed for `drag_threat_s`, then resume. One trigger, one measured signal, one timer | `e3` `e2c` `tu95` `ef111` `mi8` `ah64` |
| **T2 — Visual fighter** | `route`, `bfm` | eye (+ IRST where the row has one) | gun + rear-aspect IR round | the existing `Bfm` phase with **no radar picture at all**: `FBBfmTrack` built from the eye and the IRST instead of from radar contacts. It fights what it can see | `mig17` `su7` `f5e` `su22` |
| **T3 — GCI interceptor** | `route`, `intercept`, `bfm` | eye + radar + RWR (+ IRST) | SARH or IR + gun | the existing seven-state `FBEngagement` machine, **without** the cooperative half: no PPLI, therefore no sort, no cover deferral, no flight report — exactly the MiG-29's measured position ([`../../formation.md`](../../formation.md) F3). Its picture comes from `set brief_gci` and, once §Spec 7 exists, from a live controller | `mig21` `mig23` `mig25` `mirf1` |
| **T4 — Peer** | everything `f16` accepts, incl. `formation` | full | full, incl. an active-radar round where the row has one | the full machine. Two rows only, and both are opponents the campaigns treat as equals | `f15c` `su27` |

**The admission rule, and it is a gate rather than an intention:**

> A row may be granted a tier only if its own **measured** hooks support that tier's phases. `Bfm` on a
> raw airframe needs four numbers that are measurements and not settings — `PitchStickMax`,
> `AlphaLimitDeg`, `BfmRollPlantA`, `BfmRollPlantKDegS` — because
> [`../../pilot.md`](../../pilot.md)'s close-combat law **inverts the roll plant**, and with another
> aircraft's plant it is not a limiter but an oscillator (measured: the MiG-29 rolls 2.6× harder for the
> same stick, a = 0.819 / K = 201 against the F-16's 0.734 / 78.7). A generated deck has **no** plant
> until step 9 of the recipe measures one. **A row whose plant is unmeasured is T0 or T1 and nothing
> else**, and the boot refuses `set task bfm` with `SET_REJECTED` rather than flying it badly.

**What `FBAirPilot` actually adds:** two states, `Orbit` and `Drag`, for T0/T1. Everything above T1 is
`FBPilot` unchanged with the row's hooks. That is the whole class.

**What no tier gets, deliberately:** a reaction to a surface-to-air launch. That is
[`../ground/module.md`](../ground/module.md) G11, it is open for the F-16 and the MiG-29 too, and
giving it to a catalogue cell first would be building the behaviour where it is least measurable.

### 6. What it perceives

| Detector | Base | Configured how | The price it already pays |
|---|---|---|---|
| `FBAirRadar` | `sensors/FBRadarSystem` | search volume + STT volume from the row; `FrameS` **derived** from the sourced azimuth field by the tree's own relation (§Knowledge 2) | filters `FBUnitKind::Aircraft` (`sensors.md` gap 3) — correct here, and it means a catalogue fighter cannot find a ground target either |
| `FBAirRwr` | `sensors/FBRwrSystem` | **verbatim, not derived** where the row has a generic receiver; the row declares elevation and azimuth channelisation | no range, ever |
| `FBAirIrst` | `sensors/FBIrstSystem` | the aspect law and the reach pair per row (`mig25`'s TP-26Sh: 25 km low against an afterburning target, 50 km+ high) | no range without a laser, no IFF at all, a cloud deck ends the line of sight |
| `FBAirEye` | `sensors/FBVisualSystem` | **verbatim.** Its per-row input is the presented extent, i.e. span and length — **the only sensor input the catalogue can source for every row without exception** | all five currencies of `sensors.md` §9.2, including that it stops working at night |

**The registry reader list does not grow, and that is checkable.** All four bases already hold
`#include "FBUnitRegistry.h"`, and so does `FBDatalinkSystem` (from which §Spec 7's one new slot
derives). A derivation adds none. `tools/verify_layers.py`'s `PERCEPTION_READERS` list stays at **six**
entries and prints its own length; the acceptance criterion is that the printed number is unchanged.

### 7. The early-warning row — the most dangerous line in the catalogue

**It sees 400 km and it tells somebody. That is precisely where a perception boundary falls by
accident.** The ground half solved the same problem
([`../../air-defence-network.md`](../../air-defence-network.md)): *the net moves an antenna, it never
creates a track*, with a payload that has no id field and no team field.

**Does the same solution carry when the node flies? Yes — and here is what it costs, item by item.**

| Piece | Carries unchanged? | Detail |
|---|---|---|
| The node's own detection | **yes** | `FBAirRadar : FBRadarSystem` with a per-row range gate and frame time. Contacts are `FBRadarContact`: `TrackNum` + geometry, no id, no team. Nothing new |
| The payload | **yes, verbatim** | `FBNetReport` — a point reconstructed from the node's own contact, carrying `TgtLookAgeS` (the node's own look age) and no identity. The type already exists and already has no id field |
| The link | **yes, and it gets EASIER** | `FBDatalinkSystem` skips every unit that is not `FBUnitKind::Aircraft`; the ground round had to add `SetCarriesTerminal` to work at all. Here both ends are aircraft, so the built default applies. And the radio horizon `1.23·(√h₁+√h₂)` nm, which was ~0 for two ground antennas and needed a `[SET]` mast, is **finally the right formula**: 9 000 m and 6 000 m give ≈ 460 nm [DERIVED], which is the physically correct answer and needs no setting |
| The member's use of it | **yes, verbatim** | two command-bus entries, one per decision tick, each latency-charged and each rejectable: re-centre the own search volume's `AzCenterDeg` on the bearing to the cued point and `ElCenterDeg` on `atan2(Δalt, planar range)`. Then the member's **own** radar must detect, must firm over `kHitsToFirm` looks, and must pass its own gate. This is the MiG-29's already-built and already-measured GCI chain (**8.0 s** from call to radiating radar, `mig29-intercept.fbm`), made live |
| **The receiver's block** | **NO — this is the price** | `FBDatalinkSystem` publishes **one** `FBState` block, and on an F-16 that block is Link-16 PPLI. A controller feed written into it would overwrite the cooperative picture the formation round depends on. The fix is `air-defence-network.md` §2's **design B**, deferred there with the words *"it becomes right the day an aircraft joins a control net"*: `sensors/FBNetLinkSystem : FBDatalinkSystem` with its own block. **One derivation, no new registry reader, no change to the base's built behaviour** |

**Three further prices, each named with its number rather than discovered later:**

1. **The cue is worth one antenna pointing and nothing more, and its error is derivable.** A 6 rpm
   rotodome has a 10.0 s frame [DERIVED from the sourced rate], so the node's own look age at the moment
   it reports is up to 10 s; the link adds up to one net period. At a 250 m/s target that is **≥ 2.5 km**
   of positional error the cued fighter has to search out with a ±60° × ±10.5° volume. **A cue is
   therefore never a firing solution, and the arithmetic says so rather than a rule.**
2. **The node's track file holds eight.** `FBRadarSystem` reports at most 8 tracks and, when full,
   displaces nothing (`sensors.md` §4.3). An E-3 over a twelve-aircraft campaign publishes the first
   eight **in registry order** — determinism, but not information.

   | Design | Verdict |
   |---|---|
   | **A: accept 8, state the number, book the widening** | **RECOMMENDED for this round.** Every campaign geometry written today has ≤ 8 aircraft a side, so the limit is not yet binding; and a per-row track capacity is a change to a shared sensor that should be paid for by a measurement, not by a plan |
   | B: a per-row `MaxTracks` on `FBRadarSystem` | booked as a gap. It becomes right the first time a campaign measures the limit binding, and the measurement is one column (`radar_tracks` pinned at 8 while contacts remain unreported) |
3. **What the AEW may NOT do, structurally rather than by discipline:** it cannot say *hostile* (no value
   exists — `FBIffReply` is two-valued); it cannot name a unit (`FBNetReport` has no id field); it cannot
   hand a fighter a track (the fighter's own radar has to firm one); and it cannot see through terrain
   any better than anybody else, because nobody can (`C4`). **The identification anti-cheat pair
   (`w5-03`/`o2-08`) therefore survives an AWACS overhead**, and that is the sharpest available proof
   that the boundary held.

### 8. Damage — no new type, no new friend, no new id

**Decision: a catalogue aircraft uses `core/FBSystemHealth` exactly as it stands**, as a member of
`FBSimUnit`, monotone, mutators private, one friend. Six of the fourteen existing ids carry everything a
row has:

| Id | On a catalogue aircraft | Consequence when `Failed` |
|---|---|---|
| `Structure` (2) | the airframe | `CombatEffective()` false; `kill unit` met |
| `Engine` / `Engine2` | the powerplant | `PropulsionOut()`; a twin keeps flying on one with a capped throttle, and both ids already exist |
| `FlightControls` (3) | control runs | `SetControlAuthority` — the existing degraded 0.5 / failed 0.0 |
| `Radar` (6) | the set | block `Invalid` → `Emission()` stops → silent and blind, by the coupling written years before this |
| `Stores` (8) | rails and wiring | flies, never launches |
| `Gun` (12) | the cannon | rows with one |

**A mover row declares one zone and `Structure` alone** — exactly what `target_hard` declares, and for
the same reason: nothing else about it has a behaviour to lose.

### 9. Mission grammar

**Module keys** — one per catalogue row, registered by walking `kAircraftCatalogue`:
`e3` · `e2c` · `f15c` · `mig21` · `mig23` · `mig25` · `mig17` · `su7` · `su22` · `su27` · `mirf1` ·
`f5e` · `kc135` · `tu95` · `an26` · `ef111` · `mi8` · `ah64`.

```
unit magic
  module e3                            # the TYPE — capability comes from the catalogue row
  team friendly
  spawn 44.2000 20.1000 9000 270.0 300 # air start; a mover row refuses 'ground'
  set orbit 40000 600                  # racetrack: 40 km legs, 600 s per circuit
  set drag_threat_s 90                 # the T1 reflex, in seconds

unit bandit1
  module mig21
  team hostile
  spawn 44.9000 20.9000 6000 180.0 450
  set task intercept
  set brief_gci 195 42 6000            # existing key: the controller's BRAA
  store 1 k13
  store 2 k13
  set pilot_shot_rtr 1.2               # existing variant lever
```

**The two new author-facing keys, and no more:**

| Key | Values | Effect | Why it is mission data and not catalogue data |
|---|---|---|---|
| `orbit` | `<legM> <periodS>` | a closed racetrack around the last two `wp` lines; absent = fly the route once and hold the last leg | **[SET]**. A station is a *tasking*, not a property of the airframe — W1's tanker track and W3's AWACS orbit are different geometry for the same aircraft |
| `drag_threat_s` | seconds, `0` = never | T1's reflex duration after its own RWR reports an `AirborneFireControl` threat | **[SET]**. How long a high-value asset runs before it turns back is a commander's rule; and the value **0** is the honest way to declare an asset that was ordered to hold its orbit |

**Everything else reuses a key that exists:** `task` (§Spec 5's tier decides which values are accepted),
`store <station> <type>` (the loadout), `brief_gci`, `iff_xpdr`, `fcr`, `jam_comm_m`, `fuel_pct`,
`gear`, `pilot_*`, `flight` + `brief_sort` (T4 only). An unknown key, or a `task` value the row's tier
refuses, stays a runtime FAIL (`SET_REJECTED`).

### 10. Observable

| Channel | Content |
|---|---|
| Telemetry, own source `air` | `air_tier`, `air_state` (the pilot phase ordinal, incl. `Orbit`/`Drag`), `air_cue_age_s` (the two staleness terms summed, −1 when uncued), `air_drag_s` |
| Reused unchanged | the full `FBFdmTelemetrySource` schema for deck rows; `bfm_*` for T2+, `eng_*` for T3+, `flt_*` for T4, `sms_*`, `rwr_*`, `radar_*`, `vis_*`, `irst_*`, `dmg_*` |
| Mover rows | the same `FBFdmTelemetrySource` columns with `fuelLbs` and `gearLoadFactor` at 0 — the schema already handles a unit without an airframe (`fdm.md` §13) |
| Events | `air ORBIT` / `air DRAG` (bearing, threat kind, seconds) / `air RESUME`, `net REPORT` (point, own look age) on a node, `net CUE` / `net CUE_SUPERSEDED` on a member |

### 11. Acceptance criteria

The round is done when these are **measured**, not argued.

| # | Criterion | Measurement |
|---|---|---|
| 1 | The gate did not widen | `make -C sim verify-layers` prints *6 registry reader(s) inside the perception boundary* |
| 2 | Existing missions are untouched | all committed `telemetry*.csv` byte-identical, `events.log` identical modulo `wallS`/`speedup` |
| 3 | **Every deck row is inside its band** | `make -C sim test-air` measures each row's eight anchors against [`flight-model-recipe.md`](flight-model-recipe.md) §5's bands and prints the deviation per anchor. A row outside stays `ALPHA` and may not answer a campaign question |
| 4 | The tier gate bites | a `.fbm` giving `set task bfm` to a T1 row exits 1 with `SET_REJECTED`; the same line on the same row after its roll plant is measured is accepted |
| 5 | **The cue moves an antenna and never a track** | one E-3, one F-16, one target: with the F-16's `Radar` id failed by `damage`, the run produces `net CUE` lines and **zero** `radar CONTACT` lines. Unfailed, the same geometry acquires |
| 6 | The cue is worth measurable time | the same geometry with and without the node: time to first firm contact, and the difference reported beside the node's own frame time |
| 7 | The identification pair survives an AWACS | `w5-03`'s experiment re-run with an `e3` on station: the interceptor's telemetry stays byte-identical between the `neutral` and `hostile` subject up to its own first discriminating sensor tick |
| 8 | The drag reflex is a sensor, not a clock | two runs identical except the attacker's `set fcr on\|off`: the silent run produces zero `air DRAG` lines |
| 9 | Look-down is a real limit | a `mig23` row against a target 1 500 m below it: zero contacts at the sourced floor, contacts above it |
| 10 | **Attribution** | §Spec 11's `band_deck` and `band_doctrine` computed per deck row on its own decisive campaign geometry, with the control cell (below). Both numbers printed beside every campaign result that uses the row |
| 11 | Determinism | one fingerprint over `--threads 1/2/4` × 3 repeats |

#### The attribution test — *did he lose as a MiG-21, or as a coarse deck?*

This is the round's real acceptance, because a catalogue opponent that decides a campaign **by being
badly modelled** turns every campaign result into a measurement of our own error. Three instruments,
all of them already implemented:

| # | Instrument | What it produces | Reads |
|---|---|---|---|
| **1** | **The anchor residual**, measured before any campaign runs | per row, the deviation of each of its eight anchors from the published number | a row outside its band is not admitted. This is the MiG-29's own promotion gate re-used, and it catches gross error only |
| **2** | **The two bands**, on the tournament instrument [`../../duels.md`](../../duels.md) already runs | `band_deck` = outcome spread when the row's declared-ignorance deck values are perturbed (`CD0` ±10 %, `e` ±10 %, `Ixx` ±10 %, thrust ±5 %) with doctrine held fixed · `band_doctrine` = outcome spread when the mission's doctrine levers are swept with the deck held fixed (the O1 yardstick, `band = max_v O(v) − min_v O(v)`) | **the verdict rule: the row is admissible as an opponent iff `band_deck ≤ 0.25 × band_doctrine`.** If perturbing what we admit we do not know moves the result as far as changing the doctrine does, the campaign is measuring the model. The file then prints both bands **instead of** a result |
| **3** | **The control cell** — the falsification | the same geometry flown with the row's deck **replaced by the pinned F-16 deck or the measured MiG-29 deck**, keeping the row's sensors, weapons and pilot tier | if the outcome does not move, the deck was not what decided and instrument 2 must have said so. **A disagreement between 2 and 3 is a defect of the instrument, not a finding** |

**The threshold `0.25` is `[SET]`**, and the reason is stated rather than implied: no source gives one,
a quarter keeps the deck's contribution below the doctrine's even when both extremes land the same way,
and **both bands are reported next to every result**, so a reader who disagrees with the number can
re-decide without re-running anything.

**Precedent that the instrument works:** the same measurement on one MiG-29 over one geometry produced a
doctrine band of **978.7 points** and turned six losses into none ([`../../duels.md`](../../duels.md)) —
so a `band_deck` of, say, 200 points against that would already fail the rule, and the number is not
academic.

---

## State

**Nothing is built.** No `sim/src/modules/air/`, no `core/FBAircraft.h`, no generated deck, no `.fbm`
mission that flies a catalogue row. Every campaign that needs one of these types still substitutes an
`f16` or a `mig29` and declares the substitution in its own header.

What **already exists and is consumed unchanged** by this contract, which is why it is a bounded round
rather than eighteen airframe projects:

| Piece | Where | Used for |
|---|---|---|
| the module registry walking a catalogue | `FBStoreModuleRegistration`, `FBSiteModuleRegistration` | one class, N registry names |
| an optional airframe on a unit | `units/FBSimUnit` (`std::unique_ptr<FBFdm>` may be null) | every mover row, with **no** change |
| the generated-deck precedent | `sim/assets/aircraft/{aim120,aim9,r73,r27r,v750,v601,3m9,9m33,strela2,igla,agm88,…}` — **seventeen** FlightBox-own models from three recipes | the proof that "N models from one recipe" is a built form and not a hope |
| the anchor-measured deck precedent | `sim/assets/aircraft/mig29` + `make test-mig29`, 22 anchors, four missed with named causes | the recipe's whole method, and the source of its deviation bands |
| the generic pilot with virtual airframe hooks | `pilot/FBPilot` + `FBF16Pilot` + `FBMig29Pilot` | four of the five tiers, with no new pilot class |
| the raw-airframe close-combat path | `systems/FBFlightControl`'s `Manual` branch + the four screws of `71cb99f` | every deck row's `Bfm`, and the reason the tier admission rule exists |
| radar volume/frame/firming, RWR, IRST, the eye | `sensors/` | every row's perception, by configuration |
| a report that is a point with an age and no identity | `FBNetReport` + `FBDatalinkSystem` + `FBMissileUplink`'s own precedent | the early-warning cue, §Spec 7 |
| weapon-as-unit, three seeker kinds, chaff and flare | `weapons.md`, `modules/missile/` | every catalogue round; **no new seeker kind is asked for** |
| damage register and the failure→`Invalid` coupling | `core/` | all of §Spec 8, free |

---

## Gaps

### The honest headline

**The catalogue makes the campaigns' opponents *present*; it does not make them *equal*.** A generated
deck carries the linear aerodynamic range and stops at the α limiter. Every row's post-stall behaviour,
every row's high-α departure, and every swing-wing row's second planform are absent. The consequence is
directional and must be repeated wherever a result is published: **a catalogue fighter is at its most
faithful in the BVR arena and at its least faithful in a slow-speed knife fight** — which is the same
scale [`../mig29/module.md`](../mig29/module.md) already declares for the MiG-29, applied one level
down and one step coarser.

### Named, quantified, refused for a reason

| # | Gap | Detail |
|---|---|---|
| A1 | **No post-stall or departure aerodynamics** | the recipe has no per-row high-α analogy and will not invent one; the MiG-29 needed the whole NASA HARV database (`flight-model-spec.md` §6.4) for ten post-linear curves. Every deck row's α limiter is therefore not a *modelled* boundary but the **edge of the model**, and the file says so instead of letting a pilot fly past it |
| A2 | **Variable geometry is one planform** | `mig23` (37.35 m² spread / 34.16 m² swept), `su22` (38.5 / 34.5) and `ef111` (63.0 ft / 32.0 ft span) all sweep. `<metrics>` carries one `wingarea` and the entire polar is non-dimensionalised by it, so a row must declare **which** planform it is. Taking the spread wing understates supersonic Vmax; taking the swept wing overstates the turn. Named per row in [`catalogue.md`](catalogue.md), and the second deck is a separate, cheap round |
| A3 | **No RCS for any row** | the tree has exactly two measured-against-each-other cross-sections (F-16 1.2 m², MiG-29 4.0 m²). Until a row has one, it is detected at the F-16's reference range — which **understates** the big movers (`e3`, `kc135`, `tu95`) badly and **overstates** the small fighters (`f5e`, `mig21`). The σ^¼ law makes a wrong value cheap to spot, which is exactly why inventing sixteen is worse than declaring none (`../ground/cast.md`'s own argument, kept) |
| A4 | **The tanker cannot give fuel** (`C5`) | the row exists, the boom does not, and W2's defining constraint stays unexpressible. Naming the row does not move `C5` one metre |
| A5 | **The jammer rows jam comms only** (`C13` radar half) | `ef111` and O1's Boeing 707 both reduce to `set jam_comm_m` (`C24`, built). Bekaa's decisive mechanism is still the one thing in the campaign set with no substitute |
| A6 | **No defensive turret** | `tu95`'s tail GSh-23 and a Tu-16's seven AM-23 make a bomber intercept a two-way problem. A turret is `FBGunSystem` with a rearward mount az/el and one hook; it is not in this round because no campaign's verdict hangs on it, and it would be the second thing to add |
| A7 | **A catalogue fighter cannot see the ground either** | `FBRadarSystem` filters `Aircraft` (`C25`). So `su7`/`su22`/`mig17` — three rows whose whole reason is ground attack in O3 — attack a briefed steerpoint, exactly as the F-16 does today |
| A8 | **T3 has no cooperative half and that is a doctrine statement, not a gap** | no PPLI, therefore no sort, no cover deferral. It is the MiG-29's own measured position (distinct targets per engaged member **0.750** contract against **0.962** cooperative) and it is correct for every eastern row here. It is *listed* so that a campaign does not read it as an oversight |
| A9 | **The E-3's own track file holds eight** | §Spec 7, design A accepted with the number stated. The first campaign that measures the limit binding earns the per-row capacity |
| A10 | **Empty weight is missing for two rows** | `mig21` and `mig23` publish gross and maximum mass and leave empty blank. The mass-closure probe that validated the MiG-29 deck to 0.002 % **cannot be run** for those two, so they carry one fewer consistency check than the rest and stay `ALPHA` on that account alone until a source supplies it |
| A11 | **Two rows have no campaign anchor** | `su27` is named by **no** campaign file, and `f5e` by none either (W1's aggressors are F-16s emulating a Fulcrum, by its own anchor). Both are in the catalogue because the round asked for them; both are therefore **capability without a question**, and the honest form of that is this line |
| A12 | **No air-to-air refuelling behaviour, no rejoin, no package timing** | `C5`, `C15` and [`../../pilot.md`](../../pilot.md) 2.10 are all upstream of any tier here |

### Collisions with the existing tree, found while writing this

Three, and none of them is a defect of the tree — all three are places where machinery written for a
flown airframe or a ground position meets a thing that is neither.

| # | Collision | Detail | Resolution proposed |
|---|---|---|---|
| 1 | **An airborne mover reports weight on wheels** | a unit without an `FBFdm` yields `AnyWow = true` by construction (`runtime.md` §3, `BuildMissionSample`) — the exact fact `../ground/module.md` had to hook around for the launch interlock. A KC-135 at 25 000 ft that reports WOW is wrong for every consumer of that bit, and the release interlock is only the first | the mover publishes its own `Airborne` state into the sample instead of inheriting the no-airframe default. One field, set where the pose is set, and the ground position's `AnyWow = true` stays correct because a launcher really is on the ground |
| 2 | **The receiving fighter's Datalink block is already occupied** | `FBDatalinkSystem` publishes ONE `FBState` block; on an F-16 it carries Link-16 PPLI, which [`../../formation.md`](../../formation.md)'s station keeping, sort and cover rule all read. A controller feed written into it would silently overwrite the flight picture | `sensors/FBNetLinkSystem : FBDatalinkSystem` with its own block — [`../../air-defence-network.md`](../../air-defence-network.md) §2 **design B**, deferred there in as many words and **made due by this round**. No new registry reader (a derivation adds no include) |
| 3 | **`FBRadarSystem`'s track file is a fixed 8 and displaces nothing** | fine for a fighter, wrong in kind for a 400 km early-warning set: the eight it reports are the first eight **in registry order**, so the node's picture is a function of the mission's declaration order | accepted at 8 for this round with the number stated (§Spec 7 design A); a per-row capacity is booked and is earned by a measurement, not by a plan |

A **fourth** thing is worth naming beside them because it is the reason the tier gate exists rather than
a collision: [`../../pilot.md`](../../pilot.md)'s close-combat law **inverts an identified roll plant**,
and the two plant numbers are per-airframe measurements. A generated deck has none until it is flown, so
`Bfm` is not available to a row on the day its deck is generated — only on the day step 9 of the recipe
measures it.

---

## Knowledge

### 1. Why the two-part flight-model test never splits a row

The test is *(manoeuvre decides)* ∧ *(envelope published)*. It looks like two independent conditions
and is one:

| Row class | Manoeuvre decides? | Envelope published? | Why the two agree |
|---|---|---|---|
| fighter / fighter-bomber | **yes** — it is what the type is for | **yes** — Vmax at two altitudes, ceiling, climb, g, T/W, W/S are exactly what is published about fighters, because that is what fighters were sold and compared on | the published set exists *because* the manoeuvre matters |
| tanker / AEW / bomber / transport / helicopter | **no** — the intercept geometry is its speed and altitude, both published | **no polar, no Vmax pair, no Ps** — nobody ever needed a drag polar for a KC-135 in a fight | the data is absent *because* the manoeuvre does not matter |

That is why design C is not a compromise between A and B: it is the observation that the two designs
were each right about one half of the catalogue.

### 2. Where the frame times come from — the tree's own relation, not eighteen settings

**No published source gives a scan period for any airborne fire-control radar in this catalogue.** Only
one antenna rate is sourced at all: the E-3's rotodome at **6 rpm** → one revolution per **10.0 s**
[DERIVED].

The tree already declares the model that fills the hole. `sensors.md` §4.2, on the F-16's mode table:
*"the frame times follow the volumes: a mechanically scanning radar takes longer for a wider pattern.*
***This relation, not the absolute seconds, is the model.***" The F-16's CRM sweeps ±60° in **4.0 s**.
So, for every catalogue set:

```
FrameS = 4.0 s × (AzHalfDeg / 60°)
```

| Row | Sourced azimuth field | `FrameS` |
|---|---|---|
| `mig21` (RP-22) | ±30° (from the sourced 60° horizontal field) | **2.0 s** [DERIVED] |
| `mig23` (N003E) | ±30° | **2.0 s** [DERIVED] |
| `su27` (N001) | ±60° | **4.0 s** [DERIVED] |
| every row with **no** sourced field | **±60°, the tree's own fighter default** (F-16 CRM and N019 alike) | **4.0 s**, and the row says the field is `[TODO]` |
| `e3` | ±180° (rotodome) | **10.0 s** [DERIVED from 6 rpm] — the one row where the relation is *checked* against a source |

**Two consequences fall out without anything being tuned.** A MiG-21 sweeps its narrow field twice as
fast as an F-16 sweeps its wide one, so it firms a track it *can* see in half the time — and it can see
a quarter as much sky. And the E-3's 10.0 s frame is what makes its 400 km cue **stale by construction**
(§Spec 7), which is the mechanism that keeps a 400 km sensor from becoming a firing solution.

### 3. Why the eye is the catalogue's best-sourced sensor

`FBVisualSystem` resolves on **presented extent over range** with Johnson N50 multiples (`sensors.md`
§9, built and measured: a beam-on F-16 is detected at 3 784 m, recognised at ~950 m, identified at
~590 m). Its per-row input is the target's span and length.

**Span and length are published for all eighteen rows, at [T4] or better, without a single `[TODO]`.**
No other sensor input in this catalogue is complete: RCS is absent everywhere (A3), radar ranges are
`[TODO]` on four rows, IRST reach on all but one, RWR type on half.

The consequence is worth stating because it lands on the two campaigns whose task has no weapon in it:
**W5's and O2's identification pass runs entirely on the one channel whose inputs the catalogue can
fully source.** An An-26 (span 29.3 m) is recognised at roughly twice the range of a MiG-21
(span 7.15 m) by the ratio of their extents alone [DERIVED from the linear resolution law], and nothing
about that number was set.

### 4. The Boeing 707 that costs nothing

[`../../campaigns/o1-bekaa-1982.md`](../../campaigns/o1-bekaa-1982.md) names a **Boeing 707 ECM
aircraft** as *"the decisive mechanism, and the one that is furthest out of reach"*. In this catalogue it
is **not a row**: the 707 and the KC-135 are the same airframe family, and both reduce, in the tree as
it stands, to *a large subsonic jet on a track with a published `jam_comm_m` ring and no weapon*. So
O1's jammer is:

```
unit raven
  module kc135
  team friendly
  spawn …
  set jam_comm_m 120000        # C24, built
```

and it costs **zero new rows and zero new mechanism**. What it does *not* buy is the radar-jamming half
(`C13`), which is the part the anchor actually turns on — so the substitution is honest about being
one third of the historical aircraft.

`ef111` stays a row of its own because W3 names it six times and its **flight profile** is different in
kind (supersonic, swing-wing, escorting a strike at strike speed rather than orbiting), which is
precisely the quantity a mover row carries.

### 5. Where the numbers live

| Kind of number | Home | Rule |
|---|---|---|
| radars, envelopes, armament, performance anchors, per row | [`catalogue.md`](catalogue.md) | every row cites a source and a tier; disagreeing sources carry **both** values |
| the deck recipe, its inversions, its analogies, its bands | [`flight-model-recipe.md`](flight-model-recipe.md) | every step names what it consumes and what it cannot produce |
| the class, the layer, the tiers, the grammar, the boundary argument | this file | every setting `[SET]` with one sentence of reason |
| a mission's own doctrine | the `.fbm` | two new keys, §Spec 9 |

---

## Related

| Place | Relationship |
|---|---|
| [`catalogue.md`](catalogue.md) | the eighteen rows with their sources — the data this class is the engine for |
| [`flight-model-recipe.md`](flight-model-recipe.md) | the answer to the file's central question, in full |
| [`../ground/module.md`](../ground/module.md) | the sibling contract, and the template this one follows: one parametric class, N catalogue rows, no new registry reader, no new health id |
| [`../ground/cast.md`](../ground/cast.md) | where the air rows lived as a placeholder; its dividing question is kept and sharpened here |
| [`../mig29/flight-model-spec.md`](../mig29/flight-model-spec.md) | the method the recipe generalises — three columns, anchors as acceptance, `[SET]` as declared ignorance |
| [`../../air-defence-network.md`](../../air-defence-network.md) | *the net moves an antenna, it never creates a track* — §Spec 7 is that rule with the node airborne, and it makes that file's §2 design B due |
| [`../../pilot.md`](../../pilot.md) | the phase machine every tier above T1 is, and the roll-plant identification the tier gate hangs on |
| [`../../sensors.md`](../../sensors.md) | the perception boundary, the six readers, the frame/volume relation §Knowledge 2 rests on |
| [`../../weapons.md`](../../weapons.md) | weapon-as-unit and the three seeker kinds — every catalogue round is one of them, and **no new kind is asked for** |
| [`../../duels.md`](../../duels.md) | the tournament instrument the attribution test of §Spec 11 runs on |
| [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md) | `C7` and the nine campaigns it degrades |
