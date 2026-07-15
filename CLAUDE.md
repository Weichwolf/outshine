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
| `sim/command_center/` | `cc.c` + `world3d.h` → WASM/WebGL: terrain, sky, HUD, WebCodecs video. `render_native.c` renders the same scene headless (EGL) — use it to check the view without a browser. |
| `sim/tiles/` | Container **`fb-tiles`**: the world-data service. Dynamically obtains and *prepares* real-world data and caches it, so nothing ships a preloaded region. `GET /elev?lat=&lon=` gives the engine one number — the ground height — by fetching the Terrarium DEM tile, decoding and interpolating it. Renderer tile routes (terrain/vector/imagery) come next. |
| `sim/geo/` | **Vendored** osmmesh. No external checkout. Locally extended with a per-tile byte provider so tiles can be fetched on demand — see `osmmesh/VENDORED.md` for the delta. |
| `sim/common/protocol.h` | The wire structs (telemetry / control / video). |
| `sim/test/eval.py` | The physics validation suite — ~7500 invariants per run over real flight traces. |
| `sim/test/verify.sh` | **One command, all gates**: unit+coverage, every build, the browser render, the physics suite. Use it — see "Measure the measurement" below for why it exists. |
| `sim/test/shot.sh` | Screenshots the REAL command center in headless Chromium. Replaced `render_native.c`. |
| `.claude/agents/` | The specialist team (below). |

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/  (no bundled region: ~770 KB, was 33 MB)
sim/run-podman.sh         # builds all three images and starts the stack -> localhost:8080
sim/test/verify.sh        # ALL gates: unit+coverage, builds, browser render, physics suite
sim/test/verify.sh quick  # ...without the E2E suite, for tight edit/check loops
sim/test/shot.sh out.png [osm|photo]   # look at the real app without a browser window
```

## Toolchain — it is all already installed, do not go looking

Nothing here needs installing, and nothing needs a container pulled. This list exists because a
session once spent time and a 350 MB image pull rediscovering it.

| What | Where |
|---|---|
| **emcc** (WASM) | `~/Git/emsdk` — `build-wasm.sh` finds it |
| **node 22** | `~/Git/emsdk/node/22.16.0_64bit/bin/node` (ships with emsdk) |
| **Chromium** | `~/.cache/ms-playwright/chromium-1228/chrome-linux64/chrome` — **not** on `$PATH`, so `command -v chromium` finds nothing. `test/shot.sh` uses it; override with `FB_CHROME`. |
| Playwright browsers | `~/.cache/ms-playwright/` |

Headless Chromium needs `--use-angle=swiftshader` (there is no GPU). **Not** `--use-gl=swiftshader`:
this Chrome answers that with `gl=none,angle=none`, renders a blank white canvas, and still exits
0 with a screenshot — a green-looking check of an empty page.

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
- **Measure the measurement.** Three separate checks in this repo have reported confidently about
  the wrong thing. `run-tests.sh` published :8080, the port a live stack already holds — the bind
  failed into `/dev/null`, `set -e` was absent, and `eval.py` happily measured the LIVE aircraft
  four times over while printing four model names. `TEST_MODE=1` was read by nobody, so the suite
  flew on real Open-Meteo weather and was never reproducible: `coordination(sign)` was green
  because the wind was calm, not because the physics was right. And `render_native` compiled
  `W3_TERR=24/W3_FARTEX=256` while the browser shipped `22/512`, so "the renderer's regression
  check" checked a scene nobody ran. **A green check of the wrong thing costs more than no check.**
  The suite now takes its own port (`TEST_PORT`, default 8099), fails loudly if a container does
  not start, and pins the weather (`WX_LIVE=0` + `TEST_*` knobs) so two runs are comparable.
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

- **`fb-tiles` should serve BAKED textures, not raw bytes.** Today it hands out raw vector/photo
  tiles and the browser rasterises them on every load; the "prepares" in its job description is
  not yet true. The albedo is view-independent, so it belongs on disk in the same URL shape as the
  tiles, surviving restarts — one fetch per tile per albedo instead of 6 (vector + seam
  neighbours) or 16 (photo children), and no software rasteriser in the renderer at all. Lighting
  stays in the shader: baking an albedo is not baking light.
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
