# FlightBox — Generic system slots (`sim/src/systems/`)

Sources of this file: the comment banners of the source files under `sim/src/systems/` (state: branch
`systems`, commit `8cd3a74`) plus CLAUDE.md's `systems/` section. Every derivation, every measured number
and every setting is taken from there; nothing is extrapolated. Where code and CLAUDE.md diverge, it is
recorded under "Gaps".

**Scope:** the airframe-agnostic system slots WITHOUT sensors, pilot and weapons.

| Not here | But in |
|---|---|
| `FBDatalinkSystem`, `FBRadarSystem`, `FBRwrSystem`, `FBCountermeasureSystem` | `sensors.md` |
| `FBPilot`, `FBPilotTuning`, `FBBfmTrack`, `FBEngagement` | `pilot-ai.md` |
| `FBStoresSystem`, `FBGunSystem`, damage model | `weapons-and-damage.md` |

---

## Spec

The airframe-agnostic system slots of a module — interface and default in ONE class, a module
overrides by derivation. Number tuning stays a preset, never an empty subclass.

| Contract | Acceptance / measurement anchor |
|---|---|
| `FBCore → interface → default → module override` | every slot is instantiable without a module; the F-16 composes the defaults and only overrides behaviour |
| Peers never call each other | a module cycles its slots; slots hold no pointers to sibling slots |
| Sensors **write** `FBState`, displays **read** it | one writer per block (`core.md`) |
| The pilot reads the airframe only through `FBAirframeControls` | keeps `systems/` airframe- and instance-agnostic |
| Waypoint sequencing terminates on any fix the aircraft is actually pursuing | a route never stalls on a fix it cannot close — arrival, passage and **orbit** are all grounds; `missions/test-wp-inside-turn.fbm` (geometry) and `missions/wx-orbit.fbm` (wind) |
| Guidance is a primitive the pilot commands, not a mode the system picks | `FBAutopilot::Direct` |
| The inner loop runs at 100 Hz over 10 Hz decisions | `FBFlightControl` on `fcs/*-cmd-norm`; the F-16's own FLCS is bridged by `fcs/fbw-override=1` |
| A destroyed system is not ticked and its block goes `Invalid` | consumers must state what they do without it — `FBRadarAltimeter` is the reference case |

## State

Built: guidance, FBW inner loop, air data, radar altimeter, warnings, navigation, display slot,
airframe controls. Sensors, pilot and weapons slots are documented in their own files.

| Slot | Status | Anchor |
|---|---|---|
| `FBAutopilot` (Direct/Course/Manual guidance) | built | `681c5f8`, `9673e00` (path following) |
| `FBFlightControl` (F-16 preset) | built | `681c5f8` |
| `FBAirDataSystem`, `FBRadarAltimeter`, `FBNavSystem`, `FBWarningSystem` | built | `071ea2b` |
| `FBDisplaySystem` (generic MIL-STD-1787-like HUD) | built | `4cb92e8` |
| `FBAirframeControls` + `FBJsbsimAirframeControls` | built | `681c5f8` |
| `FBInputSystem` (the HUMAN'S HANDS: axes with a ramp + a queue of switch throws that leave only as `FBCommandBus::Post`) | **built** | player-control round |
| `FBMfdSystem` (THE MFD BANK: the module's page catalogue, cut per tick against the published blocks and the current loadout, placed on three bays, published as `FBMfdBlock`) | **built** | cockpit round, §10 |
| `FBPropulsionSystem`, `FBWeaponSystem` | NoOp defaults | — |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `systems/FBFlightControl::Run` | `0.01` hard-wired instead of `dt` — silently binds the inner loop to 100 Hz |
| `systems/FBDisplaySystem` | includes `render/FBCamera.h`: a **second** core-lib exception, only `FBHudGeometry.cpp` was documented |
| `systems/FBWeaponSystem` | vestigial — a NoOp stub ticked at 20 Hz while stores and gun are real; its banner describes a superseded state |

### Inventory (from the previous `Offene Punkte` section)

1. **`FBWeaponSystem` is vestigial.** `systems/FBSystemSlots.h` still declares it as a NoOp and
   `FBF16Module` ticks it at 20 Hz (`Weapons->Run(Mode, world, dt)`), although the real weapons work has
   long been in `FBStoresSystem` (10 Hz group) and `FBGunSystem` (every tick). Its banner text
   ("SMS/Stores, CCIP/CCRP, gun: mode-gated … not built speculatively") describes a state that no longer
   holds. To be clarified: delete it or fill it with a purpose.
2. **`FBSystemSlots.h`'s grown-out list is incomplete.** The banner names three grown-out slots (Displays,
   Comms, Sensors); in reality there are six (additionally defensive, stores, gun). A pure documentation
   gap in the code.
3. **`FBFlightControl::Run` has `0.01` hard-wired** instead of `FBFdm::kStepS` or a `dt` argument
   (integrators, slew limit). That tacitly binds the inner loop to exactly 100 Hz; a different substep
   rate would silently shift all integrator gains. Behaviour correct today, because the module ticks
   exclusively with `FBFdm::kStepS`.
4. **Asymmetry ground track vs. nose.** The leg law controls against the GROUND TRACK (`atan2(vx,−vz)`)
   and justifies that at length; COURSE controls with the same `TrackBankCmd` against the NOSE (`s.yaw`).
   The code only says about this that COURSE stays "on its own two numbers", because it is a flown,
   measured approach. Whether the drift objection does not apply to the localizer equally is open.
5. **COURSE uses `KHdg` as `k_dir`.** The same number (0.8) serves the bearing P gain in DIRECT and the
   direction error gain of the cascade in COURSE — not flagged as a double role in the header. A module
   that tunes `KHdg` for DIRECT thereby unnoticeably detunes the localizer.
6. **`FBAirDataSystem` publishes `aoaDeg` only into the telemetry, not into the block.** A consumer on
   the bus (pilot, HUD) cannot get at the AoA although the ADC has it; `FBF16Pilot`'s 11° AoA approach
   will need it.
7. **`GLoadPeak` is never reset** ("running max since boot"). For a multi-phase mission (take-off →
   combat → return flight) there is no peak per segment and no reset API.
8. **`FBNavSystem::MagVarDeg` is hard 0** — no declination model. Every display labelled "magnetic"
   currently shows true.
9. **`FBRadarAltimeter` knows no `dt` and no line-of-sight/terrain-slope modelling** — it is a pure
   difference calculator on the DEM sample beneath the aircraft (no cone footprint, no limit altitude, no
   delay). In inverted/steep attitudes the real CARA delivers nothing; here it keeps delivering.
10. **CLAUDE.md names for `FBNavSystem` "planar ENU geodesy like `home_bearing`/`home_dist`"** — in the
    code, `home_bearing`/`home_dist` has meanwhile become a field of the Platform block written by the
    client resp. the module, not by `FBNavSystem`. No behavioural difference, only an outdated reference
    chain.
11. **`FBDisplaySystem`'s default HUD uses `render/FBCamera.h` (`w3_cam_from`, `w3_horizon_dip_rad`)** —
    a `systems/` slot thereby includes a `render/` header. That is the only place of this kind in the
    slots documented here, and it fits into the core lib only because this maths is CPU-side and
    WebGPU-free (the same exception that CLAUDE.md expressly grants for `systems/FBHudGeometry.cpp` —
    which does NOT name `FBCamera`).
12. **No guidance/FCS damage gate in the module.** `FBSystemId::FlightControls` acts only physically via
    `FBFdm::SetControlAuthority`; the slots themselves keep running and keep publishing. Consistent with
    the intent ("the FLCS commands unchanged, the aircraft does not answer"), but the block invalidation
    rule of the other slots deliberately does not apply here — noted nowhere in the code.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. The slot pattern

#### 1.1 The rule

CLAUDE.md: **FBCore → interface → default implementation → module-specific override.**
In `systems/` that concretely means:

1. **Interface and default are ONE class.** No `IFoo`/`FooImpl` split. The class is directly
   instantiable and delivers the generic behaviour.
2. **Exactly ONE override point per slot** — almost always `Run()`, for `FBDisplaySystem` additionally
   `BuildHud()`. A module whose system differs *in behaviour* derives and overrides that one point.
3. **Number tuning is NOT a derivation.** Gains/configuration are public data fields on the base class or
   a static preset factory (`FBFlightControl::F16()`). An empty derivation that only changes constants
   does not exist.
4. **The default is either REAL or NoOp.** Both are permitted; a NoOp default costs one throttle
   comparison and one empty virtual call.
5. **The module COMPOSES the slots** (`std::unique_ptr<Base>`, not value members — otherwise slicing on
   exchange), owns them and ticks each at its own rate. Slots never call each other.

#### 1.2 The slot inventory

| Slot | File | Default | Override point | State |
|---|---|---|---|---|
| Guidance | `systems/FBAutopilot.h/.cpp` | REAL | `Run(const fb_fdm_state&) → FBGuidance` | full |
| Flight control | `systems/FBFlightControl.h/.cpp` | REAL | `Run(const FBGuidance&, const fb_fdm_state&) → FBControls` | full |
| Air data | `systems/FBAirDataSystem.h/.cpp` | REAL | `Run(FBState&, const fb_fdm_state&, dt)` | full |
| Radar altimeter | `systems/FBRadarAltimeter.h/.cpp` | REAL | `Run(FBState&, elevAslM, groundAslM)` | full |
| Warnings | `systems/FBWarningSystem.h/.cpp` | REAL | `Run(FBState&, dt)` | 3 bits |
| Navigation | `systems/FBNavSystem.h/.cpp` | REAL | `Run(FBState&, const fb_fdm_state&, dt)` | 1 steerpoint + bullseye |
| Displays | `systems/FBDisplaySystem.h/.cpp` | REAL | `BuildHud()` (+ `Run()`) | generic HUD |
| Airframe controls | `systems/FBAirframeControls.h/.cpp` | NoOp + real derivation | every virtual method | full (`FBJsbsimAirframeControls`) |
| Input/HOTAS | `systems/FBInputSystem.h/.cpp` | REAL | `Run(FBMasterMode, FBCommandBus&, nowS, dt)` | keyboard stick + 3 switch actions + the held trigger |
| Propulsion | `systems/FBSystemSlots.h` (`FBPropulsionSystem`) | NoOp | `Run(const fb_fdm_state&, dt)` | empty |
| Weapons (legacy stub) | `systems/FBSystemSlots.h` (`FBWeaponSystem`) | NoOp | `Run(FBMasterMode, const FBWorld*, dt)` | vestigial, see Gaps |

**What has GROWN OUT of `FBSystemSlots.h`** — because its default became REAL instead of NoOp, it got a
file of its own: Input/HOTAS (`FBInputSystem.h`), Displays (`FBDisplaySystem.h`), Comms/datalink (`FBDatalinkSystem.h`), Sensors
(`FBRadarSystem.h`), Defensive (`FBRwrSystem.h` + `FBCountermeasureSystem.h`), Stores
(`FBStoresSystem.h`), Gun (`FBGunSystem.h`). That is why `FBSystemSlots.h` contains neither an
`FBCommsSystem` nor an `FBSensorSystem` stub.

#### 1.3 The communication rules (encoded in the signature type)

| Rule | Mechanics |
|---|---|
| Sensors WRITE `FBState`, displays READ | sensor slots take `FBState&`, `FBDisplaySystem::BuildHud` takes `const FBState&` and is itself `const` |
| One block, one writer | every block in `core/FBAvionicsBlocks.h` names its writer system in the comment; readers consult the validity header first |
| No peer call | no slot holds a pointer to another; the coupling runs exclusively through `FBState` |
| The world only borrowed | whoever needs other units/terrain gets `const FBUnitRegistry*` / `const FBWorld*` per tick, never global, never as a member |
| Exactly ONE fusion exception, documented | fire control reads Nav + Platform; `FBWarningSystem` reads RadarAlt + Ufc + Airframe — both are stated in the respective block comment |

#### 1.4 Rate (how `FBF16Module` cycles the slots)

`modules/f16/FBF16Module.cpp::Run()` is the only place where these slots are ticked. `Due()` is an
accumulator throttle (`accS += dt; if (accS < 1/hz) return false; accS -= 1/hz;`).

| Group | Rate | Content (in order) |
|---|---|---|
| Input, propulsion | per `Run()` (coarsest sim tick) | `Input->Run`, `Propulsion->Run` |
| "sensor group" | 10 Hz | `PublishPlatform` → `PublishAirframe` → commands (sensors/avionics/stores) → radar → **AirData** → **RadarAlt** → set steerpoint → **Nav** → FireControl → Ufc → Sms → **Warnings** (last: pure consumer) |
| Displays | 20 Hz | `Disp->Run` (the display logic, NOT `BuildHud`) |
| Weapons (stub) | 20 Hz | `Weapons->Run` |
| Gun | per `Run()`, full `dt` | the round count is integrated — throttling would invent/swallow shots |
| Defensive | 10 Hz | RWR → commands → CMDS |
| Comms | 5 Hz | commands → datalink |
| Pilot | 10 Hz | `PilotSys->Run` → `SharedState.Bfm` → `NavSys->AdvanceWaypoint` |
| **Guidance + flight control** | **100 Hz** | fixed substep loop, spiral protection ≤ 12 substeps/frame |

The inner loop, literally the control chain:

```
for (k=0; AccS >= FBFdm::kStepS && k < 12; k++) {
  LastG = AP->Run(st);                 // Guidance
  FBControls c = FC->Run(LastG, st);   // FBW inner loop
  fdm.SetControls(c.Roll, c.Pitch, c.Yaw, c.Thr);
  fdm.Step(st);                        // JSBSim, 0.01 s
  AccS -= FBFdm::kStepS; LastSub++;
}
```

`AP->Run()`/`FC->Run()` are the ONLY virtual dispatches inside this loop (one each per substep).
Everything else is outside it and happens at most once per `Run()`.

**Ordering justifications (from the module banner):**
- Platform/Airframe are published FIRST, because the systems below them read them — "one writer per
  block" only stays true that way (previously the fire control read an altitude field that nobody
  filled).
