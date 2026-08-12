# Now

| | |
|---|---|
| **Working on** | Phase 1 — the data providers. The harness (`test/run.sh`, 3 PASS) is built and under judgement |
| **Scope** | `doc/requirements.md` is the authority |
| **Last accepted** | `src/` and `test/` at the root, the wasm surface out of the build (`b83285f`, judged `34e0f25`) |

**Four phases, in the owner's order.** Everything earlier is superseded. Two numbers hold across every
step until the renderer's API changes in phase 3, and they are a clause in each acceptance:

- the declared still is **one** picture — `852bd4246ee34f65` at `buildingTris=134990`, `terrainTris=331260`
- `impostorStands=9565 treeTris=19130`

---

## Phase 1 — the data providers

**There is no server after this, only providers.** Of `tiles/`'s 14 224 lines: **9 712 are vendored
`stb_image`** and are not ours, **456 are the server itself** (`main`, route, http, reply) and are
deleted outright, **4 056** are the providers, decoders, cache and bake that come across.

| | Done when |
|---|---|
| **1.1 The decoders become C++ and stop existing twice** | `tiles/osmmesh/terrain.c` (111 lines) is a copy of `src/world/terrain/terrain.cpp` (164) and its own header says so; `mvt.c` (780) and `pb_stream.c` (86) exist only there. Zero `.c` in the tree, one decoder each, and the `world/terrain` C-ABI exception struck — its stated reason was the sharing that turns out to be duplication |
| **1.2 `stb_image` gives way to SDL3_image** | zero vendored image code. `bake.c`'s `stbi_write_force_png_filter = 0` rests on a measured *">2× bytes for <20 % speed"* and `IMG_SavePNG` has no such knob — re-measure, do not assume |
| **1.3 Each upstream is a provider declaring what it covers** | terrarium, versatiles, arcgisonline, NOAA, Overpass behind one registry, ranked, duplicate rank refused at registration. `Absent` from one hands over to the next; the terminal absence is the exhaustion of the list |
| **1.4 The absence semantics gets an author** | `204 → Hole → Absent → terminal` is minted by `tiles/src/main.c:24` — **our own code, and no upstream sends a 204.** When the hop goes it has none, so each provider classifies for itself |
| **1.5 No process boundary to our own data** | no `TILES_BASE`, no `:8081`, no container. Transport behind the host seam so `nm -u` over the library shows no `curl_` symbol |

**This phase makes phase 2 cheaper:** in one process the tile arrival order is ours, so `test/world/tile_delay.py` — the only thing that imposes it — has nothing left to do.

## Phase 2 — delete wasm, emscripten, container and Python

| | Done when |
|---|---|
| **2.1** | zero `__EMSCRIPTEN__`, zero `<emscripten…>`. Twenty conditionals across six files that **no compiler reads** since `b83285f`; with `HttpPost.cpp` go `gAbandoned`, `kInFlight` and a justification that argues from a Makefile flag deleted in the same commit |
| **2.2 The library owns its log** | a consumer names a path, stdout or stderr. `ServerLog`, `ServerTelemetry`, `HttpPost` gone. Today, with the collector absent, a run exits 0 and **not one of its 674 log lines mentions a refused post** |
| **2.3 No Python** | `tile_delay.py` is dead once phase 1 lands. `verify_clients.py` is a closed allowlist of twenty method names — a new `Renderer::SetX` from a second TU passes in silence — and its rules become unspellable when the entry point's include set cannot name `Renderer` |
| **2.4 No container** | `tiles/Dockerfile`, and `tiles/` itself, gone with phase 1 |

## Phase 3 — the renderer, against Blender

**Order matters here and it is the design's, not mine:** the glTF reader and the coverage rung land
**before** SDL_GPU, because the port has no acceptance criterion today and rung 1 is one. The linear
readback lands **after**, or it is written twice.

| | Done when |
|---|---|
| **3.1 glTF in, and a scenario with no world** | one tree, one building, one car, one imported scene — a studio stage declares a `Ground`, a key light, a backdrop and a subject, and `Ground` is already the whole interface between world and generator, so no second code path exists |
| **3.2 Ten scenes, ascending, one new thing per rung** | triangle · quad · cube · sphere · lit cube · lit sphere · albedo and emissive · point light · shadow and a plane · texture at 1:1. **Rung *n* does not run until *n−1* is green** — ten reds is one finding |
| **3.3 Coverage parity** | IoU ≥ 0.999 and boundary p95 ≤ 0.5 px. Cycles' box filter is constant and the integer raster coordinate is the pixel centre, so the instrument floor is **0.005 px** and AA is not a confound. `film_transparent` gives an exact alpha channel, so the metric has an error bar |
| **3.4 `render/` → SDL_GPU** | 36 files, 2 739 lines of shader, HLSL through `SDL_shadercross`. 720p60 on this device, p99 ≤ 33 ms. **Accepted against 3.3** — the still becomes a *new* single sha, because a pixel identity cannot survive an API change and pretending otherwise is a false green; the geometry counters must survive unchanged |
| **3.5 A scene-referred linear readback** | 7.37 MB copy of a texture that already exists, one method shaped like `ReadDepth`, zero cost on a frame nobody asks. **It settles on day one whether `kSceneExposure = 11.0` is an exposure or physics** |
| **3.6 Radiance parity** | median relative difference ≤ 1 % against the closed form on unshadowed facets, **and Blender's own residual against the same closed form published beside ours**. Blocked until a Lambertian surface is spellable at all — `kGroundBounce` and `kSelfShelter` are soil constants applied to water, glass and paint, +17.5 % over Lambert at albedo 0.5 |

## Phase 4 — the generators

Vegetation forms, the grass stratum as a field, buildings, infrastructure, vehicles — `requirements.md`
bands III, IV and V, **1 100 open lines**, and no gate among them today. The first thing they get is what
phase 3 builds: a studio scenario, so every generator is benchable with no world and no network.

## Standing

- **Coverage has no baseline** — there is no coverage instrument. *Not yet measured.*
- **Nothing tests that a counter survives a 32-bit target.** `verify-counters` left with the browser and
  `TilePool.h`'s width is held by a comment. A `static_assert` over the ledger's field widths is
  stronger than the gate was and costs nothing.
- **`TreeMesh.h`'s "base at y = 0" is false for seven declarations** — willow at −0.933 of height,
  22.5 % of its bark vertices below the base plane, 18.9 m under the placement point at `height_m: 18`.
  Measured by the first decidable test in this tree and deliberately not asserted, because which of
  grower and contract is wrong is phase 4's decision.
