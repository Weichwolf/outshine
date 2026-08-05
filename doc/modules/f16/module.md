# FlightBox — the F-16 module (`modules/f16/`)

**Subject:** FlightBox' REBUILD of the F-16 — structure, contracts, numbers and the places where the
rebuild deviates from the original or deliberately does NOT claim it.

**Delimitation:** `doc/modules/f16/` documents the REAL jet from the manuals (DCS F-16C Viper Guide / ED EA
Guide). This file documents the CODE. Where a number comes from `doc/modules/f16/`, the reference is cited —
the content is not copied.

**State:** commit `9673e00`. Sources are the comment banners under `sim/src/modules/f16/` (23 files)
+ `modules/f16/displays/` (2), `sim/src/modules/FBModule.h`, `FBModuleRegistry.{h,cpp}` as well as
CLAUDE.md's `modules/` and `modules/f16/` sections. Update note: see [Gaps](#gaps).

## Spec

FlightBox' rebuild of the F-16 — the product. This file documents the CODE; `doc/modules/f16/` documents the
real jet from the manuals.

| Contract | Acceptance / measurement anchor |
|---|---|
| The F-16 flies the pinned vanilla JSBSim model; the MODEL is the reference, not the real jet | `CLAUDE.md` principle 5; deviations only as declared deltas (`make -C sim verify-models`) |
| The module is composition + numbers, not a second architecture | it composes the `systems/` defaults and overrides behaviour where the jet differs |
| Every number is derived from a named source | `doc/modules/f16/` reference, model geometry, or `[SET]` and named as such |
| Nothing is invented where the source is silent | no TD box / locked-target symbol (`doc/modules/f16/hud-symbology.md` knows neither), no gun bore depression, no station data beyond what the model gives |
| The HUD sits in the real combiner aperture | symbology positions checked against the DCS F-16C guide and the GPL FlightGear F-16 mod (facts cited, no code copied) |
| Module-specific display artefacts stay in a module hook | `FBF16Max7456`, never in `render/` |

## State

Built: the module, its slot composition and cadence, the command router, and every override with its
numbers.

| Piece | Status | Anchor |
|---|---|---|
| `FBF16Module` composition + tick groups | built | `59f08c8` |
| F-16 main HUD with real combiner aperture | built | `6802a6d`, `d31b1a9` |
| `FBF16Datalink` (MIDS terminal, TNDL filter) | built | `9190e7c` |
| `FBF16Fcr` (APG-68 mode set, STT) | built | `4049a7b` |
| `FBF16Rwr`, `FBF16Cmds` | built | `439f53a` |
| `FBF16FireControl` (B-range, A-A launch zone, CCIP/CCRP solution) | built | `1ecd433`, `1eeff72` |
| `FBF16Sms`, `FBF16Gun`, `FBF16Damage` (pylon geometry, gun install, damage zones) | built | `a1a8fbf`, `6d84647` |
| `FBF16Pilot` | numbers only — `Run()` is not overridden | `1ecd433` |
| `FBF16Max7456` | real NoOp override point | `6f160af` |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `modules/f16/FBF16Rwr.h` | `kOpenThreats = 16` against `kMaxRwrThreats = 8` — the OPEN cap can never bind, PRIORITY is the only effective one |
| `modules/f16/FBF16Sms` | station geometry is **longitudinally collapsed** — all nine pylons on the same fuselage station, so carriage produces no pitching moment |

### Decided and done: the flaperon mixer (was `TODO.md` §6)

**Fixed** — `mods/f16/src/aircraft/MODEL-DELTAS.md` entry **D1**, the first real model delta and the delta rule's
first live test. The flaperon summer carried the flap command differentially and the roll command
symmetrically, so `fcs/tef-control` cancelled out of `fcs/flaperon-mix-rad` and twice the aileron
command took its place. Consequences and the re-measured anchors:

