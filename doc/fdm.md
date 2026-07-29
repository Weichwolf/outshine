# FDM adapter — `sim/src/fdm/`

**Sources of this file:** the comment banners of the seven files in `sim/src/fdm/`
(`FBFdm.h`, `FBFdm.cpp`, `FBFdmBoot.h`, `FBFdmBoot.cpp`, `FBFdmTelemetrySource.h/.cpp`, `em_compat.h`),
`sim/src/clients/FBTestTwoFdm.cpp` (the coexistence proof), `sim/src/core/FBDamageModel.h` (the
consequence constants) and CLAUDE.md. Numbers without a source reference are in the code as written;
derivations are marked as such, settings with `[SET]`.

Subject: the **only** seam between FlightBox and the pinned JSBSim engine
(`sim/vendor/jsbsim`, read-only, CLAUDE.md principle 1). Everything above this seam sees a flat POD and
one class; nobody sees `FGFDMExec`.

---

## Spec

The **only** seam between FlightBox and the pinned JSBSim engine. Everything above it sees a flat POD
plus one class; nobody sees `FGFDMExec`.

| Contract | Acceptance / measurement anchor |
|---|---|
| One translation unit includes JSBSim headers | `FBFdm.cpp`, and nothing else |
| An `FBFdm` is one aircraft — instance-capable, no static mutable globals | `make -C sim test-fdm` → two coexisting FDMs with independent physics |
| Initial conditions are structurally sealed off | loading constructor private, single friend `FBFdmBoot`, no re-init/reset; only `missions/` and `clients/` files name `FBFdmBoot.h` |
| Borrowed handles cannot cheat | every command method non-const, every readback const |
| The pinned model is the truth; a copy may deviate only as a declared delta | `make -C sim verify-models` green (`../build-and-ops.md`) |
| Carriage and damage act through model-owned JSBSim APIs, never by patching model XML | point masses + a named `fb-stores` external force; `fb-damage` force, throttle cap/cutoff, control authority |
| Neutral until something happens | a clean, undamaged jet computes bit-identically to one that never heard of stores or damage (measured) |

## State

Built and closed. Seven files.

| Piece | Status | Anchor |
|---|---|---|
| Instance-capable adapter, pimpl, no globals | built | `c1bc9de` |
| IC lockdown (`FBFdmBoot`) | built | `c08a168` |
| Stores carriage: point masses + external force | built | `b62c769` |
| Damage channels: control authority, throttle cap, drag | built | `6d84647` |
| Single model root + delta rule | built | model-root round (see `../journal.md`) |
| `FBFdmTelemetrySource` (raw FDM pose) | built | `e4d7c26` |

Regression evidence for the last change: 121/121 telemetry files byte-identical over 50 missions, all
seven harnesses rc=0, corner speed unchanged at 380 KCAS / 16.2214 °/s.

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `fdm/FBFdm` | count of process-wide JSBSim state: `CLAUDE.md` says three, `FBFdm.cpp` lists four. The code is authoritative. |

### Inventory (from the previous `Offene Punkte` section)

- **Contradiction in the count of process-wide JSBSim things.** CLAUDE.md says "the three things
  documented in `FBFdm.cpp`", `FBFdm.cpp` lists **four** points (debug_lvl, logger, `Element::convert`,
  `JSBSIM_*` env). The list in `FBFdm.cpp` is the authoritative one. Strictly speaking the fourth point
  is not a state of its own but a writer onto the first — presumably the source of the counting
  difference. (The diverging two-claim in the `FBFdm.h` banner was dropped in the comment round, as was
  its outdated ownership banner.)
- **`GetGroundClearanceM` with `gearDown=false`** skips retractable contacts, but not those that are
  `ctSTRUCTURE` and were nevertheless declared retractable — the consequence for models with an unusual
  contact declaration has not been checked.
- **`GetGearPos`/`GetSpeedbrakePos` do not clamp**: they pass the model property through raw. A model
  that travels outside [0,1] would push that upwards. No case so far.
- **There is no readback for `Authority`/`ThrottleMax`/`DamageCdA`.** The damage effect is therefore only
  observable through its consequences (telemetry/behaviour), not directly. `core/FBSystemHealth`'s
  `dmg_*` columns cover the STATE, not the applied factor.
- **There is no readback for `ElevTrim`.** The trim bias is only visible in the `loaded` log entry, and
  cannot be queried afterwards.
