# Architecture

## Spec

The floor plan: who owns what, what links against what, and where a file belongs.

| Contract | Acceptance / measurement anchor |
|---|---|
| The simulator is a **library**; a client only adds an entry point and an output medium | `core-lib` → `build/libfbcore.a`; `render/`, `world/` and `missions/FBTickPool` are outside it |
| Exactly ONE core-lib exception is allowed, and it is named | `systems/FBHudGeometry.cpp` (CPU vertex maths, no WebGPU include) — a second one is a defect, not a precedent (see [`sim/systems.md`](systems.md)) |
| WASM is a cross-compile of the same source list, never a second architecture | `make -C sim wasm` |
| Server-side there are exactly two lean containers | `fb-tiles` (:8081, tile API) and `fb-sim` (:8080, web host). No SITL, no world process, no hub. |
| An aircraft = code module + JSBSim model, resolved by name through a registry | `FBModuleRegistry`; the runner never includes a concrete module header |
| One model root; the pinned submodule is the base, not a load path | `mods/f16/src/aircraft`, delta rule + `make -C sim verify-models` |
| Layering is `FBCore → interface → default → module override` | not "belongs to the F-16"; number tuning stays a preset |
| **`FBModule` is DUMB: a module DECLARES what it has, the base demands nothing** | exactly ONE pure virtual on `modules/FBModule.h` (`Run`); every slot is an enumerated declaration, and the accessor answers `nullptr` for what was never declared |
| The declaration list is DATA, readable at run time, not a set of C++ overrides | `build/fb-gym --caps` prints `<module> <capability> <c++ type>` from `modules/FBCapability.h`'s one table |
| Nothing is preloaded | every tile on demand — every point on Earth is a valid start |

## State

`FBModule` carries **one** pure virtual (`Run`), down from 28 — of which 21 were slot accessors.
`modules/FBCapability.h` holds the twenty-row table; the seven modules in the tree declare their slots
in their constructors (f16 twenty, mig29 nineteen — no bound HOTAS, so `human_input` is undeclared and a
human cannot take that seat). `fb-gym --caps` prints 1065 `<module> <capability> <type>` lines over the
56 registered module keys — 20 for `f16`, 19 for every other key. Measured unchanged across the switch:
all 296 `mods/f16/src/missions/*.fbm` byte-identical
(`tools/fb_regress.sh`), all ten harnesses, `test-air` at 7 band violations, `payerne-full --threads
1/2/4` on one signature, `verify-layers` at six perception readers, `verify-guards` 8/8.

The rest as described below: one process per client, three clients against one library, the module
registry, the single model root with its delta gate, and the multi-unit snapshot discipline. The
per-client detail is in [`clients/clients.md`](clients/clients.md), the multi-unit detail in
[`missions/runtime.md`](missions/runtime.md).

## Gaps

The architectural contradictions found while distilling live with their subsystems, not here:

| Finding | Where |
|---|---|
| `systems/FBDisplaySystem` includes `render/FBCamera.h` — a **second**, undocumented core-lib exception | [`sim/systems.md`](systems.md) |
| `missions/FBTickPool` is gym-only. The WASM loop is no longer a second loop at all: `missions/FBMissionSim` is THE loop and both clients drive it — the browser's own copy had lost the rule that ends a run | [`clients/clients.md`](clients/clients.md) |
| The renderer draws no units at all, although `FBWorld` borrows the registry | [`render/units-visual.md`](render/units-visual.md) |

**The declaration is possible, nobody has used it yet.** Every one of the seven modules still declares
all twenty slots (nineteen for the MiG-29), including a bomb's gun and the F-16's IRST — and the reason
is not inertia but `units/FBSimUnit::StartTelemetry`, which registers fifteen of them **by position**:
dropping one changes the column layout of every `telemetry.csv` and therefore every baseline in the tree.
Pruning is its own round with its own measurement, and it is the round that makes the *number* fall.
Until then the win is structural only: the base no longer forces the composition, so an ork with a club
compiles today while an F-16 keeps every column it ever wrote.