- The commands of a group are processed BEFORE the boxes of that group: a switch thrown acts on the next
  sweep, not on the one after.
- AirData/RadarAlt/Nav/FireControl/Ufc/Sms are ONE throttle group, so that FireControl reads Nav's output
  of the SAME tick.
- Warnings last: pure consumer, including the validity headers.

#### 1.5 Damage gate (`core/FBSystemHealth`, read-only)

The module decides per slot before ticking:

| State | Consequence |
|---|---|
| `Intact` | slot runs normally |
| `Degraded` | slot runs, with a modelled restriction (only where DERIVABLE — e.g. radar range ×0.707) |
| `Failed` | slot is **not ticked at all**, its block goes `Invalid` |

For the slots documented here: `FBSystemId::AirData` → AirData block, `RadarAlt` → RadarAlt block, `Nav`
→ Nav AND Cruise block (the same box publishes both messages). Guidance/flight control have NO gate in
the module — control damage acts physically via `FBFdm::SetControlAuthority`, i.e. the FLCS keeps
commanding unchanged and the aircraft merely no longer answers.

---

### 2. `FBAutopilot` — the guidance

`systems/FBAutopilot.h`, `systems/FBAutopilot.cpp`. Ported from `flightctl.h`'s outer loop, numerics
taken over verbatim (equivalence-tested); COURSE is new (phase 3, the landing) and sits on the same
level as DIRECT, not as a subclass override.

#### 2.1 Output: `FBGuidance`

| Field | Meaning |
|---|---|
| `Mode` | `FBMode::Manual` / `Direct` / `Course` (`core/FBMode.h`) |
| `BankCmdDeg` | commanded bank angle, deg |
| `AltErrM` | target altitude − actual altitude, m, UNCLAMPED (raw material for the inner loop) |
| `TargetVsMs` | desired vertical speed from the altitude loop, m/s |
| `TargetSpeedMs` | speed to be held, m/s |
| `ManualRoll/Pitch/Yaw/Thr` | pass-through in MANUAL |
| `RingDistM` | diagnostic: distance to the direct target point (in COURSE: distance to go) |

#### 2.2 Modes and their setters

| Mode | Setter | What is controlled |
|---|---|---|
| MANUAL | `SetManual(roll,pitch,yaw,thr)` | nothing — pass-through into the inner loop |
| DIRECT (point) | `SetDirect(lat,lon,altM,speedMs)` | bearing to the point + altitude hold |
| DIRECT (path) | `SetDirectLeg(fromLat,fromLon,lat,lon,altM,speedMs)` | the TRACK line from→to + altitude hold |
| COURSE | `SetCourse(refLat,refLon,courseDeg,refElevM,glidepathDeg,speedMs)` | infinite line through the reference point + straight glide path |

`SetDirectLeg` internally calls `SetDirect` and then sets `HaveLeg` — **the same mode with an optional
path origin**, not a second mode.

#### 2.3 "A point is not a path" — why ONE mode with an optional origin

From the class banner:

- Bearing pursuit (pure pursuit) is the RIGHT law when a point is all that exists: intercept, turning
  fight, search — there is no line one would have to be on there.
- It is the WRONG law as soon as a path exists, and that follows from a property of pure pursuit, not
  from tuning: **its position stiffness falls off as 1/R**, so at distance it cannot hold a line at all.
- **Measurement:** on the 19 km CCRP run-in a bank asymmetry of 0.2° drove the jet **31 m** off its own
  attack path, while the loop answered with 0.2° of bank.
- Consequence: `SetDirectLeg` — same mode, same altitude/speed half, the path as DATA from the caller.
  **No path = no line = the bearing law, unchanged.**