- **`SetStoresDrag` with `cdaFt2 <= 0` AFTER it has been created once** sets the magnitude to 0, but does
  not remove the force (JSBSim's `FGExternalReactions` knows no removal). Documented as effect-equivalent,
  but the force stays in the model structure.
- **Not checked in this round:** the concrete callers of the tank setters (`FBModule::ApplySetup` keys
  `fuel_lbs`/`fuel_pct`) were not read; the key names come from the `FBFdm.h` banner and from CLAUDE.md,
  not from `doc/missions/syntax.md`.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. Files

| File | Role |
|---|---|
| `fdm/FBFdm.h` | The public seam: `struct fb_fdm_state` (POD) + `class FBFdm`. Names NO JSBSim type. |
| `fdm/FBFdm.cpp` | The ONE translation unit with JSBSim headers. Contains `struct FBFdm::Impl` (pimpl) with the `FGFDMExec`. |
| `fdm/FBFdmBoot.h` | `struct FBFdmSpawn` (the IC as data) + `class FBFdmBoot` — the only friend of `FBFdm`'s private loading constructor. |
| `fdm/FBFdmBoot.cpp` | `FBFdmBoot::Spawn` — the only place where an `FBFdm` comes into being. |
| `fdm/FBFdmTelemetrySource.h/.cpp` | `FBTelemetrySource` for the raw FDM pose (10 columns). |
| `fdm/em_compat.h` | Build shim for JSBSim under emscripten/musl (`strerror_r`). Force-include ONLY for JSBSim sources. |

---

### 2. The single-TU seam

**Contract.** `FBFdm.cpp` is the only translation unit in the whole tree that includes a JSBSim header.
Every caller (every module, every system, every telemetry source, every client) sees exclusively:

- `fb_fdm_state` — flat POD, snake_case name, because it is the `FBModule::Run` contract against which
  every module and every telemetry source is written;
- the methods of `FBFdm`.

**Mechanics.** The engine sits behind a pimpl (`std::unique_ptr<Impl> P`, `Impl` carries the `FGFDMExec`
by value). From that follows a deliberate deviation from the FB convention "getters inline in the
header": the getters **cannot** be inline — the header must not name the type they read from.

**What that guarantees.**

| Guarantee | Why |
|---|---|
| A build break in JSBSim hits exactly one `.o` file | Only this TU knows the headers. |
| No system/module can write into the property tree past the seam | `SetPropertyValue` is reachable nowhere else. |
| The list of all touched JSBSim properties is finite and stands in the header | `FBFdm.h` names, for every method, the property it writes/reads. |
| Exceptions stay local | The firewall (§7) sits in this TU; above it there is only `bool` and `Faulted()`. |

#### `fb_fdm_state` — fields and units

| Field | Unit | JSBSim property |
|---|---|---|
| `roll`, `pitch`, `yaw` | deg (φ/θ/ψ) | `attitude/phi-deg`, `theta-deg`, `psi-deg` |
| `p`, `q`, `r` | deg/s, body rates | `velocities/[pqr]-rad_sec` × rad→deg |
| `lat`, `lon` | deg, **geodetic** | `position/lat-geod-deg`, `position/long-gc-deg` |
| `elev` | m ASL | `position/h-sl-ft` × ft→m |
| `speed` / `gs` / `cas` | m/s (TAS / ground / **density-corrected** CAS) | `velocities/vt-fps`, `vg-fps`, `vc-fps` |
| `mach` | — | `velocities/mach` |
| `vx`, `vy`, `vz` | m/s, **X-Plane-local**: +x east, +y up, +z south | `v-east-fps`, `−v-down-fps`, `−v-north-fps` |
| `nx`, `ny`, `nz` | g, body load factors (long/lat/normal) | `accelerations/N[xyz]` |
| `alphaDeg` | deg | `aero/alpha-deg` |

The `vx/vy/vz` convention is an **inheritance from the pre-pivot bridge** and stays deliberately, because
renderer and HUD maths already consume it. Every consumer outside (e.g.
`missions/FBMissionBoot.h::FBMissionSpawnStore`, `FBMissionRunner.cpp::GroundCrossing`) converts it explicitly
into NED/ENU and says so in the comment.

`cas` is density-corrected — the honest "how close to the stall" quantity at any field elevation.

---

### 3. Instance capability

**Contract.** `FBFdm` is ONE simulated aircraft. Any number of objects coexist in the same process with
independent physics; every `FGFDMExec(nullptr)` allocates its **own** `SGPropertyNode` root and its own
FDM counter. No static mutable globals in the adapter (grep-verifiable).

**Proof:** `make -C sim test-fdm` → `build/fb-test-two-fdm` (`clients/FBTestTwoFdm.cpp`). The harness
claims three things and checks them:

1. two airframes load and trim independently (two `FGFDMExec`, each with its own property tree);
2. their states **diverge according to their own commands** (A rolls right, B left) — so neither reads
   nor writes the other's physics;
3. a THIRD airframe with the same IC and the same commands reproduces A **bit for bit** — so an instance
   carries no hidden cross-talk from its neighbours.

Exit 0 = proven, 1 = not independent, 2 = setup error. No threading, no unit list: this stage proves only
instance capability.

#### What stays process-wide in JSBSim ITSELF

Verified against `vendor/jsbsim` at the pinned commit; documented in the banner of `FBFdm.cpp`. **Four
points**, none of which carries physics state:

| Process-wide | What it is | Why uncritical |
|---|---|---|
| `FGJSBBase::debug_lvl` | static debug level, `SetDebugLevel()` acts on ALL instances | Every `FBFdm` sets it to 0 in the constructor, so no instance can surprise another. It is written only from one ctor and from `FGFDMExec`'s child-FDM trim path (which FlightBox does not use), and is read-only for the whole of `Run()`. |
| `JSBSim::SetLogger`/`GetLogger` (`input_output/FGLog.cpp`) | ONE logger — at the pinned commit `thread_local` | Per-INSTANCE log routing is therefore impossible, but two threads never share a logger. FlightBox never sets it; JSBSim's own output stays off at debug 0. |
| `Element::convert` (`input_output/FGXMLElement.cpp`) | static unit conversion table, filled lazily from the `Element` ctor and read with `operator[]` — which **can insert** | Is touched exclusively when LOADING an aircraft. Precisely for that reason `fb-gym`'s parallel path spawns its units sequentially and parallelises only the STEP (`missions/FBTickPool.h`). |
| `JSBSIM_DEBUG` / `JSBSIM_DISPERSE` (env variables) | read in the `FGFDMExec` ctor into the same shared static | Applies to the whole process, not to one airframe. |

**Consequence for concurrency:** none of the four is reachable from `Step()`, so N airframes MAY integrate
CONCURRENTLY, one thread per airframe. The engine's internal reference counting (`SGReferenced`) is NOT
atomic — which is harmless for exactly that reason: every property node and every `Element` hangs off the
one `FGFDMExec` root of its instance and is never shared.

---

### 4. Ownership

| Role | Handle | Rule |
|---|---|---|
| Owner | `std::unique_ptr<FBFdm>` | Whoever owns the unit owns its airframe — today `units/FBSimUnit` (declared BEFORE the module, so that the airframe outlives the module that only borrows it). |
| Commanding borrower | `FBFdm&` | Every command method is **non-const**. |
| Reading borrower | `const FBFdm&` / `const FBFdm*` | Every readback is **const**. |

The constness is load-bearing, not cosmetic: a read handle **cannot** write the physics — that is
CLAUDE.md's "no cheating", enforced by the type system instead of by convention.

`FBFdm` is not copyable (`= delete` on copy ctor and assignment).

**`FBModule::AttachFdm(FBFdm&)` is the constructor-injection substitute.** Modules are built by
`FBModuleRegistry` **without arguments** (name → factory), so there is no constructor into which an
airframe could be handed. `AttachFdm` is called by the owner EXACTLY ONCE — right after the spawn, before
the first `Run()` — and is permanent from then on. It is at the same time the point at which the module
can pass a real `FBFdm&` on to those systems whose assignment to an airframe is FIXED
(`FBJsbsimAirframeControls`, constructor-injected).

The module never owns an airframe and cannot create one — it cannot: the IC sits behind
`fdm/FBFdmBoot.h`, which no module includes (§5).

---

### 5. IC lockdown

**Structure, not convention:**

| Element | Effect |
|---|---|
| `FBFdm::FBFdm()` + `FBFdm::Load()` are **private** | Nobody can construct or load an airframe. |
| `friend class FBFdmBoot` — the ONLY friend | Exactly one producer. |
| `FBFdmBoot` stands in a **separate header** | Whoever includes `FBFdm.h` (every module/system, in order to hold an `FBFdm&`) reaches NO IC. Whoever wants the IC must name `FBFdmBoot.h` BY NAME. |
| There is **no** `Init`/`Reset`/`Respawn` on `FBFdm` | A borrowed reference cannot re-place, re-trim or re-spawn the airframe. |
| `Spawn()` returns `nullptr` if the model did not load | An `FBFdm` that exists is ALWAYS a loaded one — no caller and no method needs a "not initialised" branch. |

**Who may name `FBFdmBoot.h`:** only `missions/` and `clients/` — `missions/FBMissionBoot.h`, `clients/FBAppWasm.cpp` and the test
harnesses. `grep -rn FBFdmBoot src/systems src/modules` is empty and **cannot silently stop being
empty**: the compiler enforces it, because the constructor is private.

Second stage of the same barrier: a `units/FBSimUnit` can only be built from an already spawned
`FBFdm` — so `FBMissionBoot.h` is also the only producer of a complete actor.

#### `FBFdmSpawn` — the IC as data

| Field | Meaning |
|---|---|
| `ModelsRoot` | the ONE model root. native/gym `assets/aircraft`, WASM the embedded FS path `/fb/aircraft` (→ `missions/FBModelRoots.h`). |
| `Aircraft` | model directory + XML name under `ModelsRoot`. |
| `LatDeg`, `LonDeg` | **geodetic** (matches GPS/HOME_LAT). |
| `GroundElevM` | resolved ground elevation under the spawn point, m ASL. |
| `HeightOffsetM` | `< 0` = **sit on the landing gear** (model-owned gear-down clearance); `>= 0` = that many metres above `GroundElevM`. `0` falls back to a provisional **3 m** (air start without an explicit offset). |
| `SpeedMs` | calibrated airspeed. `0` = standing ground start → **no** trim search. |
| `HeadingDeg` | heading; negative is computed +360. |
| `FbwOverride` | sets `fcs/fbw-override` = 1, bypasses the model-owned FLCS. |
| `Ballistic` + `PitchDeg`/`RollDeg` + `VelNorthMs`/`VelEastMs`/`VelDownMs` | The **release IC** (§6). With `Ballistic == false` completely ignored. |

**One IC application per airframe.** `Spawn()` lays down position/attitude/velocity **together** — for the
ground seat just as for the explicit airborne altitude. There is no second, separate airborne code path
and no re-init afterwards.

---

### 6. The load sequence (`LoadUnguarded`)

Step by step, because every step carries a justification:

1. **Path resolution engine/Systems.** `<root>/<aircraft>/engine` and `<root>/<aircraft>/Systems`,
   unconditionally. Every FlightBox model is complete under its own directory — exactly the layout that
   JSBSim's own loaders search FIRST (`FGPropulsion::FindEngineFullPathname` and
   `FGFCS::FindFullPathName` try `<aircraft>/engine` resp. `<aircraft>/Systems` before any passed-in
   path). The earlier probing (does `<ac>/engine` exist? otherwise `<parent>/engine`) including parent
   truncation went away with the single model root; a model without an engine (`mk82`) simply never
   resolves the path.