**`FBState` is an aircraft cockpit, not a position.** It was the obvious candidate for the base's second
duty (the owner's guess was "position/state"), and it does not survive reading: twenty-two blocks,
including `Ufc`, `Gun`, `Mfd` and `GroundMap`. Base-owning it would make every entity carry an F-16
up-front controls block, which is the defect this round removed. It stays a virtual with an all-`Invalid`
default — a module that publishes no avionics publishes none.

## Knowledge

The map itself — every structural fact of the tree, distilled.

### The process

```
fb-tiles (server: worldwide DEM/OSM/aerial imagery)  ──HTTP──▶  Command Center (client)
                                                          = JSBSim + FBW + autopilot (Pilot/Mission|Manual)
                                                            + WebGPU ECEF renderer + HUD
                                                            as ONE process (WASM browser | native CLI)
```

Nothing is preloaded — every tile on demand. From which follows: **every point on Earth is a valid
start.**

### Core lib + three clients

FlightBox Core is a **pure library**. `sim/`'s `core-lib` target builds the simulator itself into a
native archive `build/libfbcore.a`:

| Included | Not included |
|---|---|
| `core/`, `fdm/`, `systems/`, `modules/` (incl. the F-16), `units/`, the `.fbm` parser, libJSBSim | `render/`, `world/` (tile streaming), `missions/FBTickPool` |

Exactly **one** exception: `systems/FBHudGeometry.cpp` — pure CPU vertex-list maths without a
WebGPU/Dawn include, which `systems/FBDisplaySystem` structurally needs.

WASM is a different toolchain target (emcc/wasm32) and inevitably recompiles the same source-file list
itself. That is a cross-compile, not a duplication of the architecture.

Three clients link or compile against it:

| Client | Source | Target | Role |
|---|---|---|---|
| **`fb-gym`** | `clients/FBAppGym.cpp` + `missions/FBMissionRunner.cpp` (which drives `missions/FBMissionSim`, the ONE loop) | `make -C sim gym` | headless: mission in → telemetry out. **No Dawn/wgpu symbol in the binary** (verified with `nm`). The mission core. |
| **`gpu_native`** | `clients/FBAppNative.cpp` | `make -C sim native` | reference renderer and frame oracle. `--mission --interval` produces PNG proof frames through a GPU-free tick hook on the same `FBRunMission` loop. Without `--interval` it is headless. |
| **wasm** | `clients/FBAppWasm.cpp` | `make -C sim wasm` | the browser. |

`fb-gym` options: `--mission FILE | --campaign FILE [--out DIR] [--timeout N] [--threads N]
[--state FILE] [--carry LIST] [--elev tiles|const|baked] [--dem PATH]`.
`--threads` is **gym-only**.

### Directories

| Directory | Responsibility | Doc |
|---|---|---|
| `sim/src/missions/` | the simulation loop (`FBMissionSim`), the headless orchestrator, spawn, model roots, the gym thread pool | [missions/runtime.md](missions/runtime.md) |
| `sim/src/clients/` | entry points, app lifecycle, sink implementations | [clients/clients.md](clients/clients.md), [build-and-ops.md](build-and-ops.md) |
| `sim/test/` | the harnesses and their declarations, mirroring `sim/src/` path for path (`make -C sim verify-trees`) | [testing.md](testing.md) |
| `sim/src/core/` | avionics bus, command bus, log, telemetry, the two judges, mission-data types, damage model, ballistics, elevation hook, calendar + sun/moon ephemeris (`FBCivilTime.h`/`FBEphemeris.h`, moved down out of `render/` in the C2 round), base types. **Never points into `systems/` or `modules/`.** | [core.md](core.md) |
| `sim/src/math/` | value maths (`FBMat4`) | [core.md](core.md) |
| `sim/src/fdm/` | the JSBSim adapter. The one translation unit with JSBSim headers. | [fdm.md](fdm.md) |
| `sim/src/units/` | world entities: `FBUnit`, `FBSimUnit`, `FBUnitRegistry` | [missions/runtime.md](missions/runtime.md) |
| `sim/src/systems/` | the generic, airframe-agnostic system slots of a module | [systems.md](systems.md), [sensors.md](sensors.md), [pilot.md](pilot.md), [weapons.md](weapons.md) |
| `sim/src/modules/` | `FBModule` base interface + registry | [modules/f16/module.md](modules/f16/module.md) |
| `sim/src/modules/f16/` | the F-16 module and its overrides | [modules/f16/module.md](modules/f16/module.md) |
| `sim/src/modules/stores,missile,ground/` | the modules of released weapons and static ground targets | [weapons.md](weapons.md) |
| `sim/src/render/`, `render/stages/` | WebGPU renderer, one class per shader | [render/renderer.md](render/renderer.md) |
| `sim/src/world/`, `sim/src/world/terrain/` | world, tile streaming, terrain maths | [world/terrain.md](world/terrain.md) |
| `tiles/` | fb-tiles, the tile server (its own Makefile) | [world/terrain.md](world/terrain.md) |
| `temp/` | migration material of the pre-architecture. Read-only quarry, **not living architecture.** | — |

### The layering pattern

**FBCore → interface → default implementation → module-specific override.**

Not "belongs to the F-16". The simulator loads any number of flyable modules at runtime and holds each
one polymorphically behind `FBModule*`. Every module carries the same system categories but differs in
**behaviour**, not just in numbers. Interface and default live in ONE class; a module overrides by
derivation. Pure number tuning stays a preset or config — no empty derivation is created for that.

Since the orchestrator round this also applies to **access**: the generic accessors sit on `FBModule`,
and `FBMissionRunner.cpp` / `FBAppGym.cpp` never include a concrete module header.

#### Declaration, not inheritance

`FBModule` demanded twenty-one pure-virtual accessors, so **every** entity had to own a radar, an IRST,
a gun and an autopilot — a bomb answered `Guns()` and a bunker answered `Autopilot()`. That closes the
tree to everything that is not an aeroplane, which is exactly what the four campaigns of
[`mods.md`](mods.md) need it to be open to.

The pattern is the one industrial fieldbuses use — CANopen's object dictionary, IO-Link's device
description, DeviceTree: **a device enumerates its objects when it is bound; the master asks instead of
assuming.** Here:

| | |
|---|---|
| the list | `modules/FBCapability.h` — ONE `FB_MODULE_CAPABILITIES(X)` table of `(accessor, C++ type, wire name)`, twenty rows, expanded four ways (enum, member, accessor, runtime descriptor) |
| binding | a module calls `DeclareRadar(Fcr_)`, `DeclarePilotSystem(*PilotSys)`, … in its constructor. Re-declaring is how a slot whose object is swapped at `AttachFdm` keeps its entry |
| asking | `FBModule::Radar()` returns `Sensors::FBRadarSystem*`; `Has(FBCapability::Radar)` and `Capabilities()` answer the same question without dereferencing |
| the base's own duty | `Run()`, and nothing else. Position/state (`Telemetry()`), `Airborne()`, `UnitKind()`, the damage layout and the setup hook all carry honest defaults |

**Machine-readable is half the point, not a bonus.** The same table is `fb-gym --caps`'s output and,
per [`mods.md`](mods.md) §2.1, the tool schema a per-unit LLM will receive. A list that existed only as
C++ overrides could not be either. The anti-cheat property is inherited rather than re-argued: what an
LLM may call is what the module declared, and no module declares a way past the simulation.

The accessors are **not virtual**. Covariant returns (`FBF16Fcr& Radar()`, `FBMissileGuidance&
PilotSystem()`, …) are therefore gone; every module already used its own typed member internally, and no
call site outside `modules/` ever needed the derived type.

### An aircraft = module + JSBSim model

| Part | Where | What |
|---|---|---|
| Code module | `sim/src/modules/<name>/` | `FBModule` derivation: systems, presets, displays |
| JSBSim model | `mods/f16/src/aircraft/<model>/` | aero, mass, propulsion, engine data |

**One root** (`missions/FBModelRoots.h`): every model FlightBox flies lives in `mods/f16/src/aircraft` — a
self-contained directory per model, with its `.xml` and its own `engine/` and `Systems/`
subdirectories (JSBSim's own per-aircraft layout, which its loaders search before any shared path).
Today: `f16`, `mk82` (both copies from the submodule), `aim120` (FlightBox's own, the submodule has no
AMRAAM).

The pinned submodule is thereby **no longer a load path but the base** — the upstream state against
which `make -C sim verify-models` diffs every copy. A copy may only deviate as a named entry in
`mods/f16/src/aircraft/MODEL-DELTAS.md` — the delta rule, [`CLAUDE.md`](../CLAUDE.md) principle 1. The earlier
distinction "vendored or own" (`FBModule::FdmModelVendored()`) has therefore been dropped; a module
now only names its model.

Aircraft XML carries its OWN licence (F-16 = GPL, most of them LGPL) — attribution per file, the
`<fileheader>` of every copy stays unchanged.

### Control

A lean fly-by-wire layer stabilises the attitude (rate/attitude PID onto the `fcs/*-cmd-norm` inputs;
the F-16 has its own FLCS → `fcs/fbw-override=1` bypasses it). Above it the autopilot — generic
guidance in `systems/FBAutopilot`, behaviour module-overridable.

| Mode | Mechanism |
|---|---|
| `manual` (`?ap=manual`) | direct stick through the FBW. `FBInputSystem` is still the NoOp default — no bound HOTAS. |
| `pilot` (boot default) | `FBPilot` flies a `.fbm` mission as a phase machine and commands `FBAutopilot::Direct` per phase |
| `bfm` (`set task bfm`) | not an autopilot mode but a control law of its own on manual stick, against the locked radar contact |
| `intercept` (`set task intercept`) | BVR: a state machine of its own (`pilot/FBEngagement`) on direct guidance, flown with the SENSOR |
| `attack` (`set task attack`) | the only phase whose decision is a moment: one pickle on the cue of the fire-control block |

Details: [pilot.md](pilot.md), [systems.md](systems.md).

### Multi-unit

A mission describes a **formation** of several units of different factions. Per unit: module, faction,
spawn, flight plan, objectives. Each gets its own `FBFdm`, its own `FBModule`, its own monitors and its
own telemetry file; the mission verdict falls **per unit**.

The snapshot discipline stands: per tick all units compute first, then a barrier makes the new poses
jointly visible. `FBUnit::GetPose()` therefore always yields the state of the last **completed** tick —
tick order cannot influence any result.

Details and the four expansion stages: [missions/runtime.md](missions/runtime.md).
