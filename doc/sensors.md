# FlightBox — Perception: datalink, radar, RWR, IRST, countermeasures

**Subject.** How a unit learns about other units — and how it may *not*. This is the sharpest boundary
of the architecture: it is drawn not by convention but by include graph and type choice, and it is
checkable by grep.

**Primary sources (code, this repo):**

| File | Role |
|---|---|
| `sim/src/sensors/FBDatalinkSystem.{h,cpp}` | cooperative net (comms/datalink slot) |
| `sim/src/sensors/FBRadarSystem.{h,cpp}` | active air-to-air radar (sensors slot) |
| `sim/src/sensors/FBRwrSystem.{h,cpp}` | passive warning receiver (defensive, passive half) |
| `sim/src/sensors/FBIrstSystem.{h,cpp}` | passive optical search head (sensors slot, second half) |
| `sim/src/sensors/FBCountermeasureSystem.{h,cpp}` | dispenser set (defensive, active half) |
| `sim/src/modules/f16/FBF16Datalink.h`, `FBF16Fcr.{h,cpp}`, `FBF16Rwr.h`, `FBF16Cmds.{h,cpp}` | the F-16 derivations |
| `sim/src/modules/mig29/FBMig29Radar.{h,cpp}`, `FBMig29Rwr.{h,cpp}`, `FBMig29Irst.{h,cpp}` | the MiG-29 derivations |
| `sim/src/modules/missile/FBMissileSeeker.{h,cpp}`, `FBMissileUplink.{h,cpp}` | the two derivations of the missile |
| `sim/src/units/FBUnit.h`, `FBSimUnit.cpp` (`PublishPose`) | the published emission signature |
| `sim/src/modules/f16/FBF16Module.cpp` (`Run`, `ApplySetup`) | rate, health gate, mission switches |

**Value types** (`FBDatalinkTrack`, `FBRadarContact`/`FBIffReply`, `FBEmitterSignature`, `FBRwrThreat`,
`FBCmProgram`/`FBChaffCloud`) are documented as TYPES in `core.md`. This file documents their
BEHAVIOUR — who creates them, under which conditions, with which ageing and at which price. Mission
switches and telemetry columns are given completely in `doc/missions/sensors.md`; here only the
references. The real templates (ALR-56M, ALE-47, APG-68, MIDS/TNDL) are in `doc/modules/f16/radar-sensors.md`,
`doc/modules/f16/defence-rwr-cm.md`, `doc/modules/f16/datalink-iff.md`.

**Marking.** `[SET]` = a FlightBox setting without a source (the code marks it that way).
`[DERIVED]` = derived from a named formula/source. `[DOC]` = evidenced from `doc/modules/f16/`.

---

## Spec

How a unit learns about other units — and how it may **not**. This is the sharpest boundary in the
architecture: drawn by include graph and type choice, checkable by grep.

