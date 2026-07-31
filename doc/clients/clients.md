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
| **`--campaign` is gym-only** (`C0`, built) | `missions/FBCampaignRunner` is on the gym link line and in neither `libfbcore.a` nor the wasm build, for `FBTickPool`'s reason: a campaign is a sequence of headless runs, and neither the frame oracle nor the browser has one. Every step it runs is reachable as an ordinary `--mission … --state …` |

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
| **A human in the seat gets no right the AI does not have** | his stick reaches the FLCS only as `FBPilotGuidance::Manual` through the module's own `ApplyPilotCommands`, and EVERY switch action of his exists only as an `FBCommandBus::Post` — same latency class, same occupancy rule, same rejection catalogue. Acceptance: a browser run shows `CMD_REJECT`/`CMD_ACK outcome=rejected` against the human exactly as against a pilot, and the set of state writers is unchanged (`FBFdmBoot` only) |
| **What leaves the jet is resolved by ONE apparatus, not by a client's copy of one** | `missions/FBOrdnance` is on the core-lib source list and both the runner and the browser call the same three per-tick methods. Acceptance: moving it out of `FBMissionRunner.cpp` changed no artefact byte |
| Single-threaded sim loop by design | real time needs no parallel physics, and the browser is spared the pthreads/SharedArrayBuffer build |
| Model and mission data travel in Emscripten's virtual FS | one embedded model root, one build-copied mission directory (never a hand-kept second copy) |
| The frame loop must mirror the runner's phase order | elevation before STEP, publish barrier, monitors, roster |
| Eventually: everything a pilot can do in the gym, a human can do in the browser | that is the direction of roadmap R9, not a claim about today |

## State

