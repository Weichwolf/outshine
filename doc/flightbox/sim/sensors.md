# FlightBox — Perception: datalink, radar, RWR, countermeasures

**Subject.** How a unit learns about other units — and how it may *not*. This is the sharpest boundary
of the architecture: it is drawn not by convention but by include graph and type choice, and it is
checkable by grep.

**Primary sources (code, this repo):**

| File | Role |
|---|---|
| `sim/src/systems/FBDatalinkSystem.{h,cpp}` | cooperative net (comms/datalink slot) |
| `sim/src/systems/FBRadarSystem.{h,cpp}` | active air-to-air radar (sensors slot) |
| `sim/src/systems/FBRwrSystem.{h,cpp}` | passive warning receiver (defensive, passive half) |
| `sim/src/systems/FBCountermeasureSystem.{h,cpp}` | dispenser set (defensive, active half) |
| `sim/src/modules/f16/FBF16Datalink.h`, `FBF16Fcr.{h,cpp}`, `FBF16Rwr.h`, `FBF16Cmds.{h,cpp}` | the F-16 derivations |
| `sim/src/modules/missile/FBMissileSeeker.{h,cpp}`, `FBMissileUplink.{h,cpp}` | the two derivations of the missile |
| `sim/src/units/FBUnit.h`, `FBSimUnit.cpp` (`PublishPose`) | the published emission signature |
| `sim/src/modules/f16/FBF16Module.cpp` (`Run`, `ApplySetup`) | rate, health gate, mission switches |

**Value types** (`FBDatalinkTrack`, `FBRadarContact`/`FBIffReply`, `FBEmitterSignature`, `FBRwrThreat`,
`FBCmProgram`/`FBChaffCloud`) are documented as TYPES in `core.md`. This file documents their
BEHAVIOUR — who creates them, under which conditions, with which ageing and at which price. Mission
switches and telemetry columns are given completely in `doc/mission-format.md`; here only the
references. The real templates (ALR-56M, ALE-47, APG-68, MIDS/TNDL) are in `doc/f16/radar-sensors.md`,
`doc/f16/defence-rwr-cm.md`, `doc/f16/datalink-iff.md`.

**Marking.** `[SET]` = a FlightBox setting without a source (the code marks it that way).
`[DERIVED]` = derived from a named formula/source. `[DOC]` = evidenced from `doc/f16/`.

---

## Spec

How a unit learns about other units — and how it may **not**. This is the sharpest boundary in the
architecture: drawn by include graph and type choice, checkable by grep.

