# World, terrain and tile streaming — `world/`

**Sources of this file:** `sim/src/world/` (`FBWorld.h/.cpp`, `FBTerrainLoader.h/.cpp`,
`FBTilesElevation.h`), `sim/src/world/terrain/` (`geo.h/.cpp`, `geo_ecef.cpp`, `mesh.h`,
`terrain.h/.cpp`, `osmmesh.h`, `osmmesh_terrain.cpp`), `sim/src/render/FBChunkMesh.h` +
`FBChunkVtx.h` (the mesh end stage), `sim/src/clients/FBTileWorkerMain.cpp` +
`sim/web/fbtw-worker.js`, `sim/Makefile` (targets `wasm`/`worker`) and `tiles/` (server: `Makefile`,
`nginx.conf`, `src/*`) — the server is documented here **from the client's point of view**. Plus
CLAUDE.md's sections `world/`, `terrain/` and "Rendering".

Translated from the German original in the Phase-3 mirror rebuild; the weather data kind (`/wx`, the
FBWX format) split off into [`weather.md`](weather.md).

Neighbouring files: [`../render/renderer.md`](../render/renderer.md) (what happens to the geometry
once it is on the GPU), [`../architecture.md`](../architecture.md) (the lib/client split).

---

## Spec

| Contract | Acceptance / measurement anchor |
|---|---|
| Nothing is preloaded — every point on earth is a valid start | a `spawn` line anywhere works without any area being defined; the root ring is laid around the camera |
| The render loop is never blocked in the browser | `fb_stream_*` is a poll interface; fetch/decode/mesh/mips run in Web Workers |
| Line of sight must not cost coverage | a child beyond the view radius makes its parent a drawn leaf — detail is dropped, area never |
| One ground truth for everybody | mission ground spawn, AGL/radar altitude and crash detection all go through `FBElevationProvider`; the DEM the renderer draws is the DEM JSBSim collides against |
| Missing data is a defined state, not an error | 204 = hole (photo falls back to OSM), a missing neighbour tile = a gap the skirts cover, cold `/elev` = 503 and the start path asks with `?block=1` |
| An `/elev` answer is a number or it is not a measurement | the whole body must be ONE finite number; `atof` alone would cache an HTML error page as "sea level" |
| The core library must not depend on tile streaming | `FBTilesElevation` lives in `world/`, not in `core/`; the core side sees only the `FBElevationProvider` interface |

### 1 The data path, drawn through once

```
fb-tiles :8081  ──HTTP──▶  byte cache (JS map in the browser | in-memory in the native client)
      /t/terrain/z/x/y            │
      /bake/{osm|photo}/z/x/y     ▼
      /t/lights/z/x/y     osmmesh_fetch_tile ──▶ decode terrarium PNG, stitch 4 neighbours,
      /t/stars/band/0/0                          build a regular ENU grid
      /elev?lat&lon                │
                                   ▼
                    w3_chunk_build_ecef  ──▶ w3_vtx[] (ECEF offset to the tile origin,
                    (render/FBChunkMesh.h)     UV, ECEF normal) + err (m) + origin (double)
                                   │
                                   ▼        + fb_build_pyramid (sRGB mip chain of the albedo)
                            FBWorld (quadtree, budgets, 2-phase commit, eviction)
                                   │  UploadTile / SetDrawList
                                   ▼
                            FBRenderer → FBTilesStage
```

In the browser the framed block (fetch, decode, mesh, mips) runs in **its own Web Workers**; natively
it runs inline with blocking libcurl. The poll interface above it (`fb_stream_*`) is identical for
both.

### 2 `FBWorld` — the streamer

`sim/src/world/FBWorld.h/.cpp`. A chunked-LOD **quadtree** (Cesium style) which per frame hands a draw
list of the LOD cut to `FBRenderer`. Ported from the predecessor engine code (`tiles/walk.h` priority
refinement, `lru.h` grace eviction/budget), with **corrected** coverage semantics (see §2.3).

#### 2.1 What it owns and what it only borrows

| | Content |
|---|---|
| **Owns** | the node table `std::vector<Node>` + the `unordered_map` index, the draw list `DrawSlots`, the work list `WorkList`, the pyramid scratch, the night-light buffer, all counters |
| **Borrows** | `FBRenderer*` (upload target) and **`const FBUnitRegistry*`** (`SetUnits`/`Units()`) |

**Why the registry is only borrowed** — that is not cosmetics but the consequence of the lib/client
split: the cast of the world ("who exists where") is **simulation state**, not a rendering thing. It
used to be a member of `FBWorld`, i.e. on the **renderer side** of the split. `fb-gym` links no
`world/` — and therefore handed every module `world = nullptr`, so that a simulated sensor could never
have seen another unit in precisely the client that actually runs the mission loop. Today
`units/FBUnitRegistry` lives in the core lib, the client owns exactly one, `FBWorld` only holds a
pointer to it — for the **drawing side** (`FBUnitsStage`, today NoOp). No ownership, no world mutation
path.

Separately beside it stands the `const FBWorld*` a sensor gets: that is the **terrain side**
(masking), not the unit side.

#### 2.2 Constants

