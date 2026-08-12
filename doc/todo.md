# Now

| | |
|---|---|
| **In the tree** | Phase 1's five blockers — the container's Mach-O binary, nine `_GNU_SOURCE` lines that break the Linux build, `make` missing from the image, `FieldTooSmall` as a terminal hole, one `default:` over a house enumeration |
| **Then** | The renderer stage, which leads from here |
| **Blocked** | The harness — four demonstrated defects, three of them green over a failing test. Fixed by one change: the verdict is the reporter's printed trailer, not an eight-bit exit status |

**Requirements are pulled, not marched.** `doc/requirements.md` is ordered by band — engine, world,
vegetation, buildings, vehicles — and that is a **taxonomy, not a schedule**. A line is built when the
stage that needs it reaches for it. Band 0 holds 78 lines about residency that nothing currently reaches
for, while the renderer needs a linear readback sitting unticked in a band nobody was working on. A stage
is done when it renders, not when a band is exhausted.

**1 406 lines open, 227 ticked.** A tick names the file that implements it **and the test that holds it**
— today it names at most the first, and twice this week it named a file that had been deleted.

---

## Before the goal begins

**The renderer stage does not start until both of these are true.** Owner's ruling: the goal begins when
the wasm, Python and container surface is deleted and the data providers are in the library.

| | State |
|---|---|
| **Decoders are C++, one each, `stb` gone** | **done** — 3 `.c` → 0, three MVT decoders → one, 19 424 lines of a twice-vendored `stb_image` out, `malloc` outside `core/io/` at zero, geometry counters unmoved |
| **One content store, keyed by a hash the provider delivers** | `hash = filename`, a directory and nothing else. The provider's hash must cover **its own version** — the one failure it cannot catch. Caching declared per scenario, so cache-on and cache-off differ only in timing |
| **Each upstream a provider declaring what it covers** | terrarium, versatiles, arcgisonline, NOAA, Overpass behind one registry, ranked, duplicate rank refused at registration. `Absent` from one hands to the next; terminal absence is the exhaustion of the list |
| **The absence semantics gets an author** | `204 → Hole → Absent → terminal` is minted by our own `main.cpp` and **no upstream sends a 204**. When the hop goes it has none |
| **No process boundary to our own data** | no `TILES_BASE`, no `:8081`, no container. `nm -u` over the library shows no `curl_` symbol |
| **Emscripten gone** | 20 conditionals and 6 includes across six files that **no compiler has read** since the wasm target left |
| **The library owns its log** | a consumer names a path, stdout or stderr. Today, with the collector absent, a run exits 0 and **not one of its 674 log lines mentions a refused post** |
| **No Python** | `tile_delay.py` dies with the HTTP hop; `verify_clients.py` is a closed allowlist of twenty method names that a new call from a second TU passes in silence |
| **No container** | `tiles/` itself goes with the providers |

**Deleting `/bake` belongs here too** — 963 lines and **3.1 GB of a 7 GB cache serving nothing**, whose
format we spent an hour deciding how to tune before finding it has no reader.

**The scenario interface is a precondition too**, not a later step: every runnable thing is a test now,
and a test declares what it runs. Nothing under `test/` — a studio subject, a declared run, an
interactive client — works until a scenario can be declared and loaded.

### Two make targets

**`make` builds the engine. `make test` runs the tests — all of them, or one. `make clean`.** Nothing else.
Today: **450 lines, 16 targets** — `help walk walk-asan world treebench` and eight `verify-*` plus
`gates gates-build clean`. Every one of the eight is a test wearing a Makefile recipe, and the
harness now exists to run them:

| target | becomes |
|---|---|
| `verify-generators` · `verify-world` · `verify-types` · `verify-clients` | compile subjects under `test/compile/<layer>/` — already moved |
| `verify-refusals` | a test: a bench with nothing to measure refuses, an unknown growth form refuses |
| `verify-walk` · `world` · `treebench` | the test build covers them; a target that only proves a link is a test that links |
| `verify-walk-asan` | a declared run under `address,undefined` — a test the harness builds with those flags |
| `verify-still` | **waits for the providers.** It needs an imposed tile arrival order, which is a Python proxy today and is ours for free once the fetch is in-process |
| `gates` · `gates-build` | the harness *is* the runner; `--tier` already expresses the fast/full split |
| `help` | a target that prints a list of targets, when there are two |
| `clean` | **stays.** `rm -rf build`, one line |

