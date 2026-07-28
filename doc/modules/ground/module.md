# FlightBox — the ground threat unit (`modules/ground/`)

**Subject:** `C1` — **one ground unit that emits and shoots.** The contract for a data-driven unit class
with N catalogue rows: what it is, where it sits, what it may see, what it radiates, what it launches,
what killing it does.

**Status: SPECIFIED, NOT BUILT.** No line of `sim/` was touched to write this file. It is step 2 of the
owner goal and it exists because [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md) names `C1` the
single most blocking gap in the set: it blocks six campaigns, and the top four rows of the aggregated
cast table (ground radar/GCI as an emitter · AAA and MANPADS · fixed SAM · mobile SAM) are **one system
four times over**.

**Delimitation, in the tree's own terms:** this file documents the **CODE that will be built**, the same
role [`../f16/module.md`](../f16/module.md) plays for the F-16. The real hardware — bands, envelopes,
reaction times, with source and confidence tier — is [`catalogue.md`](catalogue.md), in the schema of
[`../mig29/`](../mig29/INDEX.md). The rest of the campaign cast, at a deliberately lower resolution, is
[`cast.md`](cast.md).

**Home migration:** the `C1` gap entry previously lived as a bounded placeholder in
[`../../weapons.md`](../../weapons.md) §Gaps. Its five open questions are answered in §Knowledge 1 of
this file; that entry now points here.

---

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| A ground threat is **one class with N catalogue rows**, never N classes | `modules/ground/FBSiteModule` + `core/FBSite.h`'s `kSiteCatalogue`; the registration file walks the catalogue exactly as `FBStoreModuleRegistration` walks `kStoreCatalogue`. A second C++ class in this directory is a defect, not a precedent |
| It is a `FBModule`, and it is **not a module in the documentation sense** | it has no FDM, no avionics bus worth the name, no pilot phase machine and no reference base directory. The line is drawn in §Spec 1 and it is the file's central decision |
| It sees through a sensor slot **or not at all**, and the registry reader list does **not** grow | **BUILT, measured: 6.** The two radars are configured instances of one `FBSiteRadar : FBRadarSystem`; the eye is `FBVisualSystem` and the passive receiver `FBRwrSystem` **verbatim, not derived** — an empty derivation would be noise (`CLAUDE.md`, "no empty derivation for tuning"). `tools/verify_layers.py` now prints the list's length itself |
| It radiates through the published signature, and **search and track are audible at the same time** | `FBUnitSignature` carries **two** emitter beams; a battery is two antennas by construction. The RWR reads the array with the same two geometry tests it uses today |
| A launched round is a **unit**, by the rule that already governs every weapon | `weapons.md` §1 unchanged: own `FBFdm` on its own pinned deck, own module from the same registry, own telemetry file, the same two judges |
| Command guidance is **one new seeker kind**, not a new architecture | `FBSeekerKind::CommandGuided` — no detector powered, guidance = uplink for the whole flight, uplink lost = inertial and it never comes back. Measured on both branches, as `SemiActiveRadar` was |
| Damage is the **existing** register, with **no new type, no new friend and no new system id** | `core/FBSystemHealth` unchanged and untouched; the site declares `Structure`/`Radar`/`FireControl`/`Stores`/`Gun` in an `FBDamageLayout` like every other module. Kill the radar → the block goes `Invalid` → the site stops radiating, by §8's coupling and not by a line written for this |
| Doctrine is **mission data**, capability is **catalogue data** | `set emcon`, `set alert`, `set rounds`, `set engage_max_m`, `set reaction_s`, `set scoot_s` — six AUTHOR-facing keys, each `[SET]` with a reason. Everything else about the site is said by its module name. **Seven further keys are RUNNER-GENERATED** from a mission's `net` block (`net_link`, `net_period_s`, `net_hold`, `net_sector`, `net_autonomy`, and one of `net_control`/`net_wcs`) and are never author-written — membership is a property of the NET, not of the position ([`../../air-defence-network.md`](../../air-defence-network.md) §8) |
| Emission policy is enforced by a **sensor**, never by knowledge | `emcon hold` means the set is unpowered until the site's own **passive** receiver hears an AIRBORNE emitter (`FBRwrThreat::Kind` ∈ {`AirborneFireControl`, `MissileSeeker`}) — **corrected 2026-07-28**: the build tested `ThreatCount > 0`, so a battery scrambled because the friendly early-warning set next door was sweeping. There is now such a set next door. **The SECOND legal wake-up is a net cue** — also a received signal with a range, an age and a sender that can be killed. A timer or a range trigger would be the site knowing something it never measured |
| Nothing about a site is random | no die anywhere: acquisition is the radar's own volume/frame law, deception is the existing chaff/flare inequalities, damage is geometry. Determinism over `--threads 1/2/4` × 3 repeats, one fingerprint |

### 1. The line between a module and a unit — and what both share

A jet module and a ground site are the same *kind of object* to the runner and deliberately different
*depths of model*. Five sentences:

1. A **module** is a flown airframe: it owns a JSBSim deck, a full avionics bus, a pilot phase machine
   and one C++ class per type, and it earns a reference-base directory of its own.
2. A **unit** is everything else in the world that can be perceived, shot at, and shoot back: one
   parametric class, N catalogue rows, and a per-row entry in one catalogue file.
3. They share the whole of `units/FBSimUnit` — identity, team, the published pose and signature, the
   health register, the damage model, the roster, the telemetry file, the mission judge, the `.fbm`
   `unit` block and the `FBModuleRegistry` key — because that machinery is about being *in the world*,
   not about *flying*.
4. They differ in exactly the four things a jet has because somebody sits in it: flight dynamics, an
   avionics bus with 18 blocks, a phase machine that decides what to do next hour, and a manual of its
   own.
5. The test for which side a thing is on is therefore not "how important is it" but **"does anybody fly
   it"** — and a SAM battery has no FDM, needs none, and would gain nothing measurable from one.

