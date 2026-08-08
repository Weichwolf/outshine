# Build, gates and operation

Recipes live in the Makefile, not in agents' heads. This file says which targets exist, what counts as a
proof, and what is special about this machine.

## Spec

What a proof is, and what has to hold before a change counts as verified.

| Gate | Must hold |
|---|---|
| Warnings = errors | all targets clean under `-Wall -Wextra -Wpedantic -Werror` |
| Tree congruence | `verify-trees`, and **every** orphan count it prints is read |
| Layer + count gate | `verify-layers` and `verify-clients` green, **and the numbers they print are read** — a gate whose output nobody reads is not a gate |
| Frame proof | build-effective changes need a rendered frame **or** a numerical measurement |
| Regression | every measured deviation justified individually. **No subject today** — see `## Gaps` |
| Determinism | `--threads 1/2/4` × repetitions produce a single signature |
| WASM | `make -C sim wasm` builds and the app boots in the browser — the one client used daily; a broken boot is more expensive than any other defect |
| vendor read-only | nothing under `sim/vendor/` is modified |

Two rules about how measuring is done at all: **every number carries its provenance**
([`conventions.md`](conventions.md)), and measurements run through the **control loop** (telemetry),
never through single observations.

## State

**Most gates have nothing to run against.** The library does not link
([`architecture.md`](architecture.md) §State) and `mods/` is empty, so the regression gate, the
determinism gate and every harness have no subject. The gates that still hold their meaning today are
the ones that read the *tree* rather than a run:

| Gate | Runnable today |
|---|---|
| `verify-layers`, `verify-clients`, `verify-trees`, `verify-types` | yes — they read source and directories |
| Warnings = errors | for `walk`, `wasm`, `worker` and `tiles/` |
| everything that needs a scenario | **no** |

`verify-layers` is green. `verify-trees` reports 9 orphans and `verify-types` 28 mentions; both are the
gate working, and what each is red about is in `## Gaps`.

## Gaps

| Thing | Where it is tracked |
|---|---|
| The Makefile's own header comment still describes deleted targets and a deleted dependency | this file; it is `src/`, and it is fixed with the round that fixes the targets |
| **The regression and determinism gates have no subject.** There is no scenario corpus and no headless runner to produce one. `determinism.py` covers only the render side — repeated runs of one command — not `--threads 1/2/4` over a simulation | this file |
| **`verify-trees` reports orphans it cannot resolve**: `sim/test/` is empty, and its runnable-proof check for `mods/` was removed because it demanded a deleted format | [`mods.md`](mods.md) `## Gaps` |

## Knowledge

### Make targets

Every project carries its own Makefile. `make help` prints the live list; this table is what each one is
*for*.

#### `sim/`

| Target | Result |
|---|---|
| `walk` | `build/gpu_walk` — **the pedestrian frame oracle**: `render/` + `world/` + the core value translation units those two reference |
| `wasm` | `web/gpu.js` + `web/gpu.wasm` — **depends on `worker` and always builds both** |
| `worker` | `web/fbtileworker.js` + `.wasm` — callable on its own |
| `image`, `up`, `down`, `restart` | container build and lifecycle; the image compiles `src/clients/SimHost.cpp` and nothing else |
| `verify-layers` | `sim/src` is a stack; every `#include` points down it, as a machine-checked matrix |
| `verify-clients` | **one program, two translations.** An entry point includes nothing of `render/` or `world/` and names no peer; each `main()` is under 40 lines; the scene-building renderer calls exist in exactly one translation unit, plus a printed list of declared subject-bench exceptions. The gate the shared source list could not be: a green `make wasm` proved the browser COMPILED the forest code it did not have |
| `verify-types` | how much `sim/src/` still knows about concrete named types, by cost class. **rc=1 until the count is 0** |
| `verify-trees` | `doc/`, `sim/src/` and `sim/test/` carry the same directory tree; every `mods/<id>/` carries `doc/` plus a runnable proof |

If the tile worker is missing, the WASM app hangs silently at startup (a 404 in the worker). Hence the
fixed dependency instead of two separately memorised targets.

#### `tiles/`

`build` | `image` | `run`

### The committed measurement tools

Not build targets, and deliberately so — they are analysis, not product.