- Precisely for that reason a defender commanded to a point INSIDE its own turn radius circles that point
  forever (`sim/missions/bfm-basic.fbm`) — measured: 58.9° bank, 248 KCAS, 4,000 m, 4.7 °/s, ~1,880 m
  radius, constant from t=20 s to the end of the mission.
- A second mode would be wrong, because it would duplicate the altitude, speed, capture and telemetry
  half and teach every caller, every override and `FBMode` itself a third word for the same manoeuvre.

`kMinLegM = 100.0` m: shorter than that, a "path" is a coordinate pair and not a direction — the bearing
between its ends loses its meaning long before the aircraft could fly it. `SetDirectLeg` then falls back
silently to the bearing law. Two mission fixes lie kilometres apart; the barrier therefore only catches a
degenerate declaration.

#### 2.4 The path-following law — complete derivation

**Structure (`TrackBankCmd`, one definition, two users — localizer and route/run-in leg):**

```
intercept = clamp(-k_xt * across, ±interceptMax)
dirErr    = wrap180(course + intercept - dir)
bank      = clamp(k_dir * dirErr, ±bankMax)
```

Cascaded instead of summed, on purpose: **the cap limits the initial angle a large offset may demand**.
A jet ten kilometres beside its path flies a steady angle towards it instead of an instantaneous
right-angle turn. For small errors the cascade IS the two-state feedback
`bank = −(k_xt·k_dir·y + k_dir·Δχ)` — and exactly from that the roots below are computed.

**The plant is second order.** Coordinated turn, small angles, `y` = cross-track offset, `Δχ` =
track angle error, `φ` = bank angle:

```
y'   = V · Δχ            (the offset moves with the speed times the TRACK error)
Δχ'  = (g/V) · φ         (the velocity vector turns with the bank angle)
```

Feedback of both states, `φ = −(k_y·y + k_χ·Δχ)`, closes the plant as

```
y'' + (g·k_χ/V)·y' + (g·k_y)·y = 0
ωn = √(g·k_y)          ζ = k_χ·√(g/k_y) / (2V)
```

**Two unknowns → exactly two statements, no more:**

| # | Statement | Justification |
|---|---|---|
| (1) DAMPING | `ζ = 1/√2` | the same textbook root "settle without overshoot" on which the gun tracking is also closed (`FBPilot::kBfmTrackKi`). Nothing about a route segment argues for a different one. |
| (2) AUTHORITY | the cross-track term alone reaches the bank LIMIT at an offset of exactly ONE turn radius, `R = V²/(g·tan φmax)` | the only self-referential statement available here: an offset the aircraft could not close inside its tightest turn is exactly the offset for which the tightest turn is the right answer — everything beyond that is saturated, because there is nothing left to command. |

**Solution.** From (2): `k_y = φmax / R(V) = φmax·g·tan(φmax) / V²`. Substituting into ζ yields the
remarkable part — `k_χ` falls out INDEPENDENTLY of speed AND gravity:

```
k_χ = 2·ζ·√(φmax · tan φmax)      [deg of bank per deg of track error]
k_y = φmax / R(V)                 [deg of bank per metre]
```

The whole law is thereby fixed by `BankMaxDeg`; **only the cross-track half is scheduled, and with 1/V²**
— exactly that keeps the damping at 1/√2 across every speed the same jet flies. In the cascade the
cross-track gain is `k_y/k_χ` and the intercept cap is `φmax/k_χ` (the angle at which the cross-track
term alone already demands everything the bank limit allows) — both fall out of the same two statements
instead of being chosen.

**Numbers at `BankMaxDeg = 60°`** (φmax = 1.0472 rad; the root is formed in RADIANS, because it is an
angle ratio, while `k_y` is output in deg/m):

| Quantity | Value |
|---|---|
| `k_χ` (= `kDir`) | 2·0.7071·√(1.0472·1.7321) = **1.905** deg/deg |
| Intercept cap | 60 / 1.905 = **31.5°** |
| `R` at 85 m/s | 85² / (9.80665·1.7321) = **425 m** |
| `k_xt` at 85 m/s | 60 / (1.905·425) = **0.074 deg/m** |
| `R` at 231 m/s (450 kt) | **3,141 m** |

**Cross-check — and the reason the derivation is believed at all:** at the flown F-16 approach speed
(165 KCAS ≈ 85 m/s) the schedule delivers 0.074 deg/m against the 0.08 deg/m that the localizer (`KXt`)
has been flying by hand since phase 3, and a 31.5° intercept cap against its 45°. That is **8 %**
deviation from a gain that was found independently, on an aircraft three times slower than the one on
which the path law was measured — that is the statement that the schedule describes the AIRCRAFT and
does not fit one case. COURSE nevertheless stays on its own two numbers: that is a flown, measured
approach, and this round was not about the landing.

**Why against the GROUND TRACK and not against the nose** (`FBAutopilot.cpp`, direct/leg branch): the
cross-track offset is the integral of where the aircraft ACTUALLY flies. If one controls the heading
instead, any sideslip or crab angle between the two remains as a **permanent drift rate** that no
cross-track gain can ever see. The track angle therefore comes from the velocity vector:
`trackDeg = atan2(vx, −vz)` (X-Plane-local: +x east, +z south, hence north = −z). Below
`kLegMinSpeedMs = 30.0` m/s the velocity vector has no direction worth controlling (jet on the ground,
stall) — then the law reads the nose (`s.yaw`). That threshold lies far below any speed at which a path
is flown.

**The static residual offset — and why no integrator.** Against a CONSTANT lateral disturbance (an
airframe that needs 0.2° of bank to fly straight — that of the vanilla model) a two-state feedback holds
a standing offset of `φ_d / k_y`. That is intentional:

| Argument | Number |
|---|---|
| An integrator on a path that is switched, re-anchored and discarded at every waypoint is a wind-up problem | — |
| Residual offset it would remove, at 450 kt | ~**10 m** (0.2° / 0.0191 deg/m) |
| Offset that the BEARING LAW leaves standing there anyway | ~**30 m** |

So it is a **bounded, speed-scheduled** offset instead of an unbounded, distance-driven one.

`(void)along;` in the leg branch is a statement of fact: the along-track coordinate is a matter of
SEQUENCING (`FBNavSystem::AdvanceWaypoint`), not of flying.

#### 2.5 DIRECT — altitude and bearing half

| Element | Law | Number |
|---|---|---|
| Bearing (without a path) | `bank = clamp(KHdg · wrap180(brg − yaw), ±BankMaxDeg)` | `KHdg` = 0.8 deg/deg |
| Altitude | `TargetVsMs = clamp(KAlt · AltErrM, ±25 m/s)` | `KAlt` = 0.08 (m/s)/m |
| `RingDistM` | `hypot(n,e)` — diagnostic, not part of the control | — |

The VS cap of 25 m/s is tighter than a cruise altitude correction would need: DIRECT also drives the
climb after take-off, where the error is the WHOLE climb (thousands of metres). Uncapped, the FLCS with
its own alpha schedule would keep the AoA safe, but the resulting almost 30° of pitch is an unnecessarily
aggressive climb angle for a controlled climb-out.

#### 2.6 COURSE — localizer + glide path

Tracks the infinite line through `(refLat,refLon)` on true `courseDeg` and descends on a straight glide
path `glidepathDeg` onto `refElevM` AT the reference point. Generic over what the reference point MEANS —
the caller (`FBPilot`) supplies threshold + runway direction.

| Element | Law |
|---|---|
| Projection | `FBTrackProjectM` → `along`/`across`; `distToGo = −along` (positive BEFORE the reference point) |
| Lateral | `TrackBankCmd(CourseDeg, across, s.yaw, KXt, CourseInterceptMaxDeg, KHdg, BankMaxDeg)` |
| Target altitude | `RefElevM + tan(glidepath) · max(distToGo, 0)` |
| Vertical | `TargetVsMs = clamp(−tan(gp)·gs + KAlt·AltErrM, ±ApproachVsCapMs)` |

**The feedforward and why it exists:** a pure P correction on the altitude error has a standing lag error
against this RAMPING target (classic type-1 servo lag, `e_ss = target rate / KAlt`). **Measured: ~56 m too
high at the threshold on a 9 nm final** with the default `KAlt` — i.e. touchdown deep in the runway
instead of near the threshold. The feedforward of the target sink rate (`tan(glidepath) × approach
speed`) leaves the P term only the DEVIATION from the beam, not the tracking of its slope.

Behind the reference point (`distToGo ≤ 0`, e.g. when `FBPilot` has long handed over to flare and a
`Run()` still lands here) the target simply holds `RefElevM` instead of diving below it.

The projection convention (`along=0` at the reference point, `+across` = right of the course) is the SAME
as in `FBPilot`'s centreline law and `FBMissionMonitor::OnRunway` — all three agree about what "on the
line" means.

#### 2.7 Gains (public config block, defaults = the flown F-16 preset)