| Property | Jet module (`f16`, `mig29`) | Ground threat unit (`sa2` … `p18`) |
|---|---|---|
| JSBSim deck | full aero/mass/propulsion, pinned, delta-gated | **none.** `FdmModelName()` empty → `FBSimUnit` builds no `FBFdm` (`runtime.md` §3) |
| C++ classes | ~20 per airframe | **one**, plus three thin sensor derivations and one fire control |
| Documentation | a reference base directory per type | **one catalogue row** in [`catalogue.md`](catalogue.md) |
| Avionics bus | 18 blocks, three-state validity | the bus exists (every unit has one) and carries what its own telemetry needs |
| Pilot | `pilot/FBPilot` phase machine: takeoff, route, attack, land, BFM, intercept | a **five-state engagement machine**, §Spec 5 |
| Flight controls / autopilot / air data / nav / displays / HOTAS | all present | all absent, all the `systems/` no-op defaults |
| Damage layout | zones × up to 14 system ids | **one or two zones × up to five ids** |
| Health register | `core/FBSystemHealth`, unchanged | `core/FBSystemHealth`, **unchanged and identical** |
| Registry access | through the sensor slots | through the sensor slots — the same six files |
| Verdict | flight verdict + mission verdict | `INTACT`/`DESTROYED` + mission verdict, never a flight verdict (no airframe → never shown to `FBFlightMonitor`) |

**Consequence for the tree:** `modules/ground/` grows from one class to two. `FBGroundModule`
(`target_soft`/`target_hard`, `Run()` empty, `ApplySetup` always false) stays exactly as it is — a site is
not a target with extras, it is the second class in the directory, and the inert target keeps its
property of being the cheapest possible unit.

### 2. Grob where it does not decide, treu where it does

The owner's rule, made into a table. Left column: the parameter is modelled and a mission result depends
on it. Right column: the parameter is documentation only, or absent, and the file says so.

| **Modelled, because it decides** | Why it decides |
|---|---|
| Search detection range, per target RCS | sets where the RWR's first symbol appears and therefore the whole ingress altitude/route argument |
| Search **frame time** (antenna revolution) | the difference between "he swept me" and "he has me" is time, and the RWR's mode symbol is what the pilot reads |
| Track/lock range and the search→track→launch **time sequence** | half the tactics; `kHoldS`/`kNewThreatS` already give the receiver the vocabulary |
| Weapon envelope: `Rmin`, `Rmax`, `AltMin`, `AltMax` | the low-altitude escape and the stand-off distance are exactly these four numbers |
| Reaction time (first firm track → launch) | decides whether a pop-up attack gets a shot fired at it at all |
| Whether guidance **binds** the site to the target, and for how long | decides whether one battery can engage a package or one aircraft at a time |
| Number of simultaneous engagement channels | the saturation arithmetic every SEAD package rests on |
| Rounds available and the interval between launches | decides whether a second pass is cheap |
| The emitter **kind** and **mode** (what the RWR hears) | the entire warning picture |
| Which countermeasure has a channel (chaff/flares/none) | decides whether a defensive reaction exists at all |

| **Coarse or absent, because it does not decide** | What is done instead |
|---|---|
| Transmit power, PRF, pulse width, exact frequency in MHz | `FBEmitterSignature` carries a **range gate** as its one power measure (`core.md` §8.3). The band letter is documentation in [`catalogue.md`](catalogue.md) and reaches no code |
| Antenna mechanics, sidelobes, monopulse vs conical scan | one beam window (centre + half angles), body-referenced, as for every airborne set |
| Battery composition (how many TELs, how many vans, crew) | **one unit per site.** A `.fbm` that wants two launchers declares two units |
| Emplacement/march times, reload from a transloader | one `ReloadS` per catalogue row; below it the site simply has no round |
| Missile aerodynamic individuality | the parametric slender-body deck of §Spec 4, sized per round from published diameter, mass and terminal speed — the recipe stage 2c already used for AIM-9/R-73/R-27R |
| Warhead internals, fragment count, directivity | `WarheadKg` into the existing isotropic model (`weapons.md` §6.1), whose two settings are already declared |
| Crew skill, morale, doctrine variation | `set reaction_s` — a mission lever, not a model |
| Guidance accuracy degradation with range | **refused.** It would need a measurement error or an invented bias; nothing in the tree has either (`sensors.md` gap 4). Named in Gaps |
| Ground clutter / low-altitude radar horizon | **refused today.** `AltMin` in the *weapon* envelope carries the low-altitude escape; the detection side stays optimistic until `C4` (terrain masking). Named in Gaps as the one place the model is materially too generous |

### 3. Composition and layer placement

```
modules/ground/FBSiteModule            : FBModule            (rank 8)
  ├─ FBSiteSearch    : sensors/FBRadarSystem   (rank 4)   acquisition volume, own frame time
  ├─ FBSiteTrack     : sensors/FBRadarSystem   (rank 4)   STT volume + illumination, the ONE channel
  ├─ FBSiteOptics    : sensors/FBVisualSystem  (rank 4)   the optical/MANPADS row's only detector
  ├─ FBSiteEsm       : sensors/FBRwrSystem     (rank 4)   the EMCON cue, passive, no range
  ├─ FBSiteFireControl : pilot/FBPilot         (rank 7)   the five-state engagement machine
  ├─ weapons/FBStoresSystem                    (rank 5)   the rails — used, not derived
  └─ weapons/FBGunSystem                       (rank 5)   the barrels — used, not derived
core/FBSite.h                                  (rank 1)   the catalogue: one row per registry key
```

**Why here and not elsewhere**, decided rather than assumed:

| Candidate home | Verdict |
|---|---|
| `units/` (a new `FBUnit` derivation beside `FBSimUnit`) | **rejected.** `units/` is rank 3, *below* `sensors/`. A class there could never hold a radar, and giving it one would invert the include order the whole anti-cheat structure rests on |
| `systems/` (a generic "ground threat slot") | **rejected.** `systems/` holds airframe-agnostic slots that a module composes; this thing IS the module |
| `weapons/` | **rejected.** `weapons/` holds the release paths a module owns. A site owns one; it is not one |
| **`modules/ground/`** | **chosen.** The class is resolved by `FBModuleRegistry` from a `.fbm` `module` line, which is what a module *is* mechanically; it is a `Ground` unit, which is what this directory already means; and `modules/missile/` is the exact precedent of a module whose slots are three sensor/pilot derivations |
| `core/FBSite.h` for the catalogue | **chosen**, beside `core/FBStore.h` and `core/FBGun.h`, for the same reason those are there: it is a value table read by several layers |

The documentation mirrors that: `sim/src/modules/ground/` → `doc/modules/ground/`.

### 4. What it launches