| Tool | What it does |
|---|---|
| `sim/tools/walkbench.py` | **the performance instrument.** Frame-time distribution of a MOVING camera over several hundred frames and several speeds — p50/p95/p99, never a mean ([`goal.md`](goal.md)) |
| `sim/tools/determinism.py` | runs one render command N times, groups results by md5 and quantifies the spread |
| `sim/tools/skylinedev.py` | **the silhouette instrument.** The skyline row per column at 1280×720, taken from the DEPTH buffer for a render (reversed-Z, sky is exactly 0 — no threshold, hence no haze) and from a per-column knee detector for a photo, whose own error is calibrated against that depth truth. `--calib` reports what the detector can do before `--photo` is allowed to mean anything |
| `sim/tools/meshdev.py` | what the drawn mesh throws away: the deviation per SOURCE DEM texel against the surface `ChunkBuildEcef` spans, max and RMS, per `kGrid` and per camera. CPU only, no GPU, no client |
| `sim/tools/tileproxy.py` | a recording/replaying proxy in front of fb-tiles, so a render can be made **hermetic**: `--record` keeps the bodies, `--replay` answers only from the corpus |
| `sim/tools/aa_metrics.py` | the **antialiasing instrument**: `edges` (share of neighbour pairs jumping a full contrast), `spans` (blade crossings of the sky by width plus the enclosed-fragment count), `laplace` (mean second difference per distance BAND out of the depth buffer), `ref` (RMSE and three spectral bands against a supersampled ground truth), `ghost`, `shift`. Every definition sits at the function that computes it |
| `sim/tools/elastica.py` | the blade's own bending equation, solved — Gosselin, de Langre & Machado-Almeida (2010), JFM 650:319-341 eq. (5.5)/(5.7) — and the closed form `GroundCoverStage` bakes |
| `sim/tools/wind_crest.py` · `wind_fixed.py` · `wind_probe.py` | the wind wave measured three ways: the crest tracked across a rendered sequence in metres of world and seconds; whether the wave is over the GROUND or over the SCREEN, in millimetres; and the same `(x, t)` spectrum off the state channel instead of off pixels |
| `sim/tools/vote.py` | pairwise comparisons for `sim/web/vote` — builds the manifest, evaluates the votes |
| `sim/tools/browser_shot.cjs` | one frame and the console out of the deployed WASM app; the instrument behind every "measured in the browser" claim. Needs `make wasm` + `sim/up.sh` |
| `sim/tools/strip_comments.py` | the comment-reduction check: its hash over `sim/src/` is what proves a round did not smuggle prose back into the code |

### Measurement discipline

- Measurements run through the **control loop** (telemetry), not through single observations.
- Target GPU capabilities: `doc/webgl-webgpu-report.txt`.
- A frame proof is a *reproducible* frame: hold the camera, let the streamer reach `pending=0`, keep the
  last frame. A shot taken mid-stream measures the loader, not the renderer.

### The control loop

The way of working, and today it has exactly one shape because there is no headless runner:

```
declare the scene  →  render  →  measure the frame or the numbers mechanically  →  correction  →  loop
```

**Every measurement pins its binary** — copy, hash, render from the copy. A figure taken from
`build/gpu_walk` directly is not a measurement ([`goal.md`](goal.md)). Performance is a distribution
over a moving camera: p50, p95, p99 and the trend, never a mean and never a minimum. Stutter is a p99
event.

The headless runner, its exit-code vocabulary and its per-actor telemetry files were deleted with the
simulation layer on 2026-08-07. `TelemetryBus`'s own append rule survives and still binds whatever
registers next: new sources are **always appended at the end**, so no measured column loses its
position ([`core.md`](core.md) §1.2).

### Host and operation (this machine)

No hidden agent memory — all operational knowledge is here.

| Item | State |
|---|---|
| emsdk | `~/Git/emsdk` |
| `nproc` shim | `~/.local/bin` (macOS has no `nproc`) |
| Containers | podman VM first (`podman machine start`), then `tiles/up.sh` (:8081) and `sim/up.sh` (:8080) |
| Live mount | fb-sim mounts `sim/web` live — `make wasm` takes effect on refresh |
| WASM artefacts | gitignored |
| Native builds | need `sim/vendor/.compat-headers` (gitignored, host-local) |
| Git | commit mail is the GitHub noreply alias; push via SSH `insteadOf` |
| `timeout(1)` | **does not exist on macOS** — do not build it into scripts |

`make wasm` preloads `mods/demo/scene.json` into Emscripten's virtual filesystem, so the browser and the
native oracle read the **same file** and no second copy is hand-kept. The general two-mount rule a
multi-title build needs is in [`mods.md`](mods.md) §2.1 and is not exercised today.
