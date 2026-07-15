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
| `sim/test/bench*` + `baseline.json` | The regression net. **Counters, not times** — see below for why. Records the sha256 of the WASM it actually measured and a `not_measured` list, so a number cannot outlive its caveats. `bench_stack.sh cold` gives fb-tiles an EMPTY volume, which is the only repeatable way to test a cold region: testing a real origin warms it. |
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
  `TILES_URL`.
- **A COLD CACHE now converges — proven. A FOREIGN ORIGIN is still not.** Hameln on an empty volume
  (`bench_stack.sh cold`, verified 0 files before the run): `0 pending, 0 waiting` after **432 s**,
  against ~15 s warm — a factor of 29. Verified at the pixels, not just the counters: a fully
  rendered night landscape. That settles the 404 fix below. What it does NOT settle is a foreign
  origin: Aoraki was never given enough time (111/128 at 300 s, and Hameln alone needs 432), and
  `/elev` still seeds the wrong home elevation there — the Hameln run had no confound only by luck,
  because the compiled-in seed (71.0 m) *is* Hameln's ground. A proof that works because the fixture
  happens to match the fallback is not a proof of the general case.
  - **The gate criterion is `0 pending && 0 waiting`, never a chunk count.** `W3_BUDGET` is a
    ceiling, not a target: a complete cut through the tree can be any number under it, depending on
    pose (this run: 126). `shot.sh` already waits on exactly that. Declaring "128 chunks" the
    criterion made a budget constant into a goal the code never used.
  - **`pngstat` is a DAYLIGHT predicate.** It called this run `SUSPECT — scene looks empty` at 1 %
    ground, while the screenshot shows fields, a settlement and stars. `SUN -14`: it was night, and
    `uLight = 0.20 + 0.80*day` dims the world to 20 %. Not rock and snow — **the clock**. Any cold
    run after sunset fails the gate. "Vegetation always beats blue" is an assumption about daylight
    sold as a statement about the renderer.
  - Honest throughput, from the first properly defined window: **5.9 prefetch jobs/s** (2540 jobs,
    433 s, empty volume, 0 dropped, 0 failed). Consistent with the single worker thread; **proof of
    nothing** — the test would be N threads against this same fixture, which is now repeatable.
  - `dem_fail=89` in that run, with `fetch_fail=0` and `bake_fail=0`. Unexplained. Not interpreted.
- **"Any origin on earth works" was the claim here, and it was FALSE.** A cold region did not load
  at all. Measured over the Matterhorn on an unmodified build: `0 chunks drawn, 39 pending`, one
  log line, then silence — while fb-tiles was already answering 200 for those very tiles. Hameln
  only ever worked because its cache volume has been warm for months, which means **every number
  this project has ever produced was a warm-region number**, this file's "10–20 s to stream"
  included. Cause: fb-tiles said `404` for both "queued, ask again" and "there is none", and the
  browser cached every 404 as a permanent hole. Fixed on both sides (`b2c5ede` server → `202`,
  `d681a6f` browser → only 200/204 are terminal). **Still not proven**: the cold run over Aoraki
  reached 111/128 chunks and did not converge inside 300 s. It GROUND rather than stalled — 4539
  upstream fetches with 0 failures, 1593 bakes with 0 failures, nothing dropped, chunks rising
  monotonically — so it is not the Matterhorn stall. But "grinds" is not what was promised.
  One fact, **read and not measured**: `tiles/prefetch.c` starts exactly ONE worker thread
  (`pthread_create` once, not in a loop; the file header says so in the singular). **Whether that
  thread is the bottleneck is unmeasured, and where the time actually goes is unknown** — 1593
  bakes in ~600 s is far faster than the 1.6 s per cold bake the comments claim, so the obvious
  story does not even add up. The cold Hameln run is the measurement that decides it.
  Do not restore the claim until a cold run is green.
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
- **The instrument that measures itself.** Six more, all from one evening, all the same shape —
  the tool answered a question about itself instead of about the world:
  - **Frame rate here is SIGN-INVERTED.** Headless chromium paces an idle page at ~131 ms while
    the frame costs 0.5 ms, so injecting 25 ms/frame of CPU work makes the rate go UP (7.5 → 14
    fps). A metric that improves when you add work cannot prove you removed any. `bench.sh` prints
    it as a diagnostic and says so; never quote it.
  - **A single sample is not a threshold.** "~47 % ground" was one screenshot, handed on as a
    reference. Same software, other pose: 15–38 %. As a gate it would have failed everything,
    forever. Gates are pngstat's own verdict and `128 chunks / 0 pending` — never a percentage.
  - **A shared service is a shared measurement.** `run-podman.sh` is all-or-nothing: it rebuilds
    `fb-tiles` too, so two agents on one box measure each other. Caught only by hashes and idle
    sampling (WASM `a384db73` vs `c30957cb`; 61 % idle → 3 %). `bench_stack.sh` pins the artifact
    and records its sha256 per run. **A result without artifact identity is not a result.**
  - **An alarm that always fires gets switched off.** "Texture uploads per frame must be 0" is
    violated by EVS *by design* — the video frame is a new image every frame. The thrash detector
    is `generateMipmap` (only the tile path calls it), and it was checked to stay SILENT during a
    real tile-boundary bake, not just to fire on 256/frame.
  - **`grep` finds the text, not the thing.** `world3d_render: NOCH DA` — from the tombstone
    comment naming the deleted function. Two of us, same trap, same hour. Ask the definition and
    the build, not the match. `grep -c` on minified JS counts lines, not occurrences.
  - **`pgrep -f "foo.sh"` matches the waiter that greps for it.** A watcher waited 21 minutes on
    itself and reported "still running" the whole time.
