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
| **The sim tick is a property of the SIMULATION, not a client's choice** (`C4`, built) | one constant, `missions/FBSimTick.h`'s `kSimTickS = 0.1 s`, and every client advances the sim in whole multiples of it. Acceptance: the same mission gives the same numbers at ANY frame rate — measured in Chrome at 60 fps and at ~600 fps, `aimMissM = 20.7715` in both, digit for digit |
| **A mission-declared clock binds all three clients** (`C2`, built) | one `time` line, one instant, one sun angle in gym / native / wasm; a client flag that contradicts it is a boot error, never a precedence — see below |
| **No client runs a sim loop — every client DRIVES the one loop** | `missions/FBMissionSim` owns the tick and the rule that ends a run; a client calls `Advance(budgetS)` (browser) or `RunToConclusion()` (headless) and is TOLD the run state. Acceptance: `verify-guards` (a client that drops the verdict does not compile), `verify-layers` prints **1** simulation-loop driver |
| Both judges are fed, and OBEYED, by every client | one loop feeds `FBFlightMonitor` + `FBMissionMonitor`; a physical K.O. lands in the damage register and the loop stops on it. Measured in Chrome: `monitor KO reason=CFIT` at `t=40.3` → `mission RESULT result=CRASH` → 13 s of further frames at `simTicksPerS=0` |
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
| **The browser does not own a tick and cannot end up with its own rules** | it hands `FBMissionSim::Advance(dt)` the wall time it owes a picture for; the phase order, the judges, the timeout and the verdict are the simulation's. What is left in `FBAppWasm.cpp` per tick is a READ (the eye's two poses, through the GPU-free `FBMissionTickHook`) |
| **A finished run is a picture, not a second truth** | on `FBRunState::Concluded` the client stops advancing, emits the same `mission RESULT` line the runner emits, and keeps rendering the frozen scene; the player layer's debriefing already listens for that line (`web/fbmenu.js`, [`../player-layer.md`](../player-layer.md) §5) |
| **The camera keeps the frame rate the sim does not have** | the eye pose is carried across the sub-tick gap (`EyeAt(alpha)`, extrapolated from the last two tick poses) and is read by NOTHING the simulation sees. Cost, stated: the HUD is generated by the display slot at 10 Hz, so during a manoeuvre its symbology and the terrain differ by up to one tick of attitude change |
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
| the browser's clock (`C4`) | **built, and since this round the accumulator is the SIMULATION's** (`FBMissionSim::Advance`/`TickPhase`). The frame loop hands over wall time and gets whole `kSimTickS` steps; the camera is extrapolated between them and the frame rate touches nothing else. `FBLog` in the browser now stamps SIM seconds like the runner, which is what makes one console log diffable against one `events.log` | this round, table in Gaps `5.5` |
| the campaign layer (`C0`) | **built in the gym only, by decision.** `fb-gym --campaign` loops `FBRunMission`; the 104 missions run singly stay byte-identical, 9 campaign runs give 1 fingerprint and every step replays standalone, under `swiss` as well as `const` | this round |

| **the second loop (`C5`)** | **CLOSED, and it was the defect the owner found: "der flieger ist einfach in den berg geflogen und die simulation lief weiter".** `clients/FBAppWasm.cpp` had written itself a tick (`SimTick()`) that called `RunMonitors` and threw the result away — no `FirstFlightKo`, no verdict check, no timeout, no end. The loop is now `missions/FBMissionSim` for both clients, `Tick()` is private, the unit tick surface is friend-locked to it, and `FBRunState` is `[[nodiscard]]`. | proof: `missions/cfit-oberland.fbm` in Chrome — `t=40.3 monitor KO reason=CFIT`, `mission RESULT result=CRASH reason="ground penetration"`, then 13 s of frames with `simTicksPerS=0 substepsPerFrame=0` while `frames≈60`; the same file in fb-gym: exit 2, `CRASH`, same reason, `t=39.9` (the 0.4 s is the elevation source, §5.9). Regression: 281 missions, byte-identical |