1b. **`SetGroundElevM(spawn.TerrainElevM)` — BEFORE the IC** (added 2026-07-29). `FGLGear` resolves its
   contacts **inside** `RunIC()`, so a ground told to JSBSim afterwards is one call too late: a store
   spawned nose-up on a rail has a structure point metres under a ground it is not supposed to have, the
   contact spring answers with an angular impulse, and step 1 carries it out. `TerrainElevM` is a
   **separate** field from `GroundElevM` — the latter only *places* the spawn — and defaults to `0.0`,
   i.e. JSBSim's own datum, which is what every spawn in this tree has always run its IC against. Only a
   caller whose object must not meet the ground at the IC sets it, and today there is exactly one:
   `FBMissionSpawnStore` passes `FBFdm::kNoGroundElevM` ([`weapons.md`](weapons.md) §1).
2. **Set IC:** geodetic lat/lon, ASL altitude = `GroundElevM + max(HeightOffsetM, 3 m)`, psi.
3. **Ballistic** (release): theta/phi directly from the carrier attitude, plus the full NED velocity
   VECTOR. **Otherwise**: calibrated airspeed + flight path angle 0 (level).
4. `RunIC()`.
5. **Ground-seat post-correction** (`HeightOffsetM < 0`): now the CG is valid, so it is re-placed onto the
   model gear-down clearance (`GetGroundClearanceM(true)`, threshold > 0.1 m) and `RunIC()` is repeated —
   the spawn altitude is thereby the geometrically true wheel height, without a jump at the first step.
