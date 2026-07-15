# FlightBox

Simulated FPV flying-wing control system. All C. Real iNav firmware in the loop, a WASM browser
"command center", live 3D terrain from real OSM/DEM data, live real-world weather.

**The chain:** `control input → iNav (REAL firmware, SITL) → FDM → telemetry → renderer`
Alongside it, **`fb-tiles`** serves real-world data to both the engine (ground elevation) and the
renderer (terrain/map/imagery) — dynamically fetched, prepared and cached, never preloaded.

## Three facts that surprise everyone

1. **There is no real X-Plane.** `sim/aircraft/xp_bridge.c` **is** the flight model. It merely
   *speaks* the X-Plane UDP protocol (DREF/RREF :49000) so real iNav SITL connects to it as if it
   were X-Plane. So the aerodynamics, wind, turbulence and thermals are all ours.
2. **The renderer is a pure consumer.** It only displays telemetry and cannot influence the flight —
   the simulation runs fine with no browser attached. A render freeze *looks* exactly like a flight
   kick (the view stalls, then snaps to the meanwhile-advanced pose), so always check which it is.
3. **iNav runs in truth-attitude mode** (`--sim=xp`): it reads attitude/rates straight from our
   DREFs rather than deriving them from sensors. It will happily fly a physically impossible state
   if we feed it one — it cannot detect our modelling errors. Our physics suite has to.

## Inventory