**No new weapon architecture.** `weapons.md` §1's rule holds unchanged: a fired round is a unit with its
own deck. What the catalogue names is a `FBStoreSpec` key, and the seeker kind decides what the round is
(`weapons.md` §10.2).

| Guidance in the real system | FlightBox seeker kind | New? | Behaviour |
|---|---|---|---|
| Radio command / CLOS (SA-2, SA-3, SA-8) | `CommandGuided` | **yes, one enum value** | no detector powered at all; `UpdateTarget` takes the uplink and nothing else; `TERMINAL` is unreachable; uplink lost → `INERTIAL` for good |
| Command + terminal SARH (SA-6) | `SemiActiveRadar` | no | built in stage 2c, unchanged: alive while illuminated, dead for good when not, `FBSeekerHandoverS = -1` |
| Passive infrared (MANPADS) | `Infrared` | no | built in stage 2c, unchanged: angle-only pure PN, flare-deceivable, free at launch |
| Gun (AAA) | — | no | `weapons/FBGunSystem` + a `core/FBGun.h` row; bundles resolved by the runner's existing `ResolveGunHit` against `Aircraft` |

**`CommandGuided` in full**, because it is the only new value: `ActivationRangeM = 0` and
`FBSeekerHandoverS = -1` (the shooter is bound to impact, like SARH), the seeker slot is never powered,
and `FBMissileGuidance::UpdateTarget`'s existing strict priority (own seeker > uplink > last known)
degenerates to its middle branch. `msl_range` is the uplink's range, `msl_seeker` is 0 for the whole
flight, and the round therefore *cannot* be chaff-decoyed — **the cloud seduces the SITE's tracking
radar instead**, which is where a command-guided engagement is really broken. That is a property of the
existing `SelectDecoy`, not a new mechanism.

**The launch initial condition** is the one genuinely new piece of weapon physics, and it is small. An
air-launched round separates at 250 m/s with full aerodynamic authority; a SAM leaves the rail at zero
airspeed. Two catalogue fields:

| Field | Meaning | Provenance |
|---|---|---|
| `LaunchElevDeg` | rail elevation at launch, body pitch of the spawned round | per row; where a source is silent, `[SET]` with the row's own reason |
| `GatherS` | guidance inhibited for this long after launch — the round flies the rail direction on thrust alone | `[SET]` per row. Every command system has this phase (the "gathering" of the round into the beam); its duration is not published, and a round steering at 20 m/s of airspeed would be a fin authority nobody claims |

Aim azimuth is the fire control's, not a field: the rail points at the current track bearing.

### 5. The engagement machine

`FBSiteFireControl`, a `pilot/FBPilot` derivation — the same relationship `modules/missile`'s
`FBMissileGuidance` has to the same base. It reads `FBState` (its own radar's contacts, its own ESM's
threats) and commands the SMS/gun **through the command bus**, exactly as a pilot does; it never touches
the registry and never spawns anything (`weapons.md` §2.3 unchanged).

| State | Entered when | Radiates | Leaves when |
|---|---|---|---|
| `Cold` | boot with `set alert cold`, or `Radar` failed | nothing | `WarmupS` elapsed after the cue |
| `Dark` | `set emcon hold` and no cue | nothing | the ESM hears an AIRBORNE emitter (bearing only), **or** the declared control node's report puts a point inside this member's sector — both are received signals with a sender that can be killed |
| `Search` | powered and cued | **beam 0**: the search volume, `Mode::Search` | a firm track inside `Rmax` × the envelope test |
| `Track` | a firm track designated into the STT volume | beam 0 Search + **beam 1**: pencil, `Mode::Track` | `ReactionS` elapsed → `Engage`; or the track is lost |
| `Engage` | a round released | beam 1 becomes `Mode::Guidance` while the uplink/illumination is active | impact, or all rounds gone, or the track is lost |
| `Scoot` | `set scoot_s > 0` and a launch happened | nothing | `ScootS` elapsed → `Dark` |

Five states plus `Scoot`. **The three RWR symbols map onto the three radiating states one to one** —
which is the whole reason the machine has exactly these states and not four or seven: `Search` → plain
symbol, `Track` → boxed, `Engage` → flashing plus the LAUNCH light. The time a pilot has between them is
`ReactionS`, and that is the number the campaigns argue about.

**The channel is one.** `Track` and `Engage` occupy the site's single fire-control channel for their whole
duration; a second contact is not engaged until the first engagement ends. Rows whose real system has two
channels declare `Channels = 2` and the machine holds two instances of the pair. Nothing else in the class
is per-target.

### 6. What it radiates — and the one core change

A battery is **two antennas radiating at once**: the acquisition set keeps sweeping while the fire
control stares. `FBUnitSignature` carries one `FBEmitterSignature` today.

| Design | Consequence |
|---|---|
| **A: two beams** — `FBEmitterSignature Radar[kMaxEmitterBeams = 2]`, index 0 = search, index 1 = track/guidance | one diff in the barrier, one in `FBRwrSystem`'s loop, one in `FBMig29Rwr`. Every aircraft writes index 0 only, so every existing mission is byte-identical by construction (a loop over one non-`None` entry does the same work as the scalar read) |
| B: one beam, precedence track > search | **rejected.** The moment the site locks anybody, every *other* aircraft stops hearing the search radar — the search→track transition would exist only for the aircraft being tracked, deleting exactly the signal `C1` exists to produce |
| C: two units per battery (a radar unit and a launcher unit) | **rejected.** It makes "the launcher lost its acquisition radar" a cross-unit coupling, and the only legal cross-unit channel is a sensor — so the launcher would have to *hear* its own search radar. Cute, and a lie |

**RECOMMENDED: A.** It is the one core change this contract asks for.

Second, `FBEmitterKind` gains two values, appended (`core.md` §8.3's rule: `Kind` is *what* radiates, as
the emitter knows it — what makes it a threat is what sits behind the antenna):

```
FBEmitterKind { Unknown = 0, AirborneFireControl, MissileSeeker, SurfaceEarlyWarning, SurfaceFireControl }
```

Behind a `SurfaceEarlyWarning` antenna sits a telephone; behind a `SurfaceFireControl` antenna sits a
launcher. That distinction is the **first** thing `FBMig29Rwr::PriorityRank`'s hook could read — the
device defect that `sensors.md` gap 25 says "cannot bite until the first surface emitter exists". This
file does **not** wire it: the real SPO-15 rule keys on threat letters FlightBox deliberately does not
model, and inventing the mapping is that override's own decision. What this file guarantees is that the
field exists to key on.

