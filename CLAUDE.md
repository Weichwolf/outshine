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
| `.claude/agents/` | The specialist team (below). |

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/  (no bundled region: ~770 KB, was 33 MB)
sim/run-podman.sh         # builds all three images and starts the stack -> localhost:8080
sim/test/run-tests.sh     # or: HOST=127.0.0.1:8080 python3 sim/test/eval.py
```

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

- **Aerial-imagery ground texture** — `fb-tiles` already serves the photos (`/t/imagery`); what is
  left is using them as albedo with the baked-in illumination flattened out, lit by our own sun
  (the per-pixel lighting foundation is in place).
- **Modularisation** — `xp_bridge.c` and `world3d.h` are god files; the ownership boundaries above
  are still artificial because of it.