| Field | Default | Meaning |
|---|---|---|
| `BankMaxDeg` | 60 | bank limit; **fixes the entire law in DIRECT leg** |
| `KHdg` | 0.8 | direction error → bank (DIRECT bearing and COURSE; in DIRECT leg REPLACED by the derived `k_χ`) |
| `KAlt` | 0.08 | altitude error → vertical speed |
| `KXt` | 0.08 | COURSE: cross-track offset (m) → intercept angle offset (deg) |
| `CourseInterceptMaxDeg` | 45 | COURSE: cap of that angle |
| `ApproachVsCapMs` | 8 | COURSE VS cap (a flown glide path, not a dive) |
| `kMinLegM` | 100 (constexpr) | minimum length of a path |
| `kLegZeta` | 1/√2 (file constant) | damping of the path law |
| `kLegG0` | 9.80665 | g |
| `kLegMinSpeedMs` | 30 | below: nose instead of track angle |

**`Run()` is the ONE override point.** A module whose guidance really behaves differently (not just
different gains) derives and overrides `Run()`; `FBF16Module` composes this default UNCHANGED.
Configuration differences (F-16 gains) stay data on this class, not a subclass.

---

### 3. `FBFlightControl` — the FBW inner loop

`systems/FBFlightControl.h`, `systems/FBFlightControl.cpp`. Consumes `FBGuidance` + JSBSim state, emits
normalised stick/throttle values (`FBControls{Roll,Pitch,Yaw,Thr}`). Ported from `flightctl.h`'s inner
loop, numerics verbatim.

#### 3.1 Two inner kinds, ONE flag

`Flcs` is **configuration, not a subclass** — it is tuning, not behaviour:

| `Flcs` | Kind | Approach |
|---|---|---|
| 1 | **FLCS** | command a real FLCS like a pilot (F-16): bank error → roll-rate stick, g-command PI with 1/cos(bank) turn feedforward + VS error bias/integral → pitch stick, lateral-g nulling → pedals, PI throttle. **The airframe's FLCS is the stabiliser; we command it.** |
| 0 | **Raw** | attitude-hold PD directly on the control surfaces (models WITHOUT an FLCS, the c172 path) |

#### 3.2 Relation to the real FLCS and `fcs/fbw-override`

- The vanilla JSBSim F-16 is a REAL FLCS: `fcs/*-cmd-norm` are **rate setpoints**, not control surface
  deflections.
- `FBFdmSpawn::FbwOverride` (`fdm/FBFdmBoot.h`) sets `fcs/fbw-override = 1.0` at spawn (`fdm/FBFdm.cpp`):
  with that, FlightBox's own FCS is the controller instead of the model-owned flight control — direct
  surfaces, otherwise two nested rate loops.
- The way into the physics is exclusively `FBFdm::SetControls(roll,pitch,yaw,thr)`:
  - `roll → fcs/aileron-cmd-norm`
  - `pitch → fcs/elevator-cmd-norm` as **`−pitch + ElevTrim`** (JSBSim: +elevator = nose DOWN;
    FlightBox: +pitch = nose UP; `ElevTrim` is the elevator found by the trim run = the trim tab that
    holds LEVEL at neutral stick instead of the nose-high resting attitude of the airframe)
  - `yaw → fcs/rudder-cmd-norm` (+yaw coordinates the turn; −yaw skids it — measured, strong adverse yaw
    moment)
  - `thr → fcs/throttle-cmd-norm`, **slew-limited** (a jump 0 → 0.95 blows up the engine's RPM ODE and
    makes the airframe depart)
  - Battle damage acts EXACTLY HERE: `roll/pitch/yaw *= Authority`, `thr` capped — the FCS commands
    unchanged, the aircraft no longer answers.

#### 3.2b The alpha limiter also acts at the HAND STICK (MiG-29 stage 2c)

`Run` returns early in `FBMode::Manual` — the pilot's stick is the command, and there is nothing to
regulate. Until stage 2c the limiter returned early with it, and that was a defect rather than an
omission: `AlphaLimitDeg` models the airframe's own STICK FORCE, and a stick force does not know which
mode the autopilot is in. It never showed because the manual phases (Takeoff, Flare, Rollout) do not
pull; **BFM is the first phase that really does**, and the first airframe whose DECK carries no limiter
of its own (the MiG-29) departed at 32° of incidence and 11.8 g after 8.9 s.

The law is now applied in both branches — there is only one limiter. Two refinements to the hand-stick
branch landed with the MiG-29 BFM round ([`pilot.md`](pilot.md) §5.10 — the close-combat law departed
the deckless airframe until they did):

- **The limiter may PUSH to recover, bounded to −`PitchStickMax`.** It first only forbade pull
  (`byAlpha` floored at 0) — the FLCS branch always let `byAlpha` go negative and push, and dropping that
  at the hand stick left the raw airframe no recovery authority: when α overshot the 26° SOS at low speed
  in a hard pull, the pilot's own −0.3 push could not arrest it and α ran to 150° into a mush. Now the
  hand-stick limiter pushes too, but no harder than the airframe's own deflection cap (−`PitchStickMax`,
  0.6 on the MiG) — so a genuine departure recovers, while the first-tenth transient the old floor was
  defending against (a −44.6 spike from the sampled incidence-rate discontinuity) is caught by
  `AlphaPrimed` and bounded by the cap, not by refusing to push.
- **`PitchStickMax` binds at the hand stick too.** It is an AUTHORITY limit (unlike `RollStickMax`, a
  nav-mode roll number the combat roll must not obey), and it bound only on the FLCS path; the Manual
  path passed the pilot's up-to-1.0 pitch through, and on the deckless MiG that is 35° of stabilator = a
  tumble. Now clamped on both.

For an airframe with its own FLCS in the deck `AlphaLimitDeg` is 0 and `PitchStickMax` is 1.0 (`F16()`) —
the α branch is dead, the clamp a no-op, and the whole expression is bit-identical, verified across the
mission set (13 F-16 BFM/gun/BVR/attack missions byte-identical; `mig29-full` moves 143.4 → 143.7 kt at
touchdown, the correct `PitchStickMax` now binding in the flare).

#### 3.3 The FLCS law, step by step (all at a fixed 0.01 s)

```
bc      = min(|roll|, 80°) → rad
vsErr   = TargetVsMs − vy
VsIterm = clamp(VsIterm + KVsi·vsErr·0.01, ±0.5)
nzRaw   = 1/cos(bc) + KVs2g·vsErr + VsIterm
nzCmd   = clamp(nzRaw, NzPrev ± NzSlew·0.01)      // slew limit on the g COMMAND
gErr    = nzCmd − nz
bankErr = |BankCmdDeg − roll|
blend   = clamp(1 − (bankErr − 5)/15, 0, 1)       // 1 at ≤5°, 0 from 20°
GIterm  = clamp(GIterm + blend·KGi·gErr·0.01, ±1)
Roll    = clamp(KRollRate·(BankCmdDeg − roll), ±RollStickMax)
Pitch   = blend · clamp(KG·gErr + GIterm, ±1)
NyIterm = clamp(NyIterm + KNyi·ny·0.01, ±0.6)
Yaw     = clamp(KNy·ny + NyIterm, ±1)
```

The four design decisions, each measured against the bare model:

| Element | Justification |
|---|---|
| Turn g feedforward from the ACTUAL bank angle | the g a turn needs NOW depends on the bank angle NOW |
| VS error INTEGRAL | breaks up the wrong altitude/g equilibria |
| Slew-limited g command | `NzSlew` = 1.5 g/s |
| g stick only BLENDED IN once the bank angle is nearly settled | a pilot rolls in with neutral pitch; driving the g loop in the middle of the roll stacks it onto the FLCS's own g PID → entry spikes |

**Yaw pedals:** null the lateral load factor. The model's yaw damper (raw r feedback, no washout) holds
rudder against a steady turn; a residual `ny` of about **−0.10 g** remains **model-intrinsic** — an
accepted model property, not a defect (principle 5).

**Throttle (in both kinds):**
`ThrIterm = clamp(ThrIterm + KTi·spdErr·0.01, −ThrTrim, 1−ThrTrim)` — the integrator range is the
PHYSICAL travel from the trim setting to both stops. `Thr = clamp(ThrTrim + KpSpd·spdErr + ThrIterm,
0, 1)`.