**No other field is needed.** The geometry is identical: a ground mount publishes roll = pitch = 0 and a
yaw, the beam window is the same centre/half-angle pair, and `FBRwrSystem::BeamCovers` runs unchanged —
`sensors.md` §5.2's promise that "the two sides of an illumination can never disagree about the geometry"
holds for a surface emitter without a line of change.

### 7. What it perceives

| Detector | Base | What the row uses it for | The price it already pays |
|---|---|---|---|
| `FBSiteSearch` | `sensors/FBRadarSystem` | acquisition volume, own frame time = one antenna revolution | filters `FBUnitKind::Aircraft` (`sensors.md` gap 3) — a SAM cannot see a bomb, which is correct |
| `FBSiteTrack` | `sensors/FBRadarSystem` | STT volume, the illumination for a SARH round | its `Emission()` is derived from the pattern flown — radiation and antenna state cannot diverge |
| `FBSiteOptics` | `sensors/FBVisualSystem` | the only detector of MANPADS and optical AAA | all five currencies of `sensors.md` §9.2, including **it stops working at night** |
| `FBSiteEsm` | `sensors/FBRwrSystem` | the `emcon hold` cue | no range, ever. A cue is a bearing and a power, and the site must still search for what it heard |

**The registry reader list does not grow, and that is checkable.** All four bases already hold
`#include "FBUnitRegistry.h"`; a derived class adds none. `tools/verify_layers.py`'s `RESTRICTED` table
stays at six entries, and the acceptance criterion is that `make -C sim verify-layers` prints
*"6 restricted header(s) respected"* after the round exactly as before it. This is the same argument
`sensors.md`'s Spec already makes for `FBMissileIrSeeker` ("the `RESTRICTED` list does not grow: the
derivation adds no `#include` of its own"), and it is the reason the site was designed around existing
bases rather than around a new "ground sensor".

### 8. Damage — no new type, no new friend, no new id

**Decision: the site uses `core/FBSystemHealth` exactly as it stands, and inherits nothing.**

`FBSystemHealth` is not a base class and must not become one: it is a **member** of `FBSimUnit`, monotone,
all mutators private, exactly one `friend` (`FBDamageModel`). Every unit already owns one — the site gets
it by existing, not by deriving. Giving the site its own register would mean a second write gate, and a
second gate is a second place the "no self-healing" property has to be proved.

Five of the fourteen existing ids carry everything a site has:

| Id | On a site | Consequence when `Failed`, and where it comes from |
|---|---|---|
| `Structure` (2) | the position itself | `CombatEffective()` false → `objective kill unit` met. The **only** id the existing `target_soft`/`target_hard` declare |
| `Radar` (6) | the acquisition and tracking sets | the system does not run and does not publish → block `Invalid` → **`Emission()` stops** → the site is silent and blind, by `weapons.md` §8's coupling and not by one line written here |
| `FireControl` (7) | the engagement machine's computer | no launch decision; the sets keep radiating. **The suppression case** |
| `Stores` (8) | rails and wiring | tracks, radiates, never launches |
| `Gun` (12) | barrels, feed | AAA rows only |

**Degraded** states: `Radar` degraded uses the existing `kRadarRangeDegraded = 1/√2` (derived from the
radar equation) — half the aperture, `1/√2` of the range. The others declare `Degrade == Fail` because no
degraded behaviour is derivable, which is the rule `weapons.md` §6.2 already states.

**The honest failure of this decision, named here rather than discovered later:** `CombatEffective()` asks
`Engine ∧ FlightControls ∧ Structure`. A site has no engine and no controls, so it is combat-effective
exactly while its `Structure` holds — **a battery whose radar and launcher are destroyed still counts as
effective in the mission verdict.** SEAD is precisely that outcome, and the verdict vocabulary has no word
for it. Two ways out, both stated:

| Option | Verdict |
|---|---|
| **A:** declare `Radar`/`FireControl`/`Structure` in ONE zone with comparable thresholds, so a burst that kills the antenna usually kills the van | **RECOMMENDED for this round.** Physically defensible (a radar van *is* its own structure), costs nothing, and keeps `CombatEffective` a statement about airframes as `weapons.md` §7.5 declares it |
| B: extend `CombatEffective`, or add a `suppress` objective kind | a `C12`-class change to the objective vocabulary, with its own conservation argument. **Booked as a new gap, not decided here** |

### 9. Mission grammar

**Module keys** — one per catalogue row, registered by walking `kSiteCatalogue`:
`sa2` · `sa3` · `sa6` · `sa8` · `zsu23` · `zu23` · `sa7` · `sa18` · `p18`. Plus the unchanged
`target_soft` / `target_hard`.

```
unit sam_north
  module sa6                       # the KIND of site — capability comes from the catalogue row
  team hostile
  spawn 44.7500 20.3000 ground 090.0 0    # 'ground' = elevation from the provider; hdg = the mount's yaw
  set emcon hold                   # radiate only after the passive receiver has a cue
  set rounds 3
  set reaction_s 24
  set scoot_s 120
```

**The `set` keys** — six, and the site module's `ApplySetup` accepts exactly these. `target_soft` /
`target_hard` keep refusing every key; what a *target* is, its module name says.

| Key | Values | Effect | Why it is mission data and not catalogue data |
|---|---|---|---|
| `emcon` | `free` \| `hold` | `free`: radiate from t = 0. `hold`: `Dark` until the ESM hears an airborne emitter | `[SET]`. W4's anchor is an air defence that *refuses to emit*; that is a decision a commander made, not a property of the hardware |
| `alert` | `ready` \| `cold` | `cold` adds `WarmupS` before the first radiation | `[SET]`. O5's alert-versus-CAP pair is one line apart on the aircraft side; the defender deserves the same lever |
| `rounds` | `0…N` | rounds available; `0` makes a shooter into a pure emitter | `[SET]`. Also the cheap way to build the "radar site you can switch off" seven campaigns want out of a system that also shoots |
| `engage_max_m` | metres | clamps `Rmax` **down**, never up | `[SET]`. A doctrine ("hold fire until 15 km") is a mission fact; raising a published envelope would be inventing performance |
| `reaction_s` | seconds | overrides the row's reaction time | `[SET]`. Crew training is the one quantity every anchor discusses and no source quantifies; a free lever with a logged value is honester than a hidden constant |
| `scoot_s` | seconds | after a launch: go dark for this long | `[SET]`. `w4-04`/`w4-05` state explicitly that SA-6 shoot-and-scoot cycle times are **not sourced** and that the mission must declare its own once `C1` exists — this key is that declaration |