| Contract | Acceptance / measurement anchor |
|---|---|
| The unit registry reaches the SENSOR slots and nothing else | `#include "FBUnitRegistry.h"` / `.Units()` appear in exactly **five** files under `sim/src/sensors` + `sim/src/modules` (the four sensor slots + the missile's uplink receiver), and the list is pinned in `tools/verify_layers.py`'s `RESTRICTED` table — the gate FAILS on a sixth. **`C3` raises that number to SIX and to exactly one named file, `sensors/FBVisualSystem.cpp` (§9) — declared here as a decision, not discovered at the gate** |
| Every widening of that list is a decision with a price, not a convenience | a slot joins the list only by being a SENSOR whose limits are modelled: `FBIrstSystem` pays in range (25 km at best against the radar's 50), in identity (it cannot interrogate IFF at all) and in weather (a cloud deck ends the line of sight). **`FBVisualSystem` pays in all four currencies at once and in a fifth nobody else pays — §9.2** |
| **A passive VISUAL contact is anonymous, rangeless AND typeless until the geometry earns the type** (`C3`, specified, not built) | `core/FBVisualContact` carries angles, an angular size and a look age; `HasRange` is structurally always false. A type name appears only when the observed angular size crosses a threshold DERIVED from the detection threshold — never from a table keyed on what the target is |
| A radar contact is anonymous | `core/FBRadarContact` carries range/bearing/az/el/closure and a radar-owned track number — no unit id, no callsign, no team |
| A passive optical contact is anonymous AND has no range | `core/FBIrstContact` carries angles only; `RangeM` exists solely behind a `HasRange` bit that only the laser rangefinder sets |
| A documented sensor defect is behaviour, not a comment | the N019's range-dependent Doppler envelope REJECTS detections; the SPO-15's forward hemisphere goes dark while the own radar radiates |
| The only identity source is IFF Mode 4, and it is two-valued | `FBIffReply` has no value "hostile" |
| Perception costs time | a track firms after `kHitsToFirm` consecutive looks and coasts after leaving the volume; the block carries age, never "live" |
| Cooperative ≠ active | the datalink gives identity away and needs a transmitting sender; the radar gets an echo and pays with an emission |
| What a set radiates is derived from what it is doing | `Emission()` from the pattern actually flown — antenna state and radiated signature cannot diverge |
| The RWR sees only published emissions, never truth, and has a real blind zone | elevation coverage limit at the own antenna; no range, ever (an RWR measures power) |
| Deception is a model, not a die | chaff works through the Doppler notch, measured from own quantities over a dwell |
| **An infrared SEEKER is a sensor slot, not a weapon feature** | a heat-homing round carries a derivation of `sensors/FBIrstSystem` — the same aspect law, the same afterburner term, the same anonymity — and reaches the registry only through that base. The `RESTRICTED` list does not grow: the derivation adds no `#include "FBUnitRegistry.h"` of its own |
| **A flare deceives deterministically, in the same currency the sensor already measures** | seduction is an inequality between two received IRRADIANCES (`I/r²`): the flare's published intensity from its own ageing curve against the aircraft's intensity from the aspect/afterburner law that already sets the detection range. No die, no break-lock probability. Consequence, measured on both branches: a tail-aspect afterburning target is barely deceivable, a head-on target without augmentation is deceived easily |
| A seduced head follows the DECOY, and that is the whole effect | the look substitutes the flare's geometry (the chaff rule verbatim); the track is lost when the flare leaves the field or burns out with the aircraft no longer in it. Re-lock is not a special case, it is the next look |
| **Radar reach follows the target's radar cross-section, by the FOURTH ROOT** | `R ∝ σ^¼` from the two-way radar equation (`Pr ∝ σ/R⁴`). The reference cross-section is the F-16's, so an F-16 looking at an F-16 measures exactly what it measured before this existed (`σ/σ_ref = 1` → factor 1.0, byte-identical); the asymmetry appears only against an airframe that declares a different one |
| A cross-section is a unit PROPERTY, published like every other thing a foreign sensor may notice | `FBUnitSignature::RcsM2`, declared by the module (`FBModule::RadarCrossSectionM2`) and published at the tick barrier beside the afterburner bit and the chaff clouds — not a parameter of the looking radar |

## State

Built: datalink, radar with mode set, RWR, countermeasures — plus the two missile derivations.

| Piece | Status | Anchor |
|---|---|---|
| `FBDatalinkSystem` + `FBF16Datalink` (MIDS/Link-16, 1 Hz net cycle, 3-cycle hold) | built | `9190e7c` |
| `FBRadarSystem` + `FBF16Fcr` (CRM, four ACM sub-modes, STT as its own volume) | built | `4049a7b` |
| `FBRwrSystem` + `FBF16Rwr` (ALR-56M geometry, PRIORITY/OPEN display cap) | built | `439f53a` |
| `FBCountermeasureSystem` + `FBF16Cmds` (ALE-47 programs, OFF…BYP state machine, chaff clouds) | built | `439f53a` |
| `FBMissileSeeker` / `FBMissileUplink` | built | `5c68fc5` |
| `FBIrstSystem` + `FBMig29Irst` (KOLS: aspect law, laser range, cloud masking) | built | MiG-29 stage 2b |
| `FBMig29Radar` (N019: five modes + STT, quantified notch, 6 s inertial track) | built | MiG-29 stage 2b |
| `FBMig29Rwr` (SPO-15LM: forward blanking, channel bearings, channel-wide track marking) | built | MiG-29 stage 2b |
| `FBMissileIrSeeker` (the infrared seeker as an `FBIrstSystem` derivation — one class, two rounds) | built | MiG-29 stage 2c |
| Flare deception (`FBIrstSystem::SelectFlare`, the irradiance inequality) — flares finally DO something | built | MiG-29 stage 2c |
| Radar cross-section as a unit property, fourth-root range scaling | built | MiG-29 stage 2c |
| `FBVisualSystem` (the eye — `C3`) | **nothing built.** Contract in §9 | — |

**Measured, stage 2c:**

| Anchor | Expected from the model | Measured |
|---|---|---|
| flare deception, head-on and dry | aircraft radiates `(10/25)² = 0.16` of the reference, a cartridge 1.0 → deceived | `irst FLARE_SEDUCED … tgtIntensity=0.16` at t = 9.7 s on **both** `mig29-r73` (R-73) and `f16-aim9` (AIM-9) — the identical number from the identical code |
| the consequence of being deceived | the round flies at the cartridge | R-73 `stores EXPIRED … closestM=22.80` against a 3.5 m fuze; AIM-9 `closestM=25.96` against a 6.0 m fuze |
| no deception from astern | aircraft radiates 1.0 (dry) or 2.25 (augmented) → a cartridge cannot win | rear-quarter shots hit: R-73 `missM=0.138`, AIM-9 `missM=0.0196` |
| RCS reference calibration | F-16 against F-16 = factor exactly 1.0 | all 55 F-16 missions byte-identical, telemetry and events |
| RCS asymmetry | MiG 4.0 m² against the 1.2 m² reference → `(4.0/1.2)^¼` = **1.351** one way, **0.740** the other | `duel-asym-probe`: both sets acquire, the F-16 first |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `core/FBRadarContact.h` | banner claims track numbers are reused after a drop; `FBRadarSystem::NextTrackNum_` counts monotonically and never does. Consumers rely on uniqueness **undocumented**. |
| `sensors/FBRwrSystem` | `SelfTeam_` is stored and deliberately never read — dead state with cheat potential the moment somebody touches it |

### Specified, not built

| ID | Gap | Detail |
|---|---|---|
| **`C3`** | **There is no eye.** The sensor set is radar, IRST, RWR and datalink; every merge in the tree is more sensor-driven than the thing it models, and every real identification ends in a visual pass | blocks W5 and O2's visual pass outright, degrades four more campaigns ([`campaigns/INDEX.md`](campaigns/INDEX.md)). **The contract is §9 below**; it depends on `C2` (the mission clock) for the sun and on `core/FBCloudDensity` for the cloud transmittance |

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| Terrain masking for radar, datalink and radio path | air-to-air line of sight is always clear; would need a DEM raymarch per contact per look |
| ~~No IR seeker~~ | closed in stage 2c; the MiG-29's own dispensers are still missing |
| No ECM/jammer, no MWS, IFF Mode 4 only, no measurement noise on sensor data | the whole CMDS/CMS/ECM interaction of the source material is absent |
| No formation concept — `fl` is simply the first unit | the datalink filter "flight leads only" is not real |

### Inventory (from the previous `Offene Punkte` section)

**Deliberate gaps (documented, with justification in the code):**

1. **Terrain masking is completely missing** — neither the radar (`FBRadarSystem`, no `FBWorld`, no DEM
   sample) nor the radio path of the datalink (`RadioHorizonM` knows only the geometric line of sight).
   Price: a DEM raymarch per contact and per look. Until then every air-to-air line of sight is clear;
   ground targets and low-flying units are thereby systematically too easy to see.
2. ~~**No IR seeker** → flares are dispensed, counted and have no effect.~~ **CLOSED (stage 2c):**
   `modules/missile/FBMissileIrSeeker` is an `FBIrstSystem` derivation, flares are published in
   `FBUnitSignature::Flare` and `SelectFlare` decides deception on measured irradiance. What is still
   absent is named separately: the **MiG-29 has no dispensers at all** (the BVP-30-26's programme
   parameters are stated nowhere), so it can dispense neither chaff nor flares — the one place where
   the asymmetry currently runs entirely one way.
3. **No air-to-ground radar mode.** `FBRadarSystem` filters on `FBUnitKind::Aircraft`: stores in free
   flight and ground targets are invisible to every radar. A ground target can therefore only be
   approached via the steerpoint/fire control computation, never via a sensor.
4. **No measurement errors.** Geometry is exact (poses are truth); simulated are exclusively
   availability, volume, time and ageing. There is no noise on range, bearing or closure rate, and hence
   no track confusion either.
5. **No measurement association.** The internal correlation runs over `UnitId` — a track can never jump
   onto the wrong target. That is the honest limit of the model (named in the header), not reality.
6. **Threat library one entry deep.** `Classify()` passes the emitter class through; the estimate is
   always right today. The ALIC/symbol code table from `doc/modules/f16/defence-rwr-cm.md` appendix B has not
   been taken over (the source does not transcribe it).
7. **No ECM/jammer.** `doc/modules/f16/defence-rwr-cm.md` §2.2/§2.3 describes the interaction of CMDS mode, CMS
   and ECM XMIT; FlightBox has no jammer, so this whole coupling is missing.
8. **No MWS/missile approach warner.** A launch warning arises exclusively from a supporting (guidance)
   radar or a missile seeker in the beam — a missile approaching with a silent seeker stays unnoticed.
   (Consistent with the source material: MWS not functional on Block 50.)
9. **IFF knows only Mode 4.** No Mode 1/2/3, no interrogation range limit of its own (every firm track in
   the volume is interrogated, every 5 s).
10. **No formation concept** → `datalink_filter fl` keeps the FIRST participant of the faction. A
    documented placeholder for a lead assignment.
11. **No HUD symbology for the lock.** `doc/modules/f16/hud-symbology.md` knows neither a TD box nor a
    locked-target symbol; the lock stays in FBState/telemetry/events until the symbology source covers it.

**Contradictions / inaccuracies in the existing state:**

12. **Track number reuse.** The banner of `core/FBRadarContact.h` says `TrackNum` is "assigned in
    acquisition order and REUSED after a drop". `FBRadarSystem` counts `NextTrackNum_` monotonically up
    and never reuses a number (only the array SLOT is reused). Either the comment or the assignment has
    to be corrected. Behaviourally relevant: a consumer that compares numbers across a drop may rely on
    uniqueness today — that is an undocumented reliance.
13. **`FBF16Rwr::kOpenThreats = 16` exceeds `kMaxRwrThreats = 8`.** Deliberately left standing as a
    documented number; today the cap is therefore never binding in OPEN mode and PRIORITY (5) is the only
    effective one. If the detection table grows, the behaviour changes abruptly.
14. **Chaff hangs on the dispensing unit.** A cloud can only decoy a radar that is looking at exactly
    this aircraft — never one that is tracking a wingman 500 m away. A deliberate scope decision with a
    stated consequence; the alternative would be a cloud as a unit of its own.
15. **No wind field** → clouds stand absolutely still instead of drifting in the air mass. In a strong
    upper wind that would be a measurable difference.
16. **`kBeamRangeFactor` is ONE constant for every emitter.** Receiver sensitivity is thereby implicitly
    the same for all; a missile with a small seeker is heard "too early" in the same proportion as a
    large fire control radar.
17. **The RWR only fades out in elevation.** Fuselage shadowing in azimuth (e.g. a transmitter exactly
    behind one's own airframe) is not modelled — 360° of azimuth really are 360°.
18. **`Emission()` publishes the search volume as a beam**, although the beam only sweeps across it once
    per frame. A target inside the volume is therefore "illuminated" continuously instead of pulsed; the
    duration of the actual illumination (and hence a more realistic "new" window) is not modelled.
19. **No `FBSystemId` for the optical station.** `core/FBSystemHealth` has ids for radar, RWR,
    countermeasures and the rest, but none for an IRST — so `FBMig29Irst` is the one sensor slot that is
    NOT damage-gated. Adding an id shifts the `dmg_*` telemetry columns, which is why it is held back to
    travel with the twin-engine health change the MiG-29 module already needs (its gap 4d).
20. **The SPO-15's search/track criterion is the channel marking, not the illumination duration** (§5.6),
    because the emission model publishes a searching beam as continuous (gap 18). The device's own
    §2.5 rule is therefore only half implemented, and the half that is missing is measured, not guessed.
21. **The SPO-15's 10° forward azimuth resolution is not reached**: the real device gets it from a 10°
    designed OVERLAP that lights two channels at once, and FlightBox reports one channel, so the achieved
    resolution is the channel width (20° forward, ±60° aft).
22. **The IRST sees no horizontal cloud structure** (§6.5): a broken-or-more deck is a lid. Seeing through
    the holes of a 60 % deck needs a march along the line of sight through `core/FBCloudDensity` — one
    integration per contact per look, the same price terrain masking costs the radar.
23. **No IR countermeasure model.** The KOLS's documented degradation under thermal countermeasures
    (5.4-1.6 nm) has nothing to act on: flares are dispensed and counted but never published in
    `FBUnitSignature`, and there is no IR seeker either (gap 2).
24. **The IRST aspect law's SHAPE is a setting.** Both endpoints are documented (25 km rear, 10 km
    head-on); the curve between them, `(1 + cos A)/2`, is not. A clamped plume-area law would be equally
    defensible and the source supports neither — which is why the choice sits in one line.
25. **`FBMig29Rwr`'s assumed-altitude priority rule cannot bite yet.** The constant band (26-55 kft) is
    read where the documented defect lives, but every emitter in the tree today is airborne and inside
    it, so no threat is ever de-prioritised by it. The first surface emitter is what turns it on.
26. **`FBRwrSystem` stores `SelfTeam_` and never reads it.** Intentional (comment), but dead state that
    could easily mutate into faction recognition by accident in a future library by emitter type. Check
    when touching it.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. The boundary

#### 1.1 The rule

A pilot — human or AI — **sees other units exclusively through simulated sensors**. The world truth "who
exists where" is `units/FBUnitRegistry`: borrowed `const FBUnit*` in registration = mission declaration
order. It reaches the SENSOR slots of a module and not one step further. What a pilot knows is in
`FBState` — with the range, the scan volume, the net cycle and the AGE with which the sensor wrote it
there.

`FBPilot::Run` carries neither `FBUnitRegistry` nor `FBWorld` in its signature and holds neither of them
as a member.

#### 1.2 The five files

```
$ cd sim && grep -rln "FBUnitRegistry.h" src/sensors src/systems src/modules
src/sensors/FBDatalinkSystem.cpp
src/sensors/FBIrstSystem.cpp
src/sensors/FBRadarSystem.cpp
src/sensors/FBRwrSystem.cpp
src/modules/missile/FBMissileUplink.cpp
```

**It was four until the MiG-29's passive optical station landed, and the fifth entry is a deliberate,
recorded widening rather than an erosion.** The rule was never "at most N files" — it is *a pilot sees
other units only through simulated sensors*, and the list is the enumeration of what counts as one. The
gate that enforces it (`sim/tools/verify_layers.py`, `RESTRICTED["units/FBUnitRegistry.h"]`) FAILED on
the new include until the file was added to it by name, which is exactly the intended cost of the
change: widening the boundary is a diff in the gate, reviewable on its own.

The admission test a slot has to pass is that it has REAL limits, and `FBIrstSystem` pays three:

| It pays in | How much |
|---|---|
| range | 25 km at the best aspect, 10 km head-on — against the N019's 50 km gate |
| identity | none at all. The IFF interrogator does not work with the IRST, and `FBIrstContact` has no field it could be put in |
| weather | a cloud deck between the two altitudes ends the detection; the radar has no such term |

and it gives one thing back that no other sensor here does: it costs the observer NOTHING. Nobody is
warned, because nothing is transmitted.

Five hits, there must be no more. Otherwise `FBUnitRegistry` appears in `systems/`/`modules/` only as a
forward declaration or as a passed-through parameter (`FBModule::Run` → `FBF16Module::Run` → sensor
slot). **Whoever violates this check with a fifth hit tears down the architecture — not a convention.**

| File | Why it may see the registry |
|---|---|
| `FBDatalinkSystem.cpp` | reads published PPLI transmissions (transmit bit + pose) — cooperative |
| `FBRadarSystem.cpp` | tests poses against a scan volume — active echo |
| `FBRwrSystem.cpp` | reads exclusively published emission signatures — passive |
| `FBIrstSystem.cpp` | tests poses against a field of regard and an aspect-dependent reach — passive, no emission of its own |
| `FBMissileUplink.cpp` | listens to the published guidance-link transmission of ITS shooter |

**Why the fourth belongs there.** A missile is structurally a unit like any other; its comms slot
derivation receives the midcourse uplink. An uplink is an EMISSION (`FBUnitSignature::Uplink`), not
access to the private state of the shooter: the receiver looks for the ONE unit with
`GetId() == LauncherId_`, takes `GetSignature().Uplink` by value and reads nothing else from that unit —
and nothing at all about the TARGET. The content is the shooter's radar estimate **with its error and its
age** (`ReportTimeS` = the SHOOTER's look, not the time of reception). If the shooter's lock is lost,
`Uplink.Active` goes false, the receiver publishes nothing more and the block becomes `Invalid` — the
guidance sees the age growing and falls back to inertial. No error path, no special case.

#### 1.3 The second half of the boundary: the contact itself

The registry restriction alone is not enough — a sensor could pass on what it knows. That is why the
contact type itself is the second barrier:

| Type | carries identity? | Justification |
|---|---|---|
| `FBDatalinkTrack` | **yes** — `UnitId`, `Callsign`, `Team` | A message. The sender transmits its own identification; identity is given away. |
| `FBRadarContact` | **no** — only `TrackNum` + geometry | An echo. No field for id, callsign, team. The absence IS the model. |
| `FBRwrThreat` | **no** — only `Id` + direction | A heard waveform. `Kind` is an ESTIMATE of the receiver. |
| `FBIrstContact` | **no** — only `TrackNum` + angles | A hot spot. It carries no range either, except behind the laser's own `HasRange` bit, and there is no IFF field: passive detection costs identity, and the type says so. |

`FBRadarSystem::Track` (private) holds `UnitId` as the correlation key from look to look — this key
**never leaves the object**. The same applies to `FBRwrSystem::Threat::UnitId`. The published numbers
(`TrackNum`, `FBRwrThreat::Id`) are sensor-owned file numbers from 1 up, assigned in acquisition order.

**IFF Mode 4 is the only identity source** and it is TWO-VALUED:

```
FBIffReply { NotInterrogated, NoReply, Friendly }     // there is no value "Hostile"
```

`FBRadarSystem::Interrogate` is the only line of the class that reads `GetTeam()`, and it immediately
converts the result into an answer that cannot name an enemy:

```
validReply = target transponder ON  AND  target faction == own faction (crypto)
→ Friendly, otherwise NoReply
```

An enemy with its transponder on and a friend with his off deliver the **same** `NoReply`. Whoever wants
to shoot lives with that — exactly like the pilot of the real jet. Without an interrogator of one's own
(`SetIffInterrogator(false)`) the answer is `NotInterrogated`; interrogation rate `kIffPeriodS = 5.0 s`
per firm track (an interrogation is a transmission; one interrogation on every look would be more
radiation than the real box has).

#### 1.4 What is NOT this boundary

- **`const FBWorld*`** stands separately beside the registry: that is the TERRAIN side (masking), not the
  unit side. No sensor reads it today.
- **The snapshot contract** (`FBUnit::GetPose`/`GetSignature`) always delivers the state of the last
  COMPLETED tick (barrier `FBSimUnit::PublishPose`). No sensor ever sees a half-integrated pose or a
  switch that was thrown mid-tick. With that, tick order cannot influence a sensor result — the
  precondition for `fb-gym --threads`.
- **The registry order is determinism, not information.** All four systems walk the registry IN ORDER, so
  track/symbol numbers hang on the declaration order of the mission and never on who was heard/seen first.

---

### 2. Common structure of all five systems

| Property | Form |
|---|---|
| Construction | interface + REAL default in ONE class; a module overrides by derivation (no forced empty derivation for numbers) |
| Write direction | sensors WRITE exactly ONE `FBState` block, nothing else. Displays/pilot READ |
| Validity | `FBBlockHeader` three-valued: `Invalid` (box off/failed → not an "empty picture" but NO picture), `Valid` (freshly published), `Held` (frozen, the stamp names the last real update) |
| Time base | **absolute** sim time (the module's `simTimeS`). Net cycle, antenna frame and salvo schedule run on their own rasters — the result does NOT depend on how often the module ticks the slot |
| Capacity | fixed arrays, no allocation in the tick (8 tracks / 8 contacts / 8 threats / 8 chaff clouds) |
| Operation | exclusively through the command bus (`core/FBAvionicsCommand.h`), hence rejectable, with a latency class and an acknowledgement |
| Damage coupling | a failed system is not ticked at all, its block goes `Invalid`; degraded = a DERIVABLE performance reduction |
| Observability | every class is an `FBTelemetrySource` (`dl_*`, `fcr_*`, `rwr_*`, `cm_*`) + discrete `FBLog` events |

**Rate in the F-16 module** (`FBF16Module::Run`):

| Slot | Rate | Justification |
|---|---|---|
| Sensors (FCR) | 10 Hz | with the rest of the display/fire control block, of which it is the reader |
| Defensive (RWR → commands → CMDS) | 10 Hz | a program burst interval is 0.1 s [DOC §2.2]; slower would quantise a salvo. Order = data flow: the receiver writes, the commands are answered, the dispenser reads |
| Comms (datalink) | 5 Hz | the net cycle is 1 Hz; finer is of no use |

**Health gate** (`core/FBSystemHealth`, read-only in the module):

```
Fcr_->SetRangeFactor(SystemDegraded(Radar) ? kRadarRangeDegraded : 1.0);
if (SystemWorking(Radar)) Fcr_->Run(...); else SharedState.Radar.H.Invalidate();
```
identical for `Rwr`, `Countermeasures`, `Datalink`. `kRadarRangeDegraded = 0.70710678` [DERIVED]: half
the antenna aperture, radar equation R⁴ ~ Pt·G² with G ~ A, hence R ~ √A → 1/√2.

---

### 3. Cooperative — `FBDatalinkSystem`

#### 3.1 What it is

**Not a sensor in the searching sense.** Every terminal periodically transmits its OWN navigation
solution and its OWN identity; every terminal in range receives it. There is no search, no scan volume
and no identification problem — callsign and faction travel with the message. The accuracy is the
navigation accuracy of the SENDER; what the receiver contributes is only the time at which it heard it.

#### 3.2 Constants

| Constant | Value | Origin |
|---|---|---|
| `kNetPeriodS` | 1.0 s | Link-16 PPLI: the own-position report of a fighter is around one per second [DOC] |
| `kDropAfterCycles` | 3.0 | a terminal holds a contact briefly instead of blanking it on one lost message [SET, behavioural rule from DOC] |
| `kGenericRangeNm` | 150 nm | an explicit PLACEHOLDER ("some cooperative net"), not a real terminal number |
| `FBF16Datalink::kMidsRangeNm` | 300 nm | MIDS-LVT/Link-16 air-to-air LOS range [DOC datalink-iff.md] |

#### 3.3 The net cycle (`Cycle`)

The WHOLE picture is rebuilt per cycle, registry in order:

1. **Faction filter** — `u->GetTeam() != SelfTeam_` → skipped. A cooperative net is one's own; an
   opponent can never appear in it.
2. **Only `FBUnitKind::Aircraft`** — a released store belongs to the same faction but carries no
   terminal. The test stands BEFORE the ordinal counter, because `flightIndex` is the selection criterion
   of the FR/FL filter and a bomb would otherwise shift the flight lead numbering.
3. `flightIndex++` — the fallback ordinal, **including one's own unit**, for a sender that declares no
   flight.
4. **skip one's own PPLI** (`GetId() == SelfId_`) — after the ordinal, not before.
5. `AcceptContact(sender, index)` — the override point (§3.6). The index is the sender's **declared**
   flight position minus one where the mission declared one (`core/FBFlight.h`,
   [`formation.md`](formation.md) §1), and the ordinal of step 3 where it did not — so a mission
   without `flight` lines behaves exactly as it did.
6. Capacity limit 8, then `break`.
7. **Heard?** `sig.DatalinkXmt && rangeM <= min(MaxRangeM_, RadioHorizonM(own, other altitude))`.
8. Heard → fresh track with `ReportTimeS = simTimeS`. Not heard, but an old track exists and is younger
   than `3 × 1 s` → **taken over unchanged** (position and timestamp stay standing; the age keeps
   running up). Otherwise it drops.
9. The difference old/new produces `datalink TRACK_GAINED` / `TRACK_LOST`.

**Radio horizon** (`RadioHorizonM`, static, protected):
`d[nm] = 1.23 · (√h₁[ft] + √h₂[ft])`, the 4/3-earth line-of-sight rule, summed over both antenna heights
[DERIVED]. A UHF net has no path of its own over the horizon. **Terrain masking along the path is NOT
modelled** — it would need the DEM along the route.

#### 3.4 Between the cycles

Only the AGE moves. Range and bearing are recomputed against one's OWN new position (that is the geometry
of one's own display), **never against a newer sender position**. `AgeS` is clamped to ≥ 0 (`ReportTimeS`
is `float`, `simTimeS` `double` — a message stamped in the same cycle could otherwise lie nanoseconds "in
the future").

Block header: `Publish` in a tick in which a cycle ran, otherwise `Hold`. A track is **never "live"**:
even a fresh message describes a pose that was published at the last tick barrier.

#### 3.5 The two switches

| Switch | Method | Effect |
|---|---|---|
| POWER | `SetPowered` | off = **blind AND mute**. Track list is cleared, `TRACK_LOST` with `reason=terminal off`, block `Invalid` |
| XMT | `SetTransmit` | off = **EMCON**: keeps receiving the whole picture, is only no longer carried by anyone |

`Transmitting() == Powered_ && Transmit_` — what the outside world sees
(`FBUnitSignature::DatalinkXmt`). An unpowered terminal cannot transmit, whatever XMT says. **Modelling
XMT as "does not receive either" would be wrong in the direction that matters:** EMCON means not being
seen, not being blind.

#### 3.6 `AcceptContact` — the override point

```
virtual bool AcceptContact(const FBUnit &sender, int flightIndex) const;   // default: everything
```
F-16 (`FBF16Datalink`, HSD contact filter [DOC part 13]): `fr` = all friendlies (default), `fl` = only
`flightIndex == 0`, `off` = none.

**No longer a placeholder.** A `.fbm` unit block now declares its flight and its position in it
(`flight <name> <position>`, [`formation.md`](formation.md)), so `fl` selects position 1 — the unit
the mission says leads. Without a declaration it falls back to the old mission-order ordinal, which is
what keeps every stock mission byte-identical.

#### 3.6b The flight half of the PPLI

The same message carries what a member tells its own FLIGHT, and nothing else was added to carry it:
the range limit, the 1 Hz cycle, the three-cycle hold and the age all apply to it unchanged.
`FBDatalinkTrack` gained the sender's `FlightName`/`FlightPos` (registry identity, like its callsign
and team) and an `FBFlightReport` — *is it prosecuting something, does the round it launched still need
it, and WHERE is the target*.

The last field is a **point, never a track and never an identity**: this radar does not know whom it
sees (§1.3), so it cannot tell anybody. The receiver correlates the reported point against its own
echoes and may fail to — which is the honest property of a shared picture, and is modelled rather than
assumed away. Rules, gate and measurements: [`formation.md`](formation.md) §§2, 5.

#### 3.7 The second derivation: `FBMissileUplink`

The same base class, `Run` completely overridden. It receives the ONE transmission of the programmed
shooter and publishes it as `Tracks[0]` with callsign `"UPLINK"` and **without a unit id** — the shooter
itself does not know whom its radar sees (§1.3), so the weapon cannot learn an identity that never
existed. No new bus block, no return channel: the guidance reads the thing like any other instrument, and
the block header answers the only question it has — is anybody still telling me something?

---

### 4. Active — `FBRadarSystem`

#### 4.1 The scan volume IS the mode

`FBRadarScanVolume` is a complete antenna pattern:

| Field | Meaning |
|---|---|
| `AzCenterDeg` / `AzHalfDeg` | centre + half width **relative to the NOSE** (+ = right) |
| `ElCenterDeg` / `ElHalfDeg` | centre + half height above/below the fuselage reference plane |
| `RangeM` | range gate |
| `FrameS` | time for ONE complete sweep |
| `AutoAcquire` | locks the nearest firm track without any action (the ACM property) |
| `Active` | false = the set is not radiating (an OFF mode) |
| `SingleTarget` | all power on ONE track: every other track file is no longer refreshed and runs out |

Elevation as centre+half instead of a symmetric half angle, because a real vertical scan pattern is
**not** centred on the fuselage reference line (far above it, hardly below) and the same field carries
the cursor position of a slewable pattern.

The volume is BODY-FIXED: the full roll/pitch/yaw rotation is applied (`FBEnuToBodyLos` from
`core/FBGeodesy.h` — the same single definition that the BFM control also inverts). The volume tilts
with the jet; exactly that makes the HUD-referenced ACM boxes behave in a turn the way the pilot expects.

`RelativeLos` delivers FIVE quantities per target: slant range, **world**-referenced bearing + elevation
angle and **body**-referenced az/el. The world pair is the reported position (the consumer would
otherwise have to rotate a look-old body vector back through a now-current attitude and would smear its
own roll motion into the target geometry); the body pair is the antenna's quantity.

#### 4.2 `ActiveVolume()` — THE override point

```
virtual const FBRadarScanVolume &ActiveVolume() const { return Search_; }
```

A whole mode set is nothing but a selection among volumes, and **a lock is nothing but a different
volume**. That is why ONE virtual getter carries a complete fire control set. Beside it only two further
hooks: `ModeOrdinal()` (telemetry label only) and `EmitterKind()` (what kind of box sits behind the
antenna).

**MiG-29 — `FBMig29Radar`, N019 "Rubin"** (§4.9 below for the two things that make it a different
radar and not a different number).

**F-16 — `FBF16Fcr`, AN/APG-68** (taxonomy [DOC radar-sensors.md], **angles/frames are a declared MODEL
PARAMETER SET [SET]** — the source shows MFD screenshots and names no numbers):

| `fcr_mode` | Azimuth | Elevation | Range | Frame | Auto-lock |
|---|---|---|---|---|---|
| `off` | — | — | — | (1.0 s, never swept) | does not radiate |
| `crm` (power-up) | ±60° | ±10.5° about `fcr_slew_el` | 40 nm | 4.0 s | **no** |
| `acm_hud` | ±15° | ±10° | 10 nm | 1.0 s | yes |
| `acm_bore` | ±5° | ±5° | 10 nm | 0.3 s | yes |
| `acm_vert` | ±5° | +17° ± 30° = −13°…+47° | 10 nm | 1.2 s | yes |
| `acm_slew` | ±10° about the cursor | ±10° about the cursor | 10 nm | 0.8 s | yes |
| **STT (locked)** | ±60° (gimbal) | ±60° (gimbal) | 40 nm | 0.1 s | single target |

The frame times follow the volumes: a mechanically scanning radar takes longer for a wider pattern — the
boresight cone acquires in a fraction of the HUD box time, CRM is the slowest. **This relation, not the
absolute seconds, is the model.**

`OFF` beats the lock: switching the set off has to stop the antenna, not let it stare through an OFF
mode. Every other mode hands a lock over to STT.
`fcr_slew_el` is the **antenna elevation control**, not just the ACM cursor: CRM's elevation centre
follows it as well. At BVR range ±10.5° covers only a few thousand feet — putting the antenna on the
wrong altitude band is the classic way to fly past a target one could easily have seen.

**Missile — `FBMissileSeeker`**: the same class, different numbers. `kFovHalfDeg = 10°` [SET] (no public
value for the AMRAAM), `kGimbalHalfDeg = 45°` [SET] (mechanical limit ≠ instantaneous field of view —
without the distinction a locked seeker would lose its target at 10° of offset; measured on the first
flown run: drop + reacquire at 25° of offset, 3 s before impact), `kFrameS = 0.05 s` [SET] (a stare at
20 looks/s), `AutoAcquire + SingleTarget`, **IFF interrogator OFF** (a weapon carries none), volume
centre = the direction reported by the uplink ("SLAVE"; both angles 0 = BORE).

#### 4.3 Contact build-up and loss

| Rule | Value | Consequence |
|---|---|---|
| `kHitsToFirm` | 2 consecutive looks | acquisition costs **time** (`kHitsToFirm × FrameS`); a target flashing through the beam is never reported |
| Hit series broken but not yet firm | the entry is **deleted** | only a FIRM track deserves coast |
| `Hits` on a missed look | back to 0 | re-firming needs 2 fresh looks again |
| Coast duration | `max(kMinCoastS 1.0 s, kCoastFrames 3.0 × FrameS)` | frames for a search sweep, SECONDS for a stare |
| Coast state | geometry **frozen**, `LookAgeS` runs up, `Coasting = LookAgeS > FrameS` | the loss is a PROCESS, not an event |

The seconds floor is the actual setting: an STT with a 0.1 s frame would otherwise drop 0.3 s after the
gimbal limit, whereas a real track filter extrapolates a lost target for about a second — independently
of how often the antenna hit it.

`ClosureMs` is differentiated from two consecutive looks (`PrevRangeM`/`PrevLookS`). A pulse-Doppler set
measures it directly; the difference over the observable is the same quantity and needs no assumption
about a vertical rate which the snapshot does not publish.

Further rules of the sweep (`ScanFrame`):
- own echo skipped; **only `FBUnitKind::Aircraft`** — an air-to-air set does not look for bombs, and
  painting one as a contact would be invention instead of simulation;
- track file full (8) → nothing is displaced;
- `SingleTarget && Locked` → every other unit is **not even looked at** in this frame;
- not radiating or no registry → `DropAllTracks`, block `Invalid` ("not looking" ≠ "found nothing").

#### 4.4 The frame raster

Its own absolute raster (`NextScanS_`), independent of how often the module ticks the slot.
`ActiveVolume()` is **re-read on every loop iteration**, because an acquired lock changes the pattern and
with it the frame time. The guard `guard < 64` limits the catching-up of a pathological `FrameS` and
**resynchronises** afterwards instead of falling further behind. `SetPowered(false)` resets the raster —
a set powered up again starts a fresh frame, not a stale one.

#### 4.5 `Designate()` — the pilot's lock

The ACM modes lock by themselves, because nobody operates a radar in a turning fight. A BVR search mode
does the opposite: it finds everything and locks nothing, and WHICH returning echo becomes the single
target track is a decision **with a price** — the beam warns exactly the one it points at. That is why it
needs a verb.

```
bool Designate(int trackNum, double simTimeS);   // value = the PUBLISHED track number
                                                 // 0 = break (TMS aft)
```
- `trackNum` is the anonymous handle from the bus, **never a unit id**.
- no matching firm track file → `false` (the module acknowledges `out_of_context`: the return was gone by
  the time the hand had finished).
- Events separated: `RADAR_DESIGNATE`/`RADAR_BREAK` (a decision) vs. `RADAR_LOCK` (an automatism). On the
  scope they look the same, in the debriefing they are opposites.

**Frame raster reset, and why it is necessary.** `Designate` sets `NextScanS_ = simTimeS`. Locking
REPLACES the pattern (a 4 s search sweep becomes a 0.1 s stare), but the raster would still carry the
next sweep time of the search pattern. **Measured without this line: four seconds of frozen lock in which
the fused target speed stood at zero — a shot fired in that window was programmed with a stationary
target.**

**Fallback on lock loss.** `UpdateLock` deletes a lost lock; if it was DESIGNATED (`Designated_`), the set
returns to SEARCH and does **not** grab the next contact. The auto-reacquire belongs to the ACM modes that
asked for it. Auto-lock chooses the **nearest** firm track ("the first" is read as "the nearest", because
the momentary beam position within the sweep is not modelled and the alternative would be an arbitrary
registry tiebreak). A mode change to a pattern without `AutoAcquire` discards the lock (`reason=mode
change`).

#### 4.6 `Emission()` — what the set radiates

A radar is not a passive observer; it announces itself, and the announcement is exactly what a foreign
warning receiver hears. `Emission()` is **derived from the pattern actually being flown** — which is why
**radiation and antenna state cannot diverge**, and no module has to remember to keep the two in sync.

| Antenna state | Published beam | Meaning |
|---|---|---|
| not powered OR `!Active` | `Mode::None` | does not radiate |
| searching | the WHOLE scan volume (centre/half angles of the pattern) | the beam sweeps over everything in it once per frame — "somebody is searching" |
| `SingleTarget && Locked` | **pencil** `kTrackBeamHalfDeg = 3.0°` [SET] onto az/el of the locked track | only this one aircraft hears it — "he has ME" |

`RangeM` of the emission = `GateRangeM(v)` = pattern range × `RangeFactor_`. A damaged set can therefore
not see further in one code path than in another, and what it SEES and what it ANNOUNCES stays one thing.
The range limit is at the same time the only power indicator the signature carries (instead of transmit
power/frequency).

**The guidance case is NOT formed here.** `FBSimUnit::PublishPose` raises `Track` to `Guidance` when
`Stores().Uplink().Active` holds at the same time: whether this jet is currently supporting a shot is
knowledge of the SMS and not of the radar. Both emissions are combined at the barrier, which publishes
both anyway — **neither of the two systems learns from the other**. `FBSimUnit::Retire` clears the
signature: a detonated missile does not radiate on forever.

#### 4.7 Deceivability — `SelectDecoy` + Doppler notch

The WHOLE countermeasure model on the radar side, and it rests on a single condition.

A chaff cloud is a big reflector **without a velocity of its own**. A pulse-Doppler set separates targets
from clutter by RADIAL VELOCITY: everything that moves with the air mass (ground return, and a cloud that
loses the aircraft's speed within a second) lies in ONE filter bin, and the processor discards that bin.
A target is therefore distinguishable from chaff **exactly as long as its own radial velocity lies
outside the bin** — and it lies inside when it flies across the line of sight. Hence: **chaff without a
beam manoeuvre achieves nothing, and a beam manoeuvre without chaff achieves almost nothing either.**

**Since the N019 landed, the notch is a HOOK and not a constant** (`DopplerNotchMs(rangeM)`), because a
set can have the number DOCUMENTED — and one does. Two further hooks came with it, and all three default
to the previous behaviour exactly, so the APG-68 is byte-identical to before:

| Hook | Default | Why it exists |
|---|---|---|
| `DopplerNotchMs(rangeM)` | `kDopplerNotchMs` at any range | the N019 states three range bands; the APG-68 states none |
| `NotchRejectsDetection()` | **false** | until now the notch was ONLY chaff's channel — a target in the filter stayed visible without a cloud. That is a named simplification, and a set whose source QUANTIFIES the detection threshold switches it off. Inventing a threshold for a set whose source is silent would be inventing the number that decides a BVR engagement. |
| `CoastS(volume)` | `max(1 s, 3 frames)` | the N019's source names a DURATION (6 s of inertial tracking), not a frame count |

| Constant | Value | Derivation |
|---|---|---|
| `kDopplerNotchMs` | 40 m/s (~78 kt) [SET] | order of magnitude of the half width of a main-lobe clutter filter: wide enough that ordinary crossing geometry does not notch by accident, narrow enough that it takes a deliberate beam manoeuvre |
| `kDopplerDwellS` | 0.2 s [SET] | a Doppler velocity is an INTEGRATION over a coherent processing interval, not the difference of two instantaneous positions — and here it has to be (see below) |

**Why the dwell is mandatory.** The poses of other units are published once per tick barrier (10 Hz),
while a seeker looks at 20 Hz. The difference of two consecutive looks therefore alternates between "only
I moved" and "both of us" and reads a closing target as stationary on every second look — **measured:
446 m/s against a true 654 m/s, i.e. a head-on target INSIDE the notch**. Two tenths of a second span at
least two barriers; the measurement is then the true radial velocity, whatever the two rates are. The
dwell measurement is kept **separately** from `FBRadarContact::ClosureMs` (which stays the look-to-look
number, unchanged).

**It is measured on OWN quantities, never on the truth of the target:**

```
tgtRadialMs = OwnClosureOn(line of sight)         // own velocity projected onto the line of sight
            − measuredClosureMs                    // differentiated from one's own look ranges
notch hit  ⟺  |tgtRadialMs| < kDopplerNotchMs
```
`OwnClosureOn` is the CLUTTER Doppler: what a STATIONARY point in that direction would close at.

Sequence per look (`ScanFrame`):
1. The Doppler decision falls **BEFORE** the volume test — a seduced look measures the CLOUD, and then
   the CLOUD has to lie in the beam.
2. Only a track with an existing look pair can be decoyed; a first detection has no closure rate to test.
3. **Stickiness:** if the track was seduced on the last look, a cloud keeps being searched for without
   repeating the notch test. Reason: once the tracking gate has settled into the clutter filter it stays
   there as long as there is an echo there (a cloud is in the notch by construction). Without the
   stickiness the test would flip on every look — the substituted measurement makes the NEXT closure rate
   a jump that reads as far outside the notch (**measured: seduce/resolve alternating at the seeker's
   20 Hz rate**). The seduction ends when the clouds end.
4. `SelectDecoy` tests **every** cloud of the published signature against the **same** volume as the
   aircraft (range gate + az/el window) and takes the strongest: power = `RCS / r⁴` (two-way radar
   equation), RCS from the ageing curve `FBChaffRcsNorm`. **Deterministic — no roll of the dice, no
   probability.**
5. On a transition: `radar CHAFF_SEDUCED` / `CHAFF_RESOLVED` with BOTH measured quantities, the notch,
   the cloud age and the offset cloud↔aircraft — the decision is reconstructable from the log.

The clouds come from the signature of the UNIT currently being looked at (`sig.Chaff`). The consequence,
stated expressly in the code: **a cloud can only decoy a radar that is looking at the aircraft that
dispensed it** — never one that is tracking somebody else nearby.

#### 4.7b The RADAR CROSS-SECTION, and why the reference is the F-16

`GateRangeM(volume)` was the range gate of the PATTERN; `GateRangeM(volume, targetRcsM2)` is the gate
against a particular target. The two-way radar equation gives `Pr ∝ σ/R⁴`, hence

```
R(σ) = R_ref · (σ / σ_ref)^¼
```

`σ` comes from the observed unit's published signature (`FBUnitSignature::RcsM2`, declared by its
module through `FBModule::RadarCrossSectionM2`) — never from a table inside the looking radar, which
would be identity read out of the registry by the back door. `σ = 0` means "not declared" (a store, a
ground target, any module written before this existed) and the gate then behaves exactly as before.

**`kRefRcsM2 = 1.2` is the F-16's own declared cross-section, and that choice is the calibration.**
Every radar range in this tree — every mode table, every measured acquisition — was measured against
this aircraft, so making its cross-section the reference turns the whole mechanism into the IDENTITY
for F-16 against F-16 (`pow(1.0, 0.25)` is exactly 1.0) and leaves an asymmetry only ACROSS types. Both
numbers are `[SET]` classes rather than measurements: ~1.2 m² for a clean F-16 and ~4.0 m² for a MiG-29
(the middle of the 3-5 m² band that gets quoted), T4-grade, and a two-decimal figure would be false
precision. What follows from them is not: **1.351× the detection range one way and 0.740× the other.**

The chaff comparison is deliberately NOT scaled by it: `SelectDecoy` already weighs a cloud against an
aircraft through `RCS/r⁴`, so a seduced look keeps the unscaled gate and the cloud's own size is
counted exactly once.

#### 4.8 Deliberate non-modelling: terrain masking

A real look-down picture is clipped by the horizon and by the terrain along the line of sight. This class
does **neither**: it takes no `FBWorld` and samples no DEM.

Justification, as it stands in the header: for air-to-air between two flying units — the training fight
for which this sensor exists — the line of sight is clear, and scan volume plus range gate already decide
everything. Masking would need a DEM raymarch **per contact and per look** and belongs to whichever
system first has a reason to pay that price. **Nothing is silently approximated here**, so that nobody
can mistake the picture for one that has masking. (`FBDatalinkSystem::RadioHorizonM` makes the same
statement for the radio path.)

---

#### 4.9 `FBMig29Radar` — the N019, and the two hooks it needed

| `n019_mode` | Azimuth | Elevation | Range | Frame | Auto-lock | Origin |
|---|---|---|---|---|---|---|
| `off` | — | — | — | (1.0 s) | does not radiate | |
| `rad` (search) | ±30° about the ZONE third | ±6° about the antenna knob | 50 km | 3.0 s | **no** | ZONE [DOC]; range [T4, the RCS-qualified band's lower bound]; frame [DERIVED, see below]; bar ±6° [SET] |
| `cc` (radar close combat) | ±1.75° (one beamwidth) | +12° ± 25° = −13°…+37° | 5.4 nm | 2.5 s | yes | [T4] "fixed ±37°/−13°" read with [DOC] "the antenna rotates only in the vertical axis in close combat" |
| `vs` (vertical scan) | ±1.5° | +20° ± 30° = −10°…+50° | 5.4 nm | 1.0 s | yes | [DOC] "3° wide × −10…+50°" |
| `bore` | ±1.25° | ±1.25° | 5.4 nm | 0.5 s | yes | [DOC] "2.5° cone along the aircraft axis" |
| **STT (locked)** | ±65° (gimbal) | +10° ± 46° = −36°…+56° (gimbal) | 50 km | 0.2 s | single target | [T4] gimbal limits; frame [SET] |

**The RAD frame is DERIVED, not chosen.** The source says "you may have to wait up to **six seconds**
before the target is detected … only after the radar has completed several scanning cycles". The generic
system firms a track after `kHitsToFirm` = 2 consecutive looks, so the documented latency IS 2 × FrameS
→ **FrameS = 3.0 s**, which also sits inside T4's 2.5-5 s scan-cycle band. Measured on
`mig29-radar-notch`: first firm contact at **t = 6.0 s**.

**The Doppler envelope, quantified** [DOC]:

| Range | Required | as m/s |
|---|---|---|
| > 8.0 nm | closure/lag > 81 kt | 41.67 |
| 5.4 … 8.0 nm | > 27 kt | 13.89 |
| < 5.4 nm | "not guaranteed" below 32.4 kt | 16.67 |
| `cc` mode | **no requirement** — "stable tracking at equal speeds and at a lag" | 0 |

Two readings are FlightBox's and both are marked as such. **(a) Which quantity**: the source says
"closure/lag speed", which read literally is aircraft-to-aircraft closure — but the same source explains
the effect by target ASPECT ("targets at aspect near 90° … small radial closure, small Doppler shift"),
and a beaming target is exactly the case where closure stays large while its own radial velocity goes to
zero. The physical discriminator of a pulse-Doppler set is the target's Doppler against MAIN-LOBE
CLUTTER, i.e. its radial velocity over the ground — which is the quantity `FBRadarSystem` already
measures for chaff, over the same `kDopplerDwellS` dwell. **(b) "Not guaranteed"** is read
deterministically as "lost": FlightBox rolls no dice, which makes the innermost band WIDER than the one
outside it. That inversion is in the source.

**Six seconds of inertial tracking** [DOC] replaces the generic coast rather than flooring it: the
source names one duration for one track filter, and the generic rule (3 antenna frames = 9 s in RAD,
0.6 s in the stare) would contradict it in both directions.

**A set with a separate EMISSION switch has to resync its scan raster.** ILLUM/DUMMY/OFF is a real
three-position control on this jet, and DUMMY does not sweep. Restarting the raster on the way back to
ILLUM (`ResyncScan()`) is not cosmetic: without it the catch-up guard in `Run` replays the whole silent
period in the tick the switch moves and reports a firm track in the same tenth of a second the radar
came on. **Measured: contact at t=27.9 instead of one frame later.** The F-16 has no such switch and is
untouched by the hook.

**Not modelled, named:** minimum range (250 m), PRF selection (ППС/ЗПС/АВТ and its −25 % penalty), TWS
as a capacity of its own (10 tracks), the AOJ/burn-through jamming chain, and the source's probabilistic
wording throughout.

---

### 5. Passive — `FBRwrSystem`

#### 5.1 The mirror image

The radar asks "what is out there"; the RWR asks "who is looking at me" — and answers it in the only
honest way: it hears what other units RADIATE (`FBUnitSignature::Radar`) and checks two geometries. It is
**not a threat oracle**: it never reports whether the foreign radar actually sees this aircraft, and never
whether it is tracking this or another aircraft on the same bearing [DOC defence-rwr-cm.md §2.1].

The own faction is stored and **deliberately never read**: a warning receiver hears a waveform, not an
allegiance. A friendly radar tracking you produces exactly the same symbol as a hostile one.

#### 5.2 The two checked geometries

**(1) Does the sender's beam hit?** — `BeamCovers`, computed AT THE SENDER:

```
FBEnuToBodyLos(sender roll/pitch/yaw,  vector sender→here)  →  az, el
inside  ⟺  |wrap180(az − sig.AzCenterDeg)| ≤ sig.AzHalfDeg  ∧  |el − sig.ElCenterDeg| ≤ sig.ElHalfDeg
```

The same transformation, the same convention, the same file that the radar uses for its OWN acquisition —
**the two sides of an illumination can never disagree about the geometry.** A searching radar therefore
illuminates everything in its volume; a tracking one EXACTLY ONE aircraft (which is why a track warning
is a personal one).

**(2) Can one's own antenna hear from that direction?** — `ElevCoverageDeg()`:

```
FBEnuToBodyLos(own attitude, vector here→sender)  →  rxAz, rxEl
heard  ⟺  |rxEl| ≤ ElevCoverageDeg()
```

360° of azimuth, but limited elevation. Generic default 60°; **F-16 `FBF16Rwr::kElevCoverageDeg = 45°**
[DOC §2.1: four high-band quadrant antennas + a low-band dual blade give 360° of azimuth but only ±45° of
elevation]. That is a genuine **BLIND ZONE above and below the fuselage axis, which one's own manoeuvring
opens up** — an already existing lock or launch warning disappears SILENTLY in the process, without the
sender having done anything. Only the TRANSITION is logged (`rwr THREAT_BLIND`), not every tick: what is
worth a line is the moment in which an existing warning became inaudible.

#### 5.3 No range — and why

An RWR measures bearing and RECEIVED POWER. It cannot measure range, because it never transmitted
anything whose return it could time. `FBRwrThreat` therefore carries **no metres**, but:

| Field | Meaning |
|---|---|
| `BearingDeg` | RELATIVE to one's own nose, −180…180 (the TWA is a relative-bearing display) |
| `ElDeg` | elevation of arrival, body-fixed — not displayed on the real azimuthal scope, but the quantity on which the antenna coverage is decided, hence published instead of hidden |
| `SignalNorm` | received power 0..1 — the ONE hint of proximity the box has |
| `LethalityNorm` | 0..1, radial position on the scope (1 = centre) [DOC §2.1: the distance from the centre is relative LETHALITY, not range] |

With that, nothing downstream can accidentally fly a range solution from a warning receiver.

**Ring position** [SET, scheme from §2.1 verbatim]: `Search 0.20` / `Track 0.55` / `Missile 0.85`, plus
`kLethalitySignalWeight 0.15 × SignalNorm`, clamped to 1 — the mode chooses the ring, the power moves the
symbol within its ring.

#### 5.4 Hearing range: one-way against two-way

```
hearM = sig.Radar.RangeM × kBeamRangeFactor           // kBeamRangeFactor = 2.0  [SET, DERIVED]
SignalNorm = 1 − (rangeM / hearM)²                     // one-way propagation: power ~ 1/r²
```

Derivation (header): the transmitter needs the outbound AND the return path, its echo falls off as 1/r⁴;
the receiver sits only in the outbound half and therefore hears with 1/r². At equal receiver sensitivity
the warning range is thus a MULTIPLE of the transmitter's acquisition range, not a fraction. The public
literature quotes 1.5 to 3 depending on the receiver; 2.0 is the middle and has the property the mission
can measure: **you are warned before you are acquired.** The number `hearM` never leaves the class — what
is published is the power, never the range behind it.

#### 5.5 Mode, hold, ranking, cap

| Rule | Value / behaviour |
|---|---|
| `ModeOf` | `Kind == MissileSeeker` → **Missile** (regardless of how it scans); `EmitterMode::Guidance` → Missile; `Track` → Track; otherwise Search |
| `kHoldS` | 2.0 s [SET] — a beam sweeping away and a switched-off transmitter look the same at first; the symbol is not blanked on the first absence. In SECONDS, because this receiver has no frame of its own (it listens continuously) |
| `kNewThreatS` | 1.0 s [SET] — the substitute for the detection tone: "new" is a published state with a lifetime, long enough for a 10 Hz consumer, short enough not to blur into the standing picture |
| Ranking | first MODE (enum order `Search < Track < Missile`), then received power, tiebreak = table order (stable insertion) → the priority symbol does not flicker between two equivalent threats |
| `SearchShown` | SEARCH filter; hidden search emitters set `HiddenSearch` — **suppression has to stay distinguishable from absence** [DOC §2.1] |
| `MaxDisplayed()` | display cap OVER a detection that keeps running. F-16: PRIORITY 5 / OPEN 16 [DOC §2.1] |
| Detection table | `kMaxRwrThreats = 8`, full = nothing is displaced |
| unpowered | block `Invalid`, table cleared — "not listening" is not "nothing out there" |

`FBRwrBlock` additionally carries `MissileLaunch` (any threat in Missile mode → the LAUNCH light) and
`Activity` (any non-searching one → the ACT half of the ACT/PWR indication).

Events: `rwr THREAT_NEW` / `THREAT_MODE` / `THREAT_BLIND` / `THREAT_DROP`.

**Not here: the threat library.** `Classify()` today passes the emitter class through (the library is one
entry deep and therefore always right). The field exists so that on the day it no longer is, no consumer
has to change its shape. The ALIC/symbol code table from appendix B is deliberately not taken over: the
source describes its structure, does not transcribe it, and inventing symbol codes would mean guessing
exactly what an RWR must not guess.

---

#### 5.6 `FBMig29Rwr` — the SPO-15LM, three defects as behaviour

The ALR-56M is a detector plus a library plus a sorter. The SPO-15 is **entirely analogue** — no
processor, one azimuth channel processed at a time — and its source lists eleven consequences of that as
DEVICE LIMITATIONS. Three are behaviour rather than colour, and those three are the override:

| # | Behaviour | Consequence |
|---|---|---|
| 1 | **Own radar radiating → the whole forward hemisphere (±90°) is switched off** | using the radar costs the warning that would have told you to stop. One bit, one source: what the world hears (`FBRadarSystem::Emission()`) is what deafens the receiver in the same fuselage (`SetOwnRadiating` → the generic `Blanked(rxAz)` hook) |
| 2 | **The priority logic hard-assumes own altitude 26-55 kft** | the box has no altitude input at all. Its two type-priority rows and the azimuth criterion that picks between them (types П/F: ABEAM is the low row, everything else high) are evaluated against a CONSTANT, via the generic `PriorityRank` hook |
| 3 | **Track is a property of the azimuth CHANNEL, not of an event** | once any track is seen in a channel, the whole channel is marked for `kChannelTrackHoldS` 3.0 s — so a merely searching radar on the same bearing is reported as tracking |

Plus the geometry: **±30° elevation** (against the F-16's ±45° — a bigger blind zone), and the reported
bearing is the **CENTRE OF THE CHANNEL THAT FIRED**, not the measured angle. Eight logical channels: six
forward 20° wide covering ±60° (four Luneburg-lens feeds per side, 20° beamwidth and 20° peak
separation), one spiral antenna per side aft.

**What the source's §2.5 asks for and this does NOT do, with the measurement that decided it.** The real
criterion for search-vs-track is the LENGTH of the illumination event (>125-250 ms). FlightBox's emission
model publishes a searching radar's whole scan volume as one *continuously* illuminated window (§4.6 and
gap 18 below), so timing it would put every search emitter over the threshold within two ticks. **Measured
on the first run of the class (`mig29-pair`, t = 0.3 s): the F-16's CRM sweep reported as TRACK.** The
EVENT half therefore waits for a pulsed emission model; the CHANNEL half — the part that is a device
defect rather than a measurement — is what the override contributes.

**Not taken over, because taking it over would mean inventing:** the six threat letters (П/З/Х/Н/F/С).
Their assignment is by pulse width and PRF, and `FBEmitterSignature` deliberately carries neither. With
them fall the CW/HPRF confusion, the 6 dB two-emitter rule, low-frequency multi-sector blooming and the
781 Hz criterion — the same argument the F-16 file makes for the ALIC table.

---

### 6. Passive optical — `FBIrstSystem`

#### 6.1 The third question

The radar asks "what is out there" and transmits to find out. The RWR asks "who is looking at me" and
can only hear what somebody else radiates. An **IRST asks the radar's question and pays the RWR's
price — nothing.** It sees HEAT: no emission, no warning to the target, no reply to interrogate.

That freedom is bought back in the shape of what it produces, and all three terms are separate
documented statements rather than one fudge factor:

| Term | Model | Source |
|---|---|---|
| **Aspect** | reach scales between a rear-aspect and a head-on figure with `f = (1 + cos A)/2`, A = the angle between the target's heading and the bearing this sensor sees it on (A = 0 → dead astern, full nozzle) | endpoints [DOC], the curve [SET] |
| **Afterburner** | reach × `kAfterburnerRangeFactor` 1.5 [SET]; for a point source irradiance falls as 1/r², so range goes as √intensity — 1.5 is a plume ≈2.25× the unaugmented aircraft | [DOC] states "an exception to the size rule" and no figure |
| **Cloud** | a deck at or above `kCloudMaskCover` 0.5 ("broken") between the two ALTITUDES is a lid: no detection | [SET] threshold, deck geometry from `core/FBCloudDensity` |

#### 6.2 The contact type is the boundary again

`core/FBIrstContact` carries `TrackNum`, world bearing/elevation, body az/el and a look age. **No unit
id, no team, no IFF — and no range.** The one exception is a collimated **laser rangefinder**: a short
deliberate active measurement, on command, inside its own much shorter reach, published behind its own
`HasRange` bit so that "nobody measured a range" cannot be read as zero metres. The telemetry column
`irst_lock_nm` carries **-1** when it did not fire, for the same reason.

The laser produces **no** `FBEmitterSignature`: a warning receiver cannot detect it [DOC], which is
precisely what makes the "stealth attack" of the source material possible.

#### 6.3 Structure — the same contract as the other three

`FBIrstFieldOfRegard` is a type ALIAS of `FBRadarScanVolume`, not a copy: a window in body-referenced
az/el, one `FrameS` sweep, a gate, `AutoAcquire`, `SingleTarget`. Two fields are read differently and
both readings are written into the header — `RangeM` is the CLEAN-AIR REAR-ASPECT reach that the aspect
law scales per target, and `Active` means the head is uncaged, not that it radiates.

`ActiveField()` is THE override point, exactly as `ActiveVolume()` is for the radar. Track build-up
(`kHitsToFirm` = 2), coasting, the absolute frame raster, fixed capacity (8) and three-state validity
are all identical to the radar's, because they are properties of a scanning sensor rather than of a
transmitter.

**One place where copying the radar would have been wrong:** the tracking field is NOT `SingleTarget`.
A radar in STT spends all its transmitted power on one target and stops refreshing every other file; a
passive head has no power to spend and keeps seeing its whole field while it follows one mark. The only
exclusive thing about the track is where the LASER points.

**Auto-acquire picks the smallest ANGULAR offset from the field centre, not the nearest target** — an
angle-only sensor has no "nearest". That is the one place the missing range changes a decision rather
than a number.

#### 6.4 `FBMig29Irst` — the KOLS

| Quantity | Value | Origin |
|---|---|---|
| rear-aspect clean-air reach | 25 km | [DOC] 13.5 nm |
| head-on reach | 10 km | [DOC] 5.4 nm |
| laser rangefinder | 6 km | [DOC/T4] — the bound on where the passive channel can produce a TRUE range |
| field of regard (search/IR CC) | ±30°/±15° az, ±15° el | [DOC/T4] |
| frame, search | 5.0 s | [DOC] "4-6 s dwell per increment", midpoint; with two-look firming, acquisition costs 10 s |
| IR CC aspect gate | 135° | [DOC] rear-hemisphere mode, aspect up to 3/4 |

Modes `off` / `ir` / `ir_cc` / `bore`. The search field does **not** auto-acquire: capture is a
documented two-handed act ("slew the strobe, press and HOLD LOCKON 2-3 s"), and an angle-only sensor
locking itself onto whatever is nearest the centre would be a decision the pilot never made about a
target he cannot identify.

**Not modelled, named rather than approximated:** the Shchel-3UM helmet sight (nothing to designate to
— no IR missile exists), the IR GAIN knob, the documented degradation under thermal countermeasures
(flares are dispensed and counted but not published in `FBUnitSignature`), and the SPAN angular ranging
method.

#### 6.5 The weather coupling, and what it is not

This is the **first tactical effect weather has on a sensor** in FlightBox. It is deliberately the
crudest honest form: a broken-or-more deck between the two altitudes is a lid, decided per look on the
deck geometry `core/FBCloudDensity` derives from the same weather provider the wind comes from. The
sample is resolved by the OWNER once per decision tick (`FBSimUnit::UpdateSky` →
`FBModule::SetCloudSky`), exactly as ground elevation is — the sensor never queries the world itself.

**The gap, stated instead of approximated:** the deck's HORIZONTAL structure. `core/FBCloudDensity` is a
closed-form field and could be marched along the line of sight for a real transmittance, which would let
a 60 %-cover deck be seen through where its holes are. That march costs one integration per contact per
look — the same price terrain masking costs the radar, and it is declined for the same stated reason.

---

### 7. Active-defensive — `FBCountermeasureSystem`

#### 7.1 A program is DATA

The parameter scheme is that of the AN/ALE-47, field by field and range by range [DOC §2.2], per type
(chaff/flare):

| Field | Range | Meaning |
|---|---|---|
| `BurstQty` (BQ) | 0..99 | cartridges in ONE burst |
| `BurstIntervalS` (BI) | 0.020..10.000 s | time between cartridges within a burst |
| `SalvoQty` (SQ) | 0..99 | salvoes in the program |
| `SalvoIntervalS` (SI) | 0.50..150.00 s | time between salvoes |

`BQ` **or** `SQ` at 0 takes the type out of the program — that is how a pure chaff or flare program is
expressed (a rule of the real DED page, reproduced instead of being replaced by a "type" flag). A program
is thereby mission/loadout data, not behaviour.

`FBCountermeasureSystem` is the machine that PLAYS one: eject a cartridge, wait `BI`, next, wait `SI`,
stop when the salvoes or the magazine end. The schedule is **driven by absolute time** with a catch-up
guard (`guard < 128`) — a burst interval below the slot rate still ejects the right number of cartridges
at the right times instead of being stretched onto the tick rate.

**F-16 — `FBF16Cmds`** (scheme [DOC], **values [SET]**, the task of each program named in the header):

| PRGM | Chaff | Flares | Task |
|---|---|---|---|
| 1 BREAK LOCK | 2 × 0.10 s, 2 salvoes of 1.00 s (= 4 cartridges in ~1.1 s) | — | the dense reflex answer; what AUTO throws against a MISSILE |
| 2 MIXED | 2 × 0.10 s, 2 salvoes of 2.00 s | 1, 2 salvoes | unknown threat — at the cost of two magazines |
| 3 FLARE | — | 2 × 0.10 s, 4 salvoes of 1.00 s | IR only (see §7.5) |
| 4 SUSTAINED | 2 × 0.10 s, 4 salvoes of 4.00 s (= 8 over ~12 s) | — | against a mere TRACK; what AUTO repeats. Slow enough not to empty a 60-round magazine before the decision; dense enough that there is always a cloud standing within `kChaffLifeS` |
| 5 SLAP | 1 | 1 | the wall button (always within reach) |
| 6 BYPASS | 1 | 1 | the documented emergency dispense [DOC §2.2] |

Magazine 60/60, combined maximum 120 [DOC §1]; BINGO 10/10 [SET].

#### 7.2 The mode knob as a state machine

| Mode | Who may dispense | Consent | Other |
|---|---|---|---|
| `off` | nobody | — | block `Invalid`, status `NoGo`; a running program is stopped |
| `stby` | nobody | — | **the only mode in which reprogramming is allowed** |
| `man` | CMS forward dispenses the PRGM program | — | nothing automatic |
| `semi` | the system CHOOSES the program | **per dispense** (CMS aft); the prompt returns after every program | status `DispenseReady` as long as the prompt stands |
| `auto` | the system chooses AND repeats | **once per mode change** (turning to AUTO grants it); CMS right revokes it and **interrupts the running program** | — |
| `byp` | exactly 1 chaff + 1 flare | — | overrides the PRGM selection completely |

SEMI and AUTO are deliberately two states and not a flag: consent per dispense against consent per mode
entry are different machines, and the source says so expressly.

`SetMode` sets `Consent_ = (m == Auto)` [DOC §2.2: "consent counts as granted as soon as the knob stands
on AUTO"] and stops a running program at OFF/STBY. `SetConsent(false)` = CMS right →
`StopProgram("consent revoked")`.

**Two programming paths, one gate:** `SetProgram` (the DED path) requires STBY; `InstallProgram`
(protected) is the module constructor — the ground crew — and is ungated.

#### 7.3 Triggering only through the command bus

`Dispense(program, nowS, outcome, reason)` is **never called directly** (the same rule as for the pickle).
Rejection catalogue:

| Condition | Outcome / reason |
|---|---|
| Mode OFF/STBY | `Rejected` / `HardwarePrecedence` (the knob physically locks the dispenser out) |
| Program number outside 1..6 | `Rejected` / `OutOfRange` |
| Program with both types zeroed | `Rejected` / `OutOfContext` |
| No stock for the types contained in the program | `Rejected` / `Depleted` |
| Failed box | `Rejected` / `SystemFailed` (module level, applies to every system) |

Bus targets: `CmDispense` (CMS forward, HOTAS class — the one action that takes place IN a manoeuvre;
value 0 = PRGM program, 1..6 = direct), `CmConsent` (CMS aft/right, HOTAS), `CmdsMode` (knob on the left
console → **DED class**: a hand off the throttle, head down).

#### 7.4 In SEMI/AUTO it triggers on the WARNING, not on the truth

```
threatened = state.Rwr.H.Readable() && ThreatCount > 0 && Threats[0].Mode != Search
```

That is the core statement of the class. The trigger is the **RWR block** — hence what the jet KNOWS.
What stands in the blind zone of the receiver (§5.2) is not answered: exactly the combined vulnerability
the source describes. No path from here reaches the world, the registry or another unit.

Further rules of `ServiceAutomatic`:
- if a program is already running → no restart;
- **chaff BINGO** (`Chaff_ <= BingoChaff_`) suppresses **automatic** dispenses (and only those) — what is
  left belongs to the pilot for a shot he wants to answer himself [DOC §2.2];
- SEMI without consent → `AwaitingConsent_` (status `DispenseReady`), otherwise start; after the start the
  consent is withdrawn again in SEMI;
- program selection: `AutomaticProgram(worst)` — the ONE protected hook. Default doctrine [SET]: against a
  missile the dense program (1), against a mere track the economical repeating one (4). The source says
  that the system chooses "the appropriate automatic program per threat", and never which — the mapping is
  therefore a module's doctrine, expressed as a hook instead of invented in the generic layer.
  `FBF16Cmds` does NOT override it: the program table is built around exactly this doctrine, and an
  override returning the same two numbers would be the forbidden empty derivation.

#### 7.5 What a dispense actually does

**Chaff** → an `FBChaffCloud` at the position where the aircraft is NOW, with `BloomS = simTimeS`. A ring
of 8 entries: the freshest cartridges stay, older ones are the dispersed ones and are the right ones to
lose. The cloud does **not** move (FlightBox has no wind field — "stands in the air mass" is "stands").
Ageing curve `FBChaffRcsNorm` [SET, justification in the header]:

```
ageS < kChaffBloomS 0.3 s      → 0     (packed bundle, not yet a reflector)
0.3 s ≤ ageS < kChaffLifeS 8 s → linear decay from 1 to 0
ageS ≥ 8 s                     → 0     (too thin to stand against an aircraft echo)
```
**No randomness, no break-lock probability.** Whether the cloud WORKS is decided solely by the opposing
radar (§4.7) — this class does not learn it, exactly like the pilot.

It is published in `FBUnitSignature::Chaff` (barrier), hence under the same contract as pose and transmit
switch: no radar ever sees half a salvo.

**Flares** are the chaff cloud's twin and, since stage 2c, they work. The books had been kept honestly,
so the day the AIM-9 arrived exactly one thing had to change here: the ejection now also fills an
`FBFlareCloud` ring (`FBUnitSignature::Flare`), under the same publication contract as chaff. The
difference to a chaff cloud is the one physical difference between the two expendables — chaff
REFLECTS somebody else's transmitter, a flare RADIATES on its own — so the ageing curve is an
INTENSITY rather than a reflectivity (`core/FBCountermeasure.h`, `FBFlareIrNorm`: ignition 0.15 s,
burnout 4.0 s, both `[SET]`, the sources document dispense parameters only). Whether a cartridge WORKS
is decided solely by the opposing seeker (§6.6) — this class does not learn it, exactly like the pilot.

#### 6.6 The infrared decoy — `SelectFlare`, the mirror of the Doppler notch

A flare is not a reflector, so there is no clutter filter to hide in: the only question an optical head
has is which of the two hot spots in its field puts more power on the detector, and irradiance from a
point source is `I/r²`. Both intensities are in ONE unit — `kFlarePeakIntensity`, defined as the
intensity of a clean airframe seen **dead astern**, which is the case the documented reach figure is
stated for — so the comparison needs no third number:

```
flare wins  ⟺  I_flare(age)/r_f²  >  I_aircraft(aspect, burner)/r_t²
```

and the ASPECT does the rest by itself, out of the detection law that was already there
(`TargetIntensity` is `DetectRangeM` read the other way round, `intensity = (reach/reference)²`):

| Geometry | Aircraft intensity | Against one cartridge |
|---|---|---|
| head-on, dry | `(10/25)²` = **0.16** | the cartridge wins **6×** |
| dead astern, dry | **1.00** | a draw on intensity; range decides |
| dead astern, afterburner | `1.5²` = **2.25** | the cartridge loses by 2.25× |

Sequence per look, deliberately the radar's (§4.7) line for line: the decoy decision falls **before**
the field test (a seduced look measures the CARTRIDGE, and then the CARTRIDGE has to lie in the field);
the cartridge must additionally be bright enough to be seen at all, tested with its own intensity in
the same reach law; and the seduction is **STICKY** (`kFlareSticky`) — once the tracking gate has
walked onto a burning cartridge it stays there while that cartridge burns and stays in the field.
Without the stickiness the decision would flip look by look, exactly as the chaff test did before its
own. **The seduction ends when the flare burns out or leaves the field, and the next look is the
re-lock** — no reacquisition path is written, because none is needed.

Everything is measured on the head's OWN quantities. Events `irst FLARE_SEDUCED` / `FLARE_RESOLVED`
carry the aircraft intensity, the cartridge's age and the angles, so the decision is reconstructable.
The consequence stated in the code: **a cartridge can only decoy a seeker looking at the aircraft that
threw it** — the same scope decision chaff carries.

Events: `cmds PROGRAM_START` / `SALVO` / `PROGRAM_END` / `MAGAZINE_EMPTY`.

---

### 8. The five systems compared

| | **Datalink** (cooperative) | **Radar/FCR** (active) | **RWR** (passive) | **IRST** (passive optical) | **CMDS** (active-defensive) |
|---|---|---|---|---|---|
| Class | `sensors/FBDatalinkSystem` | `sensors/FBRadarSystem` | `sensors/FBRwrSystem` | `sensors/FBIrstSystem` | `sensors/FBCountermeasureSystem` |
| F-16 | `FBF16Datalink` (MIDS-LVT) | `FBF16Fcr` (APG-68) | `FBF16Rwr` (ALR-56M) | *none — the jet has no IRST; the slot is the NoOp default and its block stays Invalid* | `FBF16Cmds` (ALE-47) |
| MiG-29 | *none — no cooperative terminal; its GCI is a VOICE channel typed in by the pilot* | `FBMig29Radar` (N019) | `FBMig29Rwr` (SPO-15LM) | `FBMig29Irst` (OEPS-29/KOLS) | *stage 2c (BVP-30-26)* |
| Question | "where are my people" | "what is out there" | "who is looking at ME" | "what is out there, without asking" | "what do I dispense" |
| **Sees** | own faction, aircraft only, transmitting senders only, within min(terminal, radio horizon) | aircraft in the scan volume (az×el body-fixed) within the range gate | every unit whose beam hits this jet AND whose angle of arrival is covered by one's own antenna | aircraft in the field of regard within an ASPECT-dependent reach, with no cloud deck between the two altitudes | nothing — reads the RWR BLOCK of its own bus |
| **Gets** | id, callsign, team, position, heading, speed, age | **anonymous geometry**: track no., range, bearing, elevation angle, az/el, closure, look age, IFF | relative bearing, elevation, signal, lethality, mode, estimated emitter type, "new" | **angles only**: track no., bearing, elevation angle, az/el, look age — plus metres ONLY behind the laser's own validity bit | — |
| **Gives away** | its own PPLI (position + identity) as long as XMT is on | its own beam: search volume resp. ±3° pencil onto exactly one target; plus the IFF transponder reply | **nothing** (purely passive) | **nothing** — not even the laser, which no warning receiver can detect | chaff clouds behind the aircraft (published in the signature) |
| **Ageing** | 1 Hz net cycle; hold over 3 cycles; `AgeS` runs up; block `Held` | antenna frame (0.1–4.0 s per mode); build-up 2 looks; coast `CoastS()` (generic `max(1 s, 3 frames)`, N019 6 s); `LookAgeS`; block `Held` | continuous; hold 2 s after last hearing; `AgeS`; "new" 1 s | head frame (0.5–5.0 s per mode); build-up 2 looks; coast `max(1 s, 3 frames)`; `LookAgeS`; block `Held` | program schedule in absolute time; cloud: bloom 0.3 s, life 8 s |
| **Operating latency** | POWER/XMT/filter/range over the bus (DED class) | mode/range/slew/IFF (DED), emission switch (DED), `Designate` (HOTAS) | POWER/display/search (DED) | `IrstMode`/`IrstDesignate`/`IrstLaser` over the bus | `CmDispense`/`CmConsent` (HOTAS), `CmdsMode` (DED) |
| **CANNOT** | see opponents; go beyond the radio horizon; terrain masking; carry stores/ground targets | supply identity (except IFF friendly/unknown); see bombs or ground targets; terrain masking; more than 8 track files; see anything else in STT | measure range; hear a transmitter outside its beam; hear outside its elevation coverage; name WHO is there; distinguish faction | **measure range at all** without the laser; ask IFF (there is no interrogator and no field); see through a broken deck; see bombs or ground targets; see a cold target far off aspect | know whether it worked; react to a threat the RWR does not hear; decoy an IR seeker (no IR in the sim) |
| **Failed** | block `Invalid` | block `Invalid`, all tracks dropped | block `Invalid`, table empty | block `Invalid`, all tracks dropped (no `FBSystemId` yet — see the gaps) | block `Invalid`, status `NoGo` |
| **Degraded** | — | range × 0.7071 [DERIVED] | — | — | — |
| Override point | `AcceptContact` | **`ActiveVolume()`** (+ `ModeOrdinal`, `EmitterKind`, `DopplerNotchMs`, `NotchRejectsDetection`, `CoastS`) | `Run` (+ `ElevCoverageDeg`, `MaxDisplayed`, `Classify`, `Blanked`, `ReportBearingDeg`, `ClassifyMode`, `PriorityRank`) | **`ActiveField()`** (+ `ModeOrdinal`, `LaserRangeM`, `DetectRangeM`) | `Run` (+ `AutomaticProgram`) |
| Telemetry | `dl_*` (5) | `fcr_*`, `iff_xpdr` (11) | `blk_rwr` + `rwr_*` (10) | `blk_irst` + `irst_*` (11), appended LAST | `blk_cmds` + `cm_*` (11) |

---

### 9. Visual acquisition — `FBVisualSystem` (`C3`, **specified, nothing built**)

The fourth question, and the one the other three were built around avoiding. The radar asks "what is
out there" and transmits. The RWR asks "who is looking at me" and can only hear. The IRST asks the
radar's question and pays nothing. **The eye asks "what does it look like" and pays in every currency
there is.**

This section is a **contract**, written before any code, per [`conventions.md`](conventions.md)'s
spec-first rule. Numbers marked `[SET]` are settings; `[DERIVED]` names the relation; `[T3]`/`[T4]`
carry a public source with its link in §9.9.

#### 9.1 Why it is a sensor slot and not a pilot feature

Because it must sit **inside** the perception boundary or it is worthless. A "visual" that the pilot
derives from the registry is the cheat the whole architecture exists to prevent; a "visual" bolted onto
the radar block hands the radar an identity. So it is the sixth `#include "FBUnitRegistry.h"` under
`sensors/`, declared in `tools/verify_layers.py`'s `RESTRICTED` table with its price in the comment —
the same act by which `FBIrstSystem` joined the list, for the same stated reason.

#### 9.2 What it pays, itemised — the price that buys the widening

| Currency | The eye's payment |
|---|---|
| **Range** | the shortest of all five. A fighter beam-on is at the detection threshold at ~2.3 nm, head-on at ~1.5 nm (§9.4) — against the radar's 40 nm and the KOLS's 13.5 |
| **Identity** | none, and unlike the radar it cannot even interrogate. It can eventually say WHAT (a type) and never WHO or WHOSE (§9.7) |
| **Range measurement** | none, ever. `HasRange` is structurally false — there is no laser, no second eye baseline, no ranging method at all |
| **Weather** | the only sensor damped by cloud **and** haze **and** the sun, and the only one for which cloud is a transmittance rather than a lid (§9.6) |
| **Light** | the only sensor that stops working when the sun goes down. Below the twilight floor it contributes nothing, and the tree has no aircraft-lighting model to give it back (§9.8) |

Five currencies. That is what makes the sixth entry a decision with a cost rather than a convenience.

#### 9.3 What it measures — three inputs, one decision

```
detected  ⇔  θ_obs ≥ θ_detect · g(C_eff)          and the target is inside the field of regard
```

| Symbol | Quantity | Origin |
|---|---|---|
| `θ_obs` | observed angular subtense of the target's LARGEST presented dimension | `[DERIVED]`, §9.4 |
| `θ_detect` | the base angular threshold at reference contrast | `0.2°` = 12 arcmin `[T4]`, §9.4 |
| `C_eff` | effective contrast against the background after haze, cloud and glare | `[DERIVED]` from a chain of documented laws, §9.5–9.6 |
| `g(C)` | how much MORE angle a poor-contrast target needs | `clamp(C_ref / C, 1, g_max)` `[SET]` shape, §9.5 |

The split is not FlightBox's invention: **size and contrast are the two primary factors, and size is the
more important** — CAAP 166-2, quoted in the ATSB cockpit-visibility study `[T3]`. Modelling them as one
fudge factor would collapse exactly the distinction the sources make.

#### 9.4 Angular size and aspect — the presented extent

An eye does not see an area, it resolves an **extent**. NTSB (1987, via the ATSB study) puts the
threshold at **> 12 arcmin (0.2°) of the aircraft's largest dimension**; the same study reports
**24–36 arcmin (0.4–0.6°) as more realistic under sub-optimal conditions** `[T3]/[T4]`. FlightBox takes
the 0.2° figure as `θ_detect` and lets the contrast term (§9.5) carry the degradation, rather than
carrying two competing thresholds — one number with a stated modulation instead of two `[SET]` values.

```
θ_obs = L(â) / R                                 small-angle, radians
L(â)  = the largest dimension of the silhouette presented along the line of sight â
```

`â` is the unit line-of-sight vector expressed in the **target's** body frame (the same rotation
`FBEnuToBodyVec` already performs for a warhead burst). `L` is interpolated over the three orthogonal
reference extents of the airframe — frontal, side, plan — exactly as the RCS became a published unit
property in stage 2c.

**Where those numbers come from, and the rule that keeps them honest: from `FBDamageLayout`, not from a
second table.** The damage model already declares the F-16's presented areas (4.0 / 14.0 m², `[SET]`,
`weapons.md`) because a gun bundle needs a silhouette. **The eye and the gun look at the same aeroplane;
two presented-area tables would be two truths about one airframe.** So the layout gains a plan figure
and both consumers read the one table. A module that has no layout has no visual signature either, and
that is correct — a bomb is not seen.

Worked, so the magnitude is on the record rather than assumed (F-16C, span 9.45 m, length 15.03 m):

| Aspect | `L` | `θ_detect` = 0.2° | detection range |
|---|---|---|---|
| beam-on | ≈ 15 m | 3.49 mrad | **4,300 m ≈ 2.3 nm** |
| head-on | ≈ 9.45 m | 3.49 mrad | **2,700 m ≈ 1.5 nm** |

That ratio — a fighter head-on is worth a fraction of the same fighter side-on — is the single most
cited fact about visual air combat, and here it is a consequence of the geometry rather than a
coefficient. The absolute numbers are deliberately at the pessimistic end of the literature (the
`[T3]` SSO study puts a **DC-3 silhouette against bright sky at 17–23 km** under optimal conditions);
they describe an unalerted look at a fighter, which is the case every campaign mission actually poses.

#### 9.5 Contrast — Koschmieder, and nothing invented

`C = |L_tgt − L_bg| / L_bg`. FlightBox has no luminance field and will not acquire one for this. What
it has is exactly the right law, already in the tree:

| Term | Model | Origin |
|---|---|---|
| Inherent contrast `C₀` | two values by background class: **look-up** (target against sky) and **look-down** (target against terrain clutter) | `[SET]`, two numbers; the binary is decided by the sign of the contact's elevation angle against the horizon dip, which `FBGeodesy` already computes |
| Haze attenuation | `C(R) = C₀ · exp(−σ·R)`, `σ₀ = 3.912 / visibility`, thinned by `exp(−z/8000)` | **Koschmieder + the ISA density scale height — already derived and in use by the cloud stage** (`render/clouds.md`). Visibility comes from the same weather sample the wind does |
| Threshold coupling | `θ_required = θ_detect · clamp(C_ref/C_eff, 1, g_max)` | `[SET]` shape. **Stated honestly: the true relation is the contrast sensitivity function** (the SSO model's CSF, `[T3]`), and this linear inverse is its stand-in. A CSF would need a spatial-frequency decomposition of a silhouette FlightBox does not render on the CPU |

The look-up/look-down binary is crude and it is the right crudeness: it reproduces the one thing every
source agrees on (a target against ground clutter is far harder) with one comparison and two settings,
and it does not pretend to a terrain-albedo model that does not exist.

#### 9.6 What damps it — sun, glare, cloud

**(a) Daylight.** Reuse `DaylightFactor` verbatim — `t = clamp((sunElDeg + 9)/12, 0, 1)`, then
`t²(3−2t)`; full day above ≈ +3°, dark from ≈ −9° (nautical twilight). It is already **one** number for
sky, ground and star fade; the eye becomes its fourth consumer rather than the tree's second twilight
definition. `C_eff` is multiplied by it, so the channel fades out through dusk instead of switching.
**Needed `C2`**, which landed on 2026-07-28: `FBEnvironmentBlock.SunElDeg`/`SunAzDeg` are written per
actor per decision tick in `fb-gym` whenever the mission declares a `time`, from `core/FBEphemeris.h`.
A mission WITHOUT a clock still has no sun, and this sensor has to say what it does then.

**(b) Looking into the sun, as its own hard case.** Not a multiplier on the general daylight term but a
separate, angle-dependent one, because it is a different physical mechanism: intraocular scatter raises
the *background*, and contrast is measured against the background.

Stiles–Holladay, the classical disability-glare relation `[T3]`:

```
L_veil(θ_sun) = 10 · E / θ_sun²          θ_sun in degrees, E the source illuminance
```

Neither `E` nor the ambient `L_bg` exists in absolute units here, so their ratio collapses into **one**
setting with a stated meaning:

```
C_eff = C / (1 + k / θ_sun²)             k = θ_½²  ,  θ_½ = 10°  [SET]
```

`k = 100 deg²`. `θ_½` is defined as *the off-sun angle at which contrast is halved*, so the setting is
readable rather than magic. What it produces:

| `θ_sun` | contrast factor |
|---|---|
| 45° | 0.95 |
| 30° | 0.90 |
| 10° | **0.50** |
| 5° | 0.20 |
| 2° | 0.04 |

Plus a **hard cut** inside a stated cone about the sun's centre (`[SET]`, ≥ the 0.53° solar disc): there
the eye is not degraded, it is out of action, and a smooth function through that region would report a
faint capability that does not exist. Smooth, closed-form, deterministic — no dice, per
`conventions.md`.

**(c) Cloud — and this is where the eye buys the march the IRST declined.**

The KOLS treats a broken-or-more deck between the two altitudes as a **lid** (`kCloudMaskCover` 0.5,
`[SET]`), and `sensors.md` §6.5 already names the omission: the deck's horizontal structure, declined at
a price of one integration per contact per look. **The visual channel must pay that price**, and the
recommendation is explicit:

> Integrate `core/FBCloudDensity` along the line of sight and produce a transmittance
> `T_cloud = exp(−Σ σ_deck · ρ · ds)`, with the per-deck extinction the cloud stage already publishes
> (0.022 / 0.018 / 0.0060 m⁻¹ at density 1). Multiply it into `C_eff` in the same currency as the haze
> term. **The density function is not touched** — it is a shared, closed-form field with a C++ and a
> WGSL half kept numerically identical by `--cloudcheck`, and this is a fifth reader of it.

Three reasons, in order:

1. **The whole point of the channel is that an eye does not see through cloud.** A lid over-reports
   blindness at exactly the coverage where W4's measurement lives — a 40 % deck is mostly hole.
2. **The cost is bounded by the eye's own reach.** The march runs over ≤ 3 shell segments, the sensor's
   own range is a few km, and it looks at ≤ 8 contacts at ≤ 1 Hz. That is a different budget from a
   50 nm radar marching every contact every look. **The budget is to be measured, not assumed** — and if
   it does not hold, falling back to the IRST's lid is a documented retreat with a number attached.
3. **It is a shared investment.** Terrain masking (`C4` / roadmap R6) wants the same line-of-sight
   integration machinery. Building it for the sensor that most needs it is cheaper than building it
   twice.

The sample is resolved by the OWNER once per decision tick and pushed down (`FBSimUnit::UpdateSky` →
`FBModule::SetCloudSky`), exactly as today. **The sensor never queries the world.**

#### 9.7 Identification — a resolution test, not a lookup

W5 and O2 need "a contact" to become "a MiG-29" without that being a path to `team`. The mechanism:

> **Recognition is the same measurement as detection, at a higher threshold.** Both come from one
> quantity — presented extent over range. When `θ_obs` crosses a multiple of `θ_detect`, the contact
> gains the **type name the target's own module already publishes** (its `FBModuleRegistry` key:
> `"f16"`, `"mig29"`). It says WHAT. It never says WHO and never says WHOSE.

The multiple is **derived, not set**: Johnson's criteria (1958), the standard sizing rule for
electro-optical systems `[T4]`, give N50 values in cycles across the target's critical dimension —
detection 1.0, orientation 1.4, **recognition 4.0**, **identification 6.4**.

```
θ_recognise = 4.0 · θ_detect      [DERIVED, Johnson N50]
θ_identify  = 6.4 · θ_detect      [DERIVED, Johnson N50]
```

Consequence, on the beam-on F-16 of §9.4: detected at 4,300 m, **recognised at 1,075 m, identified at
670 m**. That is the answer W5 is actually asking for — *how close must you get* — expressed as a
geometry instead of a mission-author's guess, and it turns the campaign's `[SET]` "abeam box" from a
convention into a physical requirement.

**Why this holds the anti-cheat line, stated as the test:**

| Claim | Argument |
|---|---|
| The type is not the team | the string is a registry key already written in the mission file and already public. **Two MiG-29s, one `team friendly` and one `team hostile`, produce the identical string** |
| The type is not the identity | no callsign, no unit id. A flight of four MiG-29s is four contacts each labelled `"mig29"` |
| `w5-03` / `o2-08` stay valid **after** C3 lands | in both runs of the pair the visual channel recognises `"mig29"` at the identical tick, so a recognition is not a divergence. The first legitimate divergence is still the first tick at which a sensor *discriminates between the two cases*, and no channel in the tree can — IFF answers "no reply" in both |
| Nothing is fused | the visual block is its own block, correlated with **nothing**. A "this radar contact is that visual contact" association would hand identity to the radar through the back door, and that association is exactly the leak `w5-03` was written to catch |

**The alternative, named and rejected:** a per-mission `set visual_id_range_nm`, i.e. the author
declares the range at which the type becomes known. Rejected because (a) it is a type-name table wearing
a different hat, (b) it makes the identification range a property of the *mission* rather than of the
*geometry*, and (c) it structurally cannot answer W5's question, since the answer would be whatever was
typed into the file.

#### 9.8 What is NOT modelled — named, not approximated

| Not modelled | Why, and which direction the error runs |
|---|---|
| **Peripheral vision / the detection lobe** | the field of regard is a uniform cone; a real detection probability falls with eccentricity (the SSO's second principle, `[T3]`). **Over-reports** at the cone edge — the one place the model is generous |
| **Gaze direction, head position, helmet, scan pattern** | an attention model is a *pilot* model, not a sensor model, and it would need a decision channel that does not exist. A fixed body-referenced cone under-reports (no check-six) and never over-reports |
| **Canopy frame, bow, combiner obscuration, canopy dirt and distortion** | there is no cockpit geometry in the tree to obscure with |
| **Acuity dispersion** — pilot to pilot, age, fatigue, hypoxia, empty-field myopia | `conventions.md` forbids a die; a per-pilot acuity would be a variant parameter, and that is a different round |
| **Motion** | a moving object is far more detectable than a static one; the model is purely static-contrast. **The largest known omission**, and it under-reports a crossing target — the safe direction |
| **Lighting at night** — navigation and anti-collision lights, city glow | nothing in the tree emits light. The consequence is stated rather than papered over: *the visual channel contributes nothing at night*, so W5-09/W5-10 and every O5 night mission measure a merge without eyes, which is a **finding about the mission**, not a defect |
| **The afterburner plume as a visual cue** | the augmentation bit is already published in `FBUnitSignature` and the IRST already reads it — the cheapest possible first night cue, named as the obvious next step and **not** taken in this round |
| **Smoke and contrails** | the RD-33's smoke is a real, citable and tactically decisive visual signature; there is no plume model to hang it on |

#### 9.9 The contact type, the block, and the rules that keep it from becoming a second radar

```
FBVisualContact { TrackNum, BearingDeg, ElevDeg, AzBodyDeg, ElBodyDeg,
                  SizeMrad, State, TypeName, LookAgeS }
State ∈ { Detected, Recognised, Identified }
```

| Rule | Reason |
|---|---|
| No unit id, no callsign, no team, **no range**, **no closure** | closure needs range or a rate; an eye has neither. `HasRange` does not exist as a bit, because there is no path that could ever set it |
| Field of regard: a fixed body-referenced cone, `[SET]` ±60° az / ±40° el, no slew and no auto-acquire | there is nothing to slew and nothing to lock. The pilot's use of this channel is the DECISION to press in, not a designation |
| It never sees a unit outside the cone, at any range | no omniscient bubble; the geometry is the same `ActiveVolume()`-shaped test the radar and the IRST already use |
| Track build-up costs looks, like every other scanning sensor (`kHitsToFirm`, coast, look age) | a scanning sensor's properties, not a transmitter's — the identical argument `FBIrstSystem` §6.3 makes |
| It sees only `FBUnitKind::Aircraft` in this round | ground targets and stores are excluded, exactly as for the radar and the IRST. Widening it is a separate decision with its own price (a ground target has no presented-extent triple and no aspect) |
| Type name comes from the module registry key, `State` from the geometry | §9.7 |
| Cloud is a transmittance, never a probability; glare is a closed form, never a die | `conventions.md`: no randomness in a deterministic simulation |

**The pilot does not consume it in this round, and that is deliberate.** The channel writes its block and
nothing reads it — the same state `duels.md` defect **D3** already records for the IRST. Two consequences,
both wanted: (a) no trajectory in the tree moves, so the C3 round is measurable against the existing 84
missions column-for-column; (b) the behaviour change gets its **own** round with its own measurement,
booked in [`pilot.md`](pilot.md), where a behaviour change belongs.

**The regression gate of the C3 round is therefore weaker than C2's, and it is stated as such:** adding a
telemetry source appends columns to every unit's CSV, so `telemetry*.csv` cannot be byte-identical.
The requirement is *every pre-existing column identical, position for position and value for value; the
appended columns carrying their sentinels; `events.log` byte-identical modulo `wallS`/`speedup`.* The
last of those only holds because the channel emits no event when it sees nothing — which it does not, in
84 missions that declare no visual scenario.

Mission switches and telemetry columns: [`missions/sensors.md`](missions/sensors.md).

**Sources for this section**

- [ATSB — Cockpit Visibility Study, AO-2023-001](https://www.atsb.gov.au/investigations/2023-001) `[T3]` —
  the 0.02°–0.2° detection band, the NTSB 12-arcmin figure, the 24–36 arcmin realistic band, and
  CAAP 166-2's "size and contrast, size the more important".
- [Predicting Visibility of Aircraft (PLOS ONE, 2009)](https://pmc.ncbi.nlm.nih.gov/articles/PMC2680596)
  `[T3]` — the Spatial Standard Observer: contrast sensitivity function, eccentricity, non-linear spatial
  pooling; the DC-3-against-bright-sky figure of 17–23 km.
- [CIE equations for disability glare / Stiles–Holladay](https://www.researchgate.net/publication/288969413_CIE_equations_for_disability_glare)
  `[T3]` — `L_veil = 10·E/θ²`, and the age/pigmentation extensions FlightBox does **not** take.
- Johnson's criteria (1958), N50 cycles across the critical dimension: detection 1.0 / orientation 1.4 /
  recognition 4.0 / identification 6.4 `[T4]` — the source of the two multiples in §9.7.
- In-tree, not re-derived: Koschmieder and the ISA scale height (`render/clouds.md`), `DaylightFactor`
  (`render/renderer.md`), the presented-area table (`weapons.md`), `core/FBCloudDensity`.

---

### 10. Determinism — what is guaranteed

- **No randomness in the perception chain.** No die, no break-lock probability, no detection probability.
  Chaff effect is an inequality on measured quantities; the stronger cloud wins via `RCS/r⁴`.
- **No dependency on tick order:** all cross-unit reads go through the barrier snapshot.
- **No dependency on the slot rate:** net cycle, antenna frame and salvo schedule run on absolute rasters
  with catch-up guards.
- **No dependency on acquisition order:** the registry is walked in mission declaration order; track and
  symbol numbers follow from that.
- **No allocation in the tick:** all four systems work on fixed arrays.
- Consequence: `fb-gym --threads 1..N` delivers the same fingerprint.