6. **Start the engines** (`FGPropulsion::InitRunning` for every index) — **except with `Ballistic`**.
   Justification in the code: without a running engine there is no thrust during `FGTrim`, so a powered
   airframe reports "udot not trimmable", the IC is not an equilibrium, and the untrimmed airframe
   departs violently at the first step. For a released store it is the opposite and **not cosmetic**:
   `InitRunning` slams the throttle to 1 and marches the engine into a steady state — with a SOLID-FUEL
   MOTOR (`FGRocket`: ignition = throttle == 1, and once ignited it burns to exhaustion) that would mean
   the motor is already burning in the IC and no command could hold it. An unpowered store has no engines,
   so for every store that flew before the first missile this is bit-identical.
   **And since 2026-07-29 there is one caller that asks for exactly that**: `FBFdmSpawn::MotorRunning`
   makes the condition `!Ballistic || MotorRunning` and sets `ThrottleApplied = 1.0` at the IC. A
   **rail** launch separates *because* the motor pushed the round off the rail, so it cannot be born with
   a cold engine and a spool ramp in front of it; an air launch drops clear and then lights, which is
   what the throttle slew below models. `HaveRail` is what tells the two apart, and it is false for every
   air-launched store — so this is bit-identical for everything that ever left a pylon. Measured cost of
   *not* having it: at t = 0.51 s the round was still in free fall at **4.98 m/s = 9.81·0.51**, i.e.
   ½·9.81·0.55² = **1.48 m of sink** from a 0.5 m launcher height before any thrust existed.