**Raw branch (FLCS-less models, `flightctl.h`'s `fc_update` verbatim):**
`pitchCmd = clamp(KAltRaw·AltErrM, ±PitchMaxDeg)`; `Roll = clamp(KpRoll·(BankCmd−roll) − KdRoll·p, ±1)`;
`Pitch = −clamp(KpPitch·(pitchCmd−pitch) − KdPitch·q, ±1)`; `Yaw = clamp(−KdYaw·r + KCoord·BankCmd, ±1)`.

**MANUAL** is passed through right at the front: `o = {ManualRoll, ManualPitch, ManualYaw, ManualThr}`,
no integrator movement, no loop.

#### 3.4 Gains: default vs. the `F16()` preset

| Field | Default (raw/c172 era) | `F16()` | Unit/meaning |
|---|---|---|---|
| `Flcs` | 0 | **1** | inner loop kind |
| `RollStickMax` | 1.0 | **0.15** | cap of the roll stick ("gentle roll-in") |
| `KRollRate` | 0.06 | **0.05** | bank error → roll stick |
| `KG` | 0.4 | **0.25** | g error → pitch stick (P) |
| `KGi` | 2.0 | **0.8** | g error → pitch stick (I) |
| `KVs2g` | 0.05 | 0.05 | VS error → g command |
| `KVsi` | 0.02 | 0.02 | VS error integral |
| `KNy`/`KNyi` | 1.5 / 2.0 | 1.5 / 2.0 | lateral g → pedals (P/I) |
| `KTi` | 0.002 | 0.002 | speed integral |
| `KpSpd` | 0.03 | **0.02** | speed error → throttle |
| `ThrTrim` | 0.55 | **0.85** | throttle trim point |
| `NzSlew` | 1.5 | 1.5 | max. g command change per second |
| `KpRoll`/`KdRoll` | 0.022 / 0.004 | (unused) | raw branch |
| `KpPitch`/`KdPitch` | 0.06 / 0.010 | (unused) | raw branch |
| `KdYaw`/`KCoord` | 0.006 / 0.004 | (unused) | raw branch |
| `PitchMaxDeg`/`KAltRaw` | 15 / 0.05 | (unused) | raw branch |

**`RollStickMax = 0.15` is a measurement:** with it `nz` stays in the range **0.7…1.9 g**; at 0.35 it was
**−1.1…+3.0 g**.

#### 3.5 Secondary duties

- **`FBTelemetrySource "flightcontrol"`** — columns `rollCmd`, `pitchCmd`, `yawCmd`, `throttleNorm`.
  `Run()` caches its result in `LastControls_`; at 100 Hz the telemetry bus (10 Hz) always reads the most
  recent one.
- **`Reset()`** zeroes all integrators (`GIterm`, `VsIterm`, `NyIterm`, `ThrIterm`, `NzPrev`) — a new
  flight.
- **`GetGIterm()`/`GetVsIterm()`** are diagnostic windows into the integrators.

---

### 4. `FBAirDataSystem` — air data (ADC class)

`systems/FBAirDataSystem.h/.cpp`. Writes `FBState::AirData`. Airframe-agnostic: every module with an ADC
and a velocity vector gets the same numbers; an airframe whose air data chain really differs overrides
`Run()`.

| Block field | Source | Note |
|---|---|---|
| `CasKt` | `fdm.cas · kMsToKt` | calibrated airspeed |
| `Mach` | `fdm.mach` | direct |
| `GLoad` | `fdm.nz` | body normal load factor |
| `GLoadPeak` | running maximum since boot | is never reset (HUD peak G) |
| `TrackDeg` | `atan2(vx, −vz)`, brought to 0…360 | ground track from the velocity vector |
| `FpaDeg` | `atan2(vy, max(horiz, 0.01))` | flight path angle, + = climbing |

**FPM direction as a WORLD azimuth/elevation, not as a body-relative offset:** `FBF16Hud` projects it
through the SAME camera basis (yaw/pitch/roll) that the conformal horizon already uses — so the attitude
does not have to be composed twice.

`vx/vy/vz` are X-Plane-local (+x east, +y up, +z south) — already the local ENU frame, no geodesy needed.
`horiz = √(vx²+vz²)`.

Telemetry source `"airdata"`: `casKt`, `mach`, `nz`, `aoaDeg`. The class caches its own last values,
because `SampleTelemetry` has no `FBState` in its signature (core architecture rule: a source samples its
OWN last result). `aoaDeg` is in the telemetry, but NOT in the block.

`dt` is ignored (`(void)dt`) — the system is stateless apart from `PeakG`.

---

### 5. `FBRadarAltimeter` — the reference case for `Invalid`

`systems/FBRadarAltimeter.h/.cpp`. Writes `FBState::RadarAlt`.

#### 5.1 Contract

- **It does NOT query the terrain itself.** It converts the pair `(elevAslM, groundAslM)` in metres ASL,
  already resolved by the client, into the radar altitude in feet — the same DEM sample the app fetches
  for `FBRenderer::SetAgl` anyway. No second terrain query.
- `AglFt = (elevAslM − groundAslM) · 3.280839895`.
- One switch: `SetPowered(bool)`, default `true`.

#### 5.2 What it teaches

> The box is a POWERED box, and `doc/modules/f16/controls-commands.md` §6.4 documents the consequence verbatim:
> the CARA ALOW warning fires **only** with the radar altimeter powered and transmitting — no matter how
> willingly the DED accepted the threshold.

From that the rule that applies to the whole bus:

| Case | Behaviour |
|---|---|
| powered | `AglFt` recomputed, `H.Publish(NowS)` → `Valid` |
| unpowered | `H.Invalidate()` → `Invalid`, **the last number stays standing** |

Unpowered, the box publishes **neither** "0 ft" **nor** an old value as fresh. It invalidates its block,
and **every consumer then has to say what it does without it** — the pilot's AGL floor, the HUD's R
field, the warning system. That is the gain of the three-state header; a box that instead published 0 ft
would fly the jet into the ground.

That the last number REMAINS standing in the field is likewise intentional: a consumer who ignores the
header must not silently get a fresh-looking zero.

---

### 6. `FBWarningSystem` — the warning set as a bitmask

`systems/FBWarningSystem.h/.cpp`. Writes `FBState::Warnings` and **nothing else**; reads radar altitude,
the thresholds committed by the UFC and the Airframe block.

> It commands nothing — a warning system that could act would be a second pilot.

#### 6.1 Why it exists: making the validity headers consistent

Every condition here is a fusion of blocks written elsewhere. Every one of them can therefore be
**UNEVALUABLE** — and that is a third answer, distinct from "warning" and "no warning". The block carries
it separately:

```
struct FBWarningBlock { FBBlockHeader H; uint32_t Active; uint32_t Inhibited; };
```

An `Invalid` radar altitude block does **not** mean "not low". It means: nobody can say — and the
annunciator says exactly that.

#### 6.2 The three bits (`core/FBAvionicsBlocks.h::FBWarningBit`)

| Bit | Value | Active when | Inhibited when | Not evaluated at all when |
|---|---|---|---|---|
| `FBWarnAlow` | 1<<0 | `RadarAlt.AglFt < Ufc.AlowFt` | `Ufc` readable & `AlowFt > 0` & `RadarAlt` NOT readable | `Ufc` unreadable or `AlowFt == 0` (no threshold entered = nothing to warn about, **no** inhibit) |
| `FBWarnBingo` | 1<<1 | `Airframe.FuelLbs ≤ Ufc.BingoEffectiveLbs` | `Ufc` readable & `BingoEffectiveLbs > 0` & `Airframe` NOT readable | `Ufc` unreadable or threshold 0 |
| `FBWarnGearUnsafe` | 1<<2 | `Airframe.WeightOnWheels && GearPosition < 0.99` | `Airframe` NOT readable | — |

Subtleties that are justified in the code:

- **BINGO against the EFFECTIVE threshold, not against the entered one.** The jet carries two numbers
  (`doc/modules/f16/controls-commands.md` §6.8): the DED field shows what the pilot TYPED (`BingoLbs`), the
  warning fires at the system limit (`BingoEffectiveLbs`). Displays read the first, the warning system
  the second — merging them would have made the documented clamping invisible.
- **Gear is the control case.** It is the only condition whose inputs come from ONE block — which makes
  it the reference against the two fusions above.
- `Readable()` = `Valid` OR `Held`. A deliberately frozen block is therefore read; only `Invalid`
  inhibits.

Telemetry source `"warn"`: `warn_active`, `warn_inhibited` (both as integer bitmasks).

---

### 7. `FBNavSystem` — steerpoint + bullseye + sequencing

`systems/FBNavSystem.h/.cpp`. Writes `FBState::Nav` AND `FBState::Cruise` (one source system may publish
more than one message).

#### 7.1 Scope and setting

One active steerpoint plus a bullseye reference. The real hardware knows a steerpoint database (26…30
markpoints plus the mission steerpoints, `doc/modules/f16/navigation-ils.md`) — **this is the one-point
placeholder every module starts with.**

| Setter | Meaning |
|---|---|
| `SetSteerpoint(lat, lon, elevFt)` | `elevFt` = the steerpoint's OWN ground elevation (input for `FBF16FireControl`'s slant range) |
| `SetBullseye(lat, lon)` | in the F-16 module: the mission's runway (`FBF16Module::SetRunway`) — a `.fbm` declares no bullseye, and the runway is the one briefed geographic point all units share |

#### 7.2 Geodesy