| Client | State | Anchor |
|---|---|---|
| `fb-gym` | **built and load-bearing.** All 85 missions, all seven harnesses and the tournament runner drive it. Threading proven deterministic. | `705c90a`, `6d7ed5a` |
| `gpu_native` | **built.** Terrain + HUD frames; last proof `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs. | `c9206eb`…`2099cb0` |
| wasm | **built, and now playable.** Flies, renders, trims (`trimConverged=1` from the embedded `/fb/aircraft`), and since the player-control round: a bound keyboard stick, master arm / station select / pickle / gun trigger over `FBCommandBus`, and the full release + damage apparatus (`missions/FBOrdnance`) the runner drives. Still missing: cockpit displays and lock/TD-box symbology. | `705c90a` + model-root round + this round |
| wasm: the human in the seat (`5.1`, `5.3`) | **built.** `systems/FBInputSystem` is a REAL slot; `FBModule::HumanInput()` hands it out and is null for every module without a cockpit. The analogue half becomes `FBPilotGuidance::Manual` through the module's own `ApplyPilotCommands`; the discrete half exists ONLY as `FBCommandBus::Post`. With a human engaged the AI pilot is not run at all. | measured in Chrome: `hotas STICK state=taken` → `gun TRIGGER burstS=0.6 rounds=510` → `sms RELEASE station=3 store=mk82` → `stores IMPACT tofS=10.17` → `damage DAMAGE unit=bunker` (§Knowledge, browser proof) |
| wasm: the ordnance world (`5.1`) | **built and SHARED.** The store/gun apparatus moved out of `FBMissionRunner.cpp` into `missions/FBOrdnance` — three calls per tick (`Resolve` → `Launch` → `SnapPoses`), driven identically by the runner and the browser frame loop. The runner keeps ONE thing of its own: opening a telemetry CSV per released store, through `OnStoreSpawned`. | pure move: **31 missions** (12 chosen for weapon coverage — gun/missile/CCIP/CCRP/cluster/ARM/net/duel — plus 19 of a stratified every-4th-file sweep, incl. the 8-ship `ar-*` arena rungs), **368 artefacts** (`events.log` + every `telemetry*.csv`) **byte-identical** to the pre-move binary, 0 exit-code differences |
| wasm: WHICH mission it flies | **selectable.** `window.FB_MISSION` (or `?mission=`), sanitised to a filename and resolved against the build-copied `web/missions/`; absent = `payerne-full` as before. The mission buffer went 8 KB → 64 KB and a FULL buffer is now refused, because `fb_fetch_text` truncates silently and a mission cut at a line boundary parses into a smaller cast | the player layer's only reach into the client, [`../player-layer.md`](../player-layer.md) §11 B6 |
| the mission clock (`C2`) | **built in all three.** `missions/FBClockBoot.h` decides, `core/FBEphemeris.h` computes, `FBEnvironmentBlock` carries it. The 84 pre-round missions are byte-identical; the flag collision is a boot error with a printed reason | `26dd3f2` |
| the campaign layer (`C0`) | **built in the gym only, by decision.** `fb-gym --campaign` loops `FBRunMission`; the 104 missions run singly stay byte-identical, 9 campaign runs give 1 fingerprint and every step replays standalone, under `swiss` as well as `const` | this round |

## Gaps

### Open work (from the retired `TODO.md` §5)

| # | Client | Thing |
|---|---|---|
| ~~5.1~~ | wasm | **CLOSED this round.** The apparatus is `missions/FBOrdnance` and both clients drive it. |
| 5.2 | wasm | Cockpit displays: the values are on the bus, the presentation is missing entirely. The player strip in `web/index.html` shows the ARMAMENT half only (arm state, selected station, stores, rounds, hits) and reads it off the seat's own published blocks through one 1 Hz `hotas armament` log line — it is not an MFD. |
| ~~5.3~~ | wasm | **CLOSED this round** for the keyboard. A gamepad is still unbound, and the CONTROL CURVE is still undecided (below). |
| **5.5** | wasm | **The browser's frame-paced tick makes a different run out of the same file, and weapons made it visible.** The runner steps a fixed `dt = 0.1 s`; the browser steps whatever rAF gives it (~0.0167 s at 60 fps, clamped at 0.1). Three measurements, same binaries, same missions: (a) `cbu87-footprint` — the pilot's CCIP release executes at `aimMissM = 122.7 m` in the browser against `22.5 m` in the gym, so the canister lands ~100 m short, both targets sit outside the 400 × 200 m footprint and the run ends SUCCESS-without-a-kill where the gym kills two; (b) a held gun trigger produces `gun BURST_DROPPED … live=64` after ~1 s, because `FBF16Module` runs the gun ONCE PER `Run()` by design ("its output is a round count INTEGRATED over time") — which conserves ROUNDS but makes the number of BUNDLES equal to the FRAME RATE, 60/s against the runner's 10/s, and `FBGunProjectiles::kMaxBundles` is 64; (c) `gun-bfm` flies 520 s of browser sim with zero bursts where the gym's first hits fall at `t = 62.8 s` (confounded: the browser flew the real DEM at 1813–2059 m ground, the gym `--elev const` at 430 m, so the BFM floor is not the same problem). **This is principle 4's own case** — "gibt das Tempo das Ergebnis, ist die Kopplung nicht-deterministisch — ein Bug". Two candidate mechanisms, both UNVERIFIED: the outer `dt` itself, and `FBF16Module::Due`, which subtracts ONE period per call and therefore runs a 20 Hz slot at 10 Hz under `dt = 0.1` and at its true rate under `dt = 0.0167`. Not touched here by decision: a fixed 0.1 s browser tick moves the camera to 10 Hz and is its own round with its own acceptance. |
| **5.6** | wasm | **No control curve** (`doc/player-layer.md` §7 3b, unchanged). A key is a switch, so `FBInputSystem` ramps the axis to full deflection over `kAxisRampS`, and that constant is `FBCommandBus::kHotasLatencyS` — reused, not invented, because it is the tree's one measure of how long a HOTAS action takes. It is NOT the F-16's force-sensor law, and no source for one has been read. |
| **5.7** | wasm | **The keyboard is bound to the WINDOW for the whole session**, so the arrow keys do not scroll the debriefing either. The handler consumes only the keys it uses and returns `EM_FALSE` for everything else. |
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
| `fb-gym` options: `--mission FILE \| --campaign FILE [--out DIR] [--timeout N] [--threads N] [--state FILE] [--carry LIST] [--elev tiles\|const\|swiss]`; `--threads`, `--campaign` and `--state` are gym-only | [`../architecture.md`](../architecture.md) |
| Exit codes 0/1/2/3 = SUCCESS/FAIL/CRASH+LOC/TIMEOUT | [`../missions/runtime.md`](../missions/runtime.md) |
| Per-run output: `telemetry.csv` (10 Hz, fixed column count) + `telemetry_<callsign>.csv` per further unit + `events.log` | [`../missions/runtime.md`](../missions/runtime.md) |
| Threading measurements (2 units 1.29–1.41× at 2 threads; 4 units up to 1.77× at 4 threads; the ceiling is the machine) | [`../missions/runtime.md`](../missions/runtime.md) |
| `missions/FBTickPool` is gym-only, not part of the core lib, never reaches the WASM build | [`../architecture.md`](../architecture.md) |
| Host operation: podman VM, `tiles/up.sh` (:8081), `sim/up.sh` (:8080, mounts `sim/web` live) | [`../build-and-ops.md`](../build-and-ops.md) |

### The human in the seat — what is bound, and where each half goes

| Key | Half | Where it lands |
|---|---|---|
| `P` | seat | `FBInputSystem::TakeStick()` / `ReleaseStick()`. Taking it SEEDS the slot from the airframe (throttle from the last FLCS command, speedbrake and gear from `FBAirframeControls`) — a seat booting on defaults would put the gear down at altitude |
| `↑ ↓` `← →` `, .` | analogue | axis INTENT (−1/0/+1) → ramp over `kAxisRampS` → `FBStickInput` → `FBPilotCommands{Manual}` → `ApplyPilotCommands` → `FBAutopilot::SetManual` → FLCS → `FBFdm::SetControls`. The identical path the AI's Manual guidance takes |
| `W` `S` | analogue | throttle intent; the throttle HOLDS at intent 0 where the stick self-centres — a quadrant is not a spring |
| `B` `G` | analogue | speedbrake / gear, through the same `FBPilotCommands` fields the pilot uses |
| `M` | discrete | `FBCommandTarget::MasterArm`, value read off the seat's own published `FBStoresBlock::Arm` — the key is the THROW, the jet knows the position |
| `1..9` | discrete | `FBCommandTarget::StationSelect`; an empty station answers `OutOfContext` and the player sees why |
| `ENTER` | discrete | `FBCommandTarget::WeaponRelease` — the pickle, the one way a store leaves |
| `SPACE` | discrete, HELD | `FBCommandTarget::GunTrigger`, value = the squeeze length. A held key is the same action REPEATED, paced by two guards: `kTriggerRepeatS` (= `kHotasLatencyS + kTriggerLatencyS`) covers the time before the first completion, and `FBCommandBus::SwitchReady` covers the time after it. Only the first guard is derivable; the window itself runs from the COMPLETION, whose time is the answering box's cadence, so it is ASKED. Without either, one keypress filled the whole 8-slot queue or earned one `ChannelBusy` per repeat — both measured |

`FBCommandBus::SwitchReady` is the round's one new bus member: a const query of the occupancy rule `Post()` already applies, so it changes no outcome and only stops a doomed post from being made.

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