7. `fcs/fbw-override` = 1, if requested. `Setdt(kStepS)`.
8. **Trim** — only if `SpeedMs > 0` AND not `Ballistic`. Mode `tLongitudinal` (pitch/throttle/alpha, wings
   level): more robust than `tFull` on light/slow airframes. In `try/catch`, a throw = not trimmed.
   - **Why V=0 is not trimmed:** zero airspeed = zero aero force/moment, so no control deflection can null
     `udot`/`qdot`. Previously `FGTrim` ran anyway, reported "not trimmable" and left `ElevTrim` on the
     last iterate of the failed search (noise). Now it is set neutral — which corresponds to the untrimmed
     stick of a real jet before the roll.
9. **Hold on to `ElevTrim`:** the elevator deflection the trimmer settled on is stored as the **trim
   control bias** (`fcs/elevator-cmd-norm` after the trim; 0 at V=0). With that, a neutral stick holds
   LEVEL instead of the nose-up attitude of the airframe at neutral.
10. Final `RunIC()` — clean, level IC (attitude + speed); it is held by the trim control, not by the
    perturbed search state.
11. `FBLog::Info("fdm","loaded", …)` in three variants (ballistic / trimmed / ground start).

---

### 7. Step, exception firewall, `Faulted()`

`static constexpr double FBFdm::kStepS = 0.01` — **100 Hz**, the ONE definition of this rate. The
module's substep accumulator and the test harnesses read it here, instead of repeating `0.01`.

`Step(fb_fdm_state &out)`: advance one fixed step, then read the state into `out`.

**Firewall.** JSBSim throws `JSBSim::BaseException` (a `std::runtime_error`) out of the XML parsing and
`FGJSBBase::FloatingPointException` out of table evaluations. Uncaught, a broken `aircraft.xml` kills the
process with `std::terminate`, and the mission loop never gets a `RESULT` line to branch on. Therefore:

| Level | Guard |
|---|---|
| `Load` | wraps `LoadUnguarded` (XML parsing, IC, trim, engine start) |
| `Step` | wraps `StepUnguarded` |
| `FBFdmBoot::Spawn` | wraps the ONE thing that lies outside: the construction of the engine object itself (`FGFDMExec` ctor allocates the property root, reads `JSBSIM_*`) |