| Constant | Value | Meaning / derivation |
|---|---|---|
| `kRootZ` | 8 | root ring of the quadtree |
| `kMaxZ` | 14 | finest level (the vector source ends there, see §7.2) |
| `kGrace` | 180 passes | hysteresis before eviction (lru.h) |
| `kSseK` | `720 / (2·tan(30°))` = `720/(2·0.57735)` ≈ **623.5** | pixel focal length, normalised to 720 px height at 60° FOV |
| `kEdgeTau` | 384 px | target edge length of a tile on screen; leaf tiles land between `kEdgeTau/2` and `kEdgeTau` |
| `kCosView` | 0.5 | frustum weight: < 60° off axis = full priority, otherwise factor 0.05 |
| `kNodeCeil` | 6000 | safety cap on the working set |
| `kEarthCirc` | 40,075,016.686 m | equatorial circumference for `SpanM(z)` |
| View radius | `FB_VIEW_KM · 1000`, default **240 km** | client parameter (`clients/FBAppWasm.cpp`) |
| `Grid` | 32 | decimation of the tile grid (32×32 quads) |
| `TS` | 512 | albedo edge length in texels |

**LOD is purely distance-based** (owner decision 2026-07-23): a split happens when the projected **edge
length** `SpanM(z) · kSseK / dist` exceeds the threshold `kEdgeTau`. Height variance deliberately does
**not** enter the decision — a flat tile nearby must refine to the same level as a jagged one at the
same distance (the same albedo resolution at the same distance). Side effect: the old standstill over
flat terrain (`err ≈ 0` refused every split, `leaves` stayed at 2–3) is gone with it.

#### 2.3 The refinement — and the corrected `walk.h` semantics

Four functions, all side-effect-free except `Descend`:

| Function | Role |
|---|---|
| `Viable(z,x,y,eye)` | pure: map bounds **and** within the view radius (`dist − span·0.71 ≤ ViewM`) |
| `WantSplit(z,x,y,eye)` | pure: the geometry test above; needs **no** tile data |
| `CanCover(z,x,y,eye)` | pure: can this subtree cover its area with tiles that are ready THIS pass? |
| `Descend(...)` | the draw traversal; 1 = area covered |
| `RequestSubtree(...)` | cascades the REQUEST down to the target leaves without drawing |

**The corrected error** (comment in the header, "sim-critic"): line of sight may only **prevent** a
split, never cost coverage. A child beyond the view radius makes its parent a drawn leaf — detail is
dropped, area never. The viability of all four children is tested **side-effect-free** **before** the
parent is replaced. `walk.h` treated "outside the view" at a split like "outside the map" and thereby
deleted the parent's quadrant — a hole.

**Sequence in `Descend`** at a split node:

1. all viable children can cover → draw the **refined** level (recursion).
2. otherwise, if this node itself is resident → **hold**: it draws the whole area while
   `RequestSubtree` requests the deeper targets. No intermediate LOD is ever built — only the geometry
   **target leaves**.
3. otherwise (boot/teleport, nothing resident) → draw whatever descendants are ready; the rest is a
   hole covered by a resident ancestor higher up or by the **loading screen**.

Because `WantSplit` needs only geometry, the **target cut is known immediately** — the boot/teleport
request goes straight to the end leaves, without an LOD ladder and without build/throw-away churn.

#### 2.4 Two phases and two modes (the readiness predicates)

| Predicate | Condition | Purpose |
|---|---|---|
| `Uploaded(n)` | mesh + albedo present, slot ≥ 0 | pure bookkeeping |
| `Ready(n)` | `Uploaded && Pass > readyPass` | **2-phase commit**: drawable only ONE pass after the upload, so that the `WriteTexture` is submitted and visible |
| `ReadyMode(n)` | `Ready` and, if the VIEWED mode is not the base mode, the overlay layer resolved (`alt == 1 && Pass > altPass + 1`, or `alt == -1` = a real hole) | a fine tile is never shown in the WRONG mode; the coarser, correct parent holds |
| `CoversInMode(n)` | `ReadyMode` **or** (`Ready` and drawn in the last pass) | on a TAB switch the resident fine tile stays in the OLD mode until its new overlay lands — no re-coarsening, no flashing. A NEW tile only counts with `ReadyMode`, so that it never pops up in the wrong mode |

The `+1` in `ReadyMode` mirrors exactly the renderer side (`FrameNo > PhotoUpTick + 1`,
`FBTilesStage`) — otherwise there would be a one-frame gap in the wrong mode.

#### 2.5 Budgets per pass

The work list is sorted **worst-first** (priority `weight / dist`, `weight` = 1 in the frustum, 0.05
outside) and then worked off within budget:

| Budget | Value | Work |
|---|---|---|
| `build` | 2 | `fb_stream_build` — poll the mesh |
| `albedo` | 2 | `fb_stream_pyramid` — poll the base mip pyramid |
| `upload` | 6 | `FBRenderer::UploadTile` |
| `altBudget` | 2 | lazy overlay of the NON-base mode (only for tiles touched this pass) |
| Lights | 3 decodes | `/t/lights` per pass, lowest priority |

A photo base hole (204 from `/bake/photo`) falls back to OSM, so that **every** tile can always draw a
base. An overlay hole marks the node permanently with `alt = -1` (do not ask again).

#### 2.6 Eviction, progress, telemetry