| Anchor | before | after |
|---|---|---|
| `flaperon-mix-rad` under a pure ±0.5 roll step | −1.28 / +1.29 | **0.0000** |
| Nz peak in the roll-in (right / left) | −1.54 g / +3.46 g | **+0.97 g / +0.97 g** |
| Body-x aero force in the roll-in (right) | **+6,420 lbf** — a wing pushing the jet forward | **−5,267 lbf** (drag) |
| TEF fully out: `flaperon-mix-rad` | −0.0002 rad | **0.3490 rad** = the 20° the Flaps channel commands |
| Flap ΔCL / ΔCD | ≈ 0 | **0.122 / 0.028** |
| Roll rate at 400 KCAS, right / left | +187.8 / −132.3 °/s | **+156.4 / −156.6 °/s** |
| Roll-direction asymmetry, 250…600 KCAS | up to **55.5 °/s** | **≤ 0.2 °/s** |
| Corner (`make -C sim test-corner`) | 380 KCAS @ 16.22 °/s, nz 5.63 | **380 KCAS @ 16.18 °/s, nz 5.44** |
| 11°-AoA trim speed, gear down, 40 % fuel | 165 KCAS | **154 KCAS** |
| Landing ground roll, Payerne RWY23 | 1,341–1,597 m | **785–928 m** (D1 + the brake gate; the jet now touches down at the procedure's speed instead of floating) |

**What did NOT move, measured rather than assumed:** the ~0.2° cruise asymmetry (median |φ| on settled
route legs 0.186° → 0.185° over 60,900 samples) — it is the roll PID's steady-state residue, not the
mixer's, because at |φ| < 2° the aileron term the mixer doubled was itself tiny. And `BfmBrakeMs2`
(2.531 → 2.527 m/s²): the flaps only deploy below 250 KCAS, and that hook is measured between 325 and
400 KCAS.

**The re-tune round that followed** found that the last sentence was true and irrelevant: the flaps are
not what was wrong with `BfmBrakeMs2`. The hook bounds a CLOSURE schedule, and it was measured as the
airframe's LEVEL-FLIGHT deceleration — a different quantity, because a closure carries the pursuit
geometry as well as the drag. Re-measured on the thing itself (one-second windows in the stern conversion
at idle with the speedbrake fully out and a valid track, N = 4,595): **median 1.86, p20 1.16, p90
5.76 m/s²**. The hook is now **1.2 m/s²** — the pessimistic end, because it is a LIMIT — and the closure
cap `a/k` with it, 140 → 70 kt. Full derivation and the before/after on `gun-bfm`:
[pilot-ai.md § 5.2](../../pilot.md).

### Documentation lag (from the retired `TODO.md` §7)

This file describes state `9673e00`; commit `658014d` added the BFM hook `BfmBrakeMs2` (since re-measured
to 1.2 m/s², see above) and changed the tick wiring — §2.1/§2.2/§2.6/§3.3 of the body below are due for a
check against the current commit.

### Inventory (from the previous `Open points` section)

**Gaps in the rebuild — each one named, none glossed over:**

1. **No TD box, no locked-target symbol.** `doc/modules/f16/hud-symbology.md` knows neither (the radar-adjacent
   entry is the HMC, a different function). A lock therefore lives exclusively in
   `FBState`/telemetry/events. Before any extension the symbology source must genuinely cover it.
2. **No HOTAS binding beyond a keyboard.** `FBInputSystem` is REAL since the player-control round and
   `FBF16Module::HumanInput()` hands it out; the browser binds eleven keys to it. No gamepad, and the
   stick's force-sensor curve is still undecided ([`hotas.md`](hotas.md) §State).
3. **Further empty/NoOp slots:** `FBPropulsionSystem` (engine logic above the raw FDM) and
   `FBWeaponSystem` (the generic weapons slot; the real effect lives in `FBStoresSystem`/
   `FBGunSystem`).
4. **`WeaponSelect` is deliberately `NotImplemented`** (`doc/modules/f16/controls-commands.md` §6.6) — the jet
   has it, FlightBox does not, and the command says so instead of succeeding silently.
5. **No flight-lead concept.** `FBF16Datalink`'s `FL ON` takes the first `unit` block of the team as a
   placeholder. A real element/pair assignment does not exist.
6. **No RWR threat library.** The ALIC/symbol/system correlation table is described only structurally in
   the source; the classification stays the generic emitter-class estimate.
7. **Flares have no effect.** `FBF16Cmds` dispenses and counts them; there is no IR seeker for them to
   work against, and the generic header says so.
8. **No gun bore depression** — no angle can be evidenced in `doc/modules/f16/`, hence 0°/0°, written out.
9. **No pitching moment from carriage** — every station sits longitudinally on the CG station, because
   the station data in the source is T4 (`weapons.md` §4.5).
10. **No terrain masking in the radar** and no lofted AMRAAM midcourse in the DLZ — both explicitly
    declared as not modelled in the respective header.
11. **Waypoint elevation:** the module hands `FBNavSystem` the WAYPOINT, which carries the terrain the
    mission briefing sampled at it. The module can supply no other figure — the elevation under the
    aircraft is a different place, and a computer able to probe the ground at an arbitrary point would
    be a knowledge source no sensor paid for.
12. **The MAX7456 artefact model** does not exist — `StyleGlyph` is the identity. The override point is
    real and instantiated so that the later implementation does not touch `render/`.
13. **`FBF16Pilot::Run()` is not overridden** — the phase machine is entirely generic. This class is
    today exclusively a set of numbers.

**Update due (concurrent change):** this document describes the state of commit `9673e00`. While it was
being written, a close-combat round was running on `sim/src/modules/f16/FBF16Module.{h,cpp}` and
`FBF16Pilot.*` (together with `pilot/FBPilot.*`). Already visible in the working copy and NOT YET
captured here: an additional BFM hook `BfmBrakeMs2()` (2.4 m/s², `[MESS]` — 238 telemetry samples of the
gun-approach sweep at 4,000 m between 325 and 400 KCAS at idle, speedbrake out, < 15° bank and 1 g, with
the gravity component removed; median 2.39, p10 1.64, p90 3.80 — the MEDIAN is the number, because the
conservative p10 produced 11 instead of 14 kills and 21 s less funnel time in the sweep; **superseded**,
see the D1 re-tune paragraph above: that whole measurement was of the wrong quantity). **§3.3 (BFM
numbers), §2.1/§2.2 (composition and cadence) and the `set` table in §2.6 are to be checked against the
then-current state once this round has finished.**


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### Provenance markers

Every number in this document carries exactly one:

| Marker | Meaning |
|---|---|
| `[DOC]` | evidenced in `doc/modules/f16/` (reference cited) |
| `[MODELL]` | derived from the flown `assets/aircraft/f16/f16.xml` (derivation stated) — upstream state plus the deltas from `mods/f16/src/aircraft/MODEL-DELTAS.md`, today none |
| `[MESS]` | measured in the gym against the vanilla model (measurement setup stated) |
| `[SET]` | a setting/declared model parameter — NOT a quotation, marked as such in the source code |
| `[ABL]` | derived from another FlightBox quantity (calculation stated) |

---

### 1. What a module is

#### 1.1 `FBModule` — the base interface

`sim/src/modules/FBModule.h`

A flyable aircraft is two things: the **code module** (an `FBModule` derivation) and the **JSBSim
model** (a directory with aero/mass/propulsion). The app holds every module POLYMORPHICALLY behind
`FBModule*`; the dispatch is real, not a shortcut to the one F-16 instance that exists today.

Wiring (once each, by the owner `units/FBSimUnit` or by the spawn path `missions/FBMissionBoot.h`):

| Method | Contract |
|---|---|
| `AttachFdm(FBFdm&)` | binds the module to the airframe. The FDM is BORROWED and outlives the module; a module cannot create one itself (the IC lies behind `fdm/FBFdmBoot.h`, which no module includes). Because the registry builds modules WITHOUT ARGUMENTS, this is the substitute for constructor injection. |
| `SetUnitIdentity(id, team)` | who this unit IS — needed by every slot that observes OTHER units (terminal: skip your own PPLI; radar: skip your own echo; SMS/gun: shooter identity for the outgoing weapon). |
| `AttachHealth(const FBSystemHealth&)` | READ ONLY. Not virtual; the state lies private in the base class, access via `HealthOf()/SystemWorking()/SystemDegraded()`. Without an attach everything reports `Intact`. Nobody can write except `core/FBDamageModel` (every mutator private, a single `friend`) — this is enforced by the COMPILER, not by convention. |
| `DamageLayout()` | WHERE the systems of this airframe sit — pure module data, applied by the core alone. Default: an empty layout (a released store has nothing to lose piecewise, its end is the detonation). |

Declaration (what kind of entity this module becomes):

| Method | Contract |
|---|---|
| `FdmModelName()` | model directory/XML. DELIBERATELY not derived from the registry name: "f16"/"f16" coincide today but are not the same thing. An EMPTY name means "no airframe" (`modules/ground`). |
| `UnitKind()` | `Aircraft` (default) / `Ground`. `Weapon` is NOT set here: a store is a weapon by virtue of RELEASE, and that is the statement of the releasing path, not of the module. |

Execution:

| Method | Contract |
|---|---|
| `Run(fb_fdm_state&, dt, const FBUnitRegistry*, const FBWorld*)` | ticks its own FDM in fixed substeps over `dt` wall seconds and cycles its own slots, each at its own rate. The heterogeneous cadence is MODULE-INTERNAL, not part of the interface. `units` (snapshots of the last completed tick, ONLY for sensors) and `world` (terrain side) are borrowed, may be null, and the module passes them on only to the slots entitled to them. |
| `ApplySetup(key, value)` | applies ONE `set <key> <value>` mission line inside the spawn IC window. The MODULE interprets its own keys; runner/boot only parse the flat KV list. `false` for an unknown key → mission FAIL (exit 1), never a silent no-op. |
| `ProgramRelease(FBStoreRelease)` | the launch programming a released weapon receives at separation. Default empty. |

Generic system accessors — the reason why runner and browser client NEVER need a concrete module
header:

| Group | Accessors |
|---|---|
| The three real core slots | `Autopilot()`, `FlightControl()`, `PilotSystem()` |
| Airframe/display/air data/nav | `Controls()`, `Displays()`, `AirDataSystem()`, `NavSystem()`, `WarningSystem()`, `RadarAltimeter()` |
| Command path | `Commands()` (the avionics command bus) |
| Sensors/comms/defensive | `Datalink()`, `Radar()`, `Rwr()`, `Countermeasures()` |
| Weapons | `Stores()`, `Guns()` |
| Bus/diagnostics | `Telemetry()` (the `FBState` snapshot), `LastGuidance()`, `LastSubsteps()` |
| Mission boot | `FlightPlan()`, `SetRunway()`, `SetGroundAsl()` |

A concrete module may return a more specific type COVARIANTLY (`FBF16Module::Radar()` yields
`FBF16Fcr&`) without a generic caller ever seeing it.

#### 1.2 The registry

`sim/src/modules/FBModuleRegistry.{h,cpp}`

- `FBModuleRegistry::Register(name, factory)` / `Create(name) -> std::unique_ptr<FBModule>`.
- The map is a **function-local static** (Meyers singleton), filled EXPLICITLY by
  `FBRegisterBuiltinModules()` at a known point in `main()`. Rationale in the code: a namespace-scope
  static would have two traps — the static-initialization-order-fiasco ordering across translation
  units AND the rule that an unreferenced `.o` from a static archive is not linked in at all (the
  self-registration would have vanished silently).
- `FBRegisterBuiltinModules()` calls one entry function per module FAMILY:
  `FBRegisterF16Module()`, `FBRegisterStoreModules()`, `FBRegisterMissileModules()`,
  `FBRegisterGroundModules()`. Idempotent.
- **The rule:** `FBMissionRunner.cpp` / `FBAppGym.cpp` / `FBAppWasm.cpp` NEVER include a concrete module
  header. They resolve by name and hold everything behind `FBModule*`.
- **The one exception:** `sim/src/modules/f16/FBF16ModuleRegistration.cpp` — 13 lines, the only file in
  the F-16 directory allowed to name `FBF16Module.h` for this purpose:
  `FBModuleRegistry::Register("f16", [] { return std::make_unique<FBF16Module>(); })`.
  Analogous individual exceptions: `modules/stores/`, `modules/missile/`, `modules/ground/`.

---

### 2. `FBF16Module` — the composition

`sim/src/modules/f16/FBF16Module.{h,cpp}`

This is the ONE place where F-16 behaviour comes together, and at the same time the place where a
future override is hooked in by replacing a slot default with a derivation.

#### 2.1 What is composed

| Slot | Class | Status |
|---|---|---|
| Guidance | `systems/FBAutopilot` | **default, unchanged** |
| Flight control | `systems/FBFlightControl` | **default** with the gain preset `FBFlightControl::F16()` |
| Input/HOTAS | `systems/FBInputSystem` | **default, REAL** — cycled before every box, handed this module's own `FBCommandBus`; `HumanInput()` publishes it, and with it engaged `FBF16Pilot` is not run |
| Propulsion | `systems/FBPropulsionSystem` | default = **NoOp** |
| Weapons (generic slot) | `systems/FBWeaponSystem` | default = **NoOp** (weapon effect lives in stores/gun) |
| Air data | `systems/FBAirDataSystem` | **default, unchanged** |
| Radar altimeter | `systems/FBRadarAltimeter` | **default, unchanged** |
| Navigation | `systems/FBNavSystem` | **default, unchanged** |
| Warnings | `systems/FBWarningSystem` | **default, unchanged** |
| Airframe controls | `systems/FBAirframeControls` → `FBJsbsimAirframeControls` | default; NoOp until `AttachFdm`, after that the FDM-bound ownship implementation |
| Command bus | `core/FBCommandBus` | value member, not a slot |
| Displays | `displays/FBF16Hud` | **override** |
| Sensors | `FBF16Fcr` (APG-68) | **override** |
| Comms/datalink | `FBF16Datalink` (MIDS-LVT) | **override** |
| Defensive (passive) | `FBF16Rwr` (ALR-56M) | **override** |
| Defensive (active) | `FBF16Cmds` (ALE-47) | **override** |
| Stores | `FBF16Sms` | **override** (pylon geometry only) |
| Gun | `FBF16Gun` | **override** (armament + installation point only) |
| Pilot | `FBF16Pilot` | **override** (number hooks only, not `Run()`) |
| — (no generic slot) | `FBF16FireControl` | F-16's own fire-control system |
| — (no generic slot) | `FBF16Ufc` | F-16's own ICP/UFC/DED subset |
| — (no generic slot) | `FBF16Max7456` | chip hook for HUD rendering |
| — (pure data) | `FBF16Damage` | damage layout, supplied via `DamageLayout()` |

All slots are held through base pointers (`std::unique_ptr<base class>`) so that a future module can
insert an override without slicing.

**Gain preset `FBFlightControl::F16()`** (`sim/src/systems/FBFlightControl.cpp`), all values `[MESS]`
against the vanilla model:

| Field | Value | Meaning |
|---|---|---|
| `Flcs` | 1 | FLCS command inner loop (the `*-cmd-norm` are rate setpoints), not raw control-surface PD |
| `RollStickMax` | 0.15 | gentle roll onset; at 0.35 nz measurably wanders to −1.1…+3.0 instead of 0.7…1.9 |
| `KRollRate` | 0.05 | roll command per degree of bank error |
| `KG` / `KGi` | 0.25 / 0.8 | g loop P/I |
| `KpSpd` / `ThrTrim` | 0.02 / 0.85 | thrust PI and trim point |

#### 2.2 Cadence table

The runner calls `Run()` once per sim tick (mission core: 10 Hz). Within it:

| Slot group | Rate | Mechanism / rationale |
|---|---|---|
| Guidance + flight control | **100 Hz** | fixed substep loop, spiral guard ≤ 12 substeps per `Run()`. The only virtual calls INSIDE the inner loop. |
| Input/HOTAS, propulsion | per `Run()` | the coarsest rate |
| **Sensor group** (FCR, air data, radar alt, nav, fire control, UFC, SMS) | **10 Hz** | ONE group, so that the HUD telemetry chain is closed: fire control reads nav's output of the SAME tick. |
| Displays | 20 Hz | accumulator-throttled |
| Weapons (NoOp slot) | 20 Hz | " |
| **Gun** | per `Run()`, full `dt` | DELIBERATELY unthrottled: the output is a round count INTEGRATED over time — a different rate would lose or invent rounds. |
| Defensive (RWR → CMDS) | 10 Hz | salvo intervals are tenths of a second (`doc/modules/f16/defence-rwr-cm.md` §2.2); slower would quantise a salvo. Order = data flow: the receiver writes, the dispenser reads in the same tick. |
| Comms/datalink | 5 Hz | The NET CYCLE itself runs at 1 Hz INSIDE the system (`FBDatalinkSystem::kNetPeriodS`). The slot is entered faster so that the AGE of a held track is observable between net cycles — never to refresh the picture earlier. |
| Pilot | 10 Hz | decision rate. `FBPilotCommands` is applied only where a field is SET (`std::optional` / `Guidance::None` = "leave untouched") — phase `Idle` stays neutral, so composing the pilot changes NOTHING until the app starts the phase machine. |

The same holds for the radar: the ANTENNA RASTER runs on absolute sim time inside the system
(`FBRadarScanVolume::FrameS`, 0.1…4 s per mode); the 10 Hz slot merely makes the age of a coasting
contact visible.

All NoOp defaults cost one throttle comparison when they are not due, and one empty virtual call when
they are. No heap allocation per frame, no dispatch in the 100 Hz maths.

#### 2.3 Order within the sensor tick

The order is the data flow and not a matter of taste:

1. `PublishPlatform(st)` / `PublishAirframe()` — the TWO blocks the module writes ITSELF (attitude/
   altitude/speed/AP mode, and gear/WOW/speedbrake/engine/fuel respectively). First, because everything
   below reads them. They exist so that "one writer per block" stays true — previously consumers
   reached directly into `st` (evidenced defect: `FBF16FireControl` read an altitude field nobody
   filled).
2. `ServiceCommands(Sensors)`, `ServiceCommands(Avionics)`, `ServiceCommands(Stores)` — due commands
   FIRST, so that a switch throw takes effect on the NEXT sweep and not the one after.
3. FCR → air data → radar alt → (set steerpoint) → nav → fire control → (target estimate + release
   solution to the SMS) → UFC → SMS.
4. Warning system LAST: a pure consumer of everything above it, including the validity heads.

`st` is the FDM state from the END of the PREVIOUS `Run()` — the same one-tick lag every other writer
at this cadence has.

The steerpoint elevation is BRIEFING data, not a sample: a `wp` line declares the altitude to be FLOWN,
so the terrain under the fix is read once at spawn (`FBFlightPlan::BriefGroundElevation`, out of the
mission's own elevation provider) and travels on the waypoint. `FBNavSystem::SetSteerpoint` therefore
takes the waypoint, and the module has no way to hand in a different probe. The alternative — the fire
control sampling the ground at the aim point per tick — is refused: a briefed target legitimately
carries its map elevation, a free terrain query at the target would be a new knowledge source.

#### 2.4 How a failed system drops out of the cadence

The pattern is identical at EVERY box and carries the whole damage model:

```
if (SystemWorking(FBSystemId::X)) X->Run(...);
else SharedState.X.H.Invalidate();
```

- A FAILED system is not ticked at all, and its block becomes `Invalid`. Everything else follows from
  the bus, which has long been able to do it: the HUD dashes out (`H.Readable()` query per readout),
  `FBWarningSystem` reports INHIBITED instead of "no warning", a consumer has to say what it does
  without the source.
- A DEGRADED system runs normally, with the one DERIVABLE restriction: the radar gets
  `SetRangeFactor(kRadarRangeDegraded)` = 0.7071 `[ABL]` (half the aperture via the radar equation).
- `Nav` invalidates TWO blocks (`Nav` + `Cruise`) — the same box publishes both messages.
- The **damage gate in the command router** is the second half of this (see 2.5): without it a destroyed
  SMS would still let a round off the rail, because the release path runs through the router and not
  through `SmsSys->Run()`.

#### 2.5 Command routing

`ServiceCommands(group)` takes every DUE command of a group off the bus, executes it and acknowledges
with the result the BOX itself decided. The module only routes — which F-16 box owns "radar mode" is
knowledge of this aircraft, not of `systems/`.

| Command target | Box (`FBSystemId`) | Check in the module |
|---|---|---|
| `RadarMode` | Radar | ordinal 0…`AcmSlew`, otherwise `OutOfRange` |
| `RadarRangeNm` | Radar | > 0 and ≤ 160 nm |
| `RadarSlewAz` / `RadarSlewEl` | Radar | ±`FBF16Fcr::kGimbalAzDeg` (60°) |
| `Designate` | Radar | value = published track number, 0 = break lock. Unknown number → `OutOfContext` (not a range error: the echo the pilot designated is gone while the hand was moving — a real cockpit outcome) |
| `IffTransponder` / `IffInterrogator` | Radar | boolean |
| `DatalinkPower` / `Transmit` / `Filter` / `RangeNm` | Datalink | filter ordinal; range > 0 and ≤ 500 nm |
| `MasterArm` | Stores | ONE switch, BOTH weapon systems: master arm is a cockpit control, not an SMS attribute — a jet with armed pylons also has an armed gun |
| `StationSelect` | Stores | unknown station → `OutOfContext` |
| `WeaponRelease` | Stores | the SMS answers itself (master arm, weight on wheels, empty station) |
| `WeaponSelect` | Stores | **always rejected, `NotImplemented`** — `doc/modules/f16/controls-commands.md` §6.6: the jet has it, FlightBox does not. A silent success would be a lie the pilot then flies on |
| `GunTrigger` | Gun | the gun answers itself (SAFE, wheels on the ground, empty drum); value = duration of the trigger press |
| `CmDispense` / `CmConsent` / `CmdsMode` | Countermeasures | program 0…`kProgramCount`; mode ordinal; the dispenser answers itself (empty magazine) |
| `MasterMode` | — | ordinal ≤ `Dogfight` |
| `AlowFt` | — (UFC) | range per `FBF16Ufc::AlowInRange`; **in addition**: with the CARA unpowered `Inhibited/EffectPrecondition` — the entry stands, but the warning can never fire, and the pilot learns that (`doc/modules/f16/controls-commands.md` §6.4) |
| `BingoLbs` | — (UFC) | range per `BingoInRange`; above the documented ceiling `Clamped/ValueClamped`, NOT rejected (§6.8: ENTR succeeds, the field shows what was typed, the warning fires at the system ceiling) |
| `SteerpointNum` | — (UFC) | 1…99 |

**The damage gate sits in front of it:** `CommandOwner(target)` maps the target to the owning box; if
that has failed, nothing is touched and it is acknowledged with `Rejected/SystemFailed`. Targets
without a tracked box (UFC data entry, master-mode switch) fall through.

**Range policy:** out of range is REJECTED and named, never clamped behind the pilot's back. The one
clamp in the file is the documented BNGO ceiling and is reported as `Clamped`, not as success.

#### 2.6 `ApplySetup` — this module's mission switches

Boundary input from mission text. `ParseDouble` is STRICT (the whole value must be ONE finite number
and nothing else): `set fuel_lbs FULL` or a thousands separator would otherwise spawn the jet with an
empty tank, report success, and JSBSim would shut the engine down minutes later in the air without
`events.log` naming the cause. Every rejection produces ONE greppable `SET_INVALID_VALUE` with key, raw
value and reason.

| Key | Values | Effect |
|---|---|---|
| `gear` | up/down | landing gear (an air start spawns retracted) |
| `fuel_lbs` / `fuel_pct` | ≥ 0 / 0…100 | `FBFdm::SetFuelTotalLbs` / `SetFuelPct` |
| `store` | `<station> <type>` | one line per pylon; station 1…9 of this type, store type from `core/FBStore.h` |
| `gun_rounds` | integer ≥ 0, ≤ capacity | drum load at the start of the sortie |
| `datalink`, `datalink_xmt` | on/off | POWER and XMT/EMCON respectively — two switches, because the real terminal has two |
| `datalink_filter` | fr/fl/off | HSD contact filter |
| `datalink_range_nm` | > 0 | terminal range |
| `fcr_mode` | off/crm/acm_hud/acm_bore/acm_vert/acm_slew | FCR mode (the pilot's HOTAS selection) |
| `fcr_range_nm` | > 0 | range gate of ALL modes |
| `fcr_slew_az`, `fcr_slew_el` | ±60° | cursor of the slewable box / antenna elevation |
| `iff_xpdr`, `iff_interrogator` | on/off | the two halves of the APX-113 — separate, because they answer two different questions (can OTHERS identify me / can I identify others) |
| `rwr`, `rwr_search` | on/off | ALR-56M POWER; TWA SEARCH filter |
| `rwr_display` | priority/open | TWP MODE button |
| `cmds_mode` | off/stby/man/semi/auto/byp | mode knob |
| `cmds_program` | 1…6 | PRGM knob |
| `cmds_chaff`, `cmds_flare` | 0…120, sum ≤ 120 | ground crew loadout |
| `radalt` | on/off | CARA power switch — the one mission-declarable way to make a source block INVALID |
| `task` | route/bfm/intercept/attack | starting phase of the pilot state machine. Mission data instead of being guessed from the loadout: two jets with identical `set` lines can have opposite assignments |
| `attack_mode` | ccip/ccrp | ONE line, TWO consumers (pilot: on which cue he releases; fire control: which cue the release record names). Does NOT change the arithmetic — both cues come from the same integration |
| `brief_alow_ft`, `brief_bingo_lbs`, `brief_master_arm`, `brief_weapon`, `brief_chaff_s`, `brief_release_s` | see `doc/missions/INDEX.md` | is NOT applied here but handed to the pilot, who enters it later over the command bus — in the latency class of the operation and rejectable |
| `pilot_*` | see `pilot/FBPilotTuning` | passed through entirely: the parameter set belongs to the PILOT, not to this airframe, so there is no second, drifting copy of the key table here |

#### 2.7 Two small things with the character of a contract

- **Runway = bullseye.** `SetRunway()` additionally sets `FBNavSystem::SetBullseye` to the threshold: a
  `.fbm` declares no bullseye, and the runway is the one briefed geographic point all units of a
  mission share. No runway, no bullseye, and the HUD bearing/range pair stays at the origin default.
- **`Telemetry()` yields `SharedState`, not the app `FBState`.** The client has to seed its frame
  `FBState` FROM it before it overwrites the fields it computes itself (pose/sun/moon), otherwise
  `BuildHud` sees only half of it.
- **DED gate:** `CmdBus_.SetLoadFactor(...)` per tick — head-down entries are only possible in a jet
  that is not being flown hard. Unpublished air data reads as 1 g: a jet without an ADC is not
  manoeuvring according to any measurement this aircraft has.

---

### 3. `FBF16Pilot` — this jet's numbers

`sim/src/modules/f16/FBF16Pilot.{h,cpp}`

**What is NOT here:** the phase machine. `Run()` is generic (`pilot/FBPilot`); this class overrides
exclusively VIRTUAL NUMBER HOOKS. The only method with logic is the interpolated Vr table.

#### 3.1 Takeoff

| Hook | Value | Provenance |
|---|---|---|
| `RotationSpeedKt(gw)` | 128…198 kt over 20,000…44,000 lb, piecewise linear, CLAMPED at the ends instead of extrapolated | `[DOC]` `procedures-takeoff-taxi.md`'s weight/Vr table |
| `RotationLeadKt` | 15 kt | `[DOC]` "Afterburner: begin pull ~15 kts below takeoff speed" |
| `RotationPitchDeg` | 10° | `[DOC]` "Rotate to 8–12 deg" — the middle of the band |
| `GearUpLimitKt` | 300 kt | `[DOC]` "Retract gear before 300 kts" |
| `ClimbSpeedKt` | 350 kt | `[SET]` — no documented number for the climb leg; matches the mission's climb waypoint, conservatively below corner |
| `TakeoffThrottleNorm` | 1.0 | `[DOC]` "Full Afterburner"; in the vanilla model the end of `fcs/throttle-cmd-norm` is the AB detent |

**Documented model deviation** `[MESS]`: a full-deflection rotation from brake release produces almost
no pitch response below ~150 KCAS (elevator effectiveness ∝ q ∝ V²). The clean configuration with
little fuel (~20,600 lb) does not fly until ~170 KCAS, well ABOVE the table Vr of 128–130 kt for that
weight, and the attitude achievable at the natural lift-off point is ~5° — independent of when the pull
begins. `RotationSpeedKt`/`RotationPitchDeg` stay faithful to the documentation (the PROCEDURE is
right); the difference is the model's low-speed lift/control authority, not a number of this class.
That is principle 5 in its purest form: the model is the reference, its idiosyncrasy is not a defect.

#### 3.2 Landing

All values gear-down / ~40 % fuel.

| Hook | Value | Provenance |
|---|---|---|
| `ApproachSpeedKt` | 154 kt | `[MESS]` — the documentation names an AoA target (11°), not a CAS. Trimmed, gear down, ~40 % fuel, the model holds 11.0° AoA at **154 KCAS** (was 164.9 before `MODEL-DELTAS.md` D1 gave the trailing edge flaps their lift back). The command is an open-loop placeholder for a real AoA loop — faithful to THE MODEL's trim curve instead of a copied real-world number |
| `GlidepathAngleDeg` | 3° | `[DOC]` `navigation-ils.md` "standard glidepath angle" |
| `ApproachSpeedbrakeNorm` | 0.5 | `[SET]` |
| `FlareStartAglFt` | 50 ft | `[SET]` |
| `FlareTargetPitchDeg` | 12.5° | `[DOC]` short final (touchdown ≤ 13° AoA) minus margin against the 15° attitude K.O. of the `FBFlightMonitor` |
| `AerobrakePitchDeg` / `AerobrakeSpeedKt` | 12° / 100 kt | `[DOC]` roll-out ("~13° nose-up down to ~100 kt"), same margin |
| `RolloutBrakeNorm` | 0.8 | `[SET]` |

The wheel brakes are NOT gated on `AerobrakeSpeedKt` — they go on the moment the NOSEWHEEL is down
(`FBAirframeControls::GetNoseWheelOnGround`, latched for the roll-out). The procedure names ~100 kt as
the EXPECTATION of when the nose falls, not as a second gate: in the two-point attitude the wings still
carry the aircraft, so a brake has nothing to bite on, and when the elevator loses the attitude EARLIER
than 100 kt (measured: at ~106 KCAS) the aerobrake is over there. The measured cost of the old speed
gate was a 361 m coasting segment at 0.45 m/s² between derotation and the 100 kt mark.

#### 3.3 BFM — measured, not chosen

`make -C sim test-corner` (`test/modules/f16/FBTestCornerSpeed.cpp`) runs entry speed against instantaneous turn
rate on THIS model: 85° bank, full deflection through the model's own FLCS, 20 kt steps 180→620 KCAS at
5,000 m.

| KCAS | 280 | 340 | **380** | 400 | 420 | 500 | 620 |
|---|---|---|---|---|---|---|---|
| °/s | 13,7 | 15,2 | **16,2** | 16,4 | 15,0 | 11,5 | 12,8 |
| g | 3,5 | 4,6 | **5,4** | 5,8 | 5,7 | 5,4 | 7,3 |

| Hook | Value | Provenance |
|---|---|---|
| `BfmCornerSpeedKt` / `BfmCornerG` | 380 / 5.4 | `[MESS]` — the slowest entry within 3 % of the best rate. Re-measured after `MODEL-DELTAS.md` D1: the SPEED is unchanged, the g at it fell 5.6 → 5.4 because the roll-in into 85° of bank no longer brings a spurious lift step with it |
| `BfmMinSpeedKt` | 300 | `[MESS]` — there the rate is ~13 % below the peak |
| `BfmMaxG` | 9.0 | `[DOC]` structural limit |
| `BfmUnloadG` | 3.0 | `[SET]` |
| `BfmControlMinNm` / `MaxNm` / `AspectDeg` / `AtaDeg` | 0.5 / 1.5 / 30 / 30 | `[SET]` — the window that counts as a "control position" |
| `BfmClosureGainKtPerNm` / `BfmMaxClosureKt` | 120 / 200 | `[SET]` — the closure schedule |
| `BfmLeadAspectDeg` / `LeadRangeNm` / `LeadMaxS` / `LagTimeS` | 45 / 3.0 / 4.0 / 2.5 | `[SET]` — pursuit-type switching |
| `BfmYoYoHeightM` | 600 | `[SET]` |
| `BfmScanAfterS` / `AmplitudeDeg` / `PeriodS` | 3.0 / 8.0 / 30.0 | `[SET]` — 8° lies within the ACM box width, a 30 s period ≈ 1.7 °/s: a scan, not a turn |
| `BfmFloorFt` | 2000 | `[SET]` hard deck |
| `BfmRollPlantA` / `BfmRollPlantKDegS` | 0.734 / 78.7 | `[MESS]` ARX(1) over 15,325 ten-Hz samples below the cap — the plant the roll limiter INVERTS (`pilot.md` §5.7). Known limitation, recorded in gap 2.2: it is a small-signal fit and the limiter only ever operates at full deflection, where the same airframe fits K ≈ 110–113 and consequently holds 1.15 × its declared cap |
| `BfmRollRateMaxDegS` | 90 | `[HERL]` `180° / kBfmTurnTimeS` — a reversal IS a 180° roll in the time constant the roll serves. Since `pilot.md` §5.7.3 this is the PEAK of a cap that also carries an extent bound (no more than one reversal per `kBfmTurnTimeS`), so the SUSTAINED rate this airframe flies is its half, 45 °/s. No new hook: the bound is a law and reads this same number |
| `BfmSearchRollCap` | 1.0 | `[SET]` no-op — the F-16's cold search legitimately uses full roll authority (`pilot.md` §5.10 screw 3, where the MiG sets 0.20) |
| `BfmWvrCueDeg` | −1 (the round's own gimbal) | `[DOK]` — the generic default, and this aircraft keeps it because **no HMCS is modelled**. `doc/modules/f16/weapons.md` §2.5 gives the F-16 two ways past boresight (HMCS BORE and HMCS radar-BORE) and the tree has neither, so what the jet can hand an AIM-9M is the AIM-9M's own **±30°** [T4, `weapons.md` §4.3]. The MiG overrides with its helmet sight's 60°. `pilot.md` §5.11 |

**Two accepted model properties** (principle 5, explicitly NOT defects to be fixed): the vanilla FLCS is
a PITCH-RATE command (`f16.xml`'s pitch channel differentiates the stick against 6.2·q and only
0.02·nz), so full deflection at corner buys ~5.4 g instead of the 9 g structural limit, and the best
turn rate is ~16.4 °/s instead of the real jet's ~20+. The measured corner nevertheless lies WITHIN the
330–440 KCAS corner PLATEAU published in `aerodynamics-performance.md` — the strongest available
cross-check for a number the documentation deliberately does not tabulate.

#### 3.4 Intercept (BVR) — every number from one of the two boxes

The core of this section: none of these numbers is chosen, every one is derived from the APG-68
geometry OR the AIM-120 launch zone.

| Hook | Value | Derivation |
|---|---|---|
| `SearchRadarModeOrdinal` | 1 (= `FBF16FcrMode::Crm`) | `[ABL]` CRM is the set's power-up mode AND the only F-16 mode that searches BIG and does NOT lock by itself. As an ORDINAL, not as a name — the generic layer must know neither |
| `InterceptSpeedKt` | 550 (TRUE) | `[ABL]` the unit the AP speed loop controls in. At 8,000 m that is ~375 KCAS = the measured corner speed (380). Two things at once: the launch speed the round inherits (starting point of the Raero integration), and a jet already at its best turn rate when the engagement stops being BVR |
| `InterceptLockRangeNm` | 16 | `[ABL]` outside EVERY head-on measured Rtr of this weapon (~11 nm at 8,000 m, launch record from `missions/intercept-aim120.fbm`) and inside the APG-68's 40 nm search gate — the single-target track has settled and fire control has published a launch zone before the shot range arrives, and the warning to the target lasts seconds instead of a minute |
| `InterceptShotRtrFactor` | 1.0 | `[SET]` shot at Rtr |
| `InterceptShotAtaDeg` | 30 | `[ABL]` an AIM-120 leaves the rail pointing at the nose and has to pull onto the target; more than ~30° of catching up costs it its motor |
| `InterceptShotSpacingS` | 12 | `[SET]` |
| `InterceptCrankAtaDeg` | 45 | `[ABL]` STT gimbal ±60° (`FBF16Fcr::kGimbalAzDeg`) minus the 15° reserve a manoeuvring target needs before the track breaks. Cranking to the limit is how a shot ends up unsupported |
| `InterceptAbortRangeNm` | 5 | `[ABL]` below that the Rmin of a BVR weapon and the merge are the same problem, and `doc/modules/f16/` calls the fight from there on a dogfight, not an intercept |
| `InterceptBeamOffsetDeg` | 90 | `[ABL]` abeam the threat bearing your own radial velocity is zero = inside the other side's Doppler notch |
| `InterceptChaffIntervalS` | 3.0 | `[SET]` |
| `InterceptDefendHoldS` | 12 | `[SET]` |

#### 3.5 Attack (air-to-ground)

None of these numbers touches WHERE the round lands — that is fire control's business, the pilot only
reads it.

| Hook | Value | Provenance |
|---|---|---|
| `AttackReleaseBiasS` | 0.0 | `[SET]` release ON the cue. A hook, not a setting: a mission that wants a wrong delivery declares it (`set pilot_attack_bias_s`) |
| `AttackCcipTolM` | 45 m | `[ABL]` from the weapon's EFFECT: a Mk-82 takes out a soft installation up to ~25 m and degrades it up to ~45 m (`modules/ground/FBGroundTarget.h`) — beyond that the run would achieve nothing, and that is exactly when a pilot does not pickle but comes back around |
| `AttackEgressTurnDeg` | 135 | `[SET]` a turn well past the abeam point, i.e. away from your own impact rather than along it |
| `AttackEgressClimbM` / `RangeM` / `S` | 600 / 12,000 / 30 | `[ABL]` the egress leg is a PLACE in the world, not a heading, so it needs a distance far enough ahead (otherwise the turn would be an arrival) and a hold time in which it completes at this turn rate — 135° at a 30 °/s-limited roll rate and standard bank limit sits comfortably under 30 s |

---

### 4. `FBF16Fcr` — the APG-68 as a MODE SET

`sim/src/modules/f16/FBF16Fcr.{h,cpp}` — a derivation of `sensors/FBRadarSystem`.

#### 4.1 What is F-16 here, and what is not

**Not here** (all in the generic system): detection geometry, track establishment
(`kHitsToFirm` = 2 consecutive looks), coast (`max(kMinCoastS 1 s, kCoastFrames 3 · FrameS)`),
anonymity of the contacts, IFF Mode 4, emission signature, chaff/Doppler-notch behaviour, range scaling
under damage.

**Here**: the MODE SET. The whole taxonomy is ONE question — "which pattern is the antenna flying right
now?" — and that is exactly the one overridden hook `FBRadarSystem::ActiveVolume()`. Second override:
`ModeOrdinal()` (the `fcr_mode` telemetry column).

Taxonomy `[DOC]` `radar-sensors.md` (Chuck Part 10): BVR search / ACM / STT; CRM is the power-up mode, a
large-volume RWS search that does NOT lock by itself; the four ACM sub-modes (HUD Scan, Vertical Scan,
Boresight, Slewable) "auto-lock the first target in a close-range volume tied to HUD geometry" — the
self-acquisition IS the purpose, because in a turning fight nobody operates a radar, so the sub-mode is
the AIMING DEVICE; a lock is STT.

#### 4.2 The volumes

**Status statement, verbatim from the header:** `radar-sensors.md` documents the TAXONOMY and gives NO
angles (the guide shows the modes as MFD screenshots). The geometry below is therefore a **MODEL
PARAMETER SET** `[SET]` — matched to the publicly cited DCS F-16C ACM patterns (30×20 HUD scan, ~10°
boresight cone, narrow-tall vertical scan, 20×20 slewable, all at the ~10 nm ACM gate) as well as the
APG-68's ~40 nm search range against a fighter target and its mechanically scanned 4-bar elevation
coverage. Declared as a setting, NOT dressed up as a quotation — the same status as
`FBDatalinkSystem::kGenericRangeNm`.

| Mode | Azimuth | Elevation | Range | Frame | Auto-lock | What it is |
|---|---|---|---|---|---|---|
| `off` | — | — | — | 1.0 s (never swept, only keeps the raster well defined) | — | the set does not radiate |
| `crm` | ±60° | ±10.5° about `SlewEl` | 40 nm | 4.0 s | no | RWS search, power-up mode |
| `acm_hud` | ±15° | ±10° | 10 nm | 1.0 s | **yes** | the 30×20 HUD scan box |
| `acm_bore` | ±5° | ±5° | 10 nm | 0.3 s | **yes** | the ~10° boresight cone |
| `acm_vert` | ±5° | −13°…+47° (centre +17°, half-height ±30°) | 10 nm | 1.2 s | **yes** | narrow and tall, for the pull |
| `acm_slew` | ±10° about `SlewAz` | ±10° about `SlewEl` | 10 nm | 0.8 s | **yes** | the 20×20 slewable box |
| **STT** (locked) | ±60° | ±60° | 40 nm | 0.1 s | yes | the gimbal envelope, `SingleTarget = true` |

Constants: `kAcmRangeNm` 10, `kSearchRangeNm` 40, `kGimbalAzDeg` = `kGimbalElDeg` = 60.

**What about the numbers is NOT arbitrary** (and is therefore the real object of the test): the frame
times follow the volumes. A mechanically scanned antenna takes longer for a wider pattern — the
boresight cone acquires in a fraction of the HUD box's time, CRM is the slowest of all. **This
relation, not the absolute seconds, is what the mode proof measures.**

Further reasoned details:

- **Vertical scan is deliberately ASYMMETRIC** (−13…+47): the mode exists to be pulled THROUGH a target
  during a high-g turn, so it reaches far above the boresight and barely below it. It is the reason why
  `FBRadarScanVolume` carries a CENTRE and not merely a half-angle.
- **CRM's elevation CENTRE is the antenna-elevation control**, not a fixed zero: the 4-bar pattern
  covers ±10.5° about WHERE the pilot has pointed, and at BVR range that is a few thousand feet of
  altitude band — the wrong altitude is the classic way of flying past a target the radar could easily
  have seen. It is carried by the SAME `SlewEl` as the slewable ACM box, because it is the same
  control. Azimuth stays nose-centred (±60° covers everywhere the jet can turn within a search cycle
  anyway). Default 0 — a mission that never touches the control gets exactly the pattern it did before.
- **`off` beats the lock:** switching a set off while a target is locked has to STOP the antenna, not
  let it stare through an OFF mode (the base class then drops every track, as on a shutdown). Every
  other mode hands the lock over to STT.
- **STT keeps `AutoAcquire = true`** so that the base class retains the held lock; if it is lost, the
  pattern falls straight back into the sub-mode's box.
- **`SetRangeOverrideNm`** overrides the gate of EVERY mode with ONE number (0 = back to the table). A
  mission that tests the gate at all holds the geometry fixed and varies only the range — hence one
  value instead of six.

#### 4.3 No symbology — deliberately

`doc/modules/f16/hud-symbology.md` documents **no** TD box and **no** locked-target symbol; its radar-adjacent
entry is the HMC (HUD Mark Cue), a mark-point designation and thus a different function. Drawing a TD
box would mean INVENTING symbology. The lock therefore stays in `FBState`, telemetry and events until
the symbology reference genuinely covers it.

---

### 5. `FBF16Datalink` — MIDS-LVT

`sim/src/modules/f16/FBF16Datalink.h` (header-only) — a derivation of `sensors/FBDatalinkSystem`.

`[DOC]` `datalink-iff.md` (Chuck Part 13):

| Quantity | Value | Note |
|---|---|---|
| `kMidsRangeNm` | **300 nm** | Link-16 / MIDS-LVT, UHF line-of-sight TDMA net; replaces the deliberately generic placeholder of the base class |
| Binding horizon in practice | radio horizon of both altitudes | two jets at 2,500 m each see one another out to ~223 nm — the BASE CLASS's business, not this one's |
| Contact filter | FR ON (all friendlies) / FL ON (flight leads only) / FR OFF (none) | the ONLY thing overridden here: `AcceptContact` |
| Default | `FriendlyAll` | HSD default FR ON |

**Honesty note on the flight lead:** the simulator has NO formation structure concept — there is no
element/pair assignment from which a lead could be read. `FL ON` therefore keeps the FIRST participant
of the formation, which for a mission-declared formation is the first `unit` block of that team
(= primary actor, `doc/missions/INDEX.md`). A documented placeholder for a real lead assignment, not a
model of one.

Mission switches: `datalink`, `datalink_xmt`, `datalink_filter`, `datalink_range_nm` (see 2.6). Command
bus targets: `DatalinkPower`, `DatalinkTransmit`, `DatalinkFilter`, `DatalinkRangeNm`.

---

### 6. `FBF16Rwr` — AN/ALR-56M

`sim/src/modules/f16/FBF16Rwr.h` (header-only) — a derivation of `sensors/FBRwrSystem`. Every point
`[DOC]` from `defence-rwr-cm.md` §2.1.

| Quantity | Value | Meaning |
|---|---|---|
| `kElevCoverageDeg` | **±45°** | Four high-band quadrant antennas + a dual-blade low-band pair give 360° AZIMUTH, but only ±45° in ELEVATION. |
| `kPriorityThreats` | 5 | TWP MODE = PRIORITY |
| `kOpenThreats` | 16 | TWP MODE = OPEN |
| Default display | PRIORITY | that is how the jet powers up |

**The blind zone is the result-relevant number,** and that is why it comes first in the header: the
source spells the consequence out — "a genuine RWR blind spot directly above/below the fuselage
centerline; high-pitch or high-bank defensive maneuvers can rotate a hostile radar into that blind spot,
silently dropping lock/launch warnings". That is not a display quirk but a hole in situational awareness
that your own manoeuvring TEARS OPEN.

**The display cap is a DISPLAY limit above a detection that keeps running.** The source is explicit:
more threats can be detected than are displayed. `MaxDisplayed()` therefore acts only on the PUBLISHED
list, above a detection table that keeps hearing and ranking everything. OPEN-16 exceeds the table size
`kMaxRwrThreats` — deliberately left as the documented number instead of being trimmed to it, so that
the cap stops being the binding limit as soon as the table grows.

**What is missing and named:** the threat LIBRARY (Appendix B's ALIC/symbol/system correlation). The
source describes its structure and does not transcribe it; inventing symbol codes would mean guessing
exactly what an RWR must not guess. The classification stays the generic emitter-class estimate until a
real library exists.

Mission switches: `rwr`, `rwr_search`, `rwr_display`.

---

### 7. `FBF16Cmds` — AN/ALE-47

`sim/src/modules/f16/FBF16Cmds.{h,cpp}` — a derivation of `sensors/FBCountermeasureSystem`.

| Quantity | Value | Status |
|---|---|---|
| `kTypicalChaff` / `kTypicalFlare` | 60 / 60 | `[DOC]` §1: "ground crew sets loadout, max 120 combined (typical 60/60)" |
| `kMaxCombined` | 120 | `[DOC]` §1 — the ceiling the mission line is checked against |
| `kBingoChaff` / `kBingoFlare` | 10 / 10 | `[SET]` — §2.2's CMDS BINGO page is 0–99 on the pilot's side; ten per type is this jet's brief, i.e. the point at which AUTOMATIC dispensing stops and the rest belongs to the pilot |

**Schema vs. values, cleanly separated:** the parameter SCHEMA (burst/salvo quantity and interval per
type) is the source's `[DOC]` §2.2. The VALUES are FlightBox' own `[SET]` and marked as such in the
header — the guides document the DED page and its value ranges, never what the six programs are loaded
with; that is a squadron's business, not a manual's. What each table entry was chosen for is stated per
program, so that a mission with a different pattern changes a NUMBER and not the model.

| # | Name | Chaff (burst size × interval, salvo count × interval) | Flare | Total | Task |
|---|---|---|---|---|---|
| 1 | BREAK LOCK | 2 × 0.10 s, 2 salvos at 1.00 s | — | 4 cartridges in ~1.1 s | The dense reflex and the answer to a MISSILE: four clouds within a second put several competing echoes into the seeker beam while the geometry is still changing fast |
| 2 | MIXED | 2 × 0.10 s, 2 salvos at 2.00 s | 1 × 0.10 s, 2 salvos at 2.00 s | — | Unknown threat: something for a radar and something for an IR seeker, at the price of two magazines at once |
| 3 | FLARE | — | 2 × 0.10 s, 4 salvos at 1.00 s | — | Infrared only. It is dispensed and counted; there is (as yet) nothing to decoy, and the generic system says so openly |
| 4 | SUSTAINED | 2 × 0.10 s, 4 salvos at 4.00 s | — | 8 cartridges over ~12 s | Answer to a radar that is only TRACKING, and the one AUTO repeats: slow enough that a long lock does not empty a 60-round magazine before the fight is decided; dense enough that a cloud is always still within its lifetime (`core/FBCountermeasure.h`'s `kChaffLifeS`) when the next appears |
| 5 | SLAP | 1 × 0.10 s, 1 salvo | 1 × 0.10 s, 1 salvo | 2 | the dispense button on the left console |
| 6 | BYPASS | 1 × 0.10 s, 1 salvo | 1 × 0.10 s, 1 salvo | 2 | exactly one chaff + one flare, `[DOC]` §2.2 |

**Not overridden:** the threat→program mapping. The generic doctrine (a dense pattern against a missile,
a sparingly repeating one against a track) is exactly what this table was built around; an override
returning the same two numbers would be the empty derivation CLAUDE.md forbids.

Mission switches: `cmds_mode`, `cmds_program`, `cmds_chaff`, `cmds_flare`; pilot brief `brief_chaff_s`.

---

### 8. `FBF16FireControl` — the fire-control system

`sim/src/modules/f16/FBF16FireControl.{h,cpp}`. Not a generic slot: the conventions are THIS aircraft's.
Four products, ONE box, ONE bus block (`FBFireControlBlock`).

READS the nav, platform, air-data and radar blocks, WRITES the fire-control block — a documented
FUSION, which is why it checks the validity heads: without a valid slant-range source the whole block
becomes invalid, without a radar lock it keeps publishing but with `DlzValid = false` ("no launch
solution" is an answer, not an absence).

#### 8.1 The 'B' range source

Slant range to the active steerpoint = `sqrt(horizontal distance² + altitude difference²)` against the
steerpoint's OWN elevation (baro method), NOT FCR ranging. `RangeProvider = 'B'`.

Evidence: `[DOC]` `hud-symbology.md` ("B: Range computed using steerpoint elevation/barometric
elevation") and cross-checked against the GPL-2.0 FlightGear F-16 mod
(`steerpoints.getCurrentSlantRange()` — the same Pythagorean method; **only the FORMULA** compared, no
code copied).

#### 8.2 The air-to-air launch zone (DLZ)

`SolveLaunchZone(perf, ownSpeedMs, altM, rangeM, closureMs, ownLosMs, tgtSpeedMs)` — a free STATIC
function, because it is pure arithmetic over a weapon performance table plus ONE geometry: no member
state, hence also callable from a harness or a future AI.

Model: `dv/dt = (T(t) − ½·ρ·v²·CA·S)/m(t)`, T stepped boost → sustain → 0, m falling linearly over the
burn time. ONE density for the whole integration (launch altitude, `core/FBAtmosphere.h`). Step size
`dt = 0.25 s`, cap 240 s. The round is DEAD as soon as `v < MinSpeedMs` — after that it can no longer
fly an intercept, and that bounds every range from below. The test runs only AFTER the boost, because
the round starts below MinSpeed whenever the shooter is subsonic.

From ONE integration:

| Output | Calculation | Meaning |
|---|---|---|
| `Raero` | `S_m + Vt_los·T_death` | the target keeps closing with its current LOS component |
| `Rtr` | `S_m − tgtSpeed·T_death` (≥ 0) | the target turns around at launch and runs away (`[DOC]` weapons.md §2.5) |
| `Rmin` | `closure·ArmingS + kMinTurnM` | separate, ignite, arm — and then still pull onto the target |
| `TimeToActiveS` | the moment at which r ≤ `ActivationRangeM` | seeker activation |
| `TimeToImpactS` | the moment at which r ≤ 0 | −1 means "the round dies before that", which is different from 0 |

`kMinTurnM` = 300 m `[SET]`.

**Why the copy is COARSER than the weapon model — and deliberately so:** what is integrated is the
STORED performance table of the fire-control computer (`core/FBStore.h`'s `FBWeaponPerf`), deliberately
a coarse copy of what the missile's own JSBSim model actually does. A fire-control computer works from
a table, and the ERROR between its prediction and the flown result is a real property of every shot.
The intercept mission MEASURES exactly this error instead of computing it away.

**Explicitly NOT modelled** (named in the header): the lofted midcourse of a real AMRAAM (a good part
of the range at altitude), the target's own altitude change, and the energy cost of the terminal turn.
All three would shift Raero in a way that could not be evidenced from anything available here — and a
launch zone that claims precision it does not have is worse than one whose simplifications are written
down.

`InZone` = `range ∈ [Rmin, Raero]`. The DLZ is computed only for a GUIDED weapon on the SELECTED
station and with an existing radar lock; splitting the measured closure into "mine" and "his" uses the
body-referenced angles of the SAME contact, so there is no second notion of where the target is.

#### 8.3 The EEGS gun solution

Ballistics is shared (`core/FBGunBallistics` — the same arithmetic the projectiles are afterwards flown
with; for an unguided projectile a fire-control computer solves exactly that). FIRE CONTROL is the
three questions the funnel answers: is the target in the range window, how big is it there in angle,
and how far is the required lead from the nose direction.

| Constant | Value | Status |
|---|---|---|
| `kFunnelMinRangeM` | 182.88 m (600 ft) | `[DOC]` weapons.md §2.5 "Funnel geometry (Level II)", quoted exactly and converted once |
| `kFunnelMaxRangeM` | 914.40 m (3,000 ft) | `[DOC]` ibid. |
| `kTargetSpanM` | 9.144 m | `[MODELL]` — the source says explicitly that the target span MUST be configured, and does not supply it. A like-type target is assumed, and for that the `<wingspan>` of the pinned `f16.xml` (30 ft): a real number from the model AND the right one for every engagement this simulator can fly today (F-16 against F-16). A future FCC that knows what it sees replaces the constant with the span of the identified type — the geometry does not change |

`GunInFunnel` = in range AND nose within the tolerance the geometry itself sets:
`0.5·span/range + 1.5σ` of the dispersion cone `[ABL]` — half the angular extent (aiming at the centre,
the skin is still half a span away) plus 1.5 sigma of the pattern. That is EXACTLY the condition under
which the hit-density model puts a meaningful number of rounds onto the target, so trigger release and
damage arithmetic cannot drift apart.

#### 8.4 The air-to-ground release solution (CCIP/CCRP)

Arithmetic = `core/FBBallistics` (shared: CCIP and CCRP are the same integration with two questions and
must not be able to contradict each other). **What lives here are the THREE INPUTS — each one a
convention of THIS jet, not a calculation:**

| Input | What | Why exactly that |
|---|---|---|
| **Release state** | position + full velocity vector from the platform block | A store leaves the pylon with the carrier's motion (the separation IC in `missions/FBMissionBoot.h` says the same); the station offset is metres against a fall of kilometres, so the computer works from the CG — the SMS's pylon geometry is not fire-control knowledge |
| **Aim point** | the active steerpoint | `[DOC]` weapons.md §2.2 names the steerpoint as the aim point of "pre-planned" delivery. Both modes solve against it: CCRP because that IS its designation; CCIP because an AI has no eye and the steerpoint is the briefed point. Reconstructed from the bearing/distance of the NAV BLOCK, not from a second steerpoint copy — the box reads the bus like any other consumer (one writer per block) |
| **Impact plane** | the elevation of the steerpoint | the same 'B' ranging source this box already uses for the slant range = the module's elevation sample = the number the radar altimeter reads as well. **This file NEVER queries terrain; a fire-control computer cannot.** |

The release cue lives on the GROUND TRACK (what the air-data block publishes and what the round
inherits), not on the heading — and its head is checked: without air data there is an impact point but
no release cue, which is a real distinction and not a missing number.

A guided weapon or an empty station leaves the whole solution invalid: a computer without a bomb has no
impact point, and that is different from an impact point of zero.

`FBReleaseSolution` (predicted impact, time of flight, aim point, miss, arming reserve, mode, stamp) is
handed to the SMS and leaves the jet WITH the weapon — so that the owner of the simulation can lay
prediction and measured impact side by side and quantify the error.

`SetDeliveryMode` (from `set attack_mode`) changes NOTHING about the arithmetic — both cues come from
the same solution. The mode is what the release RECORD has to carry: the statement of which of the two
cues the pilot released on.

#### 8.5 The target estimate

A radar contact is an echo without a velocity (`core/FBRadarContact.h`), but launch zone and midcourse
need to know where the target is GOING. This box therefore holds its OWN `pilot/FBBfmTrack` instance,
fed exclusively from the LOCKED contact, and publishes the result as `FBWeaponTargetState`. The SMS
copies it onto the round at launch and radiates it as the guidance uplink for as long as the lock
holds.

**The PILOT's BFM tracker is a SEPARATE instance for a separate consumer** (his pursuit control) and
stays unaffected by this: one tracker per consumer, no shared mutable picture.

---

### 9. The pure geometry/equipment classes

Common pattern: **the BEHAVIOUR is airframe-agnostic and lives in the generic system, the INSTALLATION
is not and lives here.**

#### 9.1 `FBF16Sms` — the nine pylons

`sim/src/modules/f16/FBF16Sms.{h,cpp}`. Everything the SMS DOES (inventory, master-arm interlock,
release path, point-mass/drag effect of the carriage) is `weapons/FBStoresSystem`. Here stands
exclusively WHERE the nine pylons sit — and hence what a loadout does to the balance.
`DeclareStation(number, xIn, yIn, zIn)` in the structural-frame unit of the loaded model (inches).

| Station | y (BL, in) | Anchoring |
|---|---|---|
| 1 / 9 | ∓180 | `[MODELL]` wing tips = half the model's span (`<wingspan>` 30 ft → 180 in) |
| 2 / 8 | ∓140 | `[SET]` distributed between the two anchors |
| 3 / 7 | ∓105 | `[SET]` ditto |
| 4 / 6 | ∓65 | `[MODELL]` the butt line of the model's own external tanks, which its propulsion section itself calls "(station 4)"/"(station 6)" |
| 5 | 0 | centreline |

| Axis | Value | Rationale |
|---|---|---|
| x (longitudinal) | −193 in for EVERY station | `[MODELL]` + `[DOC]`: the model's CG station. `weapons.md` §4.5 marks the public station data as **T4** ("community, not independently confirmed") — so there is no citable fuselage station per pylon, and putting a loadout on the CG station is the MINIMAL assumption: it adds mass and drag without inventing a pitching moment nobody can evidence. The LATERAL offsets, by contrast, are modelled because they follow from the wing geometry the model declares |
| z | −30 in | `[MODELL]` under the wing: the model's wing tanks sit at z = −15, its main gear at z = −72 — a pylon-carried store in between is geometry, not an estimate |

Master arm starts SAFE (the generic system's default) and is armed the way a pilot does it: over the
command bus, from the mission's brief.

#### 9.2 `FBF16Gun` — the installation of the M61A1

`sim/src/modules/f16/FBF16Gun.{h,cpp}`. The same pattern: all behaviour (drum accounting, interlocks,
trigger command, burst stream) is `weapons/FBGunSystem`. Here: WHICH gun this jet carries (`kM61A1`
from `core/FBGun.h`) and WHERE it sits. `Install(spec, fwdM, rightM, downM, boreDownDeg, boreRightDeg)`
in the body frame (+forward/+right/+down from the CG, metres).

| Axis | Value | Derivation |
|---|---|---|
| forward | **+4.6 m** | a `[MODELL]`-backed `[SET]`: the model puts its CG station at FS −193 in and the nose at the forward end of a 49 ft 5 in fuselage; the muzzle port — on the real jet just forward of the windscreen — thus lands ~180 in ahead of the CG |
| right | **−0.9 m** | `[SET]` to port: the strake installation is on the LEFT, which is why the number is negative and not zero — worth a metre of lateral offset at the muzzle |
| down | **−0.3 m** | `[SET]` slightly above the reference line: the port sits high on the strake transition |
| Bore | **0°/0°** | `[SET]`, and explicitly justified as such: the real gun is boresighted to a small depression that the aiming system compensates — but NO source in `doc/modules/f16/` names the angle, and inventing one would skew every burst this simulator fires. Zero is the honest choice and is written out rather than left as an omission |

`weapons.md` documents no gun installation coordinates at all (§4.5 itself marks the station data as
T4). The three offsets are therefore minimal assumptions consistent with the airframe and not
quotations — and their MAGNITUDE is the point: they shift the muzzle by metres, which at 600–3,000 ft
of combat range is fractions of a milliradian.

#### 9.3 `FBF16Ufc` — ICP/UFC/DED, reduced

`sim/src/modules/f16/FBF16Ufc.{h,cpp}`. Not a generic slot: the ICP/DED is THIS type's control head.
Reduced to what it really owns today — the COMMITTED values of three DED fields (CARA ALOW floor, BNGO
fuel threshold, selected steerpoint number) — and publishes them as the UFC block. Here ENDS the
propose→commit/reject cycle from `doc/modules/f16/controls-commands.md` §1.2.

| Constant | Value | Status |
|---|---|---|
| `kBingoCeilingLbs` | 6070 lb | `[DOC]` §6.8 (FUEL QTY SEL = NORM) |
| `kAlowMinFt` / `kAlowMaxFt` | 0 / 50,000 | `[SET]` — FlightBox' OWN range policy |
| `kBingoMinLbs` / `kBingoMaxLbs` | 0 / 20,000 | `[SET]` ditto |
| `kSteerNumMin` / `Max` | 1 / 99 | `[SET]` ditto |
| Default ALOW | 500 ft | `[SET]` MIL-STD-1787 placeholder |
| Default BNGO | 0 | 0 = no threshold entered, the warning stays inhibited |

**Two documented failure forms, deliberately DIFFERENT:**

- **BNGO clamp** (`[DOC]` §6.8): the entry is ACCEPTED — ENTR succeeds, the field shows what was typed —
  but the warning fires independently of that at the system ceiling. Modelled as accept-with-clamp, not
  as a rejection, because that is what the jet does. `EffectiveBingo()` makes the clamp explicit for the
  warning system.
- **Range rejection**: NEITHER source documents any numerical range check on a DED field. The limits
  above are therefore FlightBox' own modelling decision and are reported as such
  (`FBCommandReason::OutOfRange`), never clamped silently. Silence is the one behaviour the material
  rules out: every documented DED error is VISIBLE to the pilot.

---

### 10. `FBF16Damage` — zones and fragility

`sim/src/modules/f16/FBF16Damage.{h,cpp}`. Pure MODULE DATA, supplied via `FBModule::DamageLayout()`;
it is applied exclusively by `core/FBDamageModel` (geometry, energy, thresholds). Which box sits in the
radome and which in the tail is known only to the aircraft.

#### 10.1 The axis is the model's

Every zone boundary is read from the structural frame of the pinned `f16.xml` (x positive AFT, CG at
FS −193 in) and converted into metres AHEAD of the CG. **No number comes from a drawing or a manual** —
all `[MODELL]`:

| Reference in f16.xml | FS (in) | m ahead of CG |
|---|---|---|
| RADOME contact point (nose tip) | −486.6 | **+7.46** |
| EYEPOINT (cockpit) | −336.2 | +3.64 |
| Nose gear | −299.6 | +2.71 |
| CG | −193.0 | 0.00 |
| Main gear | −158.6 | −0.87 |
| Wing tips | −121.3 | −1.82 |
| Ventral fins (start of the engine bay) | −97.6 | −2.42 |
| Thruster (nozzle) | 0.0 | −4.90 |
| Arresting hook (aftmost extremity) | +100.7 | **−7.46** |

Cross-check: +7.46 to −7.46 = 14.9 m of airframe — that IS the F-16's own length of 15.03 m.

#### 10.2 The four sections and their systems

Which system sits where is the arrangement every F-16 photograph shows; a MEASUREMENT is explicitly not
claimed for it.

| Zone | Range (m ahead of CG) | Systems (degrade/fail class) |
|---|---|---|
| **Nose** | +3.64 … +7.46 | Radar (avionics) — APG-68 antenna + transmitter; AirData (avionics) — pitot/AoA probes at the cone; Structure |
| **Forward** | 0.00 … +3.64 | Nav (avionics) — INS; **Gun (STRUCTURE)**; FireControl (avionics) — FCC; RadarAlt (avionics) — CARA; Datalink (avionics) — MIDS terminal; Structure |
| **Center** | −2.42 … 0.00 | Stores (avionics) — SMS + station wiring at the wing roots; FlightControls (FLCS) — hydraulics + actuator runs; Structure |
| **Aft** | −7.46 … −2.42 | Engine; Rwr (avionics) — ALR-56M aft receivers; Countermeasures (avionics) — ALE-47 dispensers; FlightControls (FLCS) — empennage actuators; Structure |

**Why the gun gets STRUCTURE thresholds** and not avionics ones: M61A1 and drum sit in the left wing-root
strake, hence in this zone — but a gun is a mechanical installation with the mass and cross-section of
the airframe around it, not a black box in a rack. What stops it is what punches through the structure
it is bolted to. Not an effect but a statement about the component.

#### 10.3 The four fragility classes — the actual SETTING

All `[SET]`, in J/m² of fragment areal energy. This is the real modelling decision of this file and
stands named as such in it:

| Class | Degrade | Fail | Rationale of the component |
|---|---|---|---|
| Avionics | 1.2e4 | 3.0e4 | one box: thin skin, no redundancy |
| Engine | 5.0e4 | 1.5e5 | accessories/nozzle: military thrust only from then on |
| FLCS | 5.0e4 | 1.5e5 | one of two hydraulic systems |
| Structure | 8.0e4 | 2.5e5 | skin and stringers: resistance |

Scale (AIM-120, 20.5 kg warhead, ~850 m/s head-on closure):

| J/m² | 1.2e4 | 3.0e4 | 5.0e4 | 8.0e4 | 1.5e5 | 2.5e5 |
|---|---|---|---|---|---|---|
| corresponds to r ≈ | 11.6 m | 7.3 m | 5.7 m | 4.5 m | 3.3 m | 2.5 m |

Read: everything that triggers the proximity fuze at all costs avionics; only a burst within ~3 m takes
the engine or the flight controls with it. The ladder is chosen ONCE such that it reads against the
geometry of this airframe the way a fragmentation warhead behaves; **every intermediate case then
follows from the 1/r² energy law and not from another number.**

**Why most boxes are never "degraded":** apart from the radar (whose range follows from the radar
equation, `kRadarRangeDegraded` = 0.7071) no avionics device has DERIVABLE degradation behaviour. Every
such box therefore declares its degrade threshold EQUAL to its fail threshold and never enters the
state. Modelling "a bit of noise" on an INS or ADC would mean inventing a number — this file does not
do that.

#### 10.4 Presented areas (what a projectile stream sees)

| Quantity | Value | Status |
|---|---|---|
| `FrontalAreaM2` | 4.0 | `[SET]`, from `[MODELL]` geometry: fuselage ~1 × 1.5 m cross-section plus the thin edge of a 27.9 m² wing and the fins. An EQUIVALENT AREA — nowhere is the real shape projected |
| `LateralAreaM2` | 14.0 | `[SET]` ditto: `<wingarea>` 27.9 m² / `<wingspan>` 9.14 m over 14.5 m of length → a planform of that order, the side view considerably less; 14 is the middle, and a SINGLE number for "across the axis" cannot honestly be more |
| `FrontalExtentM` | 4.57 | `[MODELL]` half the span (9.14/2) — seen from astern |
| `LateralExtentM` | 7.3 | `[MODELL]` half the length (14.5/2) — seen from the side |

Two scales, because a fighter has a lot of span and little material. The areas scale the expected hit
count LINEARLY — which is why they stand once and named at this place.

---

### 11. `FBF16Max7456` — the chip hook

`sim/src/modules/f16/FBF16Max7456.{h,cpp}`. A REAL NoOp override point held and instantiated by
`FBF16Module` (`StyleGlyph(x, y, r, g, b)` = identity), not a dead placeholder.

**Why separate from the font system:** `systems/FBHudFont.h` + `FBHudStage`'s text path are the GENERIC,
airframe-agnostic bitmap font machinery (16×16 cells with real 8-bit area coverage, baked from B612
Mono; "sharp bilinear" reconstruction in the shader). NOTHING chip-specific belongs there. MAX7456
idiosyncrasies — interlace jitter, brightness/contrast curve, edge enhance, sync artefacts — are
properties of ONE OSD chip in ONE aircraft. They therefore get this override point (the same pattern as
the one virtual point of `FBAutopilot`/`FBFlightControl`), on which a future artefact model hangs
without touching `render/` or another module.

---

### 12. `displays/FBF16Hud` — the symbology

`sim/src/modules/f16/displays/FBF16Hud.{h,cpp}`. Overrides `FBDisplaySystem::BuildHud(state, env, out)`
— the displays override point. **Pure symbology: reads `FBState`, writes nothing.**

Source situation: `[DOC]` `doc/modules/f16/hud-symbology.md`, DCS F-16C Viper Guide Part 16 p.706 as the frame
of reference; positions cross-checked against the GPL-2.0 FlightGear F-16 mod (github.com/NikolaiVChr/f16,
`Nasal/HUD/HUD_main.nas` + `hud_math.nas`) — **FACTS verified, no code copied**.

#### 12.1 The drawn window

The real F-16 HUD is a small window in front of the pilot; two documented angular specifications `[DOC]`
(hud-symbology.md's "Technical depth"; DTIC ADA430578's TFOV note, cross-checked against `TFOV=25deg`
in the FlightGear mod) describe it: **TFOV ~25°**, **IFOV ~20 × 13.5°**.

**FlightBox does not draw that window any more.** The owner's ruling this round — *"das HUD … nutzt
nicht die kompletten oberen zwei Drittel des Bildschirms"* — makes the WINDSCREEN the aiming surface:
the drawn window is the grid's top two rows inset by `kWindowInsetPx` = 10 px (1260 × 460 at 720p,
against the 520 × 348 combiner rectangle drawn before). The ~25° aperture survives as a documented
reference in `hud-symbology.md`, and the deviation is booked there as a deviation.

What did NOT change is the SCALE. `Kc` is the scene's own projector,
`Kc = (Height/2)/tan(kSceneVerticalFovDeg/2) = 623.5` px per unit tangent at 720p — the world is not
stretched to fill the window, only the symbology's own extents are. `kHudScale` = 1.9 `[SET]`
(successor to `kHudMagnify`) is the fixed pixel scale of symbols, ticks and text; it deliberately does
NOT grow with the window, or a larger screen would turn the HUD into a billboard.

**A defect fell out of this and was fixed, not booked:** the HUD's projector had been built with a
**80°** field of view while the scene renders at **60°**, so the symbology was not conformal at all —
it was compressed toward the boresight by 623.5/429 = 1.45. Both now read the single constant
`core/FBCamera.h` `kSceneVerticalFovDeg`. Measurement (HUD horizon ink row vs. the projection, three
camera pitches): residual ≤ 1.2 px against the conformal prediction, up to 40 px against the old one —
table in [`hud-symbology.md`](hud-symbology.md).

Text-scale FLOORS `kHudReadoutScale` 1.15 / `kHudSecondaryScale` 1.08 `[ABL]`, unchanged: 1.15·1.9 ≈
2.19 (ink ~9.8 px, above the 9 px floor for primary readouts), 1.08·1.9 ≈ 2.05 (~9.2 px, above the
8 px floor for secondary text).

#### 12.2 The elements

Everything conformal runs through ONE az/el projector from THE SAME camera basis (yaw/pitch/roll) the
generic default HUD also uses. "Az/el" is WORLD-referenced (0 = north, +el = up) — a world direction
(ground track, bearing to the steerpoint) therefore needs no separate body-frame composition.

The list is SHORT on purpose; what is not here is on an MFD page, element by element, in
[`hud-symbology.md`](hud-symbology.md)'s cut table.

| Element | Position | Scissored? | Details / source |
|---|---|---|---|
| Horizon | two segments flanking the boresight, gap for FPM/ladder, long enough to cross the window at any bank | **yes** | horizon DIP from altitude |
| Pitch ladder | earth-referenced rungs every 5° from −45 to +45 | **yes** | positive solid / negative dashed (MIL-STD-1787). The azimuth spread is a WINDOW WIDTH computed back through the projector (`atan(0.045·halfW/Kc)` inner, `atan(0.30·halfW/Kc)` outer), not a magic angle — that is what makes the ladder fill the window instead of clinging to its centre |
| FPM | at the velocity vector (`AirData.TrackDeg` / `FpaDeg`) | **yes** | circle + wings + tail = aircraft reference symbol (MIL-STD-1787) |
| Steerpoint diamond | at the steerpoint's bearing/elevation angle | **yes** | outside the field of view: clamped to the WINDOW EDGE and CROSSED OUT, with an inset so the cross strokes are not halved |
| Tadpole | next to the FPM, X clamped to the window half-width | **yes** | rotated: points UP when the steerpoint lies ahead of the track, DOWN when behind `[DOC]` |
| Heading band | the window's TOP edge, ticks pointing DOWN, labels below, value box on the rail | no | magnetic (`Yaw − MagVar`, MagVar a 0° placeholder). Ticks every 5°, labels every 10°, N/E/S/W written out. The band spans ±35° across the FULL window width |
| CAS band | the window's LEFT edge | no | ±120 kt visible, minor ticks every 20 kt, labels every 100; the box carries the exact value |
| Alt band | the window's RIGHT edge, mirrored | no | ±2000 ft visible, minor ticks every 200 ft, labels every 1000 with a thousands comma ("10,000") |
| Steering line | bottom left, out of the aiming zone | no | `STPT nn dd.d` — the only status text left in the HUD |
| "NO TELEMETRY" | centred | — | fallback when `env.Have` is false |

**Not in the HUD** (and each one's destination): bank scale → SYS · bullseye → HSD · TTG → HSD · slant
range → HSD · G/peak G/Mach/ARM-SIM/radar altitude/ALOW → SYS and SMS (last round).

#### 12.3 Which blocks are read — and who writes them

| Block | Writer | Use in the HUD |
|---|---|---|
| `Platform` | **`FBF16Module` itself** (`PublishPlatform`) | attitude for the projector, altitude for the alt band and the horizon dip, heading band |
| `AirData` | `systems/FBAirDataSystem` | FPM direction (track/FPA), CAS band |
| `Nav` | `systems/FBNavSystem` | steerpoint diamond, tadpole, steering line, MagVar |
| `Ufc` | `modules/f16/FBF16Ufc` | steerpoint number |

Four blocks, down from seven: `RadarAlt`, `Cruise`, `FireControl` and `Stores` are no longer read by
the HUD at all — their numbers are on the MFD pages, which read them instead.

#### 12.4 The validity rule

**Every readout asks the head of its source block FIRST.** A dead box gets DASHES, not its last number
in the same typeface as a live one — that is exactly what the head exists for (`core/FBBlockStatus.h`),
and exactly what the real jet's manual insists on: the pilot must be able to tell a FAILED sensor from
a quiet one.

| State | HUD behaviour |
|---|---|
| `Invalid` | dashes: `---` (CAS), `STPT --  ---.-`. On the CAS band the rail and the box remain (the instrument is there, after all), the moving ticks and the number disappear |
| `Held` | shows its value. Deliberately frozen ≠ broken; hiding it would throw away information the pilot is entitled to |
| `Nav` invalid | **no steering symbology**: neither diamond nor tadpole. A diamond from an unwritten block would point at a steerpoint that does not exist (MIL-STD-1787 declutter rule). A BFM mission without waypoints is exactly this case |

---

### 13. The choice of model

| Point | Statement |
|---|---|
| Model | `mods/f16/src/aircraft/f16` — the **full-scale JSBSim F-16** with a real FLCS, FlightBox' copy of the pinned upstream (today byte-identical, `mods/f16/src/aircraft/MODEL-DELTAS.md` names no delta). `FBF16Module::FdmModelName()` yields `"f16"`; there is no longer a choice of root |
| Not derived | The model name is DELIBERATELY not derived from the registry name: the two coincide today but are not the same thing |
| "The MODEL is the reference" (principle 5) | In practice: the target quantity is NOT "does the number agree with the real jet" but "does FlightBox fly the model FAITHFULLY". Evidenced consequences in this module: the ~5.4 g / ~16 °/s at corner (instead of 9 g / 20+ °/s) are ACCEPTED model properties and not a defect to be tuned; likewise the ~170 KCAS lift-off speed despite the 128 kt table Vr; `ApproachSpeedKt` 154 is the MEASURED trim curve of the model and not a copied real-world number. The one thing that IS a defect and was therefore corrected is a model line that produced a physical impossibility — negative drag (`MODEL-DELTAS.md` D1); the delta rule, not principle 5, governs that case. What is judged is correct integration + rendering |
| FBW/FLCS | The JSBSim F-16 has a real FLCS (`fcs/*-cmd-norm` = rate setpoints). FlightBox' FBW commands it through the `Flcs=1` preset instead of distorting it; `fcs/fbw-override=1` bypasses it (direct control surfaces) — this module uses the FLCS path |
| Licence | The aircraft XML carries its OWN licence — the F-16 is **GPL** (`<license licenseName="GPL">`, `<author>Erik Hofman</author>`), most other JSBSim models LGPL. Attribution is PER FILE, the copy's `<fileheader>` stays unchanged; the submodule itself is never patched. FlightBox' own sources carry FlightBox' licence, JSBSim's LGPL banner is not copied |
| Own models | A model the upstream does not know at all lives in the same root and simply has no counterpart in the provenance table (first case: the AIM-120 in `modules/missile/`) |