Catching goes via `std::exception` (JSBSim's hierarchy derives from it) plus a catch-all — so no JSBSim
type has to be named here. **Identical in WASM:** libJSBSim from the submodule, this TU and the final link
all carry `-fexceptions` (`vendor/build_jsbsim_wasm.sh`, the `wasm` make target) — the ONE place where
exceptions are switched on, and the firewall sits inside it.

**`Faulted()` is latched:** once the integrator has blown up, the physics of this airframe is over. Every
later `Step` is a no-op, `out` keeps its last good values (never half-written), the caller reads a frozen
but FINITE state. The app-side judge (`core/FBFlightMonitor` via `FBFlightMonitorSample::FdmFault`) turns
that into a `NumericalDivergence` knockout. The module sees nothing of this.

---

### 8. Command channels

The only way in which anything above this class influences the physics (the simulated control surface —
CLAUDE.md "no cheating").

| Method | JSBSim property | Range / note |
|---|---|---|
| `SetControls(roll,pitch,yaw,thr)` | `fcs/aileron-cmd-norm`, `fcs/elevator-cmd-norm`, `fcs/rudder-cmd-norm`, `fcs/throttle-cmd-norm` | roll/pitch/yaw ∈ [−1,1], thr ∈ [0,1] |
| `SetGear(cmd)` | `gear/gear-cmd-norm` | [0,1], 1 = down; the kinematic transit is the model-owned `flight_control` channel |
| `SetFlap` / `SetSpeedbrake` | `fcs/flap-cmd-norm`, `fcs/speedbrake-cmd-norm` | [0,1] |
| `SetWheelBrakes(l,r)` | `fcs/left-brake-cmd-norm`, `fcs/right-brake-cmd-norm` | each [0,1], clamped |
| `SetNosewheelSteer(cmd)` | `fcs/steer-cmd-norm` | [−1,1], wired generically in `FGGroundReactions`; how much deflection results is said by the gain block of the respective `aircraft.xml` — this class only claims the pilot command |
| `EngineStart()` | `propulsion/cutoff_cmd`=0, `starter_cmd`=1 | propulsion-wide (`FGPropulsion::SetStarter/SetCutoff` apply by default to ALL engines) |
| `EngineCutoff()` | `starter_cmd`=0, `cutoff_cmd`=1 | ditto |

#### Three peculiarities in `SetControls`

1. **Damage effect sits HERE**, between the commanding system and the physics: `roll/pitch/yaw` are
   scaled with `Authority`, `thr` is capped at `ThrottleMax`. Both are 1.0 on an undamaged airframe —
   so arithmetic without effect, until something really has been shot up.
2. **Throttle is ramped, not stepped.** `kEscSpinupS = 0.5 s` ⇒
   `kThrottleSlew = kStepS / kEscSpinupS = 0.01/0.5 = 0.02` per step (full travel 0→1 in 0.5 s).
   Justification in the code: a 0→0.95 jump blows up the engine's RPM ODE and departs the airframe.
3. **Sign and trim.** JSBSim's `+elevator` = nose DOWN, FlightBox's `+pitch` = nose UP, so `-pitch +
   ElevTrim` goes out. `+yaw` coordinates the turn, `−yaw` skids it (measured: strong adverse yaw moment).

---

### 9. External stores (carriage)

Two mechanisms, **both model-owned**, both populated at RUNTIME instead of by a model-XML patch —
`vendor/jsbsim` stays read-only (principle 1), and no model gets a station it did not have.

| Method | Mechanism | Effect |
|---|---|---|
| `AddStorePointMass(name, xIn, yIn, zIn) → index` | `FGMassBalance::AddPointMass` (the `<pointmass>` mechanism, of which the F-16 declares exactly one: its pilot) | mass, centre of gravity AND the r² inertia contribution come from the engine |
| `SetStorePointMassLbs(index, lbs)` | `inertia/pointmass-weight-lbs[i]` | the mass also changes on release (to 0) — that is why a loaded jet flies differently from a clean one and an unloaded one differently from both |
| `SetStoresDrag(cdaFt2, xIn, yIn, zIn)` | own `<external_reactions>` force `fb-stores` | force `CdA · qbar` along the body **−x** axis, acting at the centroid of the occupied stations — the MOMENT of an off-centre load therefore comes from the same physics as the force |

**Coordinates:** structural inches (`IN`) in the frame in which the `aircraft.xml` already places pilot,
landing gear and tanks.

**Index discovery instead of counting along.** After `AddPointMass` our mass is the last one; the index is
read from the property tree (the first index whose `inertia/pointmass-weight-lbs[n]` does NOT exist is the
count; `n−1` is us). With that the adapter is generic over every model, no matter how many point masses
its XML brought along.

**`EnsureDragForce(name, x, y, z)` — the shared helper routine** (carriage and battle damage both need it,
which is why it is a function and not a block). It creates a named body **−x** force on the loaded model.
Subtlety: `FGExternalReactions::Load` APPENDS forces and afterwards re-binds its six aggregate output
properties — which the loaded model has already bound if it declared an external force of its own, and a
double tie logs one error per property. Therefore the aggregates (`moments/[lmn]-external-lbsft`,
`forces/fb[xyz]-external-lbs`) are **untied** beforehand and re-bound by `Load` to the same object: no
output, no model file touched, and the aircraft keeps every force it declared itself.

**Cost per step:** exactly one `SetPropertyValue` per active channel
(`magnitude = CdA · aero/qbar-psf`), and only on an airframe that really carries something.
`SetStoresDrag` is called per loadout CHANGE, not per frame. With `cdaFt2 <= 0` (default) the force is
**never created** — a clean airframe is bit-identical to one that never heard of stores.

---

### 10. Damage channels

Three consequences that a resolved detonation can have on an airframe (`core/FBDamageModel` names the
values and their justification), each through a mechanism JSBSim already has. They are set **only** by
the owner of the unit (`units/FBSimUnit::ApplyDamageToAirframe`), never by a module: a module reaches a
non-const `FBFdm` only if the owner hands it over — and the owner hands it over for control, not for
damage.

| Method | Physical effect | Values (from `core/FBDamageModel.h`) |
|---|---|---|
| `SetControlAuthority(norm)` | Scales EVERY commanded deflection (roll/pitch/yaw) **inside** `SetControls`. The FCS keeps commanding unchanged, the aircraft simply no longer answers — which is what a severed actuator run means. | degraded **0.5** [SET, but with a structural reason: the F-16 has two independent hydraulic systems, so the loss of one of them is the natural meaning of "degraded"]; failed **0.0** |
| `SetThrottleLimit(maxNorm)` | Caps the commanded throttle — the afterburner range is simply no longer reachable. The thrust itself remains JSBSim's engine model. | degraded **0.6** [DERIVED: that is where the AB gate lies in the `throttle-cmd-norm` convention of the F-16 model]; failed = cutoff (JSBSim's own engine-out, no thrust term invented here) |
| `SetDamageDrag(cdaFt2)` | Additional drag AREA along body −x **through the CG** — i.e. drag without a claimed pitching moment (where the holes are, this model does not know). Own force channel `fb-damage`, so that carriage and damage never overwrite each other. | degraded **1.5 ft²**, failed **6.0 ft²** [SET; for orientation: the zero-lift drag area of a clean F-16 is on the order of 4 ft², so "degraded" is noticeably dirty and "failed" is a hole in the airframe] |

