# Clients — gym, native, wasm

**Subject:** the three programs that link or compile against the core library, and what each of them
is allowed to be. The library itself is described in [`../architecture.md`](../architecture.md); the
mission loop they share is in [`../missions/runtime.md`](../missions/runtime.md), the
build targets and gates in [`../build-and-ops.md`](../build-and-ops.md).

The split is deliberate: the simulator is a **library** (`sim/`'s `core-lib` target →
`build/libfbcore.a`), and a client adds exactly one thing — an entry point and an output medium.
Nothing about the physics or the verdict may depend on which client is running.

## Spec

### All three

| Contract | Acceptance / measurement anchor |
|---|---|
| The simulation is identical across clients | same mission, same result; the renderer is a bolt-on, never a dependency of the physics or the termination logic |
| **A mission-declared clock binds all three clients** (`C2`, built) | one `time` line, one instant, one sun angle in gym / native / wasm; a client flag that contradicts it is a boot error, never a precedence — see below |
| Both judges are fed by every client that runs a sim loop | `FBRunMission` and the WASM frame loop each feed `FBFlightMonitor` + `FBMissionMonitor` — one definition, no parallel test |
| Only a client may apply initial conditions | `FBFdmBoot` is reachable from `missions/` and `clients/` only |
| A client owns exactly one unit registry and passes it down at tick time | `FBSimUnit::Run` → `FBModule::Run` → sensor slot; `FBWorld` only borrows it for drawing |

### `fb-gym` — the reference path

| Contract | Acceptance / measurement anchor |
|---|---|
| Headless, no GPU device at all | no Dawn/wgpu symbol in the binary, verified with `nm` |
| Runs as fast as the machine allows | wall-clock speed must not change the result (principle 4) |
| `--threads N` parallelises exactly the STEP phase; everything else stays sequential | identical fingerprint over `--threads 1..4` × 5 repetitions |
| Runs without a network | `--elev swiss` when the baked DEM asset exists, else `const`; a bare `fb-gym --mission FILE` always runs |
| This is the mission control loop | mission → telemetry → analysis → correction (`../build-and-ops.md`) |

### `gpu_native` — the frame oracle

| Contract | Acceptance / measurement anchor |
|---|---|
| Reference renderer: whatever it draws is the truth about the picture | headless PNG proof frames, `--mission --interval` |
| Proof frames ride on the SAME `FBRunMission` loop through a GPU-free tick hook | `--mission` without `--interval` stays headless and numerically unchanged |
| A frame proof is a deliverable, not a screenshot | build-effective changes count as verified only with a rendered frame or a numerical measurement |

### wasm — the browser

| Contract | Acceptance / measurement anchor |
|---|---|
| Same source list, other toolchain (emcc/wasm32) — cross-compile, not a second architecture | `make -C sim wasm` builds gpu.js/gpu.wasm **and** the tile worker |
| Single-threaded sim loop by design | real time needs no parallel physics, and the browser is spared the pthreads/SharedArrayBuffer build |
| Model and mission data travel in Emscripten's virtual FS | one embedded model root, one build-copied mission directory (never a hand-kept second copy) |
| The frame loop must mirror the runner's phase order | elevation before STEP, publish barrier, monitors, roster |
| Eventually: everything a pilot can do in the gym, a human can do in the browser | that is the direction of roadmap R9, not a claim about today |

## State