| Path | What |
|---|---|
| `sim/aircraft/` | Container **`fb-aircraft`**: real iNav 9.1.0 SITL + `xp_bridge.c` (FDM, atmosphere, ephemeris, MSP client, autonomous autopilot, telemetry downlink). `inav-patches/` carries our fixes to iNav itself. |
| `sim/flightbox/` | Container **`fb-flightbox`**: HTTP/WebSocket server + the built WASM under `web/`. |
| `sim/command_center/` | `cc.c` + `world3d.h` → WASM/WebGL: terrain (chunked-LOD quadtree), sky, HUD, WebCodecs video. There is no second renderer — to look at the view without a browser window, `test/shot.sh` screenshots this one. |
| `sim/tiles/` | Container **`fb-tiles`**: the world-data service. Dynamically obtains and *prepares* real-world data and caches it, so nothing ships a preloaded region. `GET /elev?lat=&lon=` gives the engine one number — the ground height — by fetching the Terrarium DEM tile, decoding and interpolating it. Renderer tile routes (terrain/vector/imagery) come next. |
| `sim/geo/` | **Vendored** osmmesh. No external checkout. Locally extended with a per-tile byte provider so tiles can be fetched on demand — see `osmmesh/VENDORED.md` for the delta. |
| `sim/common/protocol.h` | The wire structs (telemetry / control / video). |
| `sim/test/eval.py` | The physics validation suite — ~7500 invariants per run over real flight traces. |
| `sim/test/verify.sh` | **One command, all gates**: unit+coverage, every build, the browser render, the physics suite. Use it — see "Measure the measurement" below for why it exists. |
| `sim/test/shot.sh` + `shot.js` | Screenshots the REAL command center in headless Chromium (playwright). Replaced `render_native.c`. Waits for the streamer's own "0 pending", never a sleep — exit 2 = it shot an unfinished world, which is not a verdict on the renderer. |
| `sim/test/pngstat.py` | Decides whether a screenshot has ground on it. Stdlib-only PNG reader; the predicate is "beats blue by a margin", because sky and its haze never do and vegetation always does. |
| `.claude/agents/` | The specialist team (below). |

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/  (no bundled region: ~770 KB, was 33 MB)
sim/run-podman.sh         # builds all three images and starts the stack -> localhost:8080
sim/test/verify.sh        # ALL gates: unit+coverage, builds, browser render, physics suite
sim/test/verify.sh quick  # ...without the E2E suite, for tight edit/check loops
sim/test/shot.sh out.png [osm|photo] [WxH] [timeout-s]   # look at the real app, no browser window
python3 sim/test/pngstat.py out.png    # ...and get a second opinion on whether it has ground
```

## Toolchain — it is all already installed, do not go looking

Nothing here needs installing, and nothing needs a container pulled. This list exists because a
session once spent time and a 350 MB image pull rediscovering it.

| What | Where |
|---|---|
| **emcc** (WASM) | `~/Git/emsdk` — `build-wasm.sh` finds it |
| **node** | `node` is on `$PATH` (v20). A second one ships with emsdk (`~/Git/emsdk/node/22.16.0_64bit/bin/node`) — irrelevant, `build-wasm.sh` finds its own. |
| **playwright** | installed globally; `require('playwright')` just resolves. |
| **Chromium** | `~/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome` — **not** on `$PATH`, so `command -v chromium` finds nothing. `shot.sh` pins it and exports `FB_CHROME`. |

Headless Chromium needs `--use-angle=swiftshader` (there is no GPU). **Not** `--use-gl=swiftshader`:
this Chrome answers that with `gl=none,angle=none`, renders a blank white canvas, and still exits
0 with a screenshot — a green-looking check of an empty page.

Two traps that already cost a session each, both fixed in `shot.js` — do not undo them:
- **Never `--virtual-time-budget`.** It fast-forwards the *page's* clock while the tile fetches run
  in real seconds, so it does not give streaming time, it takes it away and then shoots. Wait for
  the streamer's `0 pending` instead.
- **Pin the Chromium path.** The installed playwright asks for a revision it did not install (1.38
  wants `chromium-1080`, the box has 1228) and answers `npx playwright install` — a browser
  download to run a check that already has a browser.
- The distro playwright throws `rimraf: callback function required` out of `close()`, after the
  shot is safely on disk. `shot.js` swallows that one deliberately; a good screenshot exiting 1
  gets ignored just as fast as a bad one exiting 0.

- After changing `xp_bridge.c`: rebuild the **aircraft** image. After changing the renderer:
  rebuild the WASM **and** the **flightbox** image (the WASM is baked in).
- **Restart the containers together** — the aircraft caches the flightbox address and asks
  `fb-tiles` for home's elevation at start-up, so start `fb-tiles` first.
- The DEM/tile cache lives in the `fbtiles-cache` volume: upstream is hit once per tile, ever.
- `ORIGIN_LAT`/`ORIGIN_LON` set the shared home and reach the browser via `/config.js`, as does
  `TILES_URL`. **Any origin on earth works** — nothing is preloaded, every tile is fetched.
- `printf` from the WASM goes to the **browser console**, not the container log.

## Process

- **Measure before theorising.** Every long debugging failure here began with a confident theory.
- **Measure the measurement.** Five separate checks in this repo have reported confidently about
  the wrong thing. `run-tests.sh` published :8080, the port a live stack already holds — the bind
  failed into `/dev/null`, `set -e` was absent, and `eval.py` happily measured the LIVE aircraft
  four times over while printing four model names. `TEST_MODE=1` was read by nobody, so the suite
  flew on real Open-Meteo weather and was never reproducible: `coordination(sign)` was green
  because the wind was calm, not because the physics was right. And `render_native` compiled
  `W3_TERR=24/W3_FARTEX=256` while the browser shipped `22/512`, so "the renderer's regression
  check" checked a scene nobody ran. **A green check of the wrong thing costs more than no check.**
  The suite now takes its own port (`TEST_PORT`, default 8099), fails loudly if a container does
  not start, and pins the weather (`WX_LIVE=0` + `TEST_*` knobs) so two runs are comparable.
- **The replacement inherits the disease — twice, from the same hand, on the same afternoon.**
  `shot.sh` was written to kill `render_native` for checking a scene nobody ran, then used
  `--virtual-time-budget` under a comment claiming it "gives the tile streaming time to actually
  land". It does the opposite, so it photographed empty worlds. And `pngstat.py` scored those
  photographs `OK — real terrain on screen`, because "not strongly blue" counted the horizon haze,
  the title bar and the HUD text as ground. Together they produced a confident, reproducible,
  two-tool, false report that the renderer was broken — while the renderer drew 128/128 chunks in
  the user's browser the whole time. **Before believing a checker, watch it fail.** Both now do:
  the three known-empty screenshots score 0 % ground, the real one 40 %.
- **One renderer.** There is no second renderer to keep in sync — `render_native.c` was deleted
  because it drifted. The visual check is a headless browser on the real artifact (`test/shot.sh`).
- **Space telemetry samples by `seq`, never by arrival time** — packets arrive batched, so `Δ/dt`
  explodes into nonsense. This single mistake has produced two wrong diagnoses.
- **Measure the input as well as the output.** The worst bug in the project's history (iNav emitting
  a −3.0 yoke ratio = 3× full aileron) was invisible for a long time because only the aircraft's
  *attitude* was ever measured, never the *command* driving it.
- **No regression:** the physics suite must stay green. `coordination(sign)` and `coord-turn-rate`
  encode "a real aircraft cannot turn one way while banked the other" — the worst bug we ever had.
- Commit messages and code comments are in German/English as the file already uses; comment *why*,
  not *what*.

## The specialist team (`.claude/agents/`)

They know each other and consult via `SendMessage`. **Details live with them — this file stays an
overview.**

| Agent | Domain | Owns |
|---|---|---|
| `selig-fdm` | Flight dynamics & atmosphere | `physics_step` + aero constants |
| `inav-firmware` | iNav, SITL↔X-Plane bridge, MSP, mixer, PIDs, real HW | `sim/aircraft/*`, protocol & MSP layer |
| `renderer-gfx` | GLSL/WebGL, lighting, sky, tiles, video, HUD | `sim/command_center/*` |
| `geo-mapdata` | osmmesh, PMTiles/MVT, DEM, projections, imagery | `sim/geo/*` |
| `verify-measure` | Measurement rigour, the physics suite, falsification | `sim/test/*` |

File ownership is enforced socially, not by tooling: **concurrent edits to the same file clobber
each other** — this has actually happened. Coordinate before editing outside your area.

## Open work

- **Terrain LOD: the quadtree runs, the budget does not fit.** Measured warm, both grounds:
  `128 chunks drawn (budget 128), 0 pending`, `levels z8..z14 = 32/8/12/9/21/22/24`, streamed in
  10–20 s. But `31 wanted split, 10–13 over budget` — the tree wants finer ground than 128 chunks
  can hold, so `W3_BUDGET` is currently what limits detail, not the screen-space error. Decide
  whether to raise the budget or to spend it better (the SSE is computed per node from
  `w3_terr_vbo`'s real decimation error, so the ordering is honest — there is just not enough
  room). Related: `W3_TEX=512` gives 2.94 m/texel at z14 against 1.47 from the old near ring;
  matching it needs z15, which Shortbread (maxz=14) does not have. `W3_TEX 1024` doubles VRAM.
- **Aerial photo: flatten the baked-in illumination.** TAB switches the ground between the OSM
  render (SVS) and the real photo (EVS) — today the photo is raw, shadows and all. Measured on a
  real Esri tile: 78–90 % of the detail is in luma, chroma std is 5.6/255, so the old
  "keep UV, drop Y" idea is dead (neutral-grey roads would vanish). Homomorphic flatten
  (`Y / lowpass(Y)`, sigma ~50 m ≈ our DEM quad size) removes the terrain gradient but NOT hard
  building shadows — those are high-frequency and survive.
- **Night lights** — bake the OSM street layer into the texture's alpha as emissive; physically
  honest (lamps *are* emissive) and makes the synthetic view useful at night.
- **Modularisation** — `xp_bridge.c` (771) and `world3d.h` are still god files. Out so far, each
  100 %-covered: `fdm/{ephemeris,atmosphere}`, `terrain`, `gfx/{mat4,style}`, `tiles/{lru,prefetch}`.
- **`coordination(sign)` is weather-sensitive, and that is a decision for a human.** With the
  weather pinned at `TURB=1.0` about 3 % of samples fail — not broken physics, but
  `steady = abs(roll_rate) < 12` letting through exactly the adverse-yaw transients its own
  comment says to exclude (a wing rolling at 11°/s is not in steady flight). Tightening the filter
  or picking a calmer test wind both change what the invariant means. Do not quietly retune it.