**All three are neutral until something has been hit** — authority 1, no throttle limit, no drag; the
`fb-damage` channel is never created with `cdaFt2 <= 0`. An undamaged airframe computes **bit-identically**
to one that never heard of damage (measured, CLAUDE.md).

What FOLLOWS from that — a jet that no longer rolls; an engine without afterburner; an airframe that does
not hold altitude — is JSBSim integrating the aircraft it now is. No second, parallel flight model.

---

### 11. Tank wiring

Generic over `FGPropulsion`'s own tank inventory — enumerated by index, never assuming how many tanks an
`aircraft.xml` declares.

| Method | Behaviour |
|---|---|
| `SetFuelTankLbs(idx, lbs)` | one tank; index out of range = no-op; negative → 0 |
| `SetFuelTotalLbs(lbs)` | distributes **proportionally to the capacity share of EVERY tank** (that is how a real refuelling number fills the jet), not tank by tank |
| `SetFuelPct(pct)` | 0..100 of the declared total capacity, via `SetFuelTotalLbs` |
| `GetFuelTankCount/TankLbs/TotalLbs/CapacityLbs` | readbacks; `GetFuelTotalLbs` is the `fuelLbs` telemetry column |

**This class does NOT simulate fuel starvation.** Running dry is JSBSim's own `FGEngine` model starving
the engine NATIVELY; the adapter only makes the level observable and settable. On the mission side the
values come from `set fuel_lbs` / `set fuel_pct` via `FBModule::ApplySetup`.

---

### 12. Readbacks

All const — which is why `const FBFdm&` is a genuine read-only handle.

| Method | Source | What for |
|---|---|---|
| `GetQbarPsf()` | `aero/qbar-psf` | the quantity against which both drag channels are measured |
| `GetCgXIn()` | `inertia/cg-x-in` | carriage effect on the BALANCE, observable; at the same time point of application of the `fb-damage` channel |
| `SetGroundElevM` / `GetGroundElevM` | `position/terrain-elevation-asl-ft` | world truth from the elevation hook (`FBElevationProvider`) instead of the flat default. The getter exists so that a caller can PROVE that the DEM value really arrived in JSBSim — not merely that the setter was called. |
| `GetGroundClearanceM(bool gearDown)` | `FGGroundReactions` → per gear `GetBodyLocation(3)` (body z, ft below the CG), maximum | CG height above ground when the lowest active contact touches down. `gearDown=false` skips all retractable contacts → only fixed structure (belly). **Per model and gear state**, so take-off/touchdown/crash detection and the camera eye height are geometrically true instead of a fixed number. |
| `GetGearPos()` / `GetSpeedbrakePos()` | `gear/gear-pos-norm`, `fcs/speedbrake-pos-norm` | kinematically delayed ACTUAL position |
| `GetWow()` | `gear/wow` (`FGGroundReactions::GetWOW`) | model-wide: true as soon as ANY bogey compresses. A breakdown per leg belongs to a future landing gear system, not to this seam. |
| `GetStructureContact()` | enumerates contact TYPE `ctSTRUCTURE`, never an index or aircraft name | true when a non-wheeled airframe geometry point compresses. A model without such points always reads false. Consumer: `FBFlightMonitor`'s structural knockout. |
| `GetMaxGearForceLbs()` | max. `FGLGear::GetCompForce` over all BOGEY legs | the model-owned spring/damper reaction, NO derived sink-rate heuristic. `FBFlightMonitor` compares it against `GetWeightLbs()` and judges "hard landing" purely from physics. |
| `GetWeightLbs()` | `inertia/weight-lbs` | among other things because `FBPilot`'s rotation speed table is indexed by weight |
| `GetEngineRunning(i)` | `propulsion/engine[i]/set-running` | per engine index |
| `Faulted()` | latched flag | see §7 |