| Client | State | Anchor |
|---|---|---|
| `fb-gym` | **built and load-bearing.** All 85 missions, all seven harnesses and the tournament runner drive it. Threading proven deterministic. | `705c90a`, `6d7ed5a` |
| `gpu_native` | **built.** Terrain + HUD frames; last proof `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs. | `c9206eb`…`2099cb0` |
| wasm | **built, but partial.** Flies, renders, trims (`trimConverged=1` from the embedded `/fb/aircraft`); no weapon release path, no damage path, no cockpit displays, no bound HOTAS. | `705c90a` + model-root round |
| the mission clock (`C2`) | **built in all three.** `missions/FBClockBoot.h` decides, `core/FBEphemeris.h` computes, `FBEnvironmentBlock` carries it. The 84 pre-round missions are byte-identical; the flag collision is a boot error with a printed reason | this round |

## Gaps

### Open work (from the retired `TODO.md` §5)

| # | Client | Thing |
|---|---|---|
| 5.1 | wasm | **No release and no damage path.** `FBAppWasm.cpp` drains neither `Stores().TakeRelease()` nor `Guns().TakeBurst()`, holds no `FBGunProjectiles` pool and resolves no burst. The browser can carry a weapon (point mass + drag act) but nothing leaves the jet. The whole apparatus lives in `missions/FBMissionRunner.cpp`. |
| 5.2 | wasm | Cockpit displays: the values are on the bus, the presentation is missing entirely. |
| 5.3 | wasm | HOTAS binding — deliberately last, it is only a mapping. `FBInputSystem` is NoOp. |
| 5.4 | wasm/native | No lock / TD-box HUD symbology, because `doc/modules/f16/hud-symbology.md` documents none. It will not be invented — see [`../render/hud.md`](../render/hud.md). |

### Open work (from the retired `TODO.md` §4.2)

| # | Client | Thing |
|---|---|---|
| 4.2 | native/gym | **`payerne-full` crashes under `--elev tiles`.** Three suspect areas named: z13 bilinear against the 90 m raster, the 33 m cache cell, a 503 on cold start. While this is open, the mission control loop effectively hangs on `const`/`swiss` — i.e. the loop that decides pilot-AI questions never sees real terrain. |

### Specified, not built

None open for the clients themselves. The clock (`C2`) closed in this round; its consumer (`C3`,
visual acquisition) is a `sensors/` gap, not a client one.

### Unverified

- **The WASM loop has never been compared line by line with the runner's phase order.** It demonstrably
  calls `PublishPose`, `RunMonitors`, `PrimeState` and `FBWorld::SetUnits`, but whether it mirrors the
  order exactly (elevation before STEP, roster build) was not checked
  ([`../missions/runtime.md`](../missions/runtime.md), Gaps).

## Knowledge

| Fact | Source |
|---|---|
| `fb-gym` options: `--mission FILE [--out DIR] [--timeout N] [--threads N] [--elev tiles\|const\|swiss]`; `--threads` is gym-only | [`../architecture.md`](../architecture.md) |
| Exit codes 0/1/2/3 = SUCCESS/FAIL/CRASH+LOC/TIMEOUT | [`../missions/runtime.md`](../missions/runtime.md) |
| Per-run output: `telemetry.csv` (10 Hz, fixed column count) + `telemetry_<callsign>.csv` per further unit + `events.log` | [`../missions/runtime.md`](../missions/runtime.md) |
| Threading measurements (2 units 1.29–1.41× at 2 threads; 4 units up to 1.77× at 4 threads; the ceiling is the machine) | [`../missions/runtime.md`](../missions/runtime.md) |
| `missions/FBTickPool` is gym-only, not part of the core lib, never reaches the WASM build | [`../architecture.md`](../architecture.md) |
| Host operation: podman VM, `tiles/up.sh` (:8081), `sim/up.sh` (:8080, mounts `sim/web` live) | [`../build-and-ops.md`](../build-and-ops.md) |

### Weather defaults per client (R4, commit `43b82b5`)

Precedence lives in one place, `missions`-side (`FBWeatherBoot.h`): **a mission that declares `wx`
always wins** — a scenario with declared wind keeps it. Without a declaration the client default
applies:

| Client | Default | Why |
|---|---|---|
| gym | calm | the regression baseline stays byte-identical |
| native | calm | the frame oracle needs reproducibility |
| wasm | **live `/wx`** | the browser flies today's real weather |

The browser fetches once per session (a GFS cycle is valid 6 h), flies calm until the blob arrives,
and adopts it ATOMICALLY at a frame boundary — under ASYNCIFY a callback can land between two
substeps. A dead endpoint is a warning, never a boot failure (the `/elev` lesson). No CLI flag, by
decision: weather is part of the SCENARIO like the runway and the spawn — a flag would let a
measurement silently run in air the file did not declare.

### Clock defaults per client (`C2`) — **built**

The contract of the `time` line is in [`../missions/syntax.md`](../missions/syntax.md); what belongs
*here* is which clock a client runs when the mission declares none, and what happens when a client flag
and a mission line both speak.

Each client's own path when the mission declares nothing — unchanged by the round, deliberately:

| Client | No `time` in the mission |
|---|---|
| `fb-gym` | **no clock and no ephemeris.** `FBEnvironmentBlock` stays `Invalid`, `blk_env` stays 0, nothing is computed — the reason the 84 pre-round missions are byte-identical |
| `gpu_native` | `--utc SECS` (Unix seconds; `0`/absent = the host wall clock) |
| wasm | `window.FB_SIM_UTC` (Unix seconds; `0`/unset = the host wall clock) |

Precedence in ONE place on the `missions` side (`FBClockBoot.h`, the sibling of `FBWeatherBoot.h`):

| Situation | Rule |
|---|---|
| mission declares `time` | that instant, on **all three clients**, identically |
| mission declares no `time` | the client default: gym **none** (no ephemeris, exactly as today), native/wasm **their existing wall-clock or flag path**, unchanged |
| mission declares `time` **and** `--utc` / `FB_SIM_UTC` is also set | **hard boot error, exit 1.** Not a precedence. Checked BEFORE the spawn, so it needs neither terrain nor a GPU; fixture `sim/missions/negative/clock-flag-collision.fbm` |

That last row is where this rule deliberately differs from `wx`, and the difference is worth stating.
Weather has **no flag at all**, so no conflict can arise; the clock has one, and it has to keep it,
because `gpu_native` also runs **without a mission** (the cloud proof cameras `p1`…`p5` are free-camera
runs with no file to declare anything). Keeping the flag and making the collision an error costs one
comparison and buys the same guarantee the `wx` rule buys by deletion: *a measurement can never
silently run under a sky the file did not declare.* Silent precedence would leave a stale `--utc` in a
shell history able to move a measured detection range without a line in the output.

Consequence for the gym, and it is the load-bearing one: as soon as a **sensor** consumes the clock
(`C3` visual acquisition, [`../sensors.md`](../sensors.md) §9) the clock stops being a renderer switch.
`fb-gym` must then compute the same ephemeris the renderer does, from the same pure functions — which
is why those functions moved down to `core/` in the C2 round.

**How the clock reaches a client.** It is pushed by the OWNER, never pulled by a system: the runner (and
the WASM frame loop) samples `FBSolarAt(lat, lon, T0 + simT)` per actor per decision tick, beside the
cloud sample and for the same reason, and hands it down `FBSimUnit::UpdateSolar` → `FBModule::SetSolar`
→ `FBEnvironmentBlock`. **No system below the owner holds a clock**, exactly as no sensor queries the
world. The native hook takes the resolved clock through `FBMissionTickHook::OnClock` before
`OnMissionStart`, so the first drawn frame already stands at the declared instant.

**Where the layer move landed.** `core/FBEphemeris.h` (namespace `FlightBox`, `FBSunPos`/`FBMoonPos`,
`double` Unix seconds), plus `core/FBCivilTime.h` for the calendar. `render/` no longer contains an
ephemeris; both clients include it from below. Proof that the move changed nothing: the screenshot
venue's PNGs at `--utc 922312800`, SVS and EVS, are byte-identical to the pre-round binary.