- **There is no hot loop in the renderer. Measured, so stop looking for one.** SVS costs **0.8 % of
  one core** — `frame_cb` p50 = 0.55 ms against ~150 ms of browser idle between frames. A change
  that halves it saves 0.3 ms, which is below the 10.5 % noise floor: not just pointless, but
  *unprovable*. SIMD and cache-locality work on this path buys nothing and can be shown to buy
  nothing. (EVS costs a core, but that is SwiftShader decoding H.264 in software — on real hardware
  it sits in the video block. Never quote it as "EVS costs a core".) The corollary is worse: the
  measurement environment is only stable under saturation (photo spreads 0.1 % at 96.8 % of a core;
  osm spreads 27.7 % at 0.8 %) — so the only path we could prove anything on is the one we do not
  want to optimise. Refactor here for **structure and correctness**; performance is not on the table
  because there is nothing on it.
- **A number borrows authority from where it stands.** Two halves of one mistake, both made here in
  one evening. A success criterion was declared "non-negotiable" without anyone asking whether it
  was *reachable* — the cold test was given 300 s by a guess, and the guess inherited the gravity
  of the criterion beside it. **A criterion without a feasibility check is not rigour, it is a
  ritual; and a pre-registered number feels like rigour even when it was guessed.** Pre-registration
  protects against explaining a result away afterwards. It does not protect against guessing.
  Same shape, one paragraph up: a hedge ("read, not measured") placed BESIDE a claim covers the
  provable half while the invented half rides along. Put the hedge in front, or split the sentence.
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

- **Cold regions: fixed on the wire, not yet proven.** `202`/`204` replaced the overloaded `404`
  (see above), but the cold run has not gone green. Two things are missing. (1) **A repeatable cold
  fixture**: today's test burned a real origin — the act of testing warms it, so Matterhorn and
  Aoraki are now warm and each retry is a *different* fixture (different relief → different `err`,
  different split depth, not comparable). The answer is an `fb-tiles` with its OWN EMPTY VOLUME, so
  Hameln itself is cold: same fixture every time, known warm target (`128/128, 0 pending, ~15 s`),
  affordable budget. `run-podman.sh` hardwires `-v fbtiles-cache:` and `-p 8081:8081`; `test/
  bench_stack.sh` already does this pinning exercise for :8098 — same tool, opposite direction
  (the benchmark needs warmth, the cold gate needs the reverse). (2) **`tiles/prefetch.c` has ONE
  worker thread** — read, not measured. A cold region therefore streams at the speed of one serial
  curl. Whether that is a bug or deliberate politeness toward upstream is written down nowhere.
- **`/elev` blocks for 5 s and then lies quietly.** `aircraft/terrain.c` uses `curl --max-time 5`;
  over a cold region the DEM tile cannot arrive in time, so the engine seeds `HOME_ELEV` with the
  compiled-in Hameln value (~70 m) over, say, a 750 m valley floor. Same family as the 404: "not
  here yet" is indistinguishable from "does not exist". Better than the renderer was — it keeps the
  last known ground and SAYS SO in the log — but it is a time bomb for any foreign origin.
- **The gates only ever test warm.** `verify.sh` is 4/4 green on Hameln while a cold region loads
  nothing at all. A cold gate needs the empty volume above, not a foreign continent — and writing
  one before the fix is green would cement today's broken state as the expectation.
- **`pngstat` may be calibrated on Hameln green.** Its predicate is "beats blue by a margin,
  because vegetation always does" — rock and snow do not. Over the Matterhorn it called a streamed
  scene `SUSPECT`. The criterion should be "has structure no sky has", and it must be shown to fail
  over a rock face AND over empty sky, or it has only moved the calibration.
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
- **`w3_avail{READY,PENDING,ABSENT}` belongs in the SAME step as negative caching + `204` — not
  before it.** The plan was to fix `w3_bake`'s `if(n<=0)` (PENDING and ABSENT collapsed into one
  int) with an enum, guarded by `-Wswitch`. Checked before building it: **the wire can no longer
  produce a 0 at all.** `T.get` returns an empty array only on `204`, the server never sends one,
  and a `200` always has bytes (`fb_bake_ondisk` requires `st_size > 0`). So `if(n<=0)` is toothless
  today, `photo_none` is dead code, and `ABSENT` would have **no producer**. An enum whose third
  case nothing can create is a type claiming more than the wire carries — the same lie as the
  overloaded 404, mirrored. **If you have to write "the server never sends this" next to a case, the
  type is too early.** Give the three states a producer first (`cache.c:63` fetches with `curl -s
  -f`; the upstream 404 is distinguishable there, it is simply never recorded), then the enum
  guards something real and the proof is an ocean tile that stops asking.
- **Modularisation** — `xp_bridge.c` (771) and `world3d.h` are still god files. Out so far, each
  100 %-covered: `fdm/{ephemeris,atmosphere}`, `terrain`, `gfx/{mat4,style}`, `tiles/{lru,prefetch}`.
- **`coordination(sign)` is weather-sensitive, and that is a decision for a human.** With the
  weather pinned at `TURB=1.0` about 3 % of samples fail — not broken physics, but
  `steady = abs(roll_rate) < 12` letting through exactly the adverse-yaw transients its own
  comment says to exclude (a wing rolling at 11°/s is not in steady flight). Tightening the filter
  or picking a calmer test wind both change what the invariant means. Do not quietly retune it.