- **Eviction**: nodes not touched this pass age (`stale++`); after `kGrace` = 180 passes they are
  released (`ReleaseTile`, `free(verts)`, index fix by swap-pop).
- **`LoadProgress()`** = `TargetRdy / TargetTot` over the **geometry target cut** (`CountTargets`,
  side-effect-free). Until the threshold the client shows the loading screen and holds JSBSim frozen.
- **Log** (1 Hz, `FBLog::Debug("world","fbworld")`): `leaves`, `drawn`, `pending`, `evicted`, `vramMB`
  (mesh + albedo), `nodes`, `lights`, plus the **thrash probe** `buildsPerMin` / `evictPerMin`. Both go
  to 0 in a converged, stationary loiter; a steady rise is evict/rebuild churn.

#### 2.7 Night lights

Only in EVS night (`SetNightLights`, tied by the client to the day factor: `sun_el < −3°`). Per drawn
leaf, `/t/lights/z/x/y` is streamed and decoded: tile-local `(x,y)` ∈ [0,65535] → geo → ECEF at the
tile's mean height + `kLightLiftM` = 6 m lift (so that terrain occludes cleanly), minus the `Anchor`
(field origin, set once in `Open`). Class (0..7) → colour/radius/brightness from three LUTs;
`kLightGain` = 3.0 additive HDR gain, so that the cores get through the ACES compressor. Cap
`kLightBudget` = 65,536 sprites, nearest first.

### 3 `FBTerrainLoader` — the streaming C ABI

`sim/src/world/FBTerrainLoader.h/.cpp`. A flat `extern "C"` interface; `FBWorld` polls it every pass.
**Nothing ever blocks the render loop in the browser.**