| **the TACTICAL MAP, in both graphical clients** | **built.** `gpu_native --map <callsign> [--map-span-km KM]` draws the fused picture of that unit — the control node of its faction's net — instead of its cockpit: nadir over the node, OSM ground from the terrain renderer, APP-6 symbology from `render/FBTacticalMap` into the EXISTING HUD pass. In the browser **TAB is the view** (cockpit ↔ map) and the ground-albedo toggle it used to throw moved to `t`; selecting a unit on the map and pressing TAB sits in ITS cockpit, which is an INFORMATION change and sees less than the map | frames in `sim/build/tactical-map/`; the log carries `map frame own=… contacts=… bearings=…` and one `map contact` line per unknown datum, so the picture is checkable without reading pixels. [`../player-layer.md`](../player-layer.md) §12 |
| **the SCRIPTED COMMANDER** | **built, native only.** `--order AT:UNIT:KIND[:A[:B[:C]]]` hands one tactical order to one unit's AI at one sim second — the same input a click on the map is, and it ends in the same inbox. It writes no simulation state: there is no path from the flag to one | `KIND = waypoint|steer|attack (lat:lon[:altM]) | abort | emcon:0\|1 | wcs:free\|tight\|hold` |
| **the map's own keys in the browser** | **built.** `TAB` view · `N`/`B` select the next/previous own unit · `[`/`]` zoom · `E`/`R` EMCON silent/radiate · `H`/`G` weapons hold/free · `A` engage the freshest UNKNOWN datum on the map · `X` abort. Every one of them ends in `FBPilot::ReceiveOrder`, and the answer is an `order ACCEPTED`/`order REFUSED` line | there is no cursor yet, so `waypoint`/`steer` are reachable from the native `--order` and not from the browser — booked below |

## Gaps

### From the tactical-map round

| Gap | Detail |
|---|---|
| **No cursor on the map, so `waypoint`/`steer` are native-only** | the browser binds keys, not a pointer; the two orders that need a PLACE are reachable from `gpu_native --order` and from nothing else. `FBTacticalMap::Unproject` exists and is the map's own projection inverted, so the click handler is the missing half and not the arithmetic |
| **The map is ONE node's fusion** | `FBForcePicture::Ingest` is called once, for the control node. The class takes any number of contributors; what is missing is a RULE for which own units may contribute, and "all of them" would hand the map the picture of a jet that is off the link |
| **The map's node is found by callsign, and a dead node leaves the map with the ownship's own picture** | `FBModule::NetControlNode()` returns what the mission declared; if that unit is not in the actor list the client falls back to the ownship. That is the smaller claim of the two, but a NODE THAT HAS BEEN SHOT DOWN is not distinguishable on the screen from one that was never declared |
| **The browser's map is BUILT but UNMEASURED on a browser** | no Chromium in the build environment: `make wasm` builds warning-free and deploys to `sim/web/`, and the C++ behind TAB is byte-for-byte the code the native oracle's frames and `order …` lines come out of — but no browser frame of the map and no browser `view MODE` line has been seen. It is the user's hardware's proof, not this round's |
| **A refusal is a log line, not a screen element** | the map's status line shows the order that was SENT (`ORDER attack -> vip1 (SEQ 4)`); the ANSWER is `order ACCEPTED`/`order REFUSED` in `events.log` and in `web/fbmenu.js`'s feed. A commander watching only the map sees his own word and not the reply |

### What the browser did NOT have, in full (`C5` round)

The end rule was the one that cost a run, but it was not alone. Everything the runner's tick did and
the browser's copy did not, listed whether or not this round fixed it:

| # | Rule the runner had | Browser before | Now |
|---|---|---|---|
| 1 | **end the run on a physical K.O.** (`FirstFlightKo`) | absent — the `monitor KO` line was printed and integration continued | **fixed**, and generalised: the loop asks the damage register (`FBSystemHealth::Destroyed`) |
| 2 | **end the run on a deciding mission failure / all judged concluded** | absent | **fixed** (same predicate, one place) |
| 3 | **the mission `timeout`** | never read; a browser run had no clock that could end it | **fixed** (`FBMissionSim` takes it; the `?ap=manual` sandbox declares infinity, honestly, because it has no plan) |
| 4 | **`FinalizeMission` for every open judge** | absent — a `survive` objective never concluded, so no `mission OBJECTIVE` vector was ever published in the browser and the debrief had to read "never judged" as "nothing met" | **fixed** |
| 5 | **the combined RESULT line** | absent (only the monitors' own lines) | **fixed** — same line, same fields as `FBRunMission` |
| 6 | **`CheckEnvelope()`** — stall/overspeed/sink warnings | absent | **fixed** |
| 7 | **`UpdateSky()`** — the cloud decks per actor | absent: a module's optical/IR sensors flew in clear air even under an overcast the RENDERER was drawing | **fixed** |
| 8 | **`UpdateSolar()`** — sun/moon per actor under a declared `time` | absent: `FBEnvironmentBlock` stayed Invalid, so a visual sensor's day/night rules never applied in the browser | **fixed** |
| 9 | **`SampleTelemetry()`** | absent | called, but a no-op: the browser has no telemetry SINK. Open — see 5.12 |
| 10 | **spawn validation**: unresolved elevation = FAIL, declared altitude below ground = FAIL | absent: after 40 × 50 ms of polling the browser falls back to `kConfigGroundM = 430 m` and flies | **open (5.13)** — client-specific boot, not part of the tick |
| 11 | **a telemetry CSV per released store** (`OnStoreSpawned`) | absent | by design: no file system |
| 12 | **`UNIT_RESULT` per actor at the end** | absent | by design for now: the browser has no telemetry paths to name, and the debrief reads the per-unit lines out of the log |
| 13 | **a declared `wx fixture` is a FAIL** | degrades to live `/wx` | by design, already documented above |
| 14 | **campaign carry**, `--threads` | absent | by design (`C0`) |

### Open work (from the retired `TODO.md` §5)

| # | Client | Thing |
|---|---|---|
| ~~5.1~~ | wasm | **CLOSED this round.** The apparatus is `missions/FBOrdnance` and both clients drive it. |
| ~~5.2~~ | wasm | **CLOSED this round for the MFD half.** The screen is a 3x3 grid: the upper six quadrants are out-the-window + HUD (a real scene VIEWPORT, not an overlay — `doc/render/renderer.md` §2.4), the lower three are an MFD bank drawn by `systems/FBDisplaySystem::BuildMfd` from published blocks only, in the HUD stage's own geometry and therefore at zero extra render passes. Which page a bay carries is itself a published block (`FBMfdBlock`), and the PILOT switches it over `FBCommandTarget::MfdPageSelect`. The HUD gave its state readouts up in exchange (`doc/modules/f16/hud-symbology.md`). **Still missing:** ICP/DED/OSB bezels, DTE, analog instruments — and the gaps opened by this round are D1..D7 in [`../modules/f16/cockpit-displays.md`](../modules/f16/cockpit-displays.md). |
| 5.10 | wasm | **The cockpit strip has no author for a command.** It shows the pilot's PHASE (from the 1 Hz `pilot phase` line) beside the `mfd_page` `CMD_ISSUE`/`CMD_ACK` pair, which is the honest pair of facts the run publishes; it cannot show WHO posted the command, because `FBAvionicsCommand` carries no author, nor the ENGAGEMENT state, because that is a telemetry column and the browser has no telemetry sink. Both are named in `cockpit-displays.md` D2/D3 rather than reconstructed in the frontend. |
| 5.11 | wasm | **The canvas is now the whole 3x3 grid, so the DOM must stay off it.** `#fb-bar` (top) and `#fb-hud` (bottom) take their height OUT of the canvas (`height:calc(100vh - 28px - 108px)`) instead of floating over it — an overlay would have covered the bank or the windscreen, which is exactly what the layout forbids. Consequence, stated: the picture is smaller than the window by 136 px, and there is no full-screen-without-chrome mode other than `F`. |
| ~~5.3~~ | wasm | **CLOSED this round** for the keyboard. A gamepad is still unbound, and the CONTROL CURVE is still undecided (below). |
| ~~5.5~~ | wasm | **CLOSED this round, and the mechanism was measured before anything was changed.** Not the cadence `FBF16Module::Due` picks — that one is right at any dt (a 10 Hz slot fires every 6th frame at 1/60 s, i.e. still 10 Hz). It is the `dt` HANDED THROUGH a throttled slot: `FBPilot::Run` does `TimeS_ += dt` and is called at 10 Hz, so with the frame's dt its own clock runs at `dt / 0.1` of sim speed. Convicted with two probes in fb-gym (both reverted): (a) with the tick forced to 1/60 s the GYM reproduces the browser — `aimMissM 22.5 → 117.9`, `leadS 0.6 → 0.5167`, and the pilot's clock reads **11.92 s while the world is at 71.5 s** (factor 6.0), so its pickle is stamped 60 s in the past, the bus finds it due at once, the 0.5 s HOTAS latency the pilot leads for never happens and the store leaves 0.42 s early = 97 m short at 231 m/s; (b) hand each throttled slot its own PERIOD instead of the frame dt and the same 1/60 s tick gives `aimMissM = 12.1` with both targets dead — a sampling-phase residual, not a bias. The fix is therefore the CLIENT's: `missions/FBSimTick.h` is the one tick constant, `FBMissionRunner` and the browser both step it, and the browser's frame loop turns wall time into whole ticks. **After, same file, same air (`wx calm`), same machine:** cbu87-footprint `aimMissM` browser **20.77** against gym **22.54** (the 1.8 m rest is the terrain seam — the browser samples the streamed tile raster, the gym the baked swiss DEM: impact plane 430.13 m against 430.007 m; `fb-gym --elev tiles` gives 22.62), both targets `DESTROYED` in both; the held trigger gives `1, 5, 9, 10, 10 …` rounds per bundle at exactly 0.1 s spacing — **the gym's own gun-bfm sequence, digit for digit** — 53 bundles in 5.3 s and **0 `BURST_DROPPED`** (before: 128 bundles of 1 round and **185** dropped). Cross-check `attack-ccrp`: `aimLongM` gym +38.56 m, browser after +41.98 m, browser before **−72.63 m** with no `damage DAMAGE` line at all |
| **5.6** | wasm | **No control curve** (`doc/player-layer.md` §7 3b, unchanged). A key is a switch, so `FBInputSystem` ramps the axis to full deflection over `kAxisRampS`, and that constant is `FBCommandBus::kHotasLatencyS` — reused, not invented, because it is the tree's one measure of how long a HOTAS action takes. It is NOT the F-16's force-sensor law, and no source for one has been read. |
| **5.8** | modules | **The trap is disarmed, not removed.** `FBF16Module::Run` still hands the OUTER dt to every slot `Due` throttles, so the coupling that produced 5.5 is intact and merely unreachable while every client steps `kSimTickS`. Handing each slot its own period is a MODULE round with its own acceptance, because it moves the 20 Hz slots (Displays, Weapons) from the 10 Hz they run at today to their declared rate and would move the regression. Measured cost of the wrong dt, so the next round starts from a number: at a 1/60 s tick the pilot's clock runs at 1/6 sim speed and `cbu87-footprint` misses by 117.9 m instead of 22.5 m. |
| **5.9b** | wasm | **The browser's ground truth is not repeatable across runs, and it is the only thing in the loop that is not.** Two runs of `cfit-oberland` in the same browser gave the K.O. at `t = 40.3` and `t = 40.2`: `fb_stream_ground` answers out of whatever tile LOD is resident at that moment, so the terrain under the aircraft is a function of the STREAM, not of the mission. Principle 4 ("the mathematics is deterministic") holds for everything above it — the tick, the judges, the verdict — and stops exactly at this sampler. |
| **5.9** | wasm/gym | **The last metres between the two clients are terrain, and they are not measured out.** With the tick fixed, `cbu87-footprint` still differs by 1.8 m of aim miss because the browser reads the ground off the streamed tile raster and the gym off the baked swiss DEM (`--elev tiles` closes it to 0.08 m of impact plane but not to zero: the /elev endpoint and the tile raster are two samplers of one DEM). Whether they can be made ONE sampler is unasked. |
| **5.7** | wasm | **The keyboard is bound to the WINDOW for the whole session**, so the arrow keys do not scroll the debriefing either. The handler consumes only the keys it uses and returns `EM_FALSE` for everything else. |
| **5.12** | wasm | **The browser publishes no telemetry.** `FBTelemetryBus` needs a sink and the browser has no file system; the loop samples every actor per tick and the call is a no-op there. Consequence, stated: every `eng_*`-style debrief row the player layer could show is unreachable in the browser and only fb-gym can produce it. |
| **5.13** | wasm | **The browser's spawn is not validated the way the runner's is.** An unresolved `/elev` after 2 s of polling falls back to `kConfigGroundM = 430 m` and the run starts anyway; a mission whose declared altitude is below ground is refused in fb-gym and flown in the browser. Both are boot-time client code, outside `FBMissionSim`. |
| 5.4 | wasm/native | No lock / TD-box HUD symbology, because `doc/modules/f16/hud-symbology.md` documents none. It will not be invented — see [`../render/hud.md`](../render/hud.md). |

### Open work (from the retired `TODO.md` §4.2)

| # | Client | Thing |
|---|---|---|
| 4.2 | native/gym | **`payerne-full` crashes under `--elev tiles`.** Three suspect areas named: z13 bilinear against the 90 m raster, the 33 m cache cell, a 503 on cold start. While this is open, the mission control loop effectively hangs on `const`/`swiss` — i.e. the loop that decides pilot-AI questions never sees real terrain. |

### Specified, not built

None open for the clients themselves. The clock (`C2`) closed in this round; its consumer (`C3`,
visual acquisition) is a `sensors/` gap, not a client one.

### Unverified

- **The phase order is no longer a comparison.** There is ONE tick body (`missions/FBMissionSim::RunPhases`)
  and both clients run it; the only phase a client places itself is STEP (`FBActorStepper`: a thread
  pool in the gym, the frame thread in the browser). What remains unverified is the browser's BOOT
  path, which is still its own code and still differs from the runner's (Gaps 5.13).

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