**The same planar ENU approximation as everywhere else in the tree** (`core/FBGeodesy.h`, the ONE
definition): `north = Δlat · 111320 m/deg`, `east = wrap180(Δlon) · 111320 · cos(ref_lat)`. Steerpoints
lie tens of nm away, not intercontinentally — the flat-earth error is negligible, and it stays consistent
with the rest of the code instead of introducing a second geodesy convention. The wrap handling is part
of the primitive: unwrapped, a 360° difference at the antimeridian read ~38,000 km for a point one metre
further on.

Convention: **the reference point comes FIRST and owns the cosine.**

#### 7.3 What is published

| Field | Computation |
|---|---|
| `Nav.SteerBearingDeg` | bearing aircraft → steerpoint, 0…360 |
| `Nav.SteerElevAngleDeg` | `atan2(StElevFt·kFtToM − fdm.elev, max(dist,1))` |
| `Nav.SteerDistNm` | horizontal distance |
| `Nav.SteerElevFt` | passed through |
| `Nav.BullBearingDeg` / `BullDistNm` | bearing/distance **FROM the bullseye TO the aircraft** |
| `Nav.MagVarDeg` | **0.0 — placeholder, no declination model** |
| `Cruise.SteerTtgS` | `dist / max(gs, 1)` |

`Nav.H.Publish` only if a steerpoint OR bullseye is set.

#### 7.4 The `Held` case: the CRUS side with the gear down

The real jet FREEZES the computed CRUS fields with the gear down instead of clearing them
(`doc/modules/f16/controls-commands.md`, CRUS table). Because that is a property of the MESSAGE, it has a header
of its own — which is why `Cruise` is a separate block:

```
gearDown = Airframe.H.Readable() && Airframe.GearPosition > 0.5
gearDown ? Cruise.H.Hold() : (Cruise.SteerTtgS = …, Cruise.H.Publish(NowS))
```

`Hold()` does NOT move the timestamp — exactly that makes "how old is this frozen number" answerable.
Bearing/distance in the Nav block keep running unaffected.

#### 7.5 `AdvanceWaypoint` — sequencing as ACTOR behaviour

```
int AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM = 500.0)
```

**Who calls:** the MODULE itself, at the pilot rate (10 Hz), right after the decision
(`FBF16Module::Run`). **Not** the mission orchestrator — this system already knows steerpoints and
distances, so the sequencing belongs here and not in the runner's bookkeeping. The runner's judge
(`core/FBMissionMonitor`) judges independently of it on its own, immutable copy of the plan.

**Three grounds for fulfilment:**

| Ground | Test | Applies where |
|---|---|---|
| `capture` | `FBPlanarDistM(aircraft, wp) ≤ captureM` (default 500 m) | every waypoint |
| `passed` | Projection onto the leg `wp[idx−1] → wp[idx]`: `alongM ≥ legM` | the fix has a PREDECESSOR (`idx > 0`) |
| `orbited` | the approach record shows **two** failed approaches to this fix (below) | the fix has a SUCCESSOR (`idx + 1 < size`) |

**Why a capture circle alone is not enough:** a capture circle cannot answer a waypoint at which the
aircraft PHYSICALLY cannot arrive — one that lies inside its own turn radius and which it orbits from
outside forever. (`bfm-basic.fbm` uses exactly that deliberately, to make a defender turn.) The answer is
**not a larger circle** but the axis that the leg defines anyway: **beyond the perpendicular through the
fix, the fix has been passed**, however large the miss distance was.

**Why that does not destroy the deliberate permanent circle of the BFM defenders:** the rule exists
EXACTLY WHERE the LEG exists — the same two declared fixes whose track `FBPilot` flies. A FIRST waypoint
has no incoming leg, so there is no "beyond" to measure, and the point is pursued as before. In
`bfm-basic.fbm` the defender has exactly ONE `wp` line (index 0): no predecessor, no leg, no `passed`
check, the orbit remains.

**With that, sequencing and guidance agree by construction:** the flying holds a line
(`SetDirectLeg`) only where the bookkeeping can recognise that the line has ended.

##### 7.5.1 `orbited` — the ground the wind made necessary