An unknown key stays a runtime FAIL (`SET_REJECTED`), unchanged.

### 10. Observable

| Channel | Content |
|---|---|
| Telemetry, own source `site` (registered last, the append rule) | `site_state` (the six-state ordinal), `site_beam0` / `site_beam1` (emitter mode ordinals), `site_tracks`, `site_lock` (0/1), `site_rounds`, `site_engaged_s`, `site_cue` (0/1, ESM) |
| Events | `site CUE` (ESM bearing, power), `site RADIATE` / `site GO_DARK` (with the reason: emcon, scoot, damage), `site TRACK` (bearing, range, closure), `site LAUNCH` (round key, range, aspect, rounds left), `site BREAK` (track lost: reason, time into the engagement), `site DEPLETED` |
| Reuse, unchanged | `sms RELEASE` / `LAUNCH_SOLUTION`, `missile PHASE` / `ILLUMINATION_LOST`, `gun BURST` / `HIT` / `MISS`, `damage …`, `UNIT_RESULT INTACT` / `DESTROYED` |

### 11. Acceptance criteria

The round is done when these are **measured**, not argued:

| # | Criterion | Measurement |
|---|---|---|
| 1 | The gate did not widen | `make -C sim verify-layers` prints *6 registry reader(s) inside the perception boundary*. **Corrected in the build round:** the criterion originally read the wrong number off the wrong line — the head's *"N restricted header(s) respected"* counts protected HEADERS (two) and is green however many readers are added. `tools/verify_layers.py` now names the perception boundary as its own list (`PERCEPTION_READERS`) and prints its LENGTH, so the number the anti-cheat promise hangs on is visible in the output |
| 2 | Existing missions are untouched | all `telemetry*.csv` of all committed `.fbm` byte-identical to the pre-round binary; `events.log` identical modulo `wallS`/`speedup` |
| 3 | The three-symbol sequence exists | one mission, one F-16, one `sa6`: `rwr THREAT_NEW mode=Search` → `THREAT_MODE …Track` → `…Missile`, with the elapsed time between the first two equal to the declared `reaction_s` ± one tick |
| 4 | The envelope decides | the same geometry flown at `AltMin − 100 m` produces no launch and at `AltMin + 100 m` produces one |
| 5 | Breaking the chain breaks the round | an `sa6` engagement in which the target kills the tracking radar mid-flight produces `missile ILLUMINATION_LOST` and a miss; the same shot unbroken hits |
| 6 | A command round dies with its uplink | the same pair on `sa2`, ending in `INERTIAL` and a miss |
| 7 | EMCON is a sensor, not a clock | two runs identical except the attacker's `set fcr on|off` against `set emcon hold`: the silent run produces **zero** `site RADIATE` lines and zero launches |
| 8 | MANPADS is a daylight weapon | the same geometry with and without a `time` line at night: zero `site TRACK` lines at night, by `FBVisualSystem`'s measured behaviour |
| 9 | Determinism | one fingerprint over `--threads 1/2/4` × 3 repeats |
| 10 | Killing the emitter silences it | after `damage` fails `Radar`, `site_beam0` = 0 and the attacker's RWR drops the threat |

---

## State

**BUILT.** `sim/src/modules/ground/` holds the inert `target_soft`/`target_hard` pair plus four new
files: `FBSiteModule.h/.cpp` (the position), `FBSiteFireControl.h/.cpp` (the engagement machine) and
`FBSiteRadar.h` (one parametric radar, two configured instances). `core/FBSite.h` carries the nine
catalogue rows; `core/FBStore.h` six new rounds and `FBSeekerKind::CommandGuided`; `core/FBGun.h` the
two 23 mm guns. Six FlightBox-own JSBSim decks under `sim/assets/aircraft/` from one generated recipe.