**The blocker is `verify-still`**, and it resolves itself: in one process the arrival order is ours,
so the last Makefile-only gate dies with the last Python file.

### The tree ends with three directories

**`doc/`, `src/`, `test/`. Nothing else.** Six stand today and each has an answer:

| | tracked | where it goes |
|---|---|---|
| `tiles/` | 58 | into `src/` as providers; the server, the container and the `/bake` path deleted |
| `vendor/` | 4 | `stb_image*` already deleted; `build_dawn_native.sh` goes with Dawn, `fetch_curl_compat.sh` with the wasm toolchain |
| `tools/` | 4 | four Python instruments — deleted, and whatever is worth keeping becomes a test |
| `mods/` | 4 | declared scenarios — they are test inputs now, so they belong under `test/` with the fixtures |
| `assets/` | 34 | **decided: `src/assets/`.** Species, ground materials and sky are part of the core engine, not fixtures a consumer supplies — so they move into the library rather than out of it. `src/` is the library entire: its C++ and its declared data |
| `build/` | 0 | untracked output; stays out of the tree |


## The renderer stage — what it reaches for

Ordered by what unblocks what, not by band.

| | Pulls | Why |
|---|---|---|
| **A studio scenario — a stage with no world** | § I.25 | Nothing else can be rendered in isolation. A studio stage declares a `Ground` directly, and `Ground` is already the whole interface between world and generator, so no second code path exists. It also moves every generator test off the network |
| **A glTF reader** | § I.26 | The shared input for geometry and camera. Light and material are declared beside it, never through it — Blender's importer applies a 683 lm/W factor in its default mode |
| **Ten scenes, one new thing per rung** | § I.26 | Triangle · quad · cube · sphere · lit cube · lit sphere · albedo and emissive · point light · shadow and plane · texture at 1:1. **Rung *n* waits for *n−1*** — ten reds is one finding |
| **Coverage parity** | § I.26 | Needs no light model, no linear tap, no material mapping. IoU ≥ 0.999, boundary p95 ≤ 0.5 px, instrument floor **0.005 px** because Cycles' box filter is constant and the integer raster coordinate is the pixel centre |
| **`render/` → SDL_GPU** | § I.19, § I.24 | 9 340 lines in 36 files, 2 739 of shader, HLSL through `SDL_shadercross`. **Accepted against coverage parity** — the only acceptance criterion the port has. The still becomes a *new* single sha, because a pixel identity cannot survive an API change; the geometry counters must not move |
| **A punctual light** | § II.8 | Rung 8 is Blender's factory light and we have none — `Gpu.h` binds one sun irradiance and one sky term, no light list, no second shadow kind. Already scope we owed, unticked since before the oracle existed |
| **A scene-referred linear readback** | § I.26 | 7.37 MB copy of a texture that already exists, one method shaped like `ReadDepth`, zero cost when unasked. **Settles on day one whether `kSceneExposure = 11.0` is an exposure or physics** |
| **A material that can decline the soil constants** | § II.8 | `kGroundBounce = 0.12` and `kSelfShelter = 0.35` are statements about soil spliced into every surface at seven call sites — **+17.5 % over Lambert at albedo 0.5**, on water and glass alike. Radiance parity has no zero point until a Lambertian surface is spellable |
| **Radiance parity** | § I.26 | Median relative difference ≤ 1 % against the closed form, **with Blender's own residual against that same closed form published beside ours** — the oracle states its error before it judges ours |

## Then foliage, then buildings

Each pulls its own, the same way. Bands III and IV hold **458 and 346 lines** with no gate among them
today; the studio scenario is what makes any of them testable without a world and without a network.

## Standing

- **Coverage has no baseline** — no coverage instrument exists. *Not yet measured.*
- **Nothing tests that a counter survives a 32-bit target.** A `static_assert` over the ledger's field
  widths is stronger than the gate that left with the browser, and costs nothing.
- **`/bake` has no consumer.** A live container at 40 h and 10 218 prefetches reports `baked=0`;
  **3.1 GB of a 7 GB cache serves nothing**, including 177 MB whose producer is not in the source at all.
- **`TerrainSource` returns bare bytes** — absent, still fetching, transport refused and an empty 200 are
  all `{}`, with the reason on a thread-local. The provider contract would be built from that shape, so
  it is a named follow-up rather than a note.