| Function | Contract |
|---|---|
| `fb_stream_open(base, lat, lon, z)` | open once; sets the base URL and (WASM) starts the worker pool |
| `fb_stream_set_base(mode)` | boot base mode (0 = OSM, 1 = photo) — a priority hint to the worker |
| `fb_stream_campos(lat, lon)` | running camera track for the worker's nearest-first pump |
| `fb_stream_build(z,x,y,grid, &verts,&nverts,origin,&err)` | 1 = ready (malloc'd verts, caller frees), 0 = requested/pending |
| `fb_stream_pyramid(z,x,y,mode,ts,dst)` | > 0 = bytes written of the whole pyramid, 0 = pending, **−1 = a real hole** (server 204) |
| `fb_stream_ground(lat, lon)` | ground height m ASL, sampled out of the z13 DEM tile (§3.3) — ONE implementation for both link targets; ≤ −1e8 = the tile is not resident yet (WASM) or is a hole |
| `fb_stream_dem(z,x,y,&bytes,&len)` | raw terrarium bytes; 1 = ready, 0 = **pending** (do not cache as a hole!), −1 = hole |
| `fb_stream_lights(z,x,y,dst,cap)` | ≥ 4 bytes (header even at count = 0), 0 = pending, −1 = not available |
| `fb_load_image_file(path, …)` | image file → RGBA8 (WASM: embedded MEMFS, e.g. `/moon.jpg`) |
| `fb_fetch_stars(base, dst, cap)` | HYG bands concatenated, blocking start-up fetch |
| `fb_fetch_text(url, dst, cap)` | blocking text GET (e.g. the boot mission from `fb-sim`'s own `web/` mount) |
| `fb_terrain_load(base, lat, lon, z, grid, out)` | **static one-shot path** (bring-up): load a 4×4 tile field blocking and merge it into ONE vertex array |

#### 3.1 Byte access: two platforms, one contract

| | WASM (browser) | Native (CLI) |
|---|---|---|
| Mechanism | JS-side async cache (EM_JS), **non-blocking** | blocking libcurl behind a small in-memory cache |
| Status codes | 200 terminal, 204 = hole, everything else = "ask again" | the same retry rule |
| Reason | a page must not stop its frame loop | `gpu_native` is a CLI with its own control flow; a PNG dump frame may block |
| Retry | ASYNCIFY `emscripten_sleep(50)`, up to 60 attempts | `usleep`, same count |

The rest (`fb_stream_*` itself) is **shared**; only the three byte primitives `fbs_init`/`fbs_size`/
`fbs_copy` differ.

#### 3.2 The worker pool (WASM only)

`N = clamp(navigator.hardwareConcurrency − 2, 1, 6)` independent `fbtileworker` instances. Each is
**its own WASM module** with its own `osmmesh` context and its own DEM cache — the ASYNCIFY rule "one
build at a time" applies **per instance**, so N parallel builds are safe; the small cache redundancy
between instances is accepted.

Shared on the render-thread side is one structure (`Module.__fbw`):

| Field | Role |
|---|---|
| `q` | request queue |
| `req` | dedup set — a tile is at most once in the queue or in work, so nothing is built twice |
| `done` | result map the poll functions read from |
| `pump()` | fills EVERY free worker with the best job |

**Priority key**: `prio · 1e18 + dist²` — base tiles (`prio = 0`) always before the lazy overlay
(`prio = 1`), within a class **nearest first** (tile-space distance to the camera tile, from
`fb_stream_campos`). The streaming is therefore **camera-prioritised**, and precisely at the place
where it counts (the assignment to workers), in addition to the sorting of the `WorkList` in `FBWorld`.

#### 3.3 The height oracle: one tile, one implementation, both clients

`fb_stream_ground` sits OUTSIDE the platform split, and that is the point: a ground truth that differs
between two clients is not a ground truth. It reproduces `/elev` exactly — same zoom (**z13**,
`FB_DEM_Z`), same tile, same bilinear (`tiles/src/elev.c`, `fb_elev_at`, through fb-tiles' own
`tilemath.h`) — out of the Terrarium bytes `fb_stream_dem` already streams. The answer is therefore a
**pure function of position** on both sides of the wire, and the client-side cache is 12 decoded tiles
LRU.

**Why the tile and not the point.** `/elev` is one round trip per position, and in the browser that trip
lands AFTER the tick that asked — so a per-point cache can never answer the question the simulation is
asking. The same argument `tools/bake_dem.py` makes one level up ("why the tile route, not point
`/elev` requests"). One z13 tile is **4.8 km** of ground, twenty seconds of flight: the transport
happens two hundred times more rarely than the question. Native asks blocking, the browser asks
asynchronously and returns the −1e9 sentinel until the tile is resident — the caller
(`units/FBSimUnit::UpdateGroundAsl`) then keeps its last good value.

**Clamping to sea level (both platforms).** A REAL sample is clamped to ≥ 0: open bathymetry is
negative (ETOPO seabed), but the water *surface* — and with it the aircraft's ground reference and the
JSBSim ground — is at 0, not on the sea floor. The −1e9 "no sample yet" sentinel is left untouched by
this, otherwise callers could no longer distinguish whether a sample has landed.

#### 3.4 `[tileperf]` — the cold-start instrumentation

`FB_TILEPERF=1` (native) resp. the worker's own `[tileperf-worker]` log measure the stages of the same
pipeline: DEM fetch+decode (`osmmesh_fetch_tile`), mesh (`w3_chunk_build_ecef`), albedo fetch, albedo
decode (stbi), mip pyramid (`fb_build_pyramid`). Summary every 32 pyramids and at close; the last line
before convergence is the total cold-start time. Costs nothing when off (a cached env test).

### 4 `FBTilesElevation` — the elevation provider on `fb_stream_ground`

`sim/src/world/FBTilesElevation.h`. A **thin pass-through**: `GroundElevM(lat, lon)` calls
`fb_stream_ground(lat, lon)`, nothing else. The constructor only does `fb_stream_open(base, 0, 0, 8)` —
`fb_stream_ground` reads the base URL set by the open and is independent of the (lat, lon) passed
there; that only seeds the render quadtree.

`GroundElevPatch` stays an override rather than the base class's loop for ONE reason: it picks its
zoom from the patch's own post spacing, while `GroundElevM` is fixed at the zoom `/elev` uses. A 20 nm
radar-map patch therefore touches a 4×4 field instead of ~100 z13 tiles.

**It lies in `world/`, not in `core/` — and is therefore NOT part of the core lib.** Reason: it hangs
on the tile-streaming C ABI, which belongs to `render`/`world` and which the core library deliberately
excludes. The core side sees only the interface `core/FBElevationProvider.h`; which implementation
stands behind it is the client's decision:

| Provider | File | Client / switch |
|---|---|---|
| `FBConstantElevation` | `core/` | the primitive foundation |
| `FBRunwayPlateauElevation` | `core/` | `fb-gym --elev const` |
| `FBBakedDemElevation` | `core/` | `fb-gym --elev baked` (the mod's own 90 m raster) |
| **`FBTilesElevation`** | **`world/`** | `fb-gym --elev tiles`, `gpu_native`, WASM — the only LIVE DEM source |

The benefit of this seam: **one** ground truth for everything (mission ground spawn, AGL/radar altitude,
crash detection) — and the same DEM number the renderer draws also goes into JSBSim as
`position/terrain-elevation-asl-ft` (the "crash contract": the gear collides against the terrain one
sees).

### 5 The terrain library (`sim/src/world/terrain/`)

**Our code, not vendored** — a thinned-out version of libosmmesh: terrain only. The vector/building/
line/MVT/PMTiles machinery of the full osmmesh is explicitly out of scope; FlightBox streams terrain.

| File | Content |
|---|---|
| `geo.h` / `geo.cpp` | three conversion layers: **Web Mercator** (EPSG:3857 / slippy tiles) ↔ lon/lat, **MVT local coordinates** → lon/lat, **ENU** (local tangent plane, metres). The only stateful part: the ENU context, which caches sin/cos of the origin |
| `geo_ecef.cpp` | WGS84 **ECEF** conversions + `osmmesh_tile_frac_to_geo` in full double precision. Its own translation unit so that it is 100 % test-coverable (`geo.cpp` carries an unreachable defensive branch) |
| `mesh.h` | the shared mesh container: SoA (`positions`/`normals`/`uvs`/`indices`), ENU metres, the caller owns the memory |
| `terrain.h` / `terrain.cpp` | terrarium PNG → float height grid → regular ENU mesh |
| `osmmesh.h` / `osmmesh_terrain.cpp` | the context: byte provider callback, fetch tile, decode, **stitch**, mesh; plus an LRU of decoded DEMs |

**Limits that are documented and must be kept:**

- ENU is a **flat tangent plane**, not an ECEF round trip (derivation + error bounds §5.0). For global
  rendering that is **not** enough — which is why the end stage (§5.2) reprojects every node exactly.
- Web Mercator latitude is capped at ± 85.05112878° (exactly `atan(sinh(π))·180/π`, WMTS spec / OGC
  Simple Tile Scheme); `osmmesh_geo_to_tile` rejects anything above it.
- MVT `local_y` has its origin at the **top left** (0 = north edge) — opposite to the ENU N axis; the
  fast path `tile_enu_map` handles the sign explicitly.

#### 5.0 The ENU model and its error bounds

The projection, anchored at the ENU origin — the small-angle limit of the real ECEF→ENU transformation:

```
e = (lon − lon0) · cos(lat0) · (π/180) · R
n = (lat − lat0)             · (π/180) · R
u = alt                                          R = 6378137 m (WGS84 equatorial radius)
```

**Curvature error of the n axis.** Third Taylor term of sinh against the linear latitude-to-metre
relation: `Δn ≈ d³ / (6R²)`. At d = 10 km that is **~4.1·10⁻⁶ m**, i.e. micrometres.

**Ellipsoid versus sphere.** ~0.3 % in the scale factor — but a **BIAS, not a shape distortion**.
Because ENU is anchored in the same spherical model all inputs use, the round trip ENU→lon/lat is
exact; the only consequence is that "one metre ENU" is defined against a 6378137 m sphere instead of
against the local ellipsoid radius.

**Consequence:** full ECEF at this place would cost two sqrt/sin/cos per conversion and buy exactly
nothing up to ~100 km. The global path therefore stands BESIDE it (§5.2), not in its place.

**Mercator→local on a tile is likewise linear.** Within a z14 tile (~1.4 km vertical extent) the
Mercator y axis is monotone and nearly linear in latitude. The fast path `tile_enu_map` therefore
interpolates purely linearly between the ENU positions of the tile corners.

**The known residual error of the fast path — a property, not a bug.** `d(lat)/d(y_mvt)` is not
constant (~0.04 % difference top edge against bottom edge) and contributes at most **~0.3 m** of
residual at the vertical midpoint of the tile. That is LARGER than the sub-mm model error named above.
The test suite therefore explicitly checks only the **four tile corners** for 1e-3 m agreement with the
slow `osmmesh_tile_local_to_enu` — there the n linearisation is exact, and both paths use the same
longitude linearisation. A mid-tile cross-comparison would promise a precision that does not exist.
**Do not "fix" this by taking the trigonometry back into the hot loop.**

#### 5.1 Terrarium and the stitching

Height decoding (Mapzen/AWS terrarium specification, every pixel 24 bit RGB):

```
h = R·256 + G + B/256 − 32768   [m]
```

Grid orientation: row 0 = **north**, column 0 = **west** (PNG layout and slippy convention).
`build_mesh` places vertex `(r,c)` at `(c·dx, r·dy)`; because `map->scale_n` is already negative (ENU N
grows northwards, `r` southwards), the positions come out ENU-correct without any further sign change.

**Triangle winding — proved algebraically, not by trial.** If a test fails here: check the mathematics
first, do not flip the sign. With `e = origin_e + lx·scale_e` (scale_e > 0) and
`n = origin_n + ly·scale_n` (scale_n < 0): `P(r+1,c)` lies SOUTH of `P(r,c)`, `P(r,c+1)` EAST of it.

```
Triangle (r,c), (r+1,c), (r+1,c+1):
  A = P(r+1,c)   − P(r,c) = (0, −, 0)     southwards
  B = P(r+1,c+1) − P(r,c) = (+, −, 0)     south-eastwards
  A × B = (0, 0, 0 − (−)·(+)) = (0, 0, +) → points UPWARDS

Triangle (r,c), (r+1,c+1), (r,c+1):
  A = P(r+1,c+1) − P(r,c) = (+, −, 0)
  B = P(r,c+1)   − P(r,c) = (+, 0, 0)
  A × B = (0, 0, (+)·0 − (−)·(+)) = (0, 0, +) → points UPWARDS
```

Both triangles are therefore CCW seen from +z (above), consistent with right-handed, sky-pointing
normals. The test suite checks this by demanding `normal.z ≈ 1` for a flat grid.

**Normals**: per face, area-weighted, accumulated per vertex, normalised at the end. The area weighting
falls out of NOT normalising the face normal BEFORE the accumulation — the unnormalised cross product
has magnitude = 2 · area. For a flat grid all faces have the same normal (0,0,1), so after
re-normalisation the result is exactly (0,0,1).

**Stitching**: `osmmesh_fetch_tile` additionally fetches the four neighbouring tiles and matches the
edge heights — otherwise cracks gape at every tile boundary. Because the same neighbour tile is needed
again for every adjoining target (~15 accesses per output tile), an **LRU of decoded height grids**
sits in front of it (`OM_DEM_LRU_CAP` = 128, switchable off with `FB_NODEMCACHE`) — the PNG decode is
the dominant cold-start cost.

Missing tiles are **not an error**: `osmmesh_fetch_tile` returns `OSMMESH_OK` with `terrain == NULL`.
Absence is not treated as a fault at this level; only decode errors and OOM give negative codes.

#### 5.2 The end stage: `w3_chunk_build_ecef` (`render/FBChunkMesh.h`)

Formally it lies in `render/` but belongs in this chain. It takes **only the height field** from the ENU
mesh (whose edges are already stitched) and reprojects **every** node through the exact Mercator
inverse and geodetic→ECEF. There is therefore neither a tangent-plane error nor a dependency on a fixed
home origin.

| Result | Meaning |
|---|---|
| `verts` | `w3_vtx[]`: `pos` = ECEF offset from the origin (float; at z14 < 2 km → sub-centimetre), `norm` = the real ECEF face normal from cross products of the neighbour offsets (carries the curvature for free), `uv` = `(frac_x, frac_y)` |
| `err` | max \|decimated surface − source height\| in **metres** — measured identically to the ENU path (a height-field property, projection independent). That is the number the LOD is driven by |
| `origin_out` | tile centre in ECEF (double) — the anchor the frame subtracts |

**Skirts**: `skirt = max(2·err, 5 m)`, pulled radially inwards. They cover the sub-pixel cracks between
neighbouring LOD levels — and they are also the reason why a briefly missing neighbour tile is a
tolerable gap. The mesh must be a **regular grid**; a triangle soup is rejected (`return 0`).

### 6 The tile worker

`sim/src/clients/FBTileWorkerMain.cpp` (C++/WASM) + `sim/web/fbtw-worker.js` (JS shim) +
`sim/web/fbtileworker.js/.wasm` (artefact).

**Why its own WASM artefact and not a pthread?** The byte cache is a **main-thread JS map**. A pthread
would proxy every fetch back into exactly the thread one is trying to relieve. A Web Worker owns its
fetch itself. That is why the worker is its own module — **without WebGPU, without JSBSim** — with its
own small export set.

| Aspect | Detail |
|---|---|
| Build | `make -C sim worker` → `web/fbtileworker.js` + `.wasm`. `-sASYNCIFY -sFETCH -sINITIAL_MEMORY=64MB` |
| Exports | `_fbtw_open`, `_fbtw_build`, `_fbtw_verts`, `_fbtw_nverts`, `_fbtw_err`, `_fbtw_origin`, `_fbtw_mips`, `_fbtw_mipbytes`, `_fbtw_ts`, `_fbtw_release` (+ `_malloc`/`_free`) — `extern "C"`, otherwise the mangling silently destroys the names |
| Work in the worker | DEM/albedo fetch, stbi decode, osmmesh meshing **and** the sRGB mip pyramid |
| Return path | finished vertex arrays + finished pyramids as **transferables** (zero-copy over `postMessage`) |
| Concurrency | **ONE build at a time per instance** — `fbtw_build` suspends under ASYNCIFY at the synchronous fetch; an overlapping second call corrupts the shared state. The JS shim therefore calls it `{async: true}` and the main side gates it |
| Copy | the shim copies `verts`/`mips` out with `HEAPU8.slice()` **before** `fbtw_release` frees the heap for the next build |

**If the worker is missing**, the app hangs at start-up **silently**: the worker does not load its
script (404), never reports `opened`, `pump()` never finds a ready worker, no tile is ever built,
`LoadProgress()` stays 0 — and the loading screen stands until the 30 s timeout releases it (then to
empty terrain). Precisely for that reason the make target **`wasm` depends firmly on `worker`** and
always builds both; `make -C sim worker` remains separately callable. That is a dependency in the
Makefile instead of two targets one would have to remember.

### 7 `fb-tiles` from the client's point of view

The server (`tiles/`, its own Makefile, its own image, `tiles/up.sh`) is described here only: what it
delivers, under which endpoints, at which resolution.

#### 7.1 Endpoints

| Endpoint | Answer | Status codes | Client |
|---|---|---|---|
| `/t/terrain/z/x/y` | terrarium RGB PNG (DEM) | 200 / 202 "fetching" / 204 "absent" | osmmesh via the provider callback; `fb_stream_dem` raw |
| `/t/vector/z/x/y` | Mapbox vector tile (pbf) | ditto | server-internal (bakes); the sim client does not ask for it directly |
| `/t/imagery/z/x/y` | JPEG aerial imagery | ditto | server-internal (photo bakes) |
| `/bake/osm/z/x/y?tex=N&v=VER` | rendered OSM albedo (PNG) | 200 / 204 | `fb_stream_pyramid(mode=0)` |
| `/bake/photo/z/x/y?tex=N` | aerial-imagery albedo mosaic | 200 / 204 | `fb_stream_pyramid(mode=1)` |
| `/t/lights/z/x/y` | binary night-light list | 200 (even empty) / 204 (no vector datum) | `fb_stream_lights` |
| `/t/stars/{band}/0/0` | HYG star band, 6 B/star | 200 / 404 | `fb_fetch_stars` (4 bands, concatenated) |
| `/elev?lat=&lon=[&block=1]` | text: one number (m ASL) + newline | 200 / **503 "no dem"** (cold) | nothing in this tree since §3.3; the server's own point-query API |
| `/wx` | global wind/cloud package, binary format `FBWX` | 200 / **503** (no GFS run reachable) | `FBWeatherProvider` — see [`weather.md`](weather.md) |
| `/health` | text statistics line | 200 | operations |

**The status-code semantics are the actual contract:**

| Code | Meaning for the client |
|---|---|
| 200 | terminal — bytes present |
| 202 | accepted, fetch running — **ask again later** |
| 204 | a **real hole**: there is nothing here and there will not be → do not retry (`fb_stream_pyramid` returns −1, `FBWorld` remembers `alt = -1`) |
| 404 / 5xx | transient → retry |
| 503 on `/elev` | DEM still cold; `?block=1` waits instead, up to 3 s |

The difference between **absent** (204) and **empty** (200 with count = 0) is explicitly modelled at
`/t/lights`: a dark ocean tile is something different from a missing vector datum.

#### 7.2 Data sources and resolution

| Kind | Upstream | Max zoom | Content-Type |
|---|---|---|---|
| `terrain` | `s3.amazonaws.com/elevation-tiles-prod/terrarium/z/x/y.png` (Mapzen/AWS terrarium; Copernicus-based) | 15 | image/png |
| `vector` | `tiles.versatiles.org/tiles/osm/z/x/y` (OSM/Shortbread) | 14 | MVT |
| `imagery` | ArcGIS `World_Imagery/MapServer/tile/z/y/x` (Esri; **y/x swapped** in the URL pattern) | 19 | image/jpeg |

From this follows the streamer's `kMaxZ = 14`: finer than the vector source, the OSM bake gains
nothing. The height query `/elev` samples at **z13** (`FB_DEM_Z`), **bilinearly**, with a 24-tile LRU in
memory.

Bakes are requested with `?tex=N` (the client asks for 512; server default 1024). The OSM bakes carry
`?v=FB_OSM_STYLE_VER` (today **11**) — they are `Cache-Control: immutable`, so the URL must change when
the rendering changes (the incident "stripe bug still visible" came from exactly that). Photo bakes
stay unversioned. `sim/src/world/terrain/style_ver.h` and `tiles/src/style_ver.h` are the same number
on both sides.

#### 7.3 In front of the server: nginx

The container front end (`tiles/nginx.conf`) listens on **:8081** and caches itself (`proxy_cache`,
5 GB, 30 d); only misses go to fb-tiles on **127.0.0.1:8082**.

| Rule | Value |
|---|---|
| Cache key | `$uri$is_args$args` — the `?v=`/`?tex=` query IS part of the tile identity |
| Validity | 200 → 30 d, 404 → 1 min; **everything else** (202, 500, 503) is never cached, by omission |
| `proxy_cache_lock` | on, timeout 300 s — N simultaneous misses of the same tile collapse into ONE upstream request, in front of fb-tiles' own in-flight dedup |
| `proxy_read_timeout` | 300 s — a cold bake blocks and must not be cut off |
| Never cached | `/health`, `/elev` (live values) |
| `/wx` (its own `location = /wx` block) | the ONLY cacheable route without `immutable` — see [`weather.md`](weather.md) |

fb-tiles itself: a connection thread pool (`TILES_THREADS`), a disk cache under `TILES_CACHE` (default
`/var/cache/fbtiles`), star bands from `STARS_DIR`.

### 8 On-demand — and why every point on earth is a valid start

Nothing is preloaded. The consequences, in the order in which they take effect:

1. **The root ring is laid around the camera** (`kRootZ` = 8, radius `ceil(ViewM/span) + 1` tiles) —
   there is no area, no "map edge", no file that would have to be there first.
2. **The target cut is pure geometry** (`WantSplit`) and therefore known immediately: a teleport
   requests its end leaves directly instead of building itself up through an LOD ladder.
3. **The loading screen holds the sim** until the target cut is 95 % resident. The first flown frame is
   therefore already fully resolved — and the ground DEM under the spawn is loaded before JSBSim
   integrates for the first time.
4. **Missing data is a defined state, not an error**: 204 = hole (photo falls back to OSM), a missing
   neighbour tile = a gap the skirts cover, cold `/elev` = 503 and the start path asks with `?block=1`.

With that a `spawn` entry in a `.fbm` file is valid anywhere on earth, without any area having to be
defined anywhere.

## State

| Item | State |
|---|---|
| `FBWorld` quadtree | built; distance-based LOD, corrected coverage semantics, 2-phase commit, two modes |
| `FBTerrainLoader` | built; one poll ABI, two byte back-ends (EM_JS async / libcurl blocking) |
| Worker pool | built; N = clamp(hardwareConcurrency − 2, 1, 6), own WASM artefact, transferables |
| `FBTilesElevation` | built; thin pass-through, the only live DEM source |
| The height oracle | built; §3.3, one tile-sampling implementation for wasm and native. `payerne-full --elev tiles` flies to **exit 0 ("stopped on the runway", t = 734.1 s)**; against the ≈ 33 m point cache it replaced it was exit 2, hard landing at t = 719.0 s |
| Terrain library | built; ENU model with documented bounds, terrarium decode, stitching, exact ECEF end stage |
| Night lights | built; EVS night only, three LUTs, 65,536-sprite cap |
| `fb-tiles` client view | documented; six data endpoints plus `/elev`, `/wx` and `/health` |

## Gaps

| Gap | Detail |
|---|---|
| `FBUnitsStage` is NoOp | `FBWorld` already **borrows** the unit registry for the drawing side, but there is no consumer. Other units, weapons and ground targets are invisible in the picture — see [`../render/units-visual.md`](../render/units-visual.md) |
| Terrain masking for sensors is missing | the `const FBWorld*` is passed down into the module `Run()` so that a sensor CAN check lines of sight against terrain — today none does. `sensors/FBRadarSystem` documents this explicitly as a deliberate omission (a DEM raymarch per contact per look). The hook exists, the computation does not. |
| `fb_stream_ground` returns a point, not a field | `FBElevationProvider` already declares `GroundElevPatch` (an area query) for future terrain sampling; `FBTilesElevation` implements only `GroundElevM`. Terrain-following flight, radar-altitude look-ahead and CFIT prediction therefore have no source. |
| The static load path is bring-up legacy | `fb_terrain_load`, `FB_TERRAIN_MAX_TILES` = 64. Both clients run streaming; the static path still exists (including its own code half in `FBTilesStage`) and is never regularly exercised. |
| **The DEM cache is per worker instance** *(parked in the roadmap until this split; now homed here)* | at N = 6 workers the same neighbour tile is fetched and decoded up to six times. Noted in the code as "minor cross-instance cache redundancy accepted" — not measured how expensive it really is at cold start. |
| Eviction is purely time-based | `kGrace` = 180 passes, not memory-based: there is no VRAM or node-pressure trigger apart from the hard cap `kNodeCeil` = 6000, which on being reached simply **refuses every further split**. What happens when a very long flight reaches that cap is undocumented and apparently unmeasured. |
| The night-light cap is a set number | 65,536 sprites ("team-lead cap"), not derived from a measurement. Likewise the class LUTs for colour/radius/brightness — explicitly "cosmetic LUT". |
| **Two bake modes, one single visible switch** *(parked in the roadmap until this split; now homed here)* | SVS (OSM) and EVS (photo) have different lighting semantics ([`../render/renderer.md`](../render/renderer.md)): SVS pins day = 1 and switches stars/lights/clouds off. The switch today is the TAB key in the browser; `gpu_native` has `--albedo osm\|photo`. There is no mission-data layer for it — a `.fbm` cannot declare the picture mode. |
| **TLS is not wired in the tile server** *(parked in the roadmap until this split; now homed here)* | `tiles/nginx.conf`, explicitly noted as a documented gap: `FB_DOMAIN` exists as an env hook, but there is no `listen 443 ssl;` block and no ACME. For central hosting that is a piece of work of its own, not a flag. |

## Knowledge

- **Why line of sight may not cost coverage.** `walk.h` treated "outside the view" at a split like
  "outside the map" and deleted the parent's quadrant. The corrected rule tests the viability of all
  four children side-effect-free BEFORE replacing the parent, so a child beyond the view radius turns
  the parent into a drawn leaf: detail is dropped, area never.
- **Why the target cut is known immediately.** `WantSplit` needs only geometry, no tile data. A boot or
  a teleport can therefore request its end leaves directly instead of climbing an LOD ladder and
  throwing away every intermediate level it built.
- **Why the commit takes two phases.** A tile is drawable only ONE pass after its upload, so that the
  `WriteTexture` is submitted and visible. The `+1` in `ReadyMode` mirrors the renderer's own
  `FrameNo > PhotoUpTick + 1` exactly; without it there would be a one-frame window in the wrong mode.
- **Why the ENU plane is not replaced by full ECEF.** The curvature error of the n axis is
  `Δn ≈ d³ / (6R²)` — micrometres at 10 km — and the ellipsoid difference is a bias, not a shape
  distortion, because everything is anchored in the same spherical model. Full ECEF here would cost two
  sqrt/sin/cos per conversion and buy nothing up to ~100 km. The exact global path therefore stands
  beside it (§5.2) rather than in its place.
- **Why the fast path's residual is not "fixed".** `d(lat)/d(y_mvt)` is not constant and contributes up
  to ~0.3 m at the vertical tile midpoint — larger than the sub-mm model error. The tests therefore
  check only the four corners, where the n linearisation is exact. Taking the trigonometry back into
  the hot loop would buy a precision the data does not have.
- **Why `/elev` is parsed strictly.** `atof` returns 0.0 for an HTML error page, and 0.0 passes the
  validity test, gets cached, and from then on reports "sea level" wherever the aircraft is. The whole
  body must be one finite number, or it is not a measurement.
- **Why a real sample is clamped to ≥ 0 but the sentinel is not.** Open bathymetry is negative, but the
  aircraft's ground reference is the water surface. The `−1e9` "no sample yet" sentinel stays untouched
  so callers can still distinguish "no sample" from "a sample of zero".
- **Why the tile worker is its own WASM module and not a pthread.** The byte cache is a main-thread JS
  map; a pthread would proxy every fetch back into the thread one is trying to relieve. A Web Worker
  owns its fetch. The price is one build at a time per instance (ASYNCIFY suspends at the synchronous
  fetch) and a per-instance DEM cache.
- **Why `wasm` depends on `worker` in the Makefile.** A missing worker makes the app hang silently: no
  `opened`, no pump, no tile, `LoadProgress()` stuck at 0, and the loading screen stands until the 30 s
  timeout drops it to empty terrain. A firm dependency beats two targets one has to remember.