| Measured | Number |
|---|---|
| Existing missions unchanged | **303/303** `telemetry*.csv` byte-identical and **104/104** `events.log` identical modulo `wallS`/`speedup`/path, against the pre-round binary, at `--threads` 1, 2 and 4 |
| Determinism, all 112 missions | one signature over `--threads 1/2/4` (336/336 telemetry, 112/112 events) |
| Perception boundary | `verify-layers`: **6 registry reader(s) inside the perception boundary** — unchanged |
| The three stages (`sam-sa6-engage`) | first firm track t=11.8 s -> `ENGAGE` t=37.8 s (**26.0 s = the row's `ReactionS`**) -> `sms RELEASE` t=38.4 s. The attacker's receiver: `search` t=0.1 -> `track` t=6.9 -> `missile` t=38.5, so the pilot gets **31.6 s** between the boxed symbol and the launch light — MORE than `ReactionS`, because the illuminator starts painting 4.9 s before the position has its own firm track |
| Two beams at once | while tracking, `site_beam0` = 1 AND `site_beam1` = 2; the search sweep stays audible to every other aircraft |
| Doppler notch on a mount | `radar CHAFF_SEDUCED unit=sam ... ownClosMs=0 measClosMs=32.89 tgtRadialMs=-32.89 notchMs=40` — own closure is EXACTLY zero, so the notch degenerates to the target's own range rate. Not one line was written for it |
| The eye binds a MANPADS | `sam-manpads-day`: first firm `vis CONTACT` at 4.44 mrad = **3 288 m** (F-16 lateral 14.6 m), inside the eye's measured 3 784 m beam-on and well inside the Strela's 4 200 m envelope. `sam-manpads-night`, identical but for the `time` line: **zero** `vis` lines, zero `site` lines, no launch |
| The altitude gate | `sam-flak-ceiling`: two identical run-ins 600 m apart over one `zsu23` (ceiling 1 500 m). It tracks and fires at the 1 400 m one and never at the 2 000 m one |
| Killing the emitter silences it | `sam-radar-kill`: one Mk 82 fails `Radar`/`FireControl`/`Structure` in one zone, `UNIT_RESULT ew DESTROYED`, and the attacker's `rwr` drops the threat by the existing failure->`Invalid` coupling |
| Salvo and magazine | `sam-sa2-command`: two salvos of **3** at the row's 3.0 s spacing, `salvoLeft` 2/1/0, then `site DEPLETED launches=6` |
| Emission discipline | `sam-emcon-hold`: `site CUE` (a bearing and a power) -> `DARK`->`SEARCH` -> `RADIATE`; after the salvo `GO_DARK`, and `RADIATE` again exactly `scoot_s` = 60 s later |

What **already exists and is consumed unchanged** by this contract, which is why it is a bounded round
rather than a subsystem:

| Piece | Where | Used for |
|---|---|---|
| unit without an airframe | `units/FBSimUnit` | the site itself |
| weapon-as-unit, spawn from the carrier state | `missions/FBMissionBoot.h::FBMissionSpawnStore` | every launched round |
| three seeker kinds, the uplink, the phase machine | `modules/missile/` | SARH and IR rows verbatim |
| radar volume/frame/track-firming law, chaff notch as a **hook** | `sensors/FBRadarSystem` (+ the N019's `DopplerNotchMs`/`NotchRejectsDetection`/`CoastS` hooks) | the search and track sets, per-row Doppler behaviour, **no new hook needed** |
| the eye | `sensors/FBVisualSystem` | MANPADS and optical AAA |
| the warning receiver | `sensors/FBRwrSystem` | the EMCON cue |
| damage register, damage model, the failure→`Invalid` coupling | `core/` + `weapons.md` §8 | the whole "kill the radar" behaviour, free |
| gun bundles resolved against aircraft | `missions/FBMissionRunner.cpp::ResolveGunHit` | AAA |

---

## Gaps

### The honest headline

**A site cannot be seen, only heard.** `FBRadarSystem` filters `FBUnitKind::Aircraft`
(`sensors.md` gap 3), so no attacking aircraft's radar will ever find a SAM battery; the only channels
are the RWR (bearing, no range) and the eye (a few km). Combined with `C8` (no HARM in the store
catalogue) and `C4` (no terrain masking), the consequence is stark and must be stated in every campaign
that consumes this: **`C1` gives the ground the ability to shoot back long before it gives the air the
ability to shoot first.** That asymmetry is real, it is the correct order to build in (the threat is what
six campaigns are missing), and it is not a SEAD capability.

### Found while BUILDING (new, and measured)

| # | Gap | Detail |
|---|---|---|
| B1 | **A MANPADS launches without a seeker tone, and therefore mostly misses** | measured on `sam-manpads-day`: the gunner acquires at 3 288 m, the row's `reaction_s` 8 s is spent, and the round is launched at a PHANTOM placed along the measured bearing at the seeker's own reach. `msl_seeker` stays 1 (uncaged, never locked) and both rounds miss by 883 m / no detonation. The real weapon acquires BEFORE launch — the gunner has a tone — and FlightBox has no pre-launch seeker state on a store still on the rail. Named rather than papered over with a wider seeker field |
| B2 | **A command-guided round is close but not lethal at long range** | `sam-sa2-command`, six V-750 at 23-15 km against a non-manoeuvring target: closest approaches 17 226 / 16 585 / 15 940 m (the first salvo, fired at 23 km and outrun) and **14.2 / 597 / 1 504 m** (the second, fired at 15 km) against a 12 m fuze. The nearest round missed by 2.2 m. The estimate the round flies is the position's own track with no measurement error, so what is missing is not accuracy but the round's KINEMATICS at the top of its envelope |
| B3 | **An optical position applies no envelope test at all** | `FBSiteFireControl::InEnvelope` returns true immediately for a row with no tracking radar, because an eye publishes no range and no altitude, ever. What binds such a position is its gunner's sight and afterwards the round's own seeker or the barrel's own reach. Stating a range there would have been the cheat |

### Named, quantified, refused for a reason

| # | Gap | Detail |
|---|---|---|
| G1 | **No terrain masking, so detection ignores altitude** (`C4`) | a 30 m ingress is detected exactly as well as a 10 km transit. The weapon `AltMin` carries the low-altitude escape and the detection side does not. This is the one place the model is materially **too generous to the defender**, and it is named rather than patched with an invented clutter floor |
| ~~G2~~ | **Suppressed ≠ destroyed** — **CLOSED 2026-07-28** | `CombatEffective()` = `Structure` alone on a site (§Spec 8). A battery with a dead radar and dead rails still scores as effective, and SEAD is exactly that outcome. The `C12`-class round this entry asked for is specified as `C26` in [`../../air-to-ground.md`](../../air-to-ground.md) §5, in two halves: a **mechanism** — `set emcon` gains the value `react`, the crew goes off the air on its **own damage** (a fact the module may read about itself, the only cue it can legally have, since an anti-radiation round radiates nothing and there is no MWS) — and a **verdict**, `objective suppress unit\|team [emitting <s>]`, deferred, `FBObjectiveCovers` false, roster price **one bool**. Option B of §Spec 8 is thereby taken in its second form only: `CombatEffective()` is expressly **not** widened. **BUILT:** `FBSiteFireControl::SetEmconReact` + `FBModule::OwnHits()` (the register a module may read and no module may write), `FBObjectiveKind::Suppress` + `FBMissionMonitor::NoteEmitting`, `FBUnitObservation::Emitting` filled by the owner from the published signature. Proofs `sam-emcon-react.fbm`, `suppress-quiet.fbm`, `suppress-killed.fbm`. **Interface consequence for this file:** `set emcon` now reads `<free\|hold\|react> [offS [onS]]` — the third value AND an optional briefed emission plan, one key, and the author-facing key set stays at six. The plan exists because the escape window of [`../../air-to-ground.md`](../../air-to-ground.md) §2.2 is CONTINUOUS in the shutdown time and neither `scoot_s` (needs a launch) nor `react` (needs a hit) can be placed in time |
| G3 | **No RADAR jamming** (`C13`) — HALVED 2026-07-28 | the COMMUNICATIONS half is built (`C24`, `set jam_comm_m`): a jammed position loses its link, falls back on its declared `autonomy` and keeps seeing perfectly. What stays absent is any ECCM question about the RADAR: noise, deception, range-gate pull-off, angle-of-jam, burn-through |
| G4 | **Chaff has a channel only against Doppler-gating sets** | the model is a Doppler-notch model (`sensors.md` §4.7). Rows that declare no notch (the conical-scan sets) cannot be chaffed at all, and the real defeat mechanism — blanketing a corridor — is `C13`-adjacent and not modelled |
| G5 | **Command guidance is too accurate** | the uplink carries the shooter's estimate, and the shooter's radar has no measurement error anywhere in the tree (`sensors.md` gap 4). The documented accuracy falloff of a CLOS system with range therefore does not exist. Refused deliberately: reproducing it needs a die or an invented bias |
| G6 | **Heavy AAA does not fit the projectile pool** | `FBGunProjectiles` retires a bundle at `kMaxAgeS = 3 s` / `kMaxPathM = 3000 m`. At 970 m/s muzzle velocity that covers 23 mm light AAA with margin and excludes every 57/100 mm gun. W3's anchor names **100 mm** explicitly. The mechanism for a fuzed bursting shell already exists (an unguided store with `FuzeRadiusM > 0`, resolved by `weapons.md` §5.1) and what is missing is a deck and a pool budget — a separate, cheap round |
| G7 | **A mobile site does not move** (`C14`) | mobility is expressed in **time** (`set scoot_s`: emit, engage, go dark) and not in space. What the attacker experiences at range — an emitter that vanishes — is reproduced; a re-located battery is not |
| G8 | **No IRCCM** | the Igla-class rows differ from the Strela-class rows only in **aspect** (documented, and it falls out of the existing IRST aspect law). Their documented flare resistance has no model, and inventing an irradiance ratio for it would be inventing the number that decides the shot |
| G9 | **One channel, no fire distribution** | a site engages the first firm track that passes the envelope test. There is no threat evaluation and no priority by closure or by type. Coordination between two sites now EXISTS as sector responsibility and a transmitted weapons-control state ([`../../air-defence-network.md`](../../air-defence-network.md) §5), but per-target ASSIGNMENT is deliberately deferred: two sites whose sectors both contain the target both engage it |
| G10 | **A launched round is fired from a unit that is `Ground`** | air-to-air sensors ignore `Ground` (`weapons.md` §1) — correct — but it also means the **launch flash is invisible**: there is no MWS in the tree (`sensors.md` gap 8), so an IR launch produces no warning of any kind. That is consistent with the model and lethal for the pilot AI, which today has no defensive reaction to an unwarned shot |
| G11 | **Nothing consumes the threat picture yet** | `pilot/` has no SAM reaction: no defensive break, no drag, no altitude discipline. Deliberate, and the same `D3` precedent the eye set — a channel is a measurement, a behaviour change is its own round with its own measurement |
| G12 | **The catalogue's reaction times are half unsourced** | SA-6 (22–28 s), SA-8 (26 s) and MANPADS (6–13 s) are sourced; SA-2 and SA-3 are `[SET, TODO]`. `set reaction_s` exists partly because of that |

### Two collisions with the existing tree, found while writing this

Neither is a defect of the tree; both are places where a contract written for a pilot meets a machine
that has none.

| Collision | Detail | Resolution proposed |
|---|---|---|
| **The weight-on-wheels interlock refuses every ground launch** | `FBStoresSystem::Release()` rejects on weight on wheels with `hardware_precedence` (`weapons.md` §2.4, check #2), and a unit without an airframe reports `AnyWow = true` **by definition** (`runtime.md` §3, `BuildMissionSample`). A launcher is permanently on the ground, so the interlock written for the airframe's safety would refuse 100 % of SAM launches | a virtual `FBStoresSystem::GroundInterlockApplies()`, default `true`, `false` for a launcher — one hook, and the interlock stays first in the order for everything that flies |
| **The SMS assumes an airframe** | `AttachFdm` creates one JSBSim point mass per declared station and `PublishLoadout` pushes mass and drag into the deck. A site has no `FBFdm` at all | `AttachFdm` becomes optional: with no airframe, no station masses are declared and `PublishLoadout` is a no-op. The rail's mass is not a flight-mechanical fact for a thing that does not fly |

---

## Knowledge

### 1. The five open questions of `weapons.md`, answered

| # | Question as it stood | Answer | Where |
|---|---|---|---|
| 1 | Does a SAM battery emit through `FBEmitterSignature` unchanged, or does a surface emitter need fields an airborne one does not have? | **The struct is unchanged.** The geometry, the beam window and the range gate all carry over verbatim; a ground mount is an emitter with roll = pitch = 0. Two things change *around* it: the signature carries **two** beams instead of one (§Spec 6, design A, recommended), and `FBEmitterKind` gains `SurfaceEarlyWarning` and `SurfaceFireControl` — appended, so no existing ordinal moves. That second value is also the discriminator `sensors.md` gap 25 was waiting for; this file provides it and deliberately does not wire the SPO-15 rule to it | §Spec 6 |
| 2 | Is a SAM a store module with a seeker (like the AIM-120) or a new kind? | **A store module with a seeker.** Three of the four guidance families are already built (`SemiActiveRadar` for SA-6, `Infrared` for MANPADS, and a gun is not a store at all). The fourth, radio command, is **one new `FBSeekerKind` value** whose whole behaviour is "the uplink branch of the existing phase machine, forever" | §Spec 4 |
| 3 | What launches it — a ground module with a fire-control state machine, or a scripted release? | **A fire-control state machine**, `FBSiteFireControl : pilot/FBPilot`, six states, commanding through the command bus. A scripted release would be a world write path outside the simulation and would make the site's behaviour a function of the mission author rather than of the geometry | §Spec 5 |
| 4 | Does a ground unit acquire through a sensor slot, and if so does the `RESTRICTED` list grow again? | **Yes, and no.** Four detectors, all derivations of bases that already hold the include: `FBRadarSystem` ×2, `FBVisualSystem`, `FBRwrSystem`. The list stays at **six files**, and criterion 1 of §Spec 11 measures it. The price rule of `sensors.md` is satisfied without invoking it, because no new slot is admitted at all | §Spec 7 |
| 5 | AAA as a gun versus a SAM as a round — and §5.4's standing refusal | **AAA is a gun, and §5.4 does not bite.** That refusal forbids resolving a *ground burst* (a bomb detonating on the ground) against aircraft, for want of a fragment-against-airframe geometry. A gun **bundle** fired from the ground is a different object entirely and is resolved by `ResolveGunHit` against `Aircraft` with the identical dispersion/presented-area/flux arithmetic an air-to-air burst uses; the shooter's own velocity is simply zero. What *does* bite is the projectile pool's `3 s / 3000 m` cap, which admits 23 mm AAA and excludes every heavy gun — quantified in G6 | §Spec 4, G6 |

### 2. Why the eye, not the missile, sets the MANPADS engagement range

`FBVisualSystem`'s measured detection ranges against an F-16 (`sensors.md` §9 State, `vis-day`):
**3 784 m** beam-on, **2 493 m** head-on, **2 469 m** tail-on (geometric). The Igla's published envelope
is 5 200 m and the Strela-2M's 4 200 m ([`catalogue.md`](catalogue.md)).

```
engagement range = min(weapon Rmax, sensor detection range)
                 = min(5 200, 3 784)  =  3 784 m   beam-on
                 = min(5 200, 2 493)  =  2 493 m   head-on
```

**The sensor binds, at every aspect, for every MANPADS row.** Three consequences follow without anything
being tuned: a MANPADS is a last-two-kilometres weapon; it is worth far more against an aircraft crossing
its position than against one flying at it; and it does not exist at night (`FBDaylightFactor` = 0 →
contrast 0 → nothing detected, measured as *zero* `vis` lines in `vis-night`). All three match what the
anchors say about MANPADS, and none of them was put in by hand.

### 3. The Doppler notch on a stationary mount

`sensors.md` §4.7's decision is measured on **own** quantities:

```
tgtRadialMs = OwnClosureOn(line of sight) − measuredClosureMs
```

`OwnClosureOn` is the clutter Doppler: what a stationary point in that direction would close at. For a
ground mount own velocity is zero, so `OwnClosureOn = 0` and

```
tgtRadialMs = −measuredClosureMs        (the target's own range rate, sign flipped)
notch hit  ⟺  |range rate| < DopplerNotchMs(range)
```

A target flying a circle around the site is in the notch by construction. That is correct pulse-Doppler
behaviour and it makes the beam manoeuvre work against a ground set exactly as it works against an
airborne one — with **no new code**, because the N019 round already turned the notch into a per-set hook.
A row whose real set is a conical-scan pulse radar declares `DopplerNotchMs = 0` and is not notchable at
all, which is the honest statement about that hardware and the reason chaff is useless against it (G4).

### 4. The gun-pool arithmetic that admits 23 mm and refuses 57 mm

`FBGunProjectiles`: `kMaxAgeS = 3 s`, `kMaxPathM = 3000 m` (`weapons.md` §4.3).

| Gun | Muzzle velocity | Path in 3 s, ignoring drag | Published effective slant range | Fits? |
|---|---|---|---|---|
| ZU-23-2 / ZSU-23-4 (23 × 152B) | 970 m/s [T4] | 2 910 m | 2 000–2 500 m [T4] | **yes**, with the whole envelope inside both caps |
| S-60 (57 mm) | ~1 000 m/s | 3 000 m | ~6 000 m slant | no — the round is retired at less than half its useful reach |
| KS-19 (100 mm) | ~900 m/s | 2 700 m | ~12 000 m ceiling | no, by a factor of four |

The caps are therefore not an accident that happens to exclude heavy AAA; they exclude exactly the guns
whose employment is a **fuzed bursting shell** rather than a stream of impacts, and those want the store
path, not the gun path (G6).

### 5. Why `emcon hold` is a receiver and not a timer

The requirement (W4: *an air defence that refuses to emit*) needs a site that comes up **on a cue**. The
cue must be something the site measured, or the site is cheating.

| Candidate cue | Verdict |
|---|---|
| a timer (`set radiate_after_s`) | works, tells the truth about nothing, and makes the defender's behaviour independent of what the attacker does |
| a range trigger on the registry | **forbidden.** It is the registry read the whole architecture exists to prevent |
| an external cue from an EW/GCI unit | needs a cross-unit channel; the only legal one is a sensor, so the SAM would have to *hear* its own early-warning radar |
| **the site's own passive receiver** | **RECOMMENDED.** `FBSiteEsm : FBRwrSystem` hears `FBUnitSignature::Radar` — an attacker with its fire-control radar on is audible at roughly twice its own acquisition range (`kBeamRangeFactor = 2.0`), and the cue carries **a bearing and a power, never a range**, so the site still has to search for what it heard |

The measurable consequence is criterion 7 of §Spec 11 and it is a real experiment, not a demonstration: an
attacker that keeps its radar off is never cued, so a `set emcon hold` site never radiates and never
shoots — **EMCON works on both sides of the fight, and the mission measures the trade** (a silent attacker
also has no radar picture). That is `w4-01`/`w4-02`'s one-line experiment, on the defender's side of the
line.

### 6. Where the numbers live

| Kind of number | Home | Rule |
|---|---|---|
| bands, envelopes, ranges, reaction times, channels, rounds | [`catalogue.md`](catalogue.md) | every row cites a source and a tier; where sources disagree, **both** values are carried |
| the four decisive quantities of every other cast type | [`cast.md`](cast.md) | deliberately shallow: what the campaigns decide, and nothing else |
| the class, the layer, the hooks, the grammar | this file | every setting `[SET]` with one sentence of reason |
| a mission's own doctrine | the `.fbm` | six keys, §Spec 9 |

---

## Related

| Place | Relationship |
|---|---|
| [`catalogue.md`](catalogue.md) | the nine rows with their sources — the data this class is the engine for |
| [`cast.md`](cast.md) | everything else the ten campaigns need, at four quantities per type |
| [`../../weapons.md`](../../weapons.md) | weapon-as-unit, the three resolution boundaries, the damage model and the failure→`Invalid` coupling — every one of them consumed unchanged |
| [`../../sensors.md`](../../sensors.md) | the perception boundary, the emitter signature, the RWR's two geometries, the eye's five currencies |
| [`../../core.md`](../../core.md) | `FBEmitterSignature`, `FBSystemHealth`, `FBStore.h`/`FBGun.h` — the value layer this contract extends by one file and two enum values |
| [`../../missions/syntax.md`](../../missions/syntax.md) | the `unit` block a site is declared in; the six new `set` keys belong to [`../../missions/weapons.md`](../../missions/weapons.md) when they are built |
| [`../../campaigns/INDEX.md`](../../campaigns/INDEX.md) | `C1` and the six campaigns it blocks |
