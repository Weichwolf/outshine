# FlightBox

Simulated FPV flying-wing control system. All C. Real iNav firmware in the loop, a WASM browser
"command center", live 3D terrain from real OSM/DEM data, live weather.

**Chain:** `control → iNav (REAL SITL) → FDM → telemetry → renderer`. Alongside, **`fb-tiles`**
serves real-world data to engine (elevation) and renderer (terrain/map/imagery) — fetched and
cached on demand, never preloaded.

## Three facts that surprise everyone

1. **No real X-Plane.** `sim/aircraft/xp_bridge.c` **is** the flight model; it only *speaks* the
   X-Plane UDP protocol (:49000) so real iNav SITL connects to it. Aerodynamics/wind/thermals are ours.
2. **The renderer is a pure consumer** — displays telemetry, cannot influence flight. A render
   freeze looks exactly like a flight kick (view stalls, then snaps to the advanced pose); check which.
3. **iNav runs truth-attitude** (`--sim=xp`): reads attitude/rates from our DREFs, not sensors. It
   will fly a physically impossible state if we feed one — it can't detect our modelling errors, the
   physics suite has to.

## Inventory

| Path | What |
|---|---|
| `sim/aircraft/` | **`fb-aircraft`**: iNav 9.1.0 SITL + `xp_bridge.c` (FDM, atmosphere, ephemeris, MSP, autopilot, telemetry). `inav-patches/` = our iNav fixes. |
| `sim/flightbox/` | **`fb-flightbox`**: HTTP/WebSocket server + built WASM under `web/`. |
| `sim/command_center/` | `cc.c` + `world3d.h` → WASM/WebGL: terrain (chunked-LOD quadtree), sky, HUD, WebCodecs video. No second renderer — `test/shot.sh` screenshots this one. |
| `sim/tiles/` | **`fb-tiles`**: world-data service. `GET /elev?lat=&lon=` → ground height. `/t/<kind>/z/x/y` raw tiles, `/bake/<osm\|photo>/z/x/y?tex=` baked albedo. |
| `sim/geo/` | **Vendored** osmmesh + per-tile byte provider. See `osmmesh/VENDORED.md`. |
| `sim/common/protocol.h` | Wire structs (telemetry/control/video). |
| `sim/test/eval.py` | Physics suite — ~7500 invariants/run over real traces. |
| `sim/test/verify.sh` | **All gates**: unit+coverage, builds, browser render, physics. `quick` skips E2E. |
| `sim/test/shot.sh`+`shot.js` | Screenshots real CC in headless Chromium. Waits streamer's `0 pending`; exit 2 = shot an unfinished world. |
| `sim/test/pngstat.py` | Is there ground on a screenshot? Stdlib PNG reader, "beats blue by a margin". |
| `sim/test/bench*`+`baseline.json` | Regression net. **Counters, not times.** Records measured WASM sha256; `bench_stack.sh cold` = empty volume. |

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/  (~770 KB, no bundled region)
sim/run-podman.sh         # builds 3 images + starts stack -> localhost:8080
sim/test/verify.sh [quick]
sim/test/shot.sh out.png [osm|photo] [WxH] [timeout-s]
python3 sim/test/pngstat.py out.png
```

- After `xp_bridge.c`: rebuild **aircraft**. After renderer: rebuild WASM **and** **flightbox** (WASM baked in).
- **Restart containers together**, `fb-tiles` first (aircraft asks it for home elevation at startup).
- DEM/tile cache in `fbtiles-cache` volume; upstream hit once per tile ever.
- `ORIGIN_LAT`/`ORIGIN_LON`/`TILES_URL` reach the browser via `/config.js`.
- `printf` from WASM → **browser console**, not container log.

## Toolchain — already installed, do not go looking

| What | Where |
|---|---|
| **emcc** | `~/Git/emsdk` — `build-wasm.sh` finds it |
| **node** | on `$PATH` (v20) |
| **playwright** | global; `require('playwright')` resolves |
| **Chromium** | `~/.cache/ms-playwright/chromium-1228/...` — NOT on `$PATH`; `shot.sh` pins it via `FB_CHROME` |

Headless Chromium needs `--use-angle=swiftshader` (no GPU). **Not** `--use-gl=swiftshader` (renders
blank white, still exits 0). Fixed in `shot.js`, do not undo: never `--virtual-time-budget` (fast-
forwards page clock while fetches run in real time → shoots empty worlds); pin the Chromium path;
`shot.js` swallows the distro `rimraf` crash after the shot is saved.

## Process — the rules, each earned by a bug

- **Measure before theorising.** Every long debugging failure here began with a confident theory.
- **Measure the measurement.** Green checks of the wrong thing have cost more than no check: a suite
  bound to `:8080` measured a live stack; a screenshot tool photographed empty worlds and a
  predicate scored them "real terrain". Before believing a checker, **watch it fail** (mutate it,
  see the check bite).
- **Artifact identity, or it's not a result.** Two agents on one box measure each other. Pin the
  artifact, record its sha256; `set -e` does NOT catch a container that failed to start (its
  `podman run` exits 126, but curls to a *foreign* server on that port still return 0 — and bash
  disables `set -e` inside `$(...)` unless `shopt -s inherit_errexit`). "Fresh volume ⇒ cache_hits=0"
  tests warmth, not ownership.
- **No number without a measurement; no criterion without a feasibility check.** A pre-registered
  number feels rigorous even when guessed. A hedge beside a claim covers the provable half while the
  invented half rides along — put the hedge in front.
- **Space telemetry by `seq`, never arrival time** (packets arrive batched → Δ/dt explodes).
- **Measure the input as well as the output** — the worst bug ever (iNav −3.0 yoke = 3× aileron) was
  invisible because only attitude was measured, never the command.
- **No hot loop in the renderer.** SVS = 0.8% of one core; the environment is only stable under
  saturation. Refactor for structure/correctness, not performance. (EVS costs a core only because
  SwiftShader decodes H.264 in software — never quote it as real cost.)
- **Frame rate here is sign-inverted** (idle page paced ~131 ms, frame costs 0.5 ms → adding work
  raises fps). Diagnostic only, never a gate. Gates: pngstat's verdict + `0 pending`, never a percentage.
- **`grep` finds the text, not the thing** (tombstone comments; `grep -c` on minified JS counts
  lines). `pgrep -f "x.sh"` matches the waiter grepping for it.
- **Comments: only what code can't say** (a constraint, a unit, a why). Code is the truth; a comment
  that restates it only diverges. Prefer deleting to explaining.
- **No regression:** physics suite stays green. `coordination(sign)`/`coord-turn-rate` encode "can't
  turn one way while banked the other" — the worst bug we had.
- Commit messages / comments in German or English as the file already uses.

## The specialist team (`.claude/agents/`)

They consult via `SendMessage`. **Ownership is social — concurrent edits to one file clobber.**
Coordinate before editing outside your area.

| Agent | Owns |
|---|---|
| `selig-fdm` | `physics_step` + aero constants |
| `inav-firmware` | `sim/aircraft/*`, protocol & MSP |
| `renderer-gfx` | `sim/command_center/*` |
| `geo-mapdata` | `sim/geo/*` |
| `verify-measure` | `sim/test/*`, measurement rigour |

## Cold regions & foreign origins

- **Cold cache converges** (Hameln empty volume: `0 pending` after ~432 s vs ~15 s warm, verified at
  pixels). Root cause was `404` meaning both "queued" and "none" → browser cached holes forever;
  fixed server→`202`, browser→only 200/204 terminal.
- **Foreign origin still unproven.** Aoraki ground rather than stalled at 300 s — but the fetch path
  has since changed (libcurl + pool), so that figure describes a build that no longer exists; nobody
  re-ran it. `/elev` still seeds Hameln's ~71 m over foreign ground. Don't restore "any origin works".
- **Poles: renderer and server disagree.** `world3d.h`'s `w3_geo_to_tile_f` and `tiles/tilemath.h`'s
  `fb_geo_to_tile` are the same formula but the server clamps lat to ±85.0511, the renderer doesn't →
  origin beyond ±85.0511 renders nothing while fb-tiles serves clamped pole tiles. Fix is ONE shared
  definition (probably `common/`, like `protocol.h`), not a second clamp — clamping rounds to the
  nearest *wrong* tile. Honest polar behaviour: refuse at startup. Touches three owners.

## fb-tiles — done, details in code

- **Fetch path rebuilt**: libcurl with a kept thread-local handle (`http.h`), 4-thread prefetch pool
  (4 = politeness toward upstream, NOT a measured optimum — the earlier "2.89x saturates at 4" was a
  harness artefact, falsified). `/t/` answers 200/202/204 from `fb_tile_state{READY,UNKNOWN,ABSENT}`.
- **Negative cache**: a 404 writes a `.absent` marker (own file — a 0-byte tile would read as
  "not yet"), 30-day TTL (nobody caches negatives forever; unmapped land is what later gets mapped).
  The 204's `max-age` must equal the TTL.
- **Esri never 404s** — serves a 2521 B placeholder (`9eafd300`) with 200 above its coverage, which
  we used to cache as ground. Coverage is local (Hameln z19, Sahara z18, Patagonia z17, Antarctica
  <z16), so `maxz` can't be one global constant. Fixed by Esri's own tilemap oracle
  (`tilemap.{h,c}`, 1024 tiles/request). Trap: `"adjusted":true` — a 32×32 request returns a smaller
  rectangle; read `location`, never assume your own request. Oracle `-1` = UNKNOWN → must fetch
  (absence positively established or "no answer" silently becomes "no tile").
- **Nobody upstream ever answers "no data"**: ocean is a Shortbread polygon (200), holes are unmapped
  LAND, Terrarium fills no-data with 0 = sea level. So `/bake` over an ABSENT vector tile bakes the
  base ground fill; `bake_photo` overzooms to the deepest zoom the oracle confirms.
- **Open**: attribution missing entirely (ODbL "OpenStreetMap Contributors" + Esri credit are license
  obligations, belong in HUD → `renderer-gfx`); UA has no contact (user's call); the prefetch 3×3
  warm is technically "bulk downloading" per OSM policy but is the allowed "modest look-ahead".

## Open work — renderer & measurement

- **Gates only test warm.** `verify.sh` 4/4 green on Hameln while a cold region loads nothing. A cold
  gate needs the empty volume, not a foreign continent — writing one before the fix is green cements
  today's state as the expectation.
- **`pngstat` is a colour predicate.** Night fixed (`e2d7dad`: margin scales with pixel brightness);
  rock/snow are neutral and under-report, so an alpine origin can score empty. Needs a structure
  criterion + a real rock screenshot, not a third recalibration.
- **`day` exists twice, two formulas**: `atmo.h` linear in degrees `clamp((sun_el+6)/12)`, sky shader
  `smoothstep(-0.12,0.10,sinEl)` — 13 points apart at sun_el +3°. Sky and ground disagree about when
  it's day. May be deliberate; nobody decided. Unifying is a VISUAL judgement, needs a human.
- **Terrain LOD budget doesn't fit**: `128 chunks (budget 128), 0 pending`, but 31 want split, 10-13
  over budget — `W3_BUDGET` limits detail, not screen-space error. Raise budget or spend it better.
  `W3_TEX=512` = 2.94 m/texel at z14; matching the old 1.47 needs z15 (Shortbread maxz=14 lacks it).
- **DONE: texture ramp `tile.tex[mode][lod]`, progressive by screen size (`bad398a`).** 256 floor
  shown immediately, climbs stage by stage to the SSE target; 2048 free for both modes (hardware
  cap only). `lod` is part of the cache key, release only in `cache_trim`, hold by distance NOT
  frustum (turning is instant — a turned-away near chunk keeps its texture; only receding frees).
  VRAM self-regulating (937 MB normal / 1670 adversarial, under the 2 GB the user set).
  - **The proof METAMORPHOSED, and that is the honest part.** Progressive loading eliminated the
    thrash PATH structurally (all chunks climb from the shared 256 floor, so the walk/children_ready
    size-alternation is gone). So the NOKEY mutation no longer thrashes — it STALLS: without
    side-by-side slots a chunk can't hold the shown 256 and load the 512, so 185 chunks freeze at
    the floor, never sharp. `generateMipmap` p50 is 0 in ALL variants now (with slots: cache hit;
    NOKEY: it stalls instead of rebaking) — so p50 is a dead indicator here; the live one is
    "sharpening count" (NOKEY 185 stuck vs slots 0). The mutation still bites; the slots are
    load-bearing for the climb, not just anti-thrash. Test what you ship: proving the old 480/frame
    thrash would test the non-progressive code we don't ship.
  - **2048-OSM is justified — and my first "no detail" measurement was WRONG, a caught mistake.**
    I measured area-average / HF-energy, which miss EDGE sharpness — exactly where vector rasterising
    wins. Correct: MVT extent = 4096 (finer than a 2048 texture), max-gradient identical at 1024 and
    2048 → OSM rasterises sharper road edges at 2048, real gain, not upscaling. The user caught it
    ("OSM is vector, scales infinitely"). Lesson: a frequency/area mean is blind to edge sharpness.
- **`sky.h`/`hud.h` split hides a `w3_atmo`.** `world3d_render_scene` computes `sun/moon/day/cloud/
  haze/light` as locals that the terrain AND building passes consume — per-frame shared state with no
  owner. Pull out `w3_atmo` + pure `w3_atmo_from(telem,have)` (natively testable, every pixel hangs
  off it) before cutting sky. Two more couplings: star pass reads `w3_olat/w3_olon` (tile side), HUD
  reads `w3_ground_mode` (cache side). A wrong star position looks plausible — verify origins ARRIVE
  by printing them, a screenshot won't tell you.
- **`w3_avail{READY,PENDING,ABSENT}`**: server half done (204 producer exists now). Renderer's
  `w3_bake` `if(n<=0)` + `photo_none` remain — `204`→`Uint8Array(0)` means `n==0` finally means
  something. `renderer-gfx`.
- **Tile work is off-thread (worker), but a POOL is NOT the fix — on any hardware.** `2e682c4` moved
  fetch+decode+mesh to one Web Worker: frame p95 during streaming 752→27 ms, the reported load
  stutter gone. Warm convergence is ~16-28 s (high variance). Work is 72% fetch-wait, so a 4-worker
  pool was built and measured: it doubles throughput (2.05×) but a MOVING camera makes fast workers
  build ~29% more tiles (165 for 128) that `trim` discards — a **hardware-independent** churn that
  cancels the throughput, so the naive pool nets nothing. (Also unmeasurable here: headless
  swiftshader is software GL on 4 cores, the main thread contends; one worker and four are both
  16-28 s, indistinguishable.) Not committed; documented at `w3_worker_init`. Real fix if load time
  ever matters: bound the in-flight set (nearest N by distance) so camera motion does not churn —
  unbuilt, target "beats one worker", only half-measurable on swiftshader. `renderer-gfx`.
- **1c — albedo off-thread.** The worker carries the mesh (76%); the albedo decode (24%, stbi +
  glTexImage2D) is still synchronous in `w3_bake`, which is why p95 is 27 ms not <16. Move it to
  `createImageBitmap` (browser decodes off-thread, feeds `texImage2D` directly). Measurable HERE
  (p95, not convergence — no confound). `renderer-gfx`.
- **Aerial photo: flatten baked-in illumination.** 78-90% of detail is luma, chroma std 5.6/255 (so
  "keep UV drop Y" is dead). Homomorphic flatten `Y/lowpass(Y)` (sigma ~50 m ≈ DEM quad) removes the
  terrain gradient but not hard building shadows.
- **Night lights**: bake OSM streets into texture alpha as emissive.
- **Modularisation**: `xp_bridge.c` and `world3d.h` still god files. Out (each 100% covered):
  `fdm/{ephemeris,atmosphere}`, `terrain`, `gfx/{mat4,style}`, `tiles/{lru,prefetch}`, `atmo/camera/
  stars/chunkmesh`.
- **`coordination(sign)` is weather-sensitive**: at `TURB=1.0` ~3% fail — `steady = abs(roll_rate)<12`
  admits adverse-yaw transients its own comment excludes. Retuning changes what the invariant means;
  don't do it quietly.