---

### 13. `FBFdmTelemetrySource`

Sits at the **adapter seam**, not in the module: `fb_fdm_state` is the POD of the FDM, and the only
additional inputs are the airframe itself and a borrowed ground ASL — no module state is involved. All
three references are borrowed and constructor-injected; `const FBFdm*` is a read-only handle: telemetry
observes, it never commands.

**The airframe is OPTIONAL, and the pointer says so** — a unit without an airframe (static ground target)
still has pose, altitude and ground sample, i.e. the greater part of this schema.

| Column | Unit | Source |
|---|---|---|
| `lat`, `lon` | deg | `st.lat/lon` |
| `altM` | m | `st.elev` |
| `aglM` | m | `st.elev − groundAslM` |
| `vsMs` | m/s | `st.vy` (X-Plane-local +y = up) |
| `pitchDeg`, `rollDeg`, `hdgDeg` | deg | `st.pitch/roll/yaw` |
| `fuelLbs` | lb | `GetFuelTotalLbs()`, **0 without an airframe** |
| `gearLoadFactor` | — | `GetMaxGearForceLbs() / GetWeightLbs()`, **0 without an airframe**. It is exactly `FBFlightMonitor`'s own hard-landing ratio (`kHardLandingForceFactor = 3.0` triggers), but logged **every tick** instead of only on triggering — so that the touchdown harshness of a landing is measurable even when it stays clearly below the knockout threshold. |

Only the two genuinely airframe-owned columns go to 0; the column set stays identical, so every trace of a
run has the same header, whatever kind of unit produced it.

---

### 14. Namespace, `extern "C"`, build shim

**`namespace FlightBox` like the rest of the tree — no `extern "C"`.** The C linkage was there for the
long-since deleted `xp_bridge.c` of the pre-pivot architecture. Today nobody calls the adapter from C or
from JS: the only WASM exports are `fb_toggle_ground`/`fb_set_ground` in `clients/FBAppWasm.cpp`. `extern "C"`
+ `EMSCRIPTEN_KEEPALIVE` remains convention **exclusively** for symbols called BY NAME from JS
(EMSCRIPTEN_KEEPALIVE alone is not enough — C++ mangling breaks exports silently).

**`em_compat.h`** is glue at the seam, not a patch: force-included (`emcc -include`) ONLY for the JSBSim
sources, so that the submodule stays bit-vanilla. Content: emscripten defines `_GNU_SOURCE` (libc++ needs
it for `strtof_l`/`strtod_l`), but musl supplies the POSIX `int strerror_r(...)`, while
`simgear/misc/strutils.cxx` takes the `_GNU_SOURCE` branch and writes `std::string(strerror_r(...))`, thus
expecting the GNU `char*` return value. The wrapper returns the buffer; it is defined BEFORE the macro, so
that its own call reaches the real libc function (no recursion).

### The wind channel (R4, commit `43b82b5`)

`SetWindNedMs(n,e,d)` writes JSBSim's `FGWinds::SetWindNED` (ft/s, north/east/down). `vWindNED` is
the velocity of the AIR MASS, not the meteorological "from" bearing: `FGAuxiliary` computes
`vAeroUVW = vUVW − Tl2b·vTotalWindNED`, so a wind from 270° is `(N=0, E=+v, D=0)`. Gusts and
turbulence are separate vectors in the same class and stay untouched. The channel is set only by the
unit's OWNER (`FBSimUnit::UpdateWind`, decision rate) and only on CHANGE — still air is `FGWinds`'
boot state, so a calm run never touches the channel and is bit-identical to a run before the channel
existed (measured: all 50 pre-existing missions byte-identical).