| Contract | Acceptance / measurement anchor |
|---|---|
| The unit registry reaches the SENSOR slots and nothing else | `#include "FBUnitRegistry.h"` / `.Units()` appear in exactly four files under `sim/src/systems` + `sim/src/modules` (the three sensor slots + the missile's uplink receiver) |
| A radar contact is anonymous | `core/FBRadarContact` carries range/bearing/az/el/closure and a radar-owned track number — no unit id, no callsign, no team |
| The only identity source is IFF Mode 4, and it is two-valued | `FBIffReply` has no value "hostile" |
| Perception costs time | a track firms after `kHitsToFirm` consecutive looks and coasts after leaving the volume; the block carries age, never "live" |
| Cooperative ≠ active | the datalink gives identity away and needs a transmitting sender; the radar gets an echo and pays with an emission |
| What a set radiates is derived from what it is doing | `Emission()` from the pattern actually flown — antenna state and radiated signature cannot diverge |
| The RWR sees only published emissions, never truth, and has a real blind zone | elevation coverage limit at the own antenna; no range, ever (an RWR measures power) |
| Deception is a model, not a die | chaff works through the Doppler notch, measured from own quantities over a dwell |

## State

Built: datalink, radar with mode set, RWR, countermeasures — plus the two missile derivations.

| Piece | Status | Anchor |
|---|---|---|
| `FBDatalinkSystem` + `FBF16Datalink` (MIDS/Link-16, 1 Hz net cycle, 3-cycle hold) | built | `9190e7c` |
| `FBRadarSystem` + `FBF16Fcr` (CRM, four ACM sub-modes, STT as its own volume) | built | `4049a7b` |
| `FBRwrSystem` + `FBF16Rwr` (ALR-56M geometry, PRIORITY/OPEN display cap) | built | `439f53a` |
| `FBCountermeasureSystem` + `FBF16Cmds` (ALE-47 programs, OFF…BYP state machine, chaff clouds) | built | `439f53a` |
| `FBMissileSeeker` / `FBMissileUplink` | built | `5c68fc5` |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `core/FBRadarContact.h` | banner claims track numbers are reused after a drop; `FBRadarSystem::NextTrackNum_` counts monotonically and never does. Consumers rely on uniqueness **undocumented**. |
| `systems/FBRwrSystem` | `SelfTeam_` is stored and deliberately never read — dead state with cheat potential the moment somebody touches it |

### Deliberately not modelled (from the retired `TODO.md` §3)

| Thing | Consequence |
|---|---|
| Terrain masking for radar, datalink and radio path | air-to-air line of sight is always clear; would need a DEM raymarch per contact per look |
| No IR seeker | flares are counted and do nothing |
| No ECM/jammer, no MWS, IFF Mode 4 only, no measurement noise on sensor data | the whole CMDS/CMS/ECM interaction of the source material is absent |
| No formation concept — `fl` is simply the first unit | the datalink filter "flight leads only" is not real |

### Inventory (from the previous `Offene Punkte` section)

**Deliberate gaps (documented, with justification in the code):**

1. **Terrain masking is completely missing** — neither the radar (`FBRadarSystem`, no `FBWorld`, no DEM
   sample) nor the radio path of the datalink (`RadioHorizonM` knows only the geometric line of sight).
   Price: a DEM raymarch per contact and per look. Until then every air-to-air line of sight is clear;
   ground targets and low-flying units are thereby systematically too easy to see.
2. **No IR seeker** → flares are dispensed, counted and have no effect. There is no weapon today that
   they could decoy.
3. **No air-to-ground radar mode.** `FBRadarSystem` filters on `FBUnitKind::Aircraft`: stores in free
   flight and ground targets are invisible to every radar. A ground target can therefore only be
   approached via the steerpoint/fire control computation, never via a sensor.
4. **No measurement errors.** Geometry is exact (poses are truth); simulated are exclusively
   availability, volume, time and ageing. There is no noise on range, bearing or closure rate, and hence
   no track confusion either.
5. **No measurement association.** The internal correlation runs over `UnitId` — a track can never jump
   onto the wrong target. That is the honest limit of the model (named in the header), not reality.
6. **Threat library one entry deep.** `Classify()` passes the emitter class through; the estimate is
   always right today. The ALIC/symbol code table from `doc/f16/defence-rwr-cm.md` appendix B has not
   been taken over (the source does not transcribe it).
7. **No ECM/jammer.** `doc/f16/defence-rwr-cm.md` §2.2/§2.3 describes the interaction of CMDS mode, CMS
   and ECM XMIT; FlightBox has no jammer, so this whole coupling is missing.
8. **No MWS/missile approach warner.** A launch warning arises exclusively from a supporting (guidance)
   radar or a missile seeker in the beam — a missile approaching with a silent seeker stays unnoticed.
   (Consistent with the source material: MWS not functional on Block 50.)
9. **IFF knows only Mode 4.** No Mode 1/2/3, no interrogation range limit of its own (every firm track in
   the volume is interrogated, every 5 s).
10. **No formation concept** → `datalink_filter fl` keeps the FIRST participant of the faction. A
    documented placeholder for a lead assignment.
11. **No HUD symbology for the lock.** `doc/f16/hud-symbology.md` knows neither a TD box nor a
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
19. **`FBRwrSystem` stores `SelfTeam_` and never reads it.** Intentional (comment), but dead state that
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

#### 1.2 The four files

```
$ cd sim && grep -rn "FBUnitRegistry.h\|\.Units()\|->Units()" src/systems src/modules
src/systems/FBDatalinkSystem.cpp:5:  #include "FBUnitRegistry.h"
src/systems/FBDatalinkSystem.cpp:36:   for (const FBUnit *u : net.Units()) {
src/systems/FBRadarSystem.cpp:5:     #include "FBUnitRegistry.h"
src/systems/FBRadarSystem.cpp:89:     for (const FBUnit *u : net.Units()) {
src/systems/FBRwrSystem.cpp:5:       #include "FBUnitRegistry.h"
src/systems/FBRwrSystem.cpp:71:       for (const FBUnit *u : net->Units()) {
src/modules/missile/FBMissileUplink.cpp:3:  #include "FBUnitRegistry.h"
src/modules/missile/FBMissileUplink.cpp:20:  for (const FBUnit *u : net->Units()) {
```

Four hits, there must be no more. Otherwise `FBUnitRegistry` appears in `systems/`/`modules/` only as a
forward declaration or as a passed-through parameter (`FBModule::Run` → `FBF16Module::Run` → sensor
slot). **Whoever violates this check with a fifth hit tears down the architecture — not a convention.**

| File | Why it may see the registry |
|---|---|
| `FBDatalinkSystem.cpp` | reads published PPLI transmissions (transmit bit + pose) — cooperative |
| `FBRadarSystem.cpp` | tests poses against a scan volume — active echo |
| `FBRwrSystem.cpp` | reads exclusively published emission signatures — passive |
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

### 2. Common structure of all four systems

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
3. `flightIndex++` — ordinal within the flight, **including one's own unit**; index 0 is the flight lead.
4. **skip one's own PPLI** (`GetId() == SelfId_`) — after the ordinal, not before.
5. `AcceptContact(sender, flightIndex)` — the override point (§3.6).
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

**Honestly noted:** the simulator has no formation concept — there is no element/flight assignment from
which a lead could be derived. `fl` therefore keeps the FIRST participant of that faction in mission
order. A documented placeholder for a lead assignment, not a model of one.

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

### 6. Active-defensive — `FBCountermeasureSystem`

#### 6.1 A program is DATA

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
| 3 FLARE | — | 2 × 0.10 s, 4 salvoes of 1.00 s | IR only (see §6.5) |
| 4 SUSTAINED | 2 × 0.10 s, 4 salvoes of 4.00 s (= 8 over ~12 s) | — | against a mere TRACK; what AUTO repeats. Slow enough not to empty a 60-round magazine before the decision; dense enough that there is always a cloud standing within `kChaffLifeS` |
| 5 SLAP | 1 | 1 | the wall button (always within reach) |
| 6 BYPASS | 1 | 1 | the documented emergency dispense [DOC §2.2] |

Magazine 60/60, combined maximum 120 [DOC §1]; BINGO 10/10 [SET].

#### 6.2 The mode knob as a state machine

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

#### 6.3 Triggering only through the command bus

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

#### 6.4 In SEMI/AUTO it triggers on the WARNING, not on the truth

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

#### 6.5 What a dispense actually does

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

**Flares** are dispensed, counted and **have no effect** — and that is stated as such instead of being
hidden: an infrared decoy needs an infrared seeker, which this simulator does not have. The books are
kept honestly, so that on the day an AIM-9 exists nothing has to be changed here.

Events: `cmds PROGRAM_START` / `SALVO` / `PROGRAM_END` / `MAGAZINE_EMPTY`.

---

### 7. The four systems compared

| | **Datalink** (cooperative) | **Radar/FCR** (active) | **RWR** (passive) | **CMDS** (active-defensive) |
|---|---|---|---|---|
| Class | `systems/FBDatalinkSystem` | `systems/FBRadarSystem` | `systems/FBRwrSystem` | `systems/FBCountermeasureSystem` |
| F-16 | `FBF16Datalink` (MIDS-LVT) | `FBF16Fcr` (APG-68) | `FBF16Rwr` (ALR-56M) | `FBF16Cmds` (ALE-47) |
| Question | "where are my people" | "what is out there" | "who is looking at ME" | "what do I dispense" |
| **Sees** | own faction, aircraft only, transmitting senders only, within min(terminal, radio horizon) | aircraft in the scan volume (az×el body-fixed) within the range gate | every unit whose beam hits this jet AND whose angle of arrival is covered by one's own antenna | nothing — reads the RWR BLOCK of its own bus |
| **Gets** | id, callsign, team, position, heading, speed, age | **anonymous geometry**: track no., range, bearing, elevation angle, az/el, closure, look age, IFF | relative bearing, elevation, signal, lethality, mode, estimated emitter type, "new" | — |
| **Gives away** | its own PPLI (position + identity) as long as XMT is on | its own beam: search volume resp. ±3° pencil onto exactly one target; plus the IFF transponder reply | **nothing** (purely passive) | chaff clouds behind the aircraft (published in the signature) |
| **Ageing** | 1 Hz net cycle; hold over 3 cycles; `AgeS` runs up; block `Held` | antenna frame (0.1–4.0 s per mode); build-up 2 looks; coast `max(1 s, 3 frames)`; `LookAgeS`; block `Held` | continuous; hold 2 s after last hearing; `AgeS`; "new" 1 s | program schedule in absolute time; cloud: bloom 0.3 s, life 8 s |
| **Operating latency** | POWER/XMT/filter/range over the bus (DED class) | mode/range/slew/IFF (DED), `Designate` (HOTAS) | POWER/display/search (DED) | `CmDispense`/`CmConsent` (HOTAS), `CmdsMode` (DED) |
| **CANNOT** | see opponents; go beyond the radio horizon; terrain masking; carry stores/ground targets | supply identity (except IFF friendly/unknown); see bombs or ground targets; terrain masking; more than 8 track files; see anything else in STT | measure range; hear a transmitter outside its beam; hear outside ±45° elevation (F-16); name WHO is there; distinguish faction | know whether it worked; react to a threat the RWR does not hear; decoy an IR seeker (no IR in the sim) |
| **Failed** | block `Invalid` | block `Invalid`, all tracks dropped | block `Invalid`, table empty | block `Invalid`, status `NoGo` |
| **Degraded** | — | range × 0.7071 [DERIVED] | — | — |
| Override point | `AcceptContact` | **`ActiveVolume()`** (+ `ModeOrdinal`, `EmitterKind`) | `Run` (+ `ElevCoverageDeg`, `MaxDisplayed`, `Classify`) | `Run` (+ `AutomaticProgram`) |
| Telemetry | `dl_*` (5) | `fcr_*`, `iff_xpdr` (11) | `blk_rwr` + `rwr_*` (10) | `blk_cmds` + `cm_*` (11) |

---

### 8. Determinism — what is guaranteed

- **No randomness in the perception chain.** No die, no break-lock probability, no detection probability.
  Chaff effect is an inequality on measured quantities; the stronger cloud wins via `RCS/r⁴`.
- **No dependency on tick order:** all cross-unit reads go through the barrier snapshot.
- **No dependency on the slot rate:** net cycle, antenna frame and salvo schedule run on absolute rasters
  with catch-up guards.
- **No dependency on acquisition order:** the registry is walked in mission declaration order; track and
  symbol numbers follow from that.
- **No allocation in the tick:** all four systems work on fixed arrays.
- Consequence: `fb-gym --threads 1..N` delivers the same fingerprint.
