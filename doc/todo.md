# Now

| | |
|---|---|
| **Working on** | The restructure — `sim/src` → `src/`, `test/` mirroring it, the wasm and container surface deleted |
| **Scope** | `doc/requirements.md` is the authority. `doc/bugs.md` is being cut to what survives the refactor |
| **Last accepted** | The Mercator refusal, the log latch, and the library design (`f44eb96`) |

**The queue below replaces everything that came before it.** The hardening queue, the Band 0 streaming
queue and the wasm items are gone — not because they were wrong but because SDL3, SDL_GPU, the tile
source joining the engine and `src/`→`test/` moved the ground under them. What survived is here.

Every step's acceptance carries **the harness green** as a clause, and three numbers that must not move
until step 11 changes the graphics API:

- the declared still is **one** picture — `852bd4246ee34f65` at `buildingTris=134990`
- `impostorStands=9565 treeTris=19130 terrainTris=331260`
- the telemetry schema unchanged in column names and units, because the archive's comparability is what
  makes any measurement here usable

## The sequence

| # | Step | Done when |
|---|---|---|
| 1 | **The restructure** *(in flight)* | `src/` is pure C++, `test/` mirrors it, no wasm or container artefact remains, the three numbers above unchanged |
| 2 | **`Check.h`, `run.sh`, three tests** *(in flight)* | the harness prints 3 PASS, one demonstrated red, and an `ExpectFail` test the harness inverts |
| 2a | **Every trace of emscripten deleted** | zero `__EMSCRIPTEN__`, zero `<emscripten…>` in the tree — today 20 conditionals and 6 includes across 6 files that no target compiles |
| 2b | **The log belongs to the library** | a consumer names a path, stdout or stderr; `ServerLog`, `ServerTelemetry` and `HttpPost` are gone, and with them the collectorless channel whose absence a run cannot currently report |
| 2c | **No C left** | `src/world/terrain/` and the folded-in `tiles/` are C++; the C-ABI exception is struck — its stated reason was that `terrain.c` is shared with `tiles/`, and `tiles/osmmesh/terrain.c` is a 111-line copy of the 164-line C++ original that says so in its own header |
| 3 | **Layer archives** from the existing per-group compile lines | all targets link, the three numbers unchanged, object count unchanged |
| 4 | **Stable requirement ids**, harness reads `COVERS` | unknown-id count 0, a per-band coverage tally prints |
| 5 | **The negatives move to `test/negative/`** | each demonstrated red *for its own reason*, asserting exactly one error with the exact diagnostic — today they pass on any compile failure, so a typo in a fixture proves nothing |
| 6 | **Geometric invariants** — the decidable class, of which we have almost none | every species and every roof kind: closed, wound, unit normals, positive signed volume, no degenerate triangle |
| 7 | **`src/api/`** — the entry point's whole include set | a test client compiles against `-Isrc/api` alone; `Renderer` and `World` have no spelling there |
| 8 | **`src/host/`** — the porting seam, which `core/io` already almost is | `nm -u` over the archives equals a declared freestanding floor; **a host with zero workers is legal** |
| 9 | **The tile source becomes data providers.** Not a port of a server — a deletion of one. Measured: of `tiles/`'s 14 224 lines, **9 712 are vendored `stb_image`** (not ours), **456 are the server itself** (`main`, route, http, reply) and are deleted outright, and **4 056** are the providers, decoders, cache and bake that come across. `mvt.c` (780) and `pb_stream.c` (86) exist only there; `terrain.c` (111) is a copy of the 164-line C++ original | there is no server, no port and no HTTP hop to our own data; each upstream — terrarium, versatiles, arcgisonline, NOAA, Overpass — is a provider declaring what it covers, behind one registry; `Absent` from one hands over to the next, and the terminal absence is the exhaustion of the list. **Note what dies with the server:** `204 → Hole → Absent → terminal` is minted by `tiles/src/main.c:24`, our own code — no upstream sends it — so the tree's whole idea of "there is nothing here" has no author until a provider classifies for itself |
| 9a | **`stb_image` gives way to SDL3.** It is 9 712 of `tiles/`'s 14 224 lines and none of it is ours | zero vendored image code. **Two facts first:** SDL3 core decodes nothing — the codecs are in **SDL3_image** (3.4.4, a separate library, not installed here) — and SDL_image may use stb internally, so this may move the dependency rather than delete it, which is still 9 712 lines out of the tree. **What must not be lost silently:** `tiles/src/bake.c` sets `stbi_write_force_png_filter = 0` on a measured finding — *">2× bytes for <20 % speed"* — and `IMG_SavePNG` has no such knob, so the encode side is a trade to re-measure, not a substitution |
| 10 | **Exact-width integers**, one const header, the two settings tiers | zero platform-width integer declarations outside a C ABI; every number carries its origin |
| 11 | **`render/` → SDL_GPU** — 36 files, 2 739 lines of shader | 720p60 on this device, p99 ≤ 33 ms over a moving camera. The still becomes a *new* single sha — a pixel identity cannot survive an API change and must not be pretended — while the three geometry counters must survive **unchanged**, because geometry does not know which API drew it |
| 12 | **Scenarios with no world** — one tree, one building, one car | a scenario declares a subject, a stage and a light, and renders with no terrain and no streaming |
| 13 | **glTF in, Blender as the oracle** | the first *external* check this project has ever had: a scene rendered both ways, with what is comparable pinned and what is not named |

## The dependency rule

**SDL3\* wherever it can carry the job; whatever is required where it cannot** — and a dependency
that is not SDL3\* carries its reason beside it, the way a Core Guidelines deviation does. Installed:
`sdl3` 3.4.14 · `sdl3_image` 3.4.4 · `sdl3_net` 3.2.0 · `sdl3_ttf` 3.2.2.

Today: `-lcurl -ldl -lm -lpthread -lwebgpu_dawn`. Dawn goes at step 11 (SDL_GPU), `stb` at 9a
(SDL3_image). **curl stays, with its reason:** all five upstreams are `https://` and the SDL3 family
has no TLS — `SDL_net` calls itself *"a relatively thin layer over system-level APIs like BSD
sockets"*, 35 declarations. Writing TLS ourselves is not a dependency saved, it is a dependency
written badly.

**Where it lives is still the point.** The transport sits behind `Host::Fetch`: the library links
SDL3\* and never names a transport, the host implementation links what its platform offers. So
`nm -u` over the library archives shows **no `curl_` symbol** — which turns "platform agnostic" from
a rule someone follows into a command that fails.

## Standing

- **Coverage has no baseline** because there is no coverage instrument. That is *not yet measured*.
- **`[[nodiscard]]` at 214/214 is held by nothing** since the Python ledger was reverted; step 2 re-homes it.
- **`verify-still` is the only thing that imposes tile arrival order.** It is Node, so it dies in step 1;
  step 9 makes it cheap again, because in one process the order is ours.