**The defect it answers** ([MESS] `missions/wx-orbit.fbm`, and the finding recorded in
`missions/wx-gfs-fixture.fbm`'s header): at 9,000 m in a 20 m/s wind (18.6 m/s of it across the leg)
the jet's closest approach to a steerpoint dead ahead is **614 m** — 114 m outside the capture circle —
after which it settles into a **permanent limit cycle**: range 1,793…4,851 m, −59.1° of bank, 99.2 s per
lap, unchanged to the timeout. The same file with `wx calm` captures the same fix at t = 167.3 s with
**495.6 m** of closest approach. Four metres of margin is the whole difference between a route and an
orbit.

**Why it happens is a mismatch of frames, and it is stated in this file already** (§2.4, "Why against
the GROUND TRACK and not against the nose"): a fix WITHOUT a leg is flown by the bearing law (§2.5),
which controls the **nose**. In wind the ground track is not the nose, so the approach carries a
standing lateral drift the bearing gain can never see — the residual §2.4 quantifies at ~30 m for a leg
becomes 114 m of miss distance over 38 km of run-in here. And a capture circle is a GROUND test with a
FIXED radius, while the circle the aircraft can fly lives in the AIR MASS and is carried by the wind: at
213 m/s the tightest turn has R = 2,670 m, so the 500 m circle is 5 % of it.

**Why not a geometric reachability test.** The obvious statement — "unreachable when `d < 2R·sin ψ`,
with R widened by the drift" — needs the WIND, and nothing in the aircraft hands the navigation computer
a wind vector (the same fact that makes the CCRP release miss in wind, `missions/weather.md`). Worse, at
index 0 that test fires on `bfm-basic`'s defender too: in its settled orbit the fix is abeam at
1,217…2,069 m against `2R sin ψ` ≈ 2,918 m, so ANY reachability test evaluated at index 0 destroys the
deliberate orbit. The test therefore has to be built from what the aircraft observes **about itself**.

**The signature of an orbit is that it comes back.** One failed approach is a DEPARTURE — an attack
egress, a BFM conversion, a turn onto the next leg. Two are a LAP: the aircraft went as close as it
could, opened up, came all the way around, and is no nearer. Formally, a small record per active fix:

```
closing:  min ← min(min, d);   d > min + captureM  →  failures++, state ← opening, max ← d
opening:  max ← max(max, d);   d < max − captureM  →  state ← closing, min ← d
reset on every change of the active index
failures ≥ 2  →  orbited
```

| Choice | Why this and not another |
|---|---|
| Threshold **2** | [MESS, 54-mission sweep] at 1, `attack-ccip`/`attack-hardened` sequence their target fix at t = 87.9 s — the egress, not an orbit. At 2, **nothing** in the sweep trips but the instrument. One is a departure, two is a lap. |
| Margin = `captureM` | the mission's own statement of "how near counts as there" is the only tolerance in this rule; using it for both the opening and the re-closing keeps it the ONE number instead of inventing a second. |
| Bound to a **SUCCESSOR** | sequencing means ADVANCING. A fix with no successor is not a step in a route, it is its destination, and "still trying to get there" is the correct state — which is exactly what `bfm-basic`/`gun-turning`/`bvr-duel` express with a single `wp`. [MESS] without this gate the rule fires on all four BFM defenders (t = 139.4 s), on both `bvr-duel` fighters and on `gun-dry`'s pursuer. `passed` is bound to the predecessor because it needs an inbound axis; `orbited` is bound to the successor because it needs somewhere to go. |
| Cost: one lap of evidence | [MESS] `wx-orbit.fbm` fires at t = 311.6 s against a closest approach at t = 167.0 s. A rule that decided faster would be deciding on a departure. |

**Open, deliberately not done here:** the ROOT cause is the bearing law's nose reference (§2.5). Making
it control the ground track like the leg law would let the jet close the fix instead of being sequenced
past it — but it moves every index-0 trajectory in the tree, including the four BFM defenders' orbits,
and this round was about the sequencing rule the two authorities share. Recorded under "Gaps".

Return: the plan index just reached, or −1. Log line: `FBLog::Info("nav", "WP_REACHED", {idx, lat, lon,
by=capture|passed|orbited})`.

---

### 8. `FBDisplaySystem` — the generic default HUD

`systems/FBDisplaySystem.h`, `systems/FBDisplaySystem.cpp`. The first slot with a REAL instead of a NoOp
default — which is why it grew out of `FBSystemSlots.h`.

#### 8.1 Two separate entry points

| Method | Rate | Caller | Purpose |
|---|---|---|---|
| `Run(const FBState&, FBMasterMode, dt)` | 20 Hz (module) | `FBF16Module::Run` | periodic display logic (MFD pages, warning lamps) — default empty |
| `BuildHud(const FBState&, const FBHudEnv&, FBHudGeometry&) const` | 1× per rendered frame | `render/stages/FBHudStage::Encode` | regenerate the whole symbology |
| `BuildMfd(const FBState&, const FBHudEnv&, FBHudGeometry&) const` | 1× per rendered frame | same | the bottom grid row: three bays, the page each carries taken from `FBMfdBlock`. **Appends** — it does not `Reset()` — so HUD and bank land in ONE geometry and therefore ONE render pass |

`FBHudEnv` carries the grid: `Width`/`Height` stay the frame the projector's pixel scale is defined by,
and the added `ViewH` is the windscreen's lower edge — the combiner is centred in `ViewH`, the bank is
drawn between `ViewH` and `Height`. `ViewH == Height` means "no cockpit", and every frame drawn that
way is what it was before the grid existed.

#### 8.2 Division of labour with `render/`

```
systems/FBDisplaySystem::BuildHud   → LOGIC/symbology, fills …
systems/FBHudGeometry                → the reused 2D geometry buffer (pixel coordinates)
render/stages/FBHudStage            → the pure WebGPU backend: uploads the vertex streams VERBATIM
```

- `FBHudStage` holds the display system reference only BORROWED (`SetDisplaySystem`, `nullptr` = empty
  HUD); the client wires it from the active module (`R.SetHudDisplay(&module.Displays())`).
- `FBHudStage` for its part caches an `FBState` copy (`SetState`) plus `Agl`, builds
  `FBHudEnv{Width, Height, Agl, Have}` from it and calls `BuildHud` — per frame, after `Geometry.Reset()`.
- `FBHudGeometry` knows two primitives: **strokes** (`x,y,d,hw,r,g,b`, 6 vertices per segment, analytic
  coverage AA) and **glyphs** (`x,y,u,v,r,g,b`, 6 per character, bitmap font atlas). Vectors instead of
  fixed arrays: `Reset()` clears without releasing capacity, after the first frame nothing allocates any
  more.
- `FBHudGeometry::SetClip/ClearClip` is the scissor bracket for conformal symbology (Liang-Barsky for
  strokes, all-or-nothing for glyphs) — NOT used by the generic default, but by `FBF16Hud`'s combiner
  aperture.
- `FBHudEnv` carries viewport/AGL/have-telemetry, because that is render/telemetry wiring and not sim
  state: it travels separately instead of inflating `FBState` for one consumer.

#### 8.3 What the default HUD draws (MIL-STD-1787-like)

Geometry/positions ported verbatim from the retired `FBHudSymbology.h::w3_build_hud` (equivalence intent:
the same pixel layout for the retained elements).

| Element | Details |
|---|---|
| Colour | monochrome HUD green `(0.30, 1.00, 0.40)` |
| Waterline/boresight | fixed airframe reference, screen-fixed, two bars ±10…28 px + V tip 7 px deep |
| `NO TELEMETRY` | when `env.Have == false`: only waterline + red text, `return` |
| Conformal horizon | two segments beside the waterline (gap ±36 px, end ±86 px), tilted through the SAME camera projection as the scene: `w3_cam_from(yaw,pitch,roll, FOV 80°)`, centre = horizon point straight ahead (az=yaw), a second point at +20° gives the tilt |
| Horizon DIP | from `Platform.AltM` (ASL/curvature reference), **not** from AGL — with AGL the horizon breathed over terrain relief in a level loiter |
| Heading tape (top) | 5 px/deg, ticks every 5°, labels every 30° (`N/03/06/E/…`), rail ±200 px, window ±45°, box value `%03.0f` + up caret |
| Steerpoint marker | triangle + `SP` at the offset `Platform.HomeBearingDeg` (nose-relative), clamped to ±44° |
| GS tape (left, x=70) | 5 px per unit **m/s**, ticks every 5, labels every 10, rail ±150 px, box value + caret, label `GS` |
| ALT tape (right, x=W−70) | 1.5 px per **metre**, ticks every 10 m, labels every 20 m, rail ±150 px, box value + `<` caret, label `ASL` |
| AGL/VS | below the ALT box: `AGL%4.0f` (from `env.Agl`), `VS%+4.0f` (from `Platform.VsMs`) |

All values come from `state.Platform` — the default reads NO other block (no AirData, no Nav, no
RadarAlt). It is labelled in SI (m/s, m), not in kt/ft; the type-specific units and formats are the
override's business.

#### 8.4 The override

`FBF16Module` composes **not** this default but `modules/f16/displays/FBF16Hud` (the real F-16
symbology). The generic default remains the fallback for clients without a living module — e.g.
`FBAppNative.cpp`'s no-module screenshot mode (`static FBDisplaySystem hudDisplay;`).

---

### 9. `FBAirframeControls` — what the pilot's hands touch

`systems/FBAirframeControls.h/.cpp`. Interface + NoOp default in ONE class (the `FBSystemSlots.h`
pattern), plus the real ownship implementation alongside.

#### 9.1 The contract

Everything a pilot operates beyond stick/throttle (`FBFlightControl`) and guidance targets
(`FBAutopilot`):

| Command | Value range | JSBSim property (via `FBFdm`) |
|---|---|---|
| `SetGear(bool down)` | — | `gear/gear-cmd-norm` (0/1; the model travels kinematically) |
| `SetSpeedbrake(double)` | 0…1 | `fcs/speedbrake-cmd-norm` |
| `SetWheelBrakes(l, r)` | each 0…1 | `fcs/left-brake-cmd-norm`, `fcs/right-brake-cmd-norm` |
| `SetNosewheelSteer(double)` | −1…1 | `fcs/steer-cmd-norm` |
| `EngineStart()` / `EngineCutoff()` | — | `FGPropulsion::SetStarter`/`SetCutoff` (all engines) |

| Feedback | Meaning |
|---|---|
| `GetWeightOnWheels()` | model-wide WOW — a yes/no question, no breakdown per gear leg (that belongs to a future landing gear system) |
| `GetGearPosition()` | 0 = up … 1 = down, kinematically delayed |
| `GetSpeedbrake()` | 0…1, delayed readback |
| `GetGrossWeightLbs()` | live gross weight — input for `FBPilot`'s rotation speed table |
| `GetEngineRunning(int idx)` | running/not running |

NoOp default: all setters do nothing, `GetWeightOnWheels()` returns `false`, all numbers 0.

#### 9.2 `FBJsbsimAirframeControls` — the real ownship implementation

- **Every setter forwards directly to ONE borrowed `FBFdm`, every getter reads the SAME property back.**
  No shadow state: a kinematic gear travel or a WOW transition is visible the moment the FDM reports it.
- The `FBFdm&` is **constructor-injected and never null** — the assignment of this object to ONE airframe
  is fixed for its whole lifetime. Precisely for that reason every method is a pure forward without an
  "is there an FDM" branch.
- It is **airframe-agnostic**: the `FBFdm` methods used are generic FGFCS/FGGroundReactions/FGPropulsion
  bindings, so every JSBSim-flown module can reuse it, not only the F-16.
- It is wired in `FBModule::AttachFdm`: the module EXCHANGES the NoOp default for the real instance
  (`AirframeCtrl = std::make_unique<FBJsbsimAirframeControls>(fdm);`).

#### 9.3 Why the pilot reads the airframe ONLY through this

From `FBPilot.h`'s signature banner:

> The pilot never touches an FDM — it knows none and can reach none. Exactly that keeps this generic
> layer **airframe-agnostic AND instance-agnostic** (multi-unit).

- A pilot reaching past this interface into the FDM would be bound to ONE concrete FDM implementation and
  to ONE instance of it.
- The handle travels **per tick** with the rest of the perceived world (`const FBAirframeControls&`,
  `st`, `plan`, `runway`) instead of being bound at construction — a module composes its pilot long
  before an airframe exists.
- The demarcation, precisely: **airframe STATE** (gear, WOW, weight, engine) comes exclusively through
  this interface; **pose/speed** the pilot reads from the `const fb_fdm_state& st` of the same signature;
  **avionics** exclusively through the command bus. A `const FBWorld*` used to stand in the signature, was
  never read and has been removed: a path to ground truth waiting to be used.
- The way back is the same channel: `FBPilotCommands` carries `std::optional` fields, and the module
  calls the matching setter ONLY if a field is set — "not set" means "the pilot is not touching this
  control right now", which is true for most ticks for most controls.

#### 9.4 Telemetry

`FBAirframeControls` is itself an `FBTelemetrySource` (`"airframe"`) — through the SAME virtual getters
every caller uses. That is why it works unchanged for the NoOp default AND for
`FBJsbsimAirframeControls`; neither of them overrides telemetry. Columns: `gearPos`, `wow`, `speedbrake`.

---

### 9a. `FBMfdSystem` — the MFD bank as a box, not as a renderer feature

`systems/FBMfdSystem.h/.cpp`. Four verbs and **no virtual at all**, which is the point: a cockpit
differs in its CATALOGUE, not in how a bezel button works.

| Verb | Who calls it | What it does |
|---|---|---|
| `DeclarePages(pages, n)` | the module's constructor, once | ordinal `i` is `pages[i]` **for the aircraft's life** — that ordinal is what a command carries, so it may never be re-sorted. `pages[0]` is also the bank's power-up page |
| `Run(FBState&, nowS)` | the module, at the Displays cadence (20 Hz), BEFORE the display logic | re-cuts the catalogue against the published blocks and the loadout, re-places the bays, publishes `FBMfdBlock` |
| `Select(ordinal, nowS)` | the module's `ApplyCommand`, from `FBCommandTarget::MfdPageSelect` | puts the page on the attention bay; **false** = not selectable, which the module answers as `Rejected/OutOfContext` |
| `Attention()` | — | the ordinal on the middle bay |

**What makes a page exist** (`PageAvailable`), and both kinds of death are modelled because the
aircraft has both: a page dies when its BOX dies (`H.Readable()` goes false) and when its SUBJECT is
gone (the racks and the drum are empty). The loadout is read from `FBStoresBlock`/`FBGunBlock` and
never from the mission text — the block is what the jet reports, the mission is what somebody wrote
down.

| Page | Exists while |
|---|---|
| `Fcr` | `Radar.H.Readable()` |
| `Sms` | `Stores.LoadedCount > 0` **or** `Gun.RoundsRemaining > 0` |
| `Hsd` | `Nav`, `Datalink` or `NetLink` readable |
| `Rwr` | `Rwr.H.Readable()` |
| `Irst` | `Irst.H.Readable()` |
| `Sys` | `Airframe.H.Readable()` |

**Placement.** One command target names a PAGE, not a display, so where it lands is the cockpit's own
rule: the middle bay (`kMfdAttentionBay`) is the pilot's choice, the flanking two carry the remaining
selectable pages in catalogue order. Three bays therefore never show the same page twice, and a viewer
reads the AI's attention off the middle one.

`BuildHud` is `const`: it READS state, it owns none.


### 10. The still-empty slots

`systems/FBSystemSlots.h`. "NoOp default" here means: the class exists, is instantiable, is owned by the
module and called at its rate — its `Run()` is empty.

| Class | Signature | What belongs in it |
|---|---|---|
| `FBPropulsionSystem` | `Run(const fb_fdm_state &s, double dt)` | engine SYSTEM logic ABOVE the raw FDM (F110+DEEC state, BINGO/JOKER calls, EPU). **JSBSim's own propulsion model already drives the thrust** — engine management layers on top of it here. |
| `FBWeaponSystem` | `Run(FBMasterMode, const FBWorld*, double dt)` | historical stub for SMS/CCIP/CCRP/gun; **superseded** by `FBStoresSystem`/`FBGunSystem` (see Gaps) |

**Cost:** a NoOp slot that is not due costs one throttle comparison, one that is due costs an empty
virtual call. No heap allocation per frame, no dispatch in the inner 100 Hz maths.

### 10.1 `FBInputSystem` — the slot that stopped being NoOp

Two halves, and the split IS the class: the ANALOGUE half (stick, throttle, speedbrake, gear) leaves as
a plain `FBStickInput` the MODULE turns into the same `FBPilotCommands{Manual}` its pilot returns; the
DISCRETE half (master arm, station select, pickle, trigger) leaves ONLY as `FBCommandBus::Post`. There
is no third exit, which is what makes "a human gets no right the AI does not have" a property of the
code rather than a promise.

| Thing | Value | Where it comes from |
|---|---|---|
| `kAxisRampS` — centre to full deflection | `FBCommandBus::kHotasLatencyS` = 0.5 s | REUSED, not invented: it is the tree's one figure for how long a HOTAS action takes. A key is a switch and a stick is not; without a ramp every input is a jump to the stop. It is NOT the F-16's force-sensor law — that decision does not exist yet (`doc/modules/f16/hotas.md`) |
| `kTriggerRepeatS` — squeeze length, and the floor on the repeat | `kHotasLatencyS + kTriggerLatencyS` = 0.6 s | a held trigger is the same action repeated. This covers the window BEFORE the first completion, where the bus's own occupancy rule is still silent — without it one keypress filled the 8-slot queue (measured) |
| when the next squeeze is allowed | `FBCommandBus::SwitchReady()` | the window runs from the COMPLETION, and how late that falls is the answering box's cadence. Computing it earned one `ChannelBusy` per repeat (measured); asking costs one const call |
| the ONE virtual | `Route(FBHotasAction, FBMasterMode) -> FBCommandTarget` | input routing is module authority, not global. The default ignores the master mode deliberately: on this airframe the pickle is the pickle in every mode, and whether it ACHIEVES anything is the answering box's `OutOfContext` |

The seat is `TakeStick()` / `ReleaseStick()`, and taking it is a HANDOVER rather than a flag: the module
SEEDS the slot from the airframe it is taking over (throttle from the last FLCS command, speedbrake and
gear from `FBAirframeControls`), because a seat booting on defaults would put the gear down at altitude.
Until somebody takes it the slot does nothing at all, so a module that composes it and is flown by its
own pilot is bit-identical to one that never had it — measured over 12 missions, 0 differing artefacts.

**The borrowed `const FBWorld*`** is already in the signature although nobody reads it: sensors/weapons/
defensive get the world read-only per tick. A weapon system that spawns real ordnance would need a
MUTABLE world path — that comes with the first real implementation and was not built speculatively. (The
real solution has meanwhile turned out different: the system places an `FBStoreRelease` in a queue, the
OWNER spawns — see `weapons-and-damage.md`.)

---

### 11. Block bus contracts of these systems

| Block (`core/FBAvionicsBlocks.h`) | Writer | Readers documented here |
|---|---|---|
| `Platform` | the module (from the `st` it gets) resp. the client (`FBSimUnit::HudState`) | `FBDisplaySystem::BuildHud` |
| `AirData` | `FBAirDataSystem` | HUD, command bus g lock (`CmdBus_.SetLoadFactor`) |
| `RadarAlt` | `FBRadarAltimeter` | `FBWarningSystem`, HUD, pilot |
| `Nav` | `FBNavSystem` | HUD, `FBF16FireControl` |
| `Cruise` | `FBNavSystem` (second message) | HUD |
| `Warnings` | `FBWarningSystem` | HUD, pilot |
| `Airframe` | `FBAirframeControls` (via the module, which holds the FDM handle for the tank totals) | `FBNavSystem` (gear→cruise freeze), `FBWarningSystem` |
| `Ufc` | `modules/f16/FBF16Ufc` | `FBWarningSystem` |

`FBState::NowS` is the ONE bus time reference: the module stamps it once per `Run()` from its own sim
clock, before it ticks any slot, and every block header timestamp comes from there. One clock for the
whole bus is what makes the age of a block answerable without every system keeping its own "now".

The block statuses are published as telemetry columns of their own (`blk_*`, `FBStateBusTelemetry`) —
because a held value otherwise looks like a fresh one.

### FBFlightControl: the `Mig29()` preset and why it exists (stage 2a)

The class carries four airframe-dependent numbers that are **zero for an aircraft with its own FLCS**,
and the F-16 is that aircraft: `KqDamp` and `KpDampRoll` (rate feedback — the SAU-451 DAMPER),
`PitchStickMax` (the pitch-axis counterpart of `RollStickMax`) and `AlphaLimitDeg` (the α limiter). The
reason is structural rather than tuning: on the F-16 the branch's output is a **g request** into a fast
inner loop that regulates rate and limits α itself; on a mechanically signalled airframe the same output
**is** the surface deflection, with nothing behind it but a gearing unit and its rate limit. With all
four at their F-16 values the expression is arithmetically the one that was there before — verified
across all stock missions, byte for byte.

**All four are devices of the AIRCRAFT, so all four bind on the `Manual` path as well as the FLCS one,
and each of the three rounds that learned this learned it the hard way.** A hand stick does not know
which mode the autopilot is flying; neither does a stick force, an α limiter or a damper. `PitchStickMax`
and the α limiter were moved onto that path when the MiG-29 first flew BFM
([`pilot.md`](pilot.md) §5.10 screws 1–2); `KqDamp`/`KpDampRoll` followed only when the merge kept
killing it ([`pilot.md`](pilot.md) §5.10a) — BFM commands `Manual`, so until then the airframe fought
every close engagement with its damper switched off. Each of the four is **gated on its own gain or
limit being non-zero**, which is what makes the F-16 byte-identical as a structure rather than as an
IEEE argument.

Three measured failures determine the preset and live in its header comment: a saturating generic
yaw branch (1.2 s cycle → roll coupling → LOC at t=28 s; the stage-1 harness measured this
airframe's whole envelope with yaw=0), two cascaded integrators behind the two lags (a 20 s-period
limit cycle: pitch −6…+28°, VS −24…+15 m/s — removing the VS integrator killed it where no gain
change did), and a missing α limiter (full stabilator on waypoint roll-in → α spikes to 90°, LOC at
t=122 s). The limiter is the smaller of **two branches**, never a cap on the first: it may forbid
pulling, never command pushing, and its lead term is the derivative of the **limited quantity**, not
the pitch rate — a steady pull carries a steady q that a pitch-rate lead misreads as an impending
overshoot while α stands still.
