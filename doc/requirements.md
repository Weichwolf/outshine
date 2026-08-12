# Requirements — the scope ledger

**What this file is.** The whole surface Outshine has to carry, one line per feature, each with a box.
It answers *how much is there*. `doc/todo.md` answers *what next* and carries the acceptance numbers; a
line here may correspond to a todo entry and that is fine.

**It exists by the owner's decision**, which overrides `CLAUDE.md`'s "`doc/` holds three files and gets
no fourth". The override is recorded, not assumed.

**How a line is read.**

| Mark | Meaning |
|---|---|
| `- [ ]` | not built |
| `- [x]` | built — checked in the tree in this round, not remembered |
| `NO SUBSTITUTE` | the reference gets it from an authored asset and no function replaces it. Naming it is worth more than padding the count |
| `REFUSED` | the reference has it and we will not copy it; the reason stands in the same line |
| `TILE` | blocked on data the served vector schema does not carry — a tile-server change, with its cost |
| `TOOL` | the number is missing because the instrument is missing. Effort, not a limit |
| `UNSURE` | I could not confirm the reference has it, and say so rather than assert |

**Every line carries a stable identifier**, `R-<band>.<section>.<ordinal>` — `R-I.8.4` is the fourth
line of § I.8. It is assigned once, in file order, and **never reassigned**: a line whose text is
rewritten keeps its identifier, a line inserted later takes the next free ordinal in its section
rather than pushing its neighbours along. The identifier exists so that a test can name what it holds
(`Test::Covers("R-I.8.4")`, § I.20) and the harness can tabulate the mapping instead of someone
remembering it. Positional numbering would make the mapping rot on the first insertion, which is why
ordinals are allocated and not derived. *Not yet applied to the 1436 lines below — § I.20 carries that
as its own line, and until it is done a test names a line by its section and its text.*

**A ticked line names the file that implements it, and — where its class admits a test (§ I.21) — the
test that holds it.** A tick with no test behind it is a measurement someone took once; a tick with a
test behind it is a measurement the next round retakes for free. The classes that admit no test are
enumerated in § I.21 and a ticked line in one of them says so instead of naming a test.

**Order is the argument.** Six bands — residency, engine, world, vegetation, buildings, vehicles — and inside a
band a line stands after the line it depends on. Nothing is sorted by importance; importance is
`todo.md`'s job.

**References.** Kingdom Come: Deliverance for terrain, vegetation and light — a temperate
central-European vegetation picture on a known budget. GTA 5 for the built world and the verbs, taken
for its **range and construction** and never for its era or its city. The setting is post-scarcity:
modern infrastructure, lush nature.

**Botanical scope is central-European temperate**, because the acceptance places are on the Weser
(Hameln / Emmerthal / Grohnde). A form belonging to another biome says so on its own line.

**Sources for the enumerations.** CryEngine V manual (vegetation, merged meshes, touch bending, road
tool, river tool, water volume, time of day, environment editor, fog volumes, decals) · Warhorse's
published KCD material and console variables (`e_vegetationUseTerrainColorDistance`,
`e_UberlodDistanceRatio`) · Shortbread vector tile schema 1.0 as served by tiles.versatiles.org, which
`tiles/src/tilesrc.cpp` fetches · RAGE `handling.meta` field groups for vehicle construction · GTA V's 23
vehicle classes · Destatis field-crop acreages for which crops a German field actually is · the
Galio odorati-Fagetum and Arrhenatheretum elatioris species lists for the herb and grass layers.

---

## Band 0 — Residency

*Added at the owner's request, 2026-08-11, ahead of the engine band: the session
`sim/logs/demo-walk-wasm-20260811T150518Z.csv` walked a few hundred metres and the world did not
follow. This band is what a streaming engine owes, measured against CryEngine's own mechanisms.*

**Hygiene, not budget.** Far Cry shipped against 256 MB of system memory and a 64 MB card and drew an
island. CryEngine 3.8 holds its render meshes in **24 MB** (`e_StreamCgfPoolSize`) and its textures in
80 MB on a PS3. We hold 231 MiB of linear memory and 228 MiB of device memory for 130 tiles, of which
46 are in view. No line here asks for more memory; every line asks where the bytes went.

**Two thirds of resident tiles being out of view is correct.** The reference prioritises by visibility
and never evicts by it — `e_StreamCgfVisObjPriority`'s own help text names 1.0 as the thrash setting —
because a 180° turn must not stall. The defect is that nothing bounds those bytes.

**Every number in this band is a value at the calibration point** — A18 Pro, 720p60, KCD's picture — and
is an output of the control loop, never a platform constant. A line that hard-codes one is a defect.

**What may move, and the test.** A lever may change how fast the world converges. It may never change
what a stationary observer converges to. That admits prefetch radius, decode concurrency, admission
bytes, pool sizes and worker count. It forbids a residency radius below the view radius — that is not
slower convergence, it is a hole that never closes — and it forbids every dial that changes the
picture, which is pinned for the length of a declared run.

### 0.1 The ledger closes

- [x] Heap, per-pool and device bytes on one telemetry row once a second (`clients/MemoryTelemetry.cpp`)
- [x] Stream ledger: fetch, decode, mesh, DAG, residency and evictions, cumulative and never behind a switch (`clients/StreamTelemetry.cpp`, `world/TilePool.h::Ledger`)
- [x] `heapKB` as live allocated bytes (`core/io/HeapProbe.cpp` — dlmalloc `mallinfo().uordblks`, guarded at static init by the identity `uordblks + fordblks == arena + hblkhd`; the column is EMPTY, never zero, if that fails). 14 336 000 B allocated reports 14 337 120 B, the excess being 140 chunk headers × 8 B; in a live run `heapKB` falls 35 times in 138 rows where the break falls never
- [x] The wasm break published beside live bytes as its own column, since the two answer different questions (`clients/MemoryTelemetry.cpp`, `heapBreakKB`)
- [x] A residual column, published rather than subtracted by the reader (`clients/MemoryTelemetry.cpp`, `heapResidualKB` = `heapKB − poolSumKB` from one heap walk per row) — 51 972 / 66 252 / 22 956 KiB at t=1/11/31 s, peak 90 607, non-monotone in the record — DECIDABLE
- [ ] The residual under a declared ceiling, with every allocation above 1 MiB inside a named pool — DECIDABLE
- [x] The byte cache inside `poolSumKB` (`world/World.h`, `Pools::ByteCache`, and `Pools::Sum` adds all nine fields). It is *not* unspellable: a tenth field omitted from `Sum()` compiles, and `poolRegionsKB` is a published pool that is not a `Pools` field at all and is folded in by hand in `clients/MemoryTelemetry.cpp`. `C.41` does not bear on this — see the registry line below
- [x] The per-thread decoded DEM cache counted as a pool (`world/TilePool.cpp`, `poolDemCacheKB`) — 16 404.42 KiB = 64 full slots × 256 KiB + 4 × 5 228 B of context, i.e. **four threads at their ceiling**, which confirms `TerrainLoader.cpp:41-44`'s "4 MiB a thread, 24 MiB at the six-thread ceiling" to the byte rather than refuting it
- [x] The scheduler's queue, posted set and completed-result map counted as a pool (`world/TilePool.h`, `poolSchedulerKB`). The largest 1 Hz sample is 32 / 282 / 1 645 / 4 420 / 5 873 KiB over five runs of one scene and one binary, so **the peak is not yet known** — the quantity changes every frame and the probe reads it every second
- [ ] Live bytes counted at the allocator instead of walked — `--wrap=malloc,free,realloc,calloc,memalign,aligned_alloc` over one atomic, with `malloc_usable_size` on the free side. Exact, O(1) per call, the same quantity on both translations, available every frame, and it needs no allocator-internal struct: it deletes the `mallinfo` layout guard, the 0.21–1.49 ms walk and the global-lock stall it imposes on the six tile threads
- [ ] Every peak in the ledger kept as a high-water mark where the quantity changes, never as the largest 1 Hz sample — `heapPeakKB` and `poolSchedulerKB` are today the maximum of ~1 reading per second of quantities that move every frame
- [ ] A telemetry row refused when its field count is not its schema's (`core/io/Telemetry.cpp`) — today a source that declares more channels than it pushes shifts every column to its right for the whole run, and the header still looks right — DECIDABLE
- [ ] A pool registry every pool enters at construction, so the ledger is a walk over it — today the list is hand-written in `MemoryTelemetry.cpp` and drifts in silence
- [ ] Container node overhead measured by a counting allocator on the container itself, not modelled from libc++'s red-black node layout (`core/Capacity.h::TreeNodeBytes`) — the model is right today on both platforms and nothing in the tree would notice if it stopped being
- [ ] A declared run that moves live bytes by ≥ 14 MiB **in-client, inside a flat break**, so eviction is shown to be visible in the column that has to show it — DECIDABLE. Not reached: the five runs of `dfdd8e3a82efeefc` give 8 109–9 936 KiB because the byte cache stops growing at 28 970 KiB against a declared 64 MiB budget while `poolHttpGets` freezes at 309–311, which is the streaming defect and not a property of the probe
- [ ] The memory ledger read by something — a gate over the CSV that goes red on a breached ceiling, an empty column that should be full, and a row whose arithmetic does not close — TOOL. No file in this tree reads `heapKB`, `poolSumKB` or `heapResidualKB` today, so "EMPTY, never zero" has no consumer to be right for
- [ ] Device memory under the same discipline: declared budget, per-consumer counters, residual — `devSumMB` reached 227.8 MiB with neither budget nor ceiling (`render/Renderer.h:101`)
- [ ] Every budget line names which memory it constrains, linear or device — separate pools, separate exhaustion, and only one of them has a probe that can fall
- [ ] Pools borrow from one another in a declared grain against one fixed total, instead of each holding a private reservation that idles — Guerrilla's asset and render pools share physical memory in 64 MiB grants, which is what makes a fixed budget elastic without making it larger. `poolRegionsKB` sits at 38 157 KiB, 16 % of the used heap, from t=1 s to t=77 s whether or not anything generates
- [ ] What fixed `WebAssembly.Memory` each engine actually grants with `SharedArrayBuffer` in play — TOOL, one page that allocates and reports. Not yet measured; not a limit

### 0.2 The request and its priority

- [x] One scheduler, one queue, one cache, one in-flight cap for fetch, decode, mesh and DAG (`world/TilePool.h`)
- [x] A declared class order over the queue ahead of distance (`world/TilePool.h:118`, DAG < fetch < mesh)
- [x] Distance measured from a camera the scheduler is told rather than one it reads (`world/TilePool.cpp:196`, `TilePool::Camera`)
- [x] In-flight cap equal to the transport's own concurrency (`world/TilePool.h:112`, `InFlightCap() == Threads_.size()`). The **count is 4, not 6** — `world tilepool threads=4 inFlightCap=4` in 399 of the 420 run logs of 2026-08-11, the rest 1–3 — so the cap is below the six connections per origin the browser allows, and the pool is CPU-bound in the mesh build (`meshCpuMsPerTile` 237.29, 16.9 tiles/s over 4 threads) rather than transport-bound (`httpMsPerGet` 4.45). The invariant is met; the number chosen for it is not the one this line claimed
- [x] The picture's work list ranked by distance and view direction (`world/World.cpp:172-183`)
- [ ] Two admission caps, not one: how many may be in progress and how many may **start** per update — CryEngine holds 32 and 4 (`e_StreamCgfMaxTasksInProgress`, `e_StreamCgfMaxNewTasksPerUpdate`); we have one cap, `World::kMeshBuildsPerPass = 2` (`world/World.h`), and it bounds *installs* rather than *starts*: `World::AdmitMesh` spends it only in the `Ready` arm, so a pass the pool cannot answer asks every candidate (`meshCapped` 217 against `meshWanted` 2 029 402 over `demo/crossing`, 0.011 %)
- [ ] The in-cone boost additive and bounded, so a turn cannot invert the whole order — the reference adds a capped 0.5 to a 10-point importance and documents 1.0 as producing thrash; ours multiplies by 20 (`world/World.cpp:180`, 1.0 against 0.05)
- [ ] Priority recomputed on a declared time slice, not every frame — the reference caps the whole update at 0.4 ms (`e_StreamPredictionUpdateTimeSlice`) and re-sorts at most every 100 ms
- [ ] Priority recomputed only after the camera has moved more than a declared distance (`e_StreamCgfGridUpdateDistance`), so a stationary observer costs nothing
- [ ] Prefetch from camera motion: priority evaluated at the camera advanced by its own velocity over a declared horizon — the reference's is 0.5 s (`e_StreamPredictionAhead`) plus a near/far distance pair. Nothing in `world/` or `clients/Sim.cpp` reads a velocity today
- [ ] The prefetch horizon scales with measured headroom and the residency radius does not — converge sooner on a bigger machine, converge to the same frame
- [ ] A request cancellable: a tile that left the target cut before its bytes returned is dropped rather than decoded and meshed
- [ ] Why the browser pool saturates at three useful threads while native reaches 16.4 tiles/s — TOOL. A property of this design, not of the platform, and unmeasured

### 0.3 Budget, eviction and hysteresis

- [ ] A byte budget per pool declared in one place with its derivation beside it, and eviction against bytes — `kNodeCeil = 6000` (`world/World.cpp:42`) is a count of entries and stops splitting in silence
- [ ] The eviction policy is the reference's and it is neither LRU nor TTL: sort the streamables by importance descending, accumulate their content bytes, unload everything past the point where the sum crosses the budget (`ObjManStreaming.cpp:672-691`). It removes exactly the least important bytes, needs no per-entry timestamp, and costs one sort — which is why the sort is throttled to 10 Hz
- [ ] The eviction unit is one LOD rung, not a whole tile: what is far loses its fine rungs and keeps its coarse one, so evicting costs silhouette rather than coverage. The reference streams mips and mesh LODs and never whole assets
- [ ] A minimum residency in seconds and in metres of camera travel, never in frames — the reference holds a texture 10 s (`r_TexturesStreamingResidencyTime`) and a mesh record 8 priority rounds. `kGrace = 180` passes (`world/World.cpp:33`) is 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds
- [ ] Residency at the full radius rather than the frustum, with the reason on the line — the reference prioritises by visibility and never evicts by it, because a turn must not stall
- [ ] A measured re-fetch rate per tile with a published ceiling, so thrash is detected rather than assumed away — the reference measures it over a 5 s window (`r_TexturesStreamingResidencyTimeTestLimit`)
- [ ] The resident representation per tile declared and bounded — measured 1.56 MiB of device geometry per tile (202.5 MiB over 130) and 70.9 MiB of building heap over one block. Far Cry's answer to 64 MiB of video memory was a smaller resident form, not a cleverer cache
- [ ] The generated-texture cache under the same discipline as every other pool: a declared budget, an eviction unit of one mip of one texture, and a re-generation cost measured rather than assumed. Principle 2 makes a bake normal, which makes the cache load-bearing, and today it appears in no line and no column
- [ ] Shadow map memory declared and counted — four cascades exist with their bytes in no budget (`render/`), and they are device memory, which has no budget at all
- [x] A byte cache with a declared budget, exact LRU and counted evictions (`world/TilePool.cpp:230-254`; 64 MiB, `world/TerrainLoader.cpp:40`)
- [ ] The LRU victim found in O(1) rather than by linear scan under a held lock (`world/TilePool.cpp:236-240`, n ≈ 600 at 64 MiB of z14 tiles) — an intrusive list or a clock hand, per Gregory §6.2.2 on pool allocators, and the choice named on the line
- [x] An evicted tile node releases the collector's device slot, and the slot is recycled rather than freed (`world/World.cpp:534-547`, `clients/Outshine.cpp:309`, `render/stages/TerrainDraw.cpp:810`)
- [ ] Eviction for building prints and verts, water surfaces, courses and levels — the fields grow monotonically and their unit of removal does not exist (`world/BuildingField.h`, `world/WaterField.h`, `world/StreetField.h`); at 70.9 MiB it is the largest single consumer in the tree
- [ ] Every streamed pool a slab of uniform blocks or a ring, which is the argument that no defragmentation pass is needed — and the argument is owed, because both references defragment instead: CryEngine at 64 moves a frame with pin and unpin (`IDefragAllocator::DefragmentTick`, `r_buffer_pool_defrag_max_moves`), Guerrilla at the start of every frame with a 16 MiB copy cap, a three-phase move and a one-frame linger because a block may still be in use, concluding "expensive and complex, but almost no waste". Both defragment because their block sizes come from artists; ours come from us. Under wasm the break cannot be returned, so a slab is the only form in which fragmentation cannot accumulate at all
- [ ] A residency handle minted only by a pool that accepted its byte charge, so "allocated but in no budget" and "resident but not evictable" are unspellable rather than forbidden

### 0.4 Arrival without a stall

- [x] Drawable only one pass after collection, so an upload is submitted before anything references it (`world/World.h`, `World::Ready`)
- [x] A rung the stream refuses retracts the split, so the coarser rung carries the area and the load stops waiting for a tile that will never come (`world/World.h` `MeshState::Vacant` and `World::Splits`, `world/TilePool.cpp` `Poll`). The refusal is held on the parent and not read off the children, because a vacant child carries no mesh and is evicted for being untouched — which re-opens the split; measured at 26 328 evictions against 3 789 builds in 5 min before the refusal moved up. The climb is one rung per pass and terminates at the root ring by construction. Coverage is kept, detail is dropped over the parent's whole quadrant: 11.7 → 23.5 m posting, invisible at 320×180 against a same-binary control whose own floor is 0.002 mean |ΔRGB|
- [ ] A tile becomes visible when its whole residency set is complete — mesh, class, vectors, footprints, water — never one layer at a time. Verified for terrain, unverified for building and water fields
- [ ] Upload per frame as a declared **byte** budget — `World::kMeshBuildsPerPass` (`world/World.h`) admits two items of unbounded size
- [ ] Arrival batched into few large writes: every WebGPU call is validated, so N small buffer writes cost N validations; the batched form is one staging write per frame feeding one indirect draw list
- [ ] No hitch on stream-in, proven on a moving capture: p99 across a 500 m walk against the neighbourhood before each arrival, never against the run mean

### 0.5 Exhaustion, and how it is said

- [ ] Only a **declared** refusal is terminal: a 204 from the tile server, and nothing else. A timeout, a give-up, a transport failure, a 4xx that is not 404-by-contract, a decode failure and a failed allocation are all delays or defects, and none of them may retract a rung — the miss carries its reason to the caller instead of arriving as one absent-shaped answer (`world/TilePool.cpp` `RunMesh`, `Classify`, `Provider`). Ordered after the retraction above because it is what makes the retraction safe, and the reason a permanently coarse quadrant is the quietest failure this streamer can produce
- [ ] A refusal path: a piece of world that does not fit the budget is refused **by name**, counted, and the run continues — the reference logs the object and its size and skips it (`ObjManStreaming.cpp:752-759`)
- [ ] A failed allocation on an elastic path evicts, retries once, then refuses that piece of world
- [ ] A failed allocation anywhere else aborts loudly, naming the item and the bytes (`core/io/Heap.h` exists; `world/TilePool.cpp:359` is its only caller)
- [ ] Refusals and exhaustion published as columns, so they appear in the record and not only in a log line
- [ ] The toolchain's silent-null allocation behaviour turned off, so a null check is handling rather than dead code that looks like it

### 0.6 What makes all of the above decidable

- [x] The eye in every telemetry row — position and look direction (`clients/EyeTelemetry.cpp`, fed only from `Sim::Look`, registered ahead of the stream in `Sim::StartTelemetry`). Geodetic pair, ellipsoidal height, tangent-plane east/north from the standpoint the run began at, path length, yaw and pitch. `Sim::Open` calls `Look` before any run, so no row of an opened scene is blank
- [x] Per-pass admission counters: candidates considered, admitted, rejected and by which cap (`world/World.h`, `World::Admission`, sampled in `clients/StreamTelemetry.cpp`). Two identities hold **by construction**, not by sample — `Wanted == Asked + Capped` from one early return, `Asked == Admitted + Waiting + Absent` from a `switch` over the whole of `TilePool::Reply` — and hold in all 328 rows written so far
- [x] A cumulative counter that cannot be published at 32 bits: `TelemetryRow::Push` and `LogField` carry `double`, `int`, `long long`, `bool` and `std::string` and no `long`, so on both targets a `long` argument is ambiguous and the narrowing is a compile error rather than a wrapped column (`core/io/Telemetry.h`, `core/io/Log.h`). The guarantee is at the **push site only** and an explicit `(int)` walks past it: eight `long` accumulators in `world/StreetField.h`, `world/WaterField.h`, `world/OsmField.h` and `world/ClassField.h` are still published that way (`world/World.cpp:518-519`, `clients/SceneRunner.cpp:261,263`)
- [x] The load says what it is still waiting for, named rather than inferred from a false conjunction (`world/World.h` `World::Await`, `AwaitName`, emitted on `outshine load_stalled`) — six terms, so a stall has one candidate cause instead of six. Not yet a telemetry column, which is what the line below asks for
- [ ] What is holding a candidate back, not merely that it is held: `meshWaiting` conflates "queued" with "in flight", and only the pool's own `poolQueued` against `InFlightCap()` separates them from outside. A column per waiting cause, so the reader needs no second file
- [ ] Arrival latency in the record: the eye's travel between a tile entering the target cut and its mesh becoming drawable, as a distribution — DECIDABLE, and the one thing about streaming the current columns still cannot answer
- [ ] Residency age per tile as a distribution: how long resident, how long since last in the target cut — the input the eviction policy is judged on
- [ ] A declared walk gate: 500 m of travel raises `tilesTotal` and `poolHttpGets`, `tilesEvicted` rises once the budget binds, and p99 holds under 33 ms across the traverse — DECIDABLE, and it fails in CI rather than in a browser. The thresholds are **derived, never assumed**: a rung's ring radius is `span(z) · f / kEdgeTau`, so at f = 623.5 px and `kEdgeTau` = 384 px the z14 ring reaches 2 439 m and `Σ 2πR(z)·v / span(z)²` over the rungs is 0.0184 tiles/s at 1.4 m/s — **500 m buys 6.6 tiles**, and a gate demanding more is a gate that fails on correct behaviour (`demo/walk-500` measured +11 posts, +5 admitted)
- [ ] A gate over command against arrival: the profile's `distM` and the telemetry's `eyeTravelM` agree to a declared tolerance for every declared run — DECIDABLE, catches both a stance that never reaches the eye and a geodesy that scales it (`clients/SceneRunner.cpp:318` is 1.002086× east at 52 °N today), and needs the row's frame index rather than its wall-clock second to compare at all

### 0.7 Headroom and pressure, without a picture dial

- [ ] Streaming classified as non-frame work and scheduled in the idle time of frame work, so headroom is spent and yielded with no lever reading `frameMs` — Guerrilla's split of frame jobs (must complete this frame) from non-frame jobs, three priorities in each, is the whole mechanism; the scheduler is the control loop and no dial exists
- [ ] Pressure paid in detail rungs and never in coverage: the coarse ancestor is resident and stays drawn while the fine rung streams. Horizon holds four resolutions of the same ground at once and gives up mips and LODs when memory is demanded — but its bottom rung is pinned because its map is finite, and ours is the planet, so **nothing is pinned by identity**. What is guaranteed is the ancestor chain of the target cut: bounded by the cut, ~14 nodes deep at z14, travelling with the observer and evicted behind him. A walk around the Earth must not grow a resident set
- [ ] An ancestor rung built and held for the travel case, not only for the cold start — `world/World.cpp:243-253` requests the target leaves directly and builds no intermediate level, so today there is nothing to fall back to and the loading screen is the only absorber
- [ ] The pressure inequality published per frame: arrival latency against prefetch distance ÷ camera speed. Throttling is legitimate exactly while it holds with margin, and the margin is a number rather than a hope — DECIDABLE
- [ ] **The low-level fast pass as the band's hardest declared run.** A fast aeroplane at low altitude is the worst case for streaming, culling and LOD at once: the cut churns at the *finest* rung over a wide swathe rather than trading down to an ancestor, the frustum sweeps ground rather than rotating over it, and arrival latency is measured against a distance the aircraft covers in seconds. Derived, at z14 and Payerne's latitude — tile pitch `40 075 016.686 × cos 46.84° / 2^14` = 1 672 m, so 2.796 km² per tile, and new tiles per second is `2πR·v / A` at residency radius R: **0.021/s walking at 1.4 m/s · 1.46/s at 100 m/s · 3.65/s at 250 m/s, all at R = 6.5 km.** A low-level jet demands ~178× the walking rate, against a measured native pool of 16.4 tiles/s and a browser that saturates at three useful threads
- [ ] **The arrival inequality holds at flight speed**, which today it does not: 250 m/s crosses a 6.5 km residency radius in **26 s**, and the owner's session took **30 s** to make 130 tiles ready. Either arrival gets faster or the prefetch horizon grows with speed — the second is free and is already a line in 0.2 — DECIDABLE, because both sides are measured quantities
- [ ] Culling cost measured under a sweeping frustum rather than a rotating one: a rotation reuses the same candidate set, a translation does not, and only the second is what a flight does
- [ ] Convergence as its own acceptance quantity: after motion stops, `TargetRdy == TargetTot` within a declared T. A comparison compares converged frames; transit is measured as time-to-converge and never mixed into `frameMs`
- [ ] Camera speed may raise the prefetch horizon and reorder the queue, and may never lower a mip or an LOD target — REFUSED: CryEngine's speed-driven mip bias ships disabled (`e_StreamAutoMipFactorSpeedThreshold` = 0.0, with a `MaxDVD` variant naming it a transport workaround), and any dial that changes the picture is pinned for the length of a declared run

## Band I — Engine

### I.1 The machine and the process

- [ ] One declared target whose feature set is fixed and uses no vendor extension — **SDL3 · SDL_GPU · modern C++ · an A18 Pro at 720p60** (`CLAUDE.md` § *The constraints*). *This line read "wasm32 + WebGPU as the fixed target" and was ticked until 2026-08-12; `b83285f` deleted the emcc half of the build, so the target it was ticked for no longer exists, and the one that replaces it is not built either — the frame oracle links native Dawn (`Makefile` `NATIVE_BUILD`, `-lwebgpu_dawn`), which is on neither list. The requirement is that there be **exactly one** and that something build it, not which one it is. Earned at § I.19's SDL_GPU port (`doc/todo.md` step 11).*
- [ ] Native translation as frame oracle (`make walk`) and a **second** translation from one source list — **one translation today**. `b83285f` deleted `make wasm`, `AppWasm.cpp` and the emdawnwebgpu port, and nothing replaced them; the point of the line is that one source list serves more than one host, which is what stops a target-only defect (§ I.18, § I.21). Re-earned when `src/host/` has a second implementation
- [x] One object owns world and renderer and is the only thing that builds a scene (`clients/Outshine`)
- [x] A client is `main()` plus an output medium and nothing else
- [x] Server target that links no `render/` and needs no device (`make world`, `test/clients/WorldMain.cpp`)
- [x] Layering enforced by targets that stop building, never by a checker (`verify-generators`, `verify-world`, `verify-clients`, `verify-types`)
- [x] `core/` is I/O-free by directory; `generators/` cannot spell renderer, world or log
- [x] Declared internal render resolution 1280×720; the canvas only scales it
- [ ] Aspect-preserving letterboxing on a canvas of another shape — declared in `architecture.md`, not found in `PresentStage`
- [x] Bring-up phases as an enumeration rather than booleans
- [ ] Fallible asynchronous bring-up completed outside a constructor, everywhere (`C.41`) — partially held, not audited
- [ ] A gate that fails the build on an unreferenced non-static symbol — `core/Mat4.h` sat entirely dead behind a comment asserting it was not, and no target noticed
- [ ] **A gym: the simulation with no renderer attached, running as fast as the machine allows.** Minutes of world time in a second, so a scenario can be soaked rather than watched. `make world` already links no `render/`, which is half of it; what is missing is a client that steps a declared scenario to a verdict instead of serving tiles
- [ ] The gym holds **no graphics device at all** — DECIDABLE, and the old spec's own check is the right one: no device symbol in the binary, verified with `nm`
- [ ] **Wall-clock speed does not change the result.** Run the same scenario throttled and unthrottled and the fingerprints match — this is principle 7 ("if pace decides the result, the coupling is a bug") made checkable, and it is the reason a gym is worth having beyond speed
- [ ] **Thread count does not change the result** — identical fingerprint over 1…N threads across repetitions. The old gym parallelised exactly one phase and left the rest sequential, which is what made that testable
- [ ] A run's fingerprint as one comparable value, so "the same" and "different" are a string compare rather than a reading
- [ ] The gym runs with no network, from what is on disk
- [ ] **Stability soak as a declared run**: the circling aeroplane and the Payerne circuit flown for hours of world time in minutes of wall time, with drift, growth and blow-up as the verdict. This is what those two scenarios are *for* — a body that flies for ten seconds proves nothing about one that flies for ten hours
- [ ] Frame loop that survives a device loss and re-creates the swap chain
- [ ] Pause / resume without the world losing residency

### I.2 Memory

- [x] Fixed heap, forced by the graphics API refusing a resizable buffer as an argument source
- [x] Heap probe reporting bytes (`core/io/HeapProbe`)
- [x] Stack probe per thread (`core/io/StackProbe`)
- [x] Device-resident picture data with a handle and a time-to-live on the processor, never a second copy
- [ ] Per-thread stack sizes set per purpose rather than one default for a network thread and a mesher

### I.3 Threads and work

- [x] Worker pool where a pthread is a Web Worker, so a synchronous fetch blocks nothing (`world/TilePool`)
- [x] Fetch, decode, mesh and cluster-DAG build off the frame thread
- [ ] Every long-lived thread created at bring-up before the frame loop, with runtime creation a hard failure
- [ ] Thread count taken from what the runtime reports, never from the developer machine
- [ ] Dedicated non-computing threads sized by the protocol's connection limit per origin
- [x] Request-level timeout; no timeout on the load as a whole
- [ ] Audio worklet thread that neither blocks nor allocates in its callback

### I.4 Declaration

- [x] JSON reader in `core` (`core/Json.h`)
- [x] A scenario is a declared world: place, clock, weather, what runs (`mods/*/mod.json`, four of them)
- [ ] JSON schema check of a scenario before it is used, with the failing path named
- [ ] A standpoint the tile scheme cannot carry is **refused by name**, never mapped to a tile — the **declaration** now is, at the earliest possible place and before anything streams (`clients/Scene.cpp:69-78`, verified: `fb_world` at 85.0525 / 85.0530 / 85.0531 / 86.0 N all exit 2 with the latitude and the bound in the message and zero HTTP requests; the 78.2 N control exits 0), and so are the two former `osmmesh_geo_to_tile` call sites, now `TileIndex::Of` with the indices unreachable from the refusal (`world/OsmField.cpp:38`, `world/World.cpp:493`). **The point query is not**: `test/clients/WorldMain.cpp:117` still answers 86 N and 89.9 N with `groundResolved=1 groundAslM=-3448.27`, the DEM row at 85.0511 N, because `tiles/src/tilemath.h` clamps (`doc/bugs.md`). The window in which the old code fabricated tile (0,0) rather than refusing for an unrelated reason was **85.051128779807 < lat ≤ 85.053023927135, 211.7 m** — bounded above by `Schedule::Widest` finding no in-grid row at `RadiusRegions = 1`, not by the projection. `CLAUDE.md`'s *every point on Earth is a valid start* is a claim the tile scheme itself does not hold: closing the gap the other way — a polar scheme beside Mercator — is a second line and the owner's call, not this one
- [ ] A gate that fails the build on `getenv` outside `clients/` — six live variables change the picture or disarm a pass, and the layering targets cannot see them
- [ ] `scenarios/` as the decided directory name — the tree still says `mods/`
- [ ] Declared body format: segments, joints, contacts, force sources, medium, model, materials, brain
- [ ] **Scenario: an RC aeroplane circling one position.** The repository's own first flying thing (`539aebd`, `sim/aircraft/xp_bridge.c`): a level 15° bank held about a home point, altitude constant, reverting to return-to-home beyond a declared radius. The smallest complete airborne scenario there is — one body, one propulsion, one control law, no destination — and therefore the cheapest test of flight that is not a camera on rails
- [ ] **Scenario: a take-off and a landing in Switzerland.** Payerne (LSMP) RWY23, threshold 46.84335 / 6.91523 at 441 m, from `payerne-full.fbm`: a ground start with brakes set on runway heading, a climbing turn over the Broye valley and the Jura foothills, a descending leg onto the extended final, then approach, flare and rollout to a full stop. It exercises ground contact, propulsion, a control law, terrain over a real valley and a runway that exists — and its verdict is decidable, because the aircraft either stops on the runway or does not
- [ ] Declared entity catalogue a generator can fill without editing a closed enum
- [ ] Declared entity catalogue a generator can fill without editing a closed enum
- [ ] Declared capability surface an LLM calls into
- [ ] Declared strata list per ground class, with no global default, so an unclassified place grows nothing
- [x] Declared vegetation class table with per-class densities (`assets/world/vegetation.json`)
- [x] Declared ground-material table with sourced albedo and roughness (`assets/world/ground-materials.json`)
- [x] Declared species files, one per species (`assets/world/species/*.json`)
- [ ] Declared material table for built surfaces with a derivation beside every number
- [ ] Declared environment track over the day (keyed tone shoulder, fog lobe, weather transition length only)
- [ ] Declared weather state with a blend interval
- [ ] Epoch index (three) threaded to every material, vegetation density, building state and road surface
- [ ] Decay index (three) on the same path
- [ ] Epoch and decay as a selection, never an interpolation
- [ ] Epoch and decay reaching geometry or identity — REFUSED: the same dataset must stay the same dataset or the claim is untestable
- [ ] A scripting language for mechanics — REFUSED: function calling over a declared surface, or nothing
- [ ] Quality levels or graphics presets — REFUSED: there is one version during basic development

### I.5 Numbers, units, coordinates

- [x] `float64` ECEF as the truth, `float32` camera-relative, one late conversion (`core/Geodesy.h`)
- [x] Geodetic ↔ ECEF, tile addressing, slippy scheme
- [x] Metres as the only length unit at an interface (`core/Units.h`)
- [x] `uv` in metres, never 0..1, for every mesh that carries one
- [x] Ephemeris for sun and moon at true altitude and azimuth
- [x] Civil time with a declared instant per scene
- [ ] Calendar with a day-of-year that anything seasonal reads
- [x] Keyframe evaluator that knows none of its consumers (`core/Keyframes.h`)
- [x] Determinism: seed derived from the region key, so placement is a property of place (`test/generators/SameRegionSamePlacement.cpp`)
- [ ] Determinism across tile arrival order — a pinned binary does not reproduce its still today
- [ ] `FB_TAU` read from the environment removed — the picture must not depend on an undeclared variable

### I.6 Streaming and loading

- [x] Nothing preloaded; every tile on demand
- [ ] Every point on Earth a valid start — split out of the line above 2026-08-12 and un-ticked on measurement, not on principle: everything past `±85.05112877980659°` is now refused by name at the declaration (`clients/Scene.cpp`), which is right against the tile scheme and false against `CLAUDE.md`'s own sentence. **0.373 % of the Earth's surface**, 1.90 million km² in the two caps together (derived: the fraction outside both caps of a sphere is `1 − sin 85.05113° = 0.0037279`, × 4π·6371 km²), most of it Arctic Ocean and the Antarctic plateau — and it is the owner's call whether it is closed by a second projection or by striking the claim
- [x] Loading as an application phase with a progress fraction, never a renderer state (`ProgressStage`)
- [x] The renderer runs at full rate during loading
- [x] No ceiling and no timeout on the initial load

### I.7 Spatial index and level of detail

- [x] One quadtree over the sphere with a vertical extent per node; it answers *where* and owns nothing
- [ ] Vertical split only where content demands it
- [ ] Tile centre at the node's real ECEF origin everywhere — `World::Center` still puts it at `alt = 0`, so a pedestrian at altitude gets a coarser tile under his feet
- [ ] Split metric whose focal length is the projection's, and which is distance-free under an orthographic camera
- [x] One screen-space-error ladder for every instanced model (`core/ClusterDag.h`, `core/ModelLadder.h`)
- [x] Cluster DAG with model-space error per level
- [x] Impostor rung above the mesh levels, its error anchored on the atlas cell texel
- [ ] Measured screen-space error: render the chosen cut against the finest and difference the silhouette — TOOL, two renders and a difference
- [ ] A stand appears in exactly one rank per frame, counted exactly rather than statistically
- [ ] More than one prototype and more than one impostor atlas resident at a time — `render/ModelDraw` holds a single `SetPrototype` slot and a single atlas (32 784 KB), so a shrub cannot be drawn beside a tree at all; this is the line that blocks every second model kind
- [ ] The impostor atlas under a declared byte budget with an eviction unit of one cell, and a bake that is scheduled as non-frame work rather than run on arrival
- [ ] A prototype's rungs evictable independently of the prototype, so a species seen once at distance costs its impostor and not its four meshes
- [ ] Hysteresis on a rank switch, a minimum observer movement before anything updates, and a per-frame update budget

### I.8 Geometry contract

- [x] Core-defined vertex layout `pos3 @0 · uv2 @12 · nrm3 @20`, 32 B, `static_assert`ed
- [x] Declared second layout `pos3 · nrm3`, 24 B, for the water surface
- [x] Prototype plus instances, never geometry per instance
- [x] Positions as ECEF offsets from a declared anchor
- [ ] The anchor is **bounded by what is drawn from it**, so no shader computes a camera-relative position as the difference of two large floats: `|vertex − anchor|` within the block and `|anchor − eye|` within the view radius, for every field and not only the terrain. The vector fields anchor once at the standpoint and never again (`world/World.cpp` `Open`), which makes the bound the distance travelled — 1 px of near-field jitter at 65 km, 4.3 min of flight; `doc/bugs.md` carries the derivation
- [x] Crack-freedom within a generator's own soup
- [ ] Winding declared once at registration instead of hard-coded at seven sites
- [ ] Mesh invariant check: unit normals, sign agreement with winding, angle agreement with the geometric normal
- [ ] Mesh invariant check: welding, with a split vertex legitimate only where a seam is declared
- [ ] Mesh invariant check: closure, as a declared property of the yield
- [ ] Mesh invariant check: degeneracy — zero area, NaN, index past the end, winding flip within a surface

### I.9 Generator contract

- [x] A generator is a pure `const` function `(Region, Ground) -> Yield` (`generators/Generator.h`)
- [x] `Ground` carries height, slope, class, edge distance, runner-up, source feature and ring, water level, declared tables
- [x] `Ground` carries no camera, frustum, frame index, clock, LOD level, renderer, device, sun or weather — unspellable, not forbidden
- [x] Three products: occupancy, draw, point query
- [x] Occupancy carries bounds, substitute contact body, mass, contact material — never triangles, material or kind
- [x] Draw carries clusters with model-space error, instances, material row — never bounds or mass
- [x] Region pool and schedule, N concurrent regions without a lock
- [x] The engine knows only physics: a trunk is a cylinder; no content taxonomy exists in it
- [x] A generator runs continuously per region, not once at load
- [ ] Actor spawner sharing the region key and handing seed to an entity store — actors are not generators
- [ ] The draw product declares the generator's **capability**: how many rungs it can deliver, the model-space error of each, whether an impostor exists and from which rung it takes over — the renderer optimises against that declaration, and the header that declares it carries this rule as its own comment rather than a document elsewhere
- [ ] Selection on screen-space error alone, one criterion for terrain, trunk, façade and crown — a generator never chooses its own rung and never carries a distance
- [ ] Only what contributes to the image is drawn, and "contributes" is that same error against the same threshold, so a thing too small to change a pixel is never selected rather than culled by a special case
- [ ] Every kind of content on the one cluster DAG the terrain already uses — a second selection path for a second kind of content is the defect this line exists to prevent
- [ ] `DrawSink` truncation reported rather than silent (`ForestDraw.cpp:18`)
- [ ] `RegionPool::Extent::Reached` read by the thing whose budget it claims to bound

### I.10 Render frame

- [x] Forward scene pass; no G-buffer
- [x] As few passes as possible: a pass must beat its own base price of 0.35–0.5 ms before it exists
- [x] A generator's material is a row of numbers with no field that can switch pipeline state
- [x] Core derives discard, two-sided lighting, transmission, blending and emission from what the generator declares
- [ ] Blended transparency ordered back to front inside the scene pass, with a declared budget of blended clusters
- [x] No pipeline creation while playing
- [ ] A title's own entity shader compiled during loading
- [x] Shadow pass, ambient-occlusion pass, exposure pass, temporal pass, present pass
- [ ] The tone-mapping slot in the pass enumeration is empty since the fold — a dead slot is where a pass hides
- [ ] `GpuTimer::Pass::Cloud` is a dead slot

### I.11 Instruments

- [x] Frame telemetry as a time series with scenario, scene, wasm hash and browser version on every line
- [x] Per-pass GPU timestamp pairs — and the published statement that they must not be summed
- [ ] `gpuFrameMs`, one pair spanning the whole encoder, so `Σpass / gpuFrameMs` says whether attribution is even allowed — TOOL, two query slots
- [ ] `frameMs − Σ(spans)` published as its own column, so "unattributed" is measured rather than subtracted by hand
- [ ] Per-pass telemetry published as a distribution instead of a mean (`FrameTelemetry.cpp:66-72`)
- [ ] Overdraw: fragments shaded per output pixel — TOOL, and it is where a forest actually costs
- [ ] Triangle size distribution in projected pixels, p50/p95 — TOOL
- [ ] Culling yield per stage: submitted against visible — TOOL
- [ ] Early rejection count — TOOL
- [x] Run identity on every line
- [ ] Every dial that changes the picture published as its own telemetry column, so two runs of one wasm hash are comparable — TOOL
- [x] Readback of colour and depth, and a PNG writer (`src/clients/Png.cpp`, `src/clients/FileArtifacts.cpp`, held by `Makefile` `verify-still`, which fails if a run writes no still). *Posting the artefacts to `fb-sim` was the second half of this line and `b83285f` deleted `ServerArtifacts.{h,cpp}` with the collector; the artefacts are written to `OUTSHINE_OUT` and nowhere else.*
- [x] `SceneRunner` executing a declared `runs` block natively and writing still and depth
- [ ] The same `runs` block executed by the wasm client, returning still and depth over HTTP — TOOL, a readback and a POST; the sink already exists
- [ ] Cross-client picture comparison on sky/not-sky coverage against a mask frozen on one side, with the self-noise floor published first — TOOL
- [ ] Randomised order within a measurement block — the counterbalanced ABBA design aliases curved drift into the treatment at unity gain and is not an instrument
- [ ] Frame-index-matched comparison on a declared path, instead of a run's p50 as the statistic
- [x] Bench as a layer over the system, never a mode inside it (`WalkBench`, `SubjectBench`, `TreeBench`)
- [ ] `verify-types`' negative gate asserting *why* it fails — any compile error passes it today

### I.12 Physics — one system for walking, driving, flying, swimming

- [x] Substitute contact body as a cylinder with radius, height, mass, contact material (`generators/Body.h`)
- [ ] A contact representation with its own rungs, selected by distance to the observer rather than by the draw's screen-space error — a body far enough to be one impostor cell still needs a correct standing surface, and the two criteria are not the same
- [ ] Collision geometry evicted with the tile that owns it, and its bytes in the ledger under their own name
- [x] Occupancy claimed through a sink, so a proposal and a placement are one type
- [ ] **A segment or joint fails under load.** Not aerodynamic fidelity — the bar is that the engine can tell whether a leg breaks. A declared body's joints and segments carry a load limit, the solver reports the load, and exceeding it is a **state change on the body**, not a log line: the same declaration that carries a walking human's knee carries an undercarriage leg and a branch. This is what makes a hard landing, a fall and a collision have consequences without a second system to model them
- [ ] The failure threshold is declared per material and per section, so it is derived from what the body is made of rather than set per body
- [ ] Rigid-body state: position, orientation, linear and angular velocity, inertia tensor
- [ ] Integrator with a fixed timestep and an interpolated render pose
- [ ] Broad phase over the one spatial index, never a second index
- [ ] Narrow phase: sphere, capsule, box, convex hull, triangle soup (Ericson, ch. 4–5)
- [ ] Contact manifold generation and persistent contact caching
- [ ] Contact solver: restitution, friction with a declared material pair table
- [ ] Joints: hinge, ball, slider, fixed, motorised, with limits
- [ ] Force sources as a declared list, so a wheel, a propeller and a muscle are the same kind of thing
- [ ] Medium: air and water with density, and a body that knows which it is in
- [ ] Buoyancy from displaced volume against the core's water level
- [ ] Aerodynamic and hydrodynamic coefficients per body, not a table lookup
- [ ] Character controller: capsule, gravity, step height, slope limit, ground snap
- [ ] Ragdoll transition from a driven body
- [ ] Sleeping and islanding, so a parked world costs nothing
- [ ] Deterministic solver ordering, because pace deciding the result is a bug (principle 7)
- [ ] Terrain collision against the drawn surface, not against a second heightfield
- [ ] Building collision — `Buildings` deliberately claims no occupancy today, because a cylinder cuts a terrace's neighbours

### I.13 Actors, brains, sensors

- [ ] Entity store with a stable identity, spawned from a region seed
- [ ] A brain's resident cost declared per actor — context, history and sensor view — with a ceiling and an eviction order, so a populated scene has a computable memory price rather than a discovered one
- [ ] A brain that is handed a sensor view and has no name for the world — a type, not a rule
- [ ] Sensor channels: visual contact carrying a TYPE only once angular size gives it away, never a distance or an identity
- [ ] Acoustic sensor
- [ ] A system whose only mutating verb takes a force
- [ ] Whatever builds a prompt cannot read the entity registry — the concrete leak to close
- [ ] LLM function calling over the declared capability surface
- [ ] Regulator brains for the cheap classes
- [ ] Brains only where they are looked at; knowledge never observer-dependent
- [ ] Goals and inner state that survive the world being left and re-entered
- [ ] Animation driven by locomotion rather than leading it, for a human
- [ ] Crowd: pedestrians on the pavement network
- [ ] Fauna: birds, insects, deer, livestock in a field — the reference's meadow has movement in it

### I.14 Input, camera, verbs

- [x] Free camera with a declared stance, eye riding the DEM (`Sim::Look`)
- [x] Orthographic camera for a bird's eye (`demo/ortho`)
- [ ] `Sim &Simulation()`'s non-const overload dropped — it makes moving the eye without the camera basis spellable
- [ ] **Scenario selection in the interactive client**: the declared scenarios offered as a choice, the chosen one named in a resumable address — a URL where the host has one, an argument where it does not — so a chosen scenario survives a restart and can be handed to someone else, and the client loaded with it directly. A scenario settable in three places and declared in none is what cost three rounds on 2026-08-11
- [ ] **A client with an input medium exists, and a declared interactive scene runs.** `Scene::Kind` already has an interactive arm and `b83285f` deleted the only target that could execute one: `test/clients/AppWalk.cpp:101` refuses it (`scene_is_interactive`), so the arm is unreachable and `src/clients/Walker.{h,cpp}` — the stance integrator behind the verb — is compiled with no caller (`doc/bugs.md`). This is not the character controller below it and not a verb; it is the seam without which none of the verbs under it can ever be shown to work, and `CLAUDE.md` § *Setup* already asserts it exists (*"the interactive client is a test; so is the frame oracle"*)
- [ ] Walk, with the character controller under it
- [ ] Run, crouch, jump, climb, vault
- [ ] Swim, with the medium under it
- [ ] Drive, fly, ride — one physics system, three propulsion declarations
- [ ] First and third person
- [ ] Footstep response to the contact material under the foot
- [ ] Interaction: open, carry, use, sit
- [ ] Input rebinding as a declaration

### I.15 Audio

- [ ] Audio worklet with a handed stack, no allocation in its callback
- [ ] Positional mixing with distance attenuation and occlusion
- [ ] Wind in a canopy as a function of the declared wind field, not a loop
- [ ] Water at a weir, rain on a surface, footfall by material
- [ ] Vehicle engine as a function of load and revolutions
- [ ] Reverb from enclosure — depends on whatever answers enclosure for the picture (band II, occlusion)

### I.16 The tile server

- [x] `fb-tiles` serving DEM, OSM vectors, imagery, weather and stars over HTTP, and nothing else
- [ ] A **collector** for the state channel, so a run is reconstructible from something other than the process's own stdout — `b83285f` deleted `SimHost.cpp`, the only implementation of `OUTSHINE_SIM`, and `web/` with it. The emitting half survives and still points at `http://localhost:8080` (`src/clients/ServerLog.cpp`, `src/clients/ServerTelemetry.cpp`, `src/clients/HttpPost.cpp`, `test/clients/AppWalk.cpp:83`), so every run now posts into nothing. Owed with it: `ServerLog`'s own promise that a refused POST is **visible rather than silent** — measured with :8080 closed, a run exits 0 and not one of its 674 log lines names a refusal (`doc/bugs.md`)
- [x] Terrarium DEM tiles served, with **one** decoder in the tree — `src/world/tiles/TerrainGrid.cpp` `FromTerrariumPng`, over SDL3_image. *The line said `shared … by both the client and `tiles/`` and that was the defect rather than the requirement: the sharing was two copies (`doc/bugs.md`). `tiles/` decodes no DEM at all — it passes upstream bytes through (`tiles/src/main.cpp:143-160`) — so there is one decoder because there is one consumer.*
- [x] Shortbread vector tiles from tiles.versatiles.org
- [x] Aerial imagery tiles served (`tiles/src/tilemap.cpp`, Esri World Imagery)
- [ ] Imagery consumed by the engine — served and cached, nothing reads it. Measured 2026-08-12 on the running container: 68 316 imagery tiles, 1.1 GB, whose only consumer in the tree is the photo bake (`tiles/src/raster.cpp:161`), which itself has no consumer (`doc/bugs.md`)
- [x] GRIB2 weather ingest (`tiles/src/grib2.cpp`)
- [x] Star catalogue served
- [x] Peaks endpoint
- [ ] `pois` layer fetched — five layers are fetched today; POIs carry amenity, shop, tourism, man_made, name and housenumber and nothing uses them
- [ ] `addresses` and label layers fetched
- [ ] `boundaries` layer fetched
- [ ] Zoom above 14 for terrain — `/t/terrain/15/…` returns non-PNG, so z14 may be the finest served; unresolved

### I.17 Hardening — a failure is loud, or it is not a failure

*Added 2026-08-11 on the owner's standing order, and on one measurement rather than on a principle: in
the shipping wasm build an index 400 kB past a live `std::vector` writes real bytes and the program
exits 0 — **and so does the native oracle**, because the address is still inside a mapped heap.
Neither target is a safety net for the other, and the instrument that gives one is
AddressSanitizer, which was measured to work on both. Every line here exists to turn a silent write
into a line that names a file. Cost is stated where a line costs frame time; where none is stated the
line was measured free against the 0.35 ms a pass must beat.*

*Corrected 2026-08-12 against whole-program measurement. The costs first written into the run lines
below — 3.8×, 9.8×, 2.84×, 6.2× — were taken on a **bare translation unit over an index-dense loop**,
which is a sanitiser's best case: dense sequential access, hot shadow lines, no locking. The client's
load phase is the opposite case, and one of the three numbers is wrong by a factor of 25 there. A cost
measured on a microbenchmark does not transfer to a program; where both exist, the whole-program figure
is the one that decides, and it stands beside the old one rather than replacing it silently.*

**The mechanism.**

- [x] `NDEBUG` defined by no target, so `assert` ships — measured under emsdk 6.0.3 at `-O2`: the message carries expression, file, line and function, and no `-sASSERTIONS` is needed for it (`Makefile`)
- [x] One allocator behind the global `operator new`/`new[]`, plain and aligned, ending the run with the item and its byte count (`core/io/Heap.cpp`) — so every `std::vector` growth, `std::string`, `make_unique` and tree node is covered without naming it, and `-sABORTING_MALLOC=0` is enforced rather than merely set
- [x] A range type whose subscript asserts its extent (`core/Span.h`)
- [x] An answer whose payload is unreachable without its state, `[[nodiscard]]` on the only door (`core/GroundSample.h`, `core/WaterDepth.h`)
- [x] Negative compile gates that must fail **for the stated reason** — `verify-generators` and `verify-world` both require `file not found` in the compiler's answer and refuse any other failure (`Makefile`). `verify-types` is the third gate and does **not** do this; its line is under *Instruments*, unticked, and it is not covered here
- [ ] A property that is only true on a **narrower** target is decided on that target, on a printed verdict rather than on exit alone — **nothing does this today**. `b83285f` deleted `verify-counters`, `world/gate/CountersDoNotWrap.cpp` and `tools/browser_gate.cjs` with the emcc half of the build, and the property they held — a cumulative counter must not wrap where `long` is 32 bits — is now held by a comment (`src/world/TilePool.h:53-54`). The whole tree is LP64 today, so the defect is unreachable *and* undetectable: it returns the day a 32-bit host does, which § I.18 says will happen. Cheapest replacement that needs no second toolchain: a test that instantiates the ledger's arithmetic at the declared rate and fails on any type whose `std::numeric_limits<>::max()` is under 2^63, which is decidable at compile time and costs no run. *Kept because it is the baseline the replacement is compared against:* checked adversarially when it existed — Checked adversarially — with `TilePool.h`'s accumulator put back to `long` the gate answers `GATE FAIL negative after 2147 blocks: repeats=-2146967677` and exits 1 in 34 s, against `GATE PASS poolRepeats=2147999613` in 37 s unmodified
- [ ] `Span` constructible from `std::vector` and `std::array`, so an extent is taken and never typed
- [ ] `Span::Unchecked(ptr, count)` as the only spelling of a pointer and a count that were never one object, and its site count published — target: C-ABI boundaries only
- [ ] `Span::Sub`'s bound stated without a `size_t` sum that can wrap (`core/Span.h:33`)
- [ ] A rectangular field carrying two extents, so a transposed index is caught instead of legal (`core/Grid.h`) — `world/ChunkMesh.h` keeps rows and columns apart with two macros and a comment today
- [ ] An owned raw block that carries its own extent and frees by scope, taken from the one allocator (`core/io/HeapArray.h`)
- [x] No `malloc` outside `core/io/` — **zero** today (`src/core/io/Heap.cpp`, which holds the only three: `:44` and the two aligned paths at `:66`, `:69`). Measured by grep over `src/` for `malloc(`/`calloc(`/`realloc(`, 2026-08-12, after `src/world/terrain/` was deleted and its decoders rewritten as `src/world/tiles/`, which allocate through `std::vector`. *The **eight** this line carried were `world/terrain/osmmesh_terrain.cpp` (5), `world/terrain/terrain.cpp` (2) and `clients/SimHost.cpp:186`; the first seven went with the directory, the last with `b83285f`. The **seven** before that counted `malloc` and not `calloc`.* **What is not closed by this line:** six raw `free()` calls remain on blocks `Heap::Take` handed out (`clients/Outshine.cpp:108`, `core/ChunkVtx.h:53`, `world/ChunkMesh.h:210-212`, `world/TilePool.cpp:135`) — `R.15` wants the matched pair, and the line below it (`core/io/HeapArray.h`) is where that closes
- [ ] An exhausted heap distinguishable from malformed input at every allocation site
- [ ] A declared heap ceiling honoured inside `core/io/Heap`, so an exhaustion is reproducible on a target that has no fixed linear memory — the shell cannot do it on this host: `ulimit -v` is rejected, `ulimit -d` answers `Invalid argument`, `RLIMIT_AS` reads `9223372036854775807`, and a process allocates and touches 4 GB unrefused

**What the type system must refuse.**

- [ ] `[[nodiscard]]` on every function returning `bool` or a house enumeration — **214 of 214, 0 uncovered when it was last measured, and held by nothing since**: `tools/hardening_ledger.py` and its `verify-hardening` gate were deleted 2026-08-12 under the owner's ruling on Python validators, so a new unattributed `bool Foo()` now enters silently. Re-earned by a test in the suite (§ I.20). *Un-ticked on what holds it, not on the reading — the reading below stands and is the baseline the replacement is compared against.* *Corrected 2026-08-12 against that instrument; the figures below it are what the section carried before and the derivation of each difference is why they moved.* The population was written as **216 AST / 212 regex**: the ledger says **214**, because two returns name a type that is both a house enumeration and a house record — `Clients::Outshine::Stream -> Progress` and `Generators::GeneratorSet::RankAt -> Rank` — and the tool reports an ambiguity rather than guessing it (`nodiscardAmbiguousReturn`). The regex figure was low for the reason the tool was written: a redeclaration does not carry the attribute in clang's dump, so identity comes from the `prev` chain and a bare `grep` reads `clients/Png.cpp:8 bool EncodePng` and `clients/Species.cpp:8 bool ReadSpecies` as uncovered when their declarations carry it. **The rule as enforced is wider than the rule as written here** and this line now says so: the population is `bool` or **any** house enumeration, not only a status one, which is what pulled `SurfaceState::Kind`, `Keyframes::How`, `Json::Ref::GetKind`, `TreeSpecies::LeafKind KindOf`, `FacadeStyle StyleOf`, `BuildingUse UseOf`, `RoofKind RoofOf` and `TreeGrower::LeaderEnd` in. An enumeration that carries `[[nodiscard]]` on the **type** covers every function returning it and none of them is counted uncovered — that is the shape to prefer, because it cannot be forgotten at a new function. *The 38-against-134 first written here was wrong in both numbers and the derivation is why.* **38** counted every attribute in the tree, of which **10** sat on returns that are neither `bool` nor an enumeration (`std::optional` ×2 in `generators/Infrastructure.h`, `generators/Body.h`; `uint32_t Proposes` ×5; `Claim` ×2; `WaterDepth` ×1), so the covered population was **28**. No subset rule reaches 134: headers-only is 188, `bool`-only 164, declaration-without-inline-body 99. *The line once read "what holds it is now the gate and not this line"; the gate is deleted and the sentence is now false, which is exactly the failure mode a count in a document has.* Densest carriers: `clients/Scene.h` (12), `world/World.h` (11), `world/TilePool.h` (10), `clients/Sim.h` (8)
- [ ] A house wrapper carrying `[[nodiscard]]` for every third-party call that returns a status — the attribute cannot be added to emscripten's, Dawn's, curl's **or libc's** declarations, and libc is by far the largest: **88 discarded results by the ledger's count, 78 distinct sites** (`thirdPartyDiscarded`, `tools/hardening_ledger.py`), against curl 14, emscripten 1 and Dawn 0. *The **30 sites, 27 discarded** first written here is not the same population and was not wrong within its own: it counted only status-returning file operations, and the ledger counts every libc result nobody reads.* Split by callee: the file operations are **40** — `fclose` 19, `fseek` 12, `mkdir` 3, `sscanf` 2, `fputc`/`fputs`/`fwrite`/`closedir` 1 each — and the remaining **48** are formatted output, `snprintf` 31, `fprintf` 15, `printf` 2, whose result is a truncation indicator. The two want different answers: a discarded `fclose` on a write path is a buffered write that failed and said nothing, while a discarded `snprintf` is a silent truncation and the house answer to it is a formatting call that cannot truncate. **The ledger's own figure is inflated by 10** until `doc/bugs.md`'s per-inclusion defect is fixed
- [ ] A status that is read and only logged counts as discarded — a failed input binding refuses the run rather than reporting it, because a scene with no keyboard is not a scene
- [ ] A `(void)` cast of a house `[[nodiscard]]` counts as a discard, and its site count is published — **6 today**, all on a `Try*(T *out)` behind a pre-initialised zero (`generators/GroundPatch.cpp:31`, `generators/Buildings.cpp:54`, `generators/Water.cpp:20`, `generators/Water.cpp:53`, `test/clients/WorldMain.cpp:64-65`), out of **8** `(void)` casts of any call with a result — the other two are `emscripten_asm_const_int` behind `EM_ASM` at `clients/AppWasm.cpp:183,362`, which only the ledger's AST pass sees because the macro hides the callee from `grep`. *The **28** first written here counted every `(void)` token, unused-parameter casts included.* Measured by `tools/hardening_ledger.py` (`voidCastsOfHouseNodiscard`, `voidCasts`), confirmed independently by grep. Target: unused-parameter suppression only. This is the one spelling that defeats the attribute above, so the ledger that counts one must count the other
- [ ] A record whose validity depends on a field it does not require is refused by its own constructor (`C.41`, `C.42`) — `Generators::FeatureField::Feature` is an aggregate with `FeatureLevel Top = None()`, and the rule that a `Structure` or a `Water` always carries one is enforced nowhere except by `clients/Sim.cpp:218` and `:226` happening to be the only two places that mint one. A named factory taking the top as an argument deletes three of the six `(void)` sites above outright
- [x] A C-ABI status reaches a house type at the boundary, so the attribute applies to it — closed by **deleting the C ABI**, which is the stronger of the two answers this line offered. `world/terrain/`'s seven `int`-returning statuses are gone with the directory; their successors in `src/world/tiles/` are state-carrying answers whose payload is unreachable from the failing state — `TileIndex::Where`/`TryXy`, `EnuFrame::Where`/`TryFromGeo`/`TryToGeo`, `TerrainGrid::Where`/`TryField`, `TerrainMesh::Where`/`TryPositionsEnuM` (`world/tiles/TileGeodesy.h`, `world/tiles/TerrainGrid.h`), each `[[nodiscard]]`. Zero `osmmesh_` names survive anywhere in `src/`. *Owed and now recorded as a bug rather than as this line:* `TerrainSource::TakeTerrainPng` (`world/tiles/TerrainTiles.h:23`) is the one seam that did **not** take the shape — it returns bare bytes and four answers collapse into `{}` (`doc/bugs.md`), and § I.22 is where it is settled
- [ ] A standpoint is a type that cannot be constructed outside the Web Mercator band, so every entry point is refused by construction and none of them repeats the test — three exist today and they disagree: `clients/Scene.cpp:69-78` refuses at the declaration, `clients/Sim.h:77` `SetStance` validates nothing, and `test/clients/WorldMain.cpp:117` reads `argv` through `std::atof` into `Sim::At(double, double)` and answers the North Pole with the ground of 85.0511 N (`doc/bugs.md`). `Clients::Sim::Stance` is four defaulted doubles — `C.41`, `C.42`: a record whose validity depends on a field it does not require. The band is `world/tiles/TileMath.h`'s `kMercatorLatMaxDeg`, so the type belongs in `world/`, and `Sim::At` then takes it instead of two loose doubles
- [ ] No `default:` in a `switch` over a house enumeration, so a new state is a compile error under `-Wswitch -Werror` — **one today, from zero**: `world/TilePool.cpp:425` puts a `default:` over `TerrainMesh::State` and hides three distinct defects behind it (`doc/bugs.md`), written 2026-08-12 in the round that replaced the C decoder. *Un-ticked on the tree, not on the rule.* The reading below is what the line carried when it was true, and it is the baseline: **zero**, from three (`clients/SceneRunner.cpp:167` over `Outshine::Phase`, `generators/draw/TreeGrower.cpp:219` over `Architecture`, and a third at `clients/AppWasm.cpp:374` that went with the file in `b83285f`). *The five first written here counted every `default:` label, and two of those five are over a `char` escape and a protobuf wire type* (`core/Json.cpp:142`, `world/OsmVector.cpp:49`) — neither is a house enumeration and both correctly survive, and `tools/hardening_ledger.py` now publishes both numbers beside each other (`defaultOverHouseEnum` 0, `defaultLabels` 2). Held by the compiler rather than by a rule: `-Wall` carries `-Wswitch` and `Makefile`'s `CXX_WARN` carries `-Werror` (`EMCC_WARN` went with the emcc half), so a new enumerator now fails to build at both surviving sites instead of falling into the arm that reports failure
- [ ] A producer returns its product, and its consumer takes the product as an argument and cannot be called without one — the trim of an absent roof covering must not compile
- [ ] A multi-state answer whose states carry different data, so two arms cannot be written identically by accident
- [x] A bench that wrote no rows exits non-zero and names where it looked — `treebench` refuses an empty or missing species directory with `treebench: no plant declaration under <dir>` (`test/clients/TreeBench.cpp:94-97`), and the refusal is held by a run-time gate that also holds the unknown growth form (`Makefile` `verify-refusals`, 4 s)
- [ ] A two-phase object either asserts its phase on every accessor or returns a second type from its close, and the second is preferred where a thread does not forbid it (`render/ClusterCut.h` is reshapeable, `world/ClassBuilder` is not)
- [ ] Where the missing datum is a **construction** parameter, neither of those two: no default constructor and the datum in the constructor (`C.41`), so the unphased state has no spelling at all. `world/BuildingField` and `world/WaterField` take their ECEF origin through a setter guarded by two assertions (`AnchorAt`, `assert(Anchored_)`); the origin is known one call earlier and could be a constructor argument, which costs `World`, `Sim` and `Outshine` a constructor parameter each and deletes `Sim::SetStance` by resolving the snapshot standpoint before the client is built (`test/clients/AppWalk.cpp` `Stand`)

**Where an assertion earns its place.**

- [ ] `static_assert` on every enumeration-sized table, every `kCount` beside an enumeration, and on the declared 32 B vertex layout — **11 in the program** (`core` 6, `generators` 3, `render/stages` 2) plus 8 in declared negatives that ship in nothing (`test/generators/SameRegionSamePlacement.cpp` 5, `test/world/CountersDoNotWrap.cpp` 3), against the **16** first written here; none in `core/FacadeUv.h`, none on the layout `CLAUDE.md` states. Measured by `tools/hardening_ledger.py` (`staticAsserts`), which counts the 103 translation units the program is built from and every header, and not the 14 gate units
- [ ] A runtime assertion in every `render/stages/` file stating the extent its buffer writes assume — 19 files, zero assertions. The whole program carries **16** runtime assertions against 11 `static_assert`s (`world` 6, `generators` 5, `core` 2, `clients` 1, `generators/draw` 1, `render` 1, `render/stages` 0; `tools/hardening_ledger.py`, `asserts`) — *the single **28** this section once carried was the two kinds added together*
- [ ] A runtime assertion in `world/tiles/` stating the grid invariant its pointer arithmetic assumes — **still zero** after the C-ABI rewrite, and the subject is now house C++ rather than a C island, so nothing excuses it. `world/tiles/TerrainGrid.h:35-36` indexes `HeightsM_[(size_t)row * Cols_ + col]` unchecked and `AtM(row, col)`/`SetM(row, col)` take two adjacent `uint32_t` in an order a caller can swap (`I.24`), `world/tiles/TileMath.h:32` `Bilinear` takes a bare `const float *` with two extents beside it, and `world/ChunkMesh.h:31-38` re-derives the column count from the data instead of being told it (`doc/bugs.md`). **`TerrainField` is `core/Grid.h` in all but name and assertion** — it already carries both extents — so the cheapest close is to promote it to `core/` and give it the assert, which retires this line and the `core/Grid.h` line above together
- [ ] A bounds situation resolved by stating the invariant, never by clamping an index into range

**The declared runs, and the target that runs them.**

- [x] `make gates`: one target running every negative gate and every declared sanitised run, one line per gate, every gate run even after one has fallen, non-zero on any failure (`Makefile`, `GATES`, `RUN_GATES`). **Eight gates, 6 m 58 s** as of `b83285f` — `verify-generators`, `verify-clients`, `verify-types`, `verify-world`, `verify-refusals`, `verify-walk` in `GATES_BUILD`; `verify-still`, `verify-walk-asan` in `GATES_RUN`. *Three are gone rather than green: `verify-counters` and the emcc gate died with the wasm target (§ I.17 above), `verify-hardening` with the Python ledger.* The eleven-gate 8 m 24 s and seven-gate 6 m 13 s readings are what the line carried before, kept so the cost of the set is comparable across the cut: `verify-generators` 14 s · `verify-clients` 0 s · `verify-types` 0 s · `verify-world` 1 s · `verify-counters` 41 s · `verify-refusals` 4 s · `verify-walk-asan` 313 s. Checked adversarially — the treebench refusal reverted gives `FAIL verify-refusals`, `gates: RED`, exit 2, and the other gates still run
- [ ] The gate set split by what it decides, so the edit loop has one it will actually run: the five that need neither a browser nor a 10 800-frame walk — `verify-generators`, `verify-refusals`, `verify-world`, `verify-clients`, `verify-types` — cost **17.4 s** together (measured 2026-08-12; the 19 s first written here was the same five), against the full set's 8 m 24 s, and a gate that is skipped is not a defence. The split is by **kind** and used as if it were by **cost**, which is why `verify-hardening` is spliced into `GATES` by hand between the two sets instead of belonging to one
- [ ] Every gate declares its own cost, and both the fast set and the full set are derived from that declaration rather than typed out — today a gate joins a set by being named in it, so a gate whose cost changes silently changes what the edit loop runs
- [ ] The hardening ledger scans only what changed — TOOL. 97 s for 110 clang AST dumps is 0.88 s each and almost all of it is re-parsing the same headers; the build already writes dependency files (`DEPFLAGS`), so a per-unit result cached against the hash of its dependency set makes the ledger a warm-tree gate of a few seconds and lets it join the fast set. Not a limit, an unbuilt tool
- [ ] A gate that builds the **shipping wasm module** — the emcc translation of `render/` and `clients/` is covered by no gate today, and it is the only build that ships; the sanitised native gate covers the native compile of the same sources and nothing covers this one. Priced: 4.8 s with warm objects, and the link is reproducible (`web/gpu.wasm` sha256 `b56aac97b2b47638…` on two consecutive links)
- [ ] A gate that builds the **container image**, and builds it from the declared sources alone — three of the eight gates need `fb-tiles` on :8081 and nothing checks that its image can be produced. Three independent defects were live at once and all three were found by running `podman build` once, in three minutes (`doc/bugs.md`): `make` absent from the build stage, eight translation units failing `-Werror` under the image's own g++ that pass under host clang, and — the one that turns a red into a green — `COPY tiles/` dragging the host's `tiles/fb-tiles` into the context, after which `make` finds its target up to date and a **Mach-O binary is installed into a Debian image**. The last is what makes *from the declared sources alone* part of the requirement rather than a nicety: a `.dockerignore` or an explicit source list, so the image cannot depend on the working tree's untracked state
- [x] A gate that the declared still is **one picture**: one binary, one scene, N runs, one sha256 — because every A/B comparison in this repository silently assumes it (`Makefile` `verify-still`, in `GATES_RUN`; `test/world/tile_delay.py`; 103 s under Node, unmeasured since the port). **The order is imposed rather than sampled**: the proxy holds each tile response back by `FNV-1a(path, seed) mod 400 ms`, so one seed is one reproducible arrival order, and four seeds are the experiment. The runs are compared to each other and to no recorded value, so the gate says *deterministic* and never *unchanged*. Checked adversarially by the architect, `acc8478` sources built out of tree against the same gate: **RED**, `2 ingest orders drew 2 different pictures` — `23287811d36ef7fe…` at `buildingTris=135168` for the order whose first ingest is 13 footprints, `b5a9062a7593d3b2…` at `134586` for the order whose first is 10; on the fixed sources the same four seeds give one picture, `852bd4246ee34f65…` at `134990`
- [ ] The declared still's identity is a hash of the **decoded RGBA buffer**, taken before any encoder is called, and the encoded file's sha is not the pinned number — because the file is not the subject and every encoder, container and API change moves it while the picture stands still. Measured 2026-08-12: replacing `stbi_write_png_to_func` with `IMG_SavePNG_IO` in `src/clients/Png.cpp` moved the pin `852bd4246ee34f65` → `bec69fea0a4e6837` with **zero of 3 686 400 channels changed**, shown decidably rather than by comparing against a stored copy — decoding each seed's new `walk.png` and re-encoding it with the deleted stb encoder at `859f702`'s settings reproduces `852bd4246ee34f65` exactly, four of four (`doc/bugs.md`). Costs one hash over 3.69 MB the client already holds; buys that § I.17's wasm gate and § 3.4's SDL_GPU port are read as picture changes only when they are one
- [ ] A gate that samples a non-deterministic input **refuses when its sample did not vary** — `verify-still` counts the distinct ingest orders it produced and fails at fewer than two, because a repeat-based gate whose input happens not to move is green for the wrong reason and reports full confidence. Measured why it is needed: six plain warm repeats of `demo/frame` on the defective sources gave two pictures 4/2, and a run of six identical answers has probability ≈ 0.09 at that rate — a gate whose power is a coin toss cannot state its own power. Open because the clause exists in one gate and the property belongs to every repeat-based one, and because `verify-still`'s own coverage is 2 orders out of 9! and it does not say so
- [ ] Every declared run states its **instrument** in its own identity field, so a sanitised row cannot enter the archive as a shipping row — `client=gpu_walk` is a string literal today (`test/clients/AppWalk.cpp:74`) and `doc/bugs.md` carries what it has already written into `sim/logs/`
- [ ] The frame count of a declared sanitised run derived from where its coverage stops growing rather than from habit — TOOL, one `-fcoverage-mapping` build and three run lengths; 10 800 frames is an assumption nobody has measured
- [x] A declared native run under `-fsanitize=address,undefined` — `demo/walk-500`, 10 800 frames, `-fno-sanitize-recover=undefined`, three runs, zero reports (`Makefile`: `walk-asan`, `verify-walk-asan`). Whole-program cost, superseding the 3.8×/9.8× microbenchmark: wall 300.2 s against 184.7 s (1.63×), tile-mesh CPU 639.2 ms/tile against 161.1 (3.97×, the microbenchmark's 3.83× transferring here), and the frame distribution p50 26.6 / p95 27.7 / **p99 42.6** ms against 16.4 / 19.2 / 20.6 — **the sanitised oracle does not hold the 33 ms floor and is not asked to**, because what this gate decides is whether the sanitiser speaks. What makes the run comparable at all is that it walks the same world: `impostorStands=9565 treeTris=19130`, identical to the unsanitised run, and the path is a declared 30 fps replay rather than wall-clock motion. Checked adversarially — a one-word write past `TreePrototype::Ranks()` gives `heap-buffer-overflow … WRITE of size 4` naming `Outshine.cpp:197` and the allocation at `Heap.cpp:56`
- [ ] A declared wasm run under `-fsanitize=address` on the shipping flags — it **links** (`Makefile` `wasm-asan`; ASan × emdawnwebgpu × `-pthread` was the named risk and it did not materialise), boots in the same Chromium, and then does not finish `demo/frame`, **one frame**, in 480 s at ~4 cores saturated against 28.3 s unsanitised: a floor of **72×**, not the bare-translation-unit 2.84×. `user 1911 s / real 481 s` says CPU-bound rather than paged, so it is the load loop's ~190 kHz spin with every `std::set` operation and every mutex acquisition instrumented — a property of a defect this tree already carries, not of the host and not of the instrument. **Blocked on the load loop that waits instead of spinning, not refused.** Note it can decide nothing about the shipping heap in any case: emcc raises `-sINITIAL_MEMORY=296MB` to 474 611 712 B in the ASan module, 53 % more linear memory than the client that ships
- [ ] `-sSAFE_HEAP` — REFUSED: measured 6.2×, the most expensive instrument of the set, and it tests only a null and a write past the break; it did not catch the write into a neighbouring allocation that AddressSanitizer caught at half the price
- [ ] `.at()` as the bounds mechanism — REFUSED: measured, its out-of-range prints `Aborted(undefined)` with no file, line or index in the six wasm compile groups built without exceptions, and escapes `emscripten_set_main_loop` with the run's telemetry unflushed in the other two
- [ ] Exceptions as a failure channel — REFUSED, and the evidence has to be retaken: the argument was that six of the eight **wasm** compile groups built without them (`EM` against `EMPP`), and `b83285f` deleted both. No native compile line passes `-fno-exceptions` or `-fexceptions` today, so exceptions are simply on and nothing states a policy; the tree contains **two** `catch` sites. What must replace the deleted evidence: `-fno-exceptions` on every group that can take it, which is a compile error at the first `throw` and needs no gate
- [ ] The hardening ledger as published counts — unchecked subscripts, `Span::Unchecked` sites, `malloc` sites, bool-returns without `[[nodiscard]]`, `default:` labels, assertions per directory — TOOL, one instrument, so that "pristine" is a number instead of an opinion. **Un-ticked 2026-08-12 on measurement, not on principle**: the Python instrument and its `verify-hardening` gate were deleted the same day under the owner's ruling that validators are C++ under `test/`, so the counts below are the record of one reading nobody can retake (`doc/bugs.md`). What it must become: a test in the suite (§ I.20) that shells out to clang's AST the same way, so the instrument is rebuilt rather than the counts re-remembered. The reading it took, kept because it is the baseline the replacement is compared against: (`tools/hardening_ledger.py`, baseline `tools/hardening_ledger.json`, gate `verify-hardening` in `Makefile`, 97 s). Sixteen counts over the **103 translation units the program is built from**, taken from the build itself (`make print-sources`) so the 14 declared negatives under `src/*/gate/` are outside the subject, with the seven units that name emscripten walked under `em++` as well because a native pass cannot see inside `#ifdef __EMSCRIPTEN__`. The `[[nodiscard]]` population comes from clang's AST and the `prev` chain, not from a regex. The parse itself is guarded: `translationUnits` is a gated count, so a unit that stops parsing under either toolchain fails the gate instead of quietly leaving the population. Demonstrated red both ways — an unattributed `bool Fits()` gives `WORSE nodiscardUncovered: 1 > 0`, and an improvement fails until the baseline is rewritten in the same commit. **Two defects stand against it in `doc/bugs.md`** and neither un-ticks this line: three site lists count per inclusion instead of per site, and `--update` writes a regression into the baseline without comparing
- [ ] The em++ pass is chosen by what a unit **reaches**, not by what its own text says — `names_emscripten` greps the `.cpp` only, so an `#ifdef __EMSCRIPTEN__` placed in a header would silently narrow the subject with no count moving. Zero headers name it today, which is what makes this cheap to close: assert that, or take the dependency set from the compiler

---

## Band I (continued) — the engine as a library

*Added 2026-08-12 on five rulings from the owner in one session: tests are C++ under `test/` in
libc-test's shape with a shell harness and no Python; **Outshine must work as a library** that the
clients and the tests both merely load; **the library must be platform agnostic — a client using it
runs on any platform**; **SDL3 is the platform layer**; and finally **SDL_GPU is the graphics API,
every framing is removed, and there are exactly four constraints — SDL3 · SDL_GPU · modern C++ ·
this device at 720p60** (`CLAUDE.md` `392527f`). The fifth ruling supersedes the fourth's ordering:
the development platform **is** the target — Apple A18 Pro, 2+4 cores, 5 GPU cores, 8 GB, Metal 4 —
so there is no machine between the work and the budget, and wasm becomes a port destination like any
other rather than the frame everything is shaped by.*

**Why this is scope and not engineering taste.** This file's first page names CryEngine and Kingdom
Come: Deliverance as the level to match. CryEngine shipped one codebase on Windows, Linux, PS4, Xbox
One, WiiU and Android; KCD is CryEngine 3.8.6 on PC and both consoles. *Portable* and *tested* are
contents of that claim, and the claim has never been priced. These four sections price it.

**What the reference's platform seam actually abstracted**, read rather than assumed
(`CryEngine/CryCommon/platform.h` and `CryCore/Platform` as published in aws/lumberyard): thread-local
storage (`TLS_DECLARE`/`TLS_GET`/`TLS_SET`), current thread id, `CrySleep`, alignment (`Align`,
`IsAligned`, `TARGET_DEFAULT_ALIGN`, `_ALIGN`), memory-ordering barriers (`READ_WRITE_BARRIER`,
`MEMORY_RW_REORDERING_BARRIER`), `CryDebugBreak`, heap check, a few filesystem attribute calls, and
the platform/CPU detection macros — with one `<Platform>Specific.h` per target. **What it did not
abstract is as informative:** the graphics API is not in the platform layer at all — there is a
renderer *per* API — and file access, input and audio are separate `CryCommon` interfaces (`ICryPak`,
`IInput`, the Audio Translation Layer), not platform macros. So the shape is **two tiers**: a thin
low seam of primitives, and a set of ordinary abstract interfaces for services. Where it does not
transfer: CryEngine assumed threads on every target it shipped on. § I.18 does not.

### I.18 The library and its host interface

- [ ] `liboutshine` as the product `clients/` and `test/` both merely link, and the only thing either links — **not one archive**: one static archive per compile group (`Core`, `Generators`, `Draw`, `World`, `Render`, `App`), each built from the include set that group already compiles with, so a single archive cannot dissolve the layering the include sets carry
- [ ] The layering stays a **compile error and not a link rule** — an archive imposes no direction, so `-Isrc/core -Isrc/generators` remains what makes `Renderer` unspellable in a generator, and the archives only decide what a binary contains
- [ ] `src/host/` as the bottom layer, below `core/`: abstract interfaces only, no implementation, no libc beyond `<cstdint>`/`<cstddef>` — `core/` may name it, nothing above it may be named by it
- [ ] `Host::Clock` — one monotonic `NowMs()`. Always present; a target without a clock cannot run a frame
- [ ] `Host::Allocator` — `Allocate`/`Free`/`UsableBytes`, the one door `core/io/Heap` goes through. Always present
- [ ] `Host::Storage` — read a declared asset by name into a caller's buffer, one call per file. Always present; it is how a mod is read on both targets and it deletes the three `fopen` sites in `world/`
- [ ] `Host::Fetch` — a byte fetch that is **asynchronous by shape**: submit returns a handle, the caller polls, and cancel is a real operation. A blocking form is unspellable, because the browser cannot offer one on the thread that draws
- [ ] `Host::Executor` — `Submit(Job&)` and `Drain()`, never a thread. **A host with zero workers must be a legal host**: the work then runs inline on `Drain()` and residency converges more slowly, which is exactly the lever § 0.7 already admits — *a lever may change how fast the world converges, never what a stationary observer converges to*
- [ ] `Host::LogSink` and `Host::TelemetrySink` — where `core/io`'s two services already point, rebased on the interface instead of on the process
- [ ] **The library owns its log and its telemetry; a consumer says only *where* they go** — a path, stdout, stderr — and there is no collector to post to and no server that can be absent. Owner's ruling, 2026-08-12. The measurement that makes it a defect rather than a preference: `SimHost.cpp` was the only implementation of `OUTSHINE_SIM` and it left with the container, and with :8080 closed `demo/frame` **exits 0 and not one of its 674 log lines mentions a refused post** — so `ServerLog`'s stated promise, that a gap is visible rather than silent, is false today
- [ ] `ServerLog`, `ServerTelemetry` and `HttpPost` deleted, and with them the collectorless channel whose absence a run cannot report. `ServerArtifacts` is already gone; this is the rest of the same surface
- [ ] A sink that cannot be opened is a **refusal at bring-up naming the path**, never a run that proceeds writing nowhere — the same shape as § I.22's *a test that must not reach the network declares zero sources and gets a refusal by name instead of a hang*
- [ ] `Host::Surface` — the one graphics coupling: it yields the `SDL_GPUDevice`, the swapchain texture and the frame size, and nothing else about a window reaches the library
- [ ] `Host::Budget` — the device's limits as a declared record the **library** enforces, not the host: heap ceiling, worker cap, frame budget (§ I.19)
- [ ] `World::TilePool` owns no thread — it takes an `Executor&`. Six threads today (`world/TerrainLoader.cpp` `kMaxTileThreads`), created in the pool's own constructor
- [ ] The host surface is crossed **per resource, never per element** — one call per tile fetch, per file read, per job, per frame clock read, per telemetry row, per log line; a host call inside a loop over postings, vertices, instances or pixels is a defect. Priced: an indirect call is ≈5 ns, so 130 resident tiles at one call each is under 1 µs/s against a 398 ms (wasm) / 190.5 ms (native) per-tile mesh build — the cost that matters is not the call, it is the inlining a per-posting virtual call would destroy
- [ ] **`src/` carries no conditional on a platform macro no target defines.** Six files hold twenty `#ifdef __EMSCRIPTEN__` arms and six `<emscripten…>` includes that nothing has compiled since `b83285f` deleted the emcc build — `clients/HttpPost.cpp`, `core/io/HeapProbe.cpp`, `core/io/StackProbe.cpp`, `render/Renderer.cpp`, `world/TerrainLoader.cpp`, `world/TilePool.cpp`. A conditional no compiler reads is not held by `-Werror`, is not held by the layering gates, and is not held by review either, because it reads as live code; the harm is already measurable (`doc/bugs.md`: `HttpPost.cpp`'s `gAbandoned` and its six-line justification are dead and the justification cites a deleted build file). The line is satisfied either by deleting the arms or by moving them under `src/hosts/web/`, and **not** by a second toolchain that compiles them unexercised
- [ ] `src/hosts/posix/` and `src/hosts/web/` as the only places libc I/O, SDL3, curl and emscripten are named — and they are linked by a client, never by the library
- [ ] **The link test**: `nm -u` over the six archives yields only a declared freestanding floor — `memcpy`/`memset`/`memmove`/`memcmp`, libm, `operator new`/`delete`, `__cxa_*`, and the host vtables. Any `fopen`, `curl_*`, `emscripten_*`, `pthread_*` or `printf` is a failure that names the symbol and the object it came from. This is what makes *platform agnostic* decidable instead of asserted, it costs under a second, and `llvm-nm` reads both toolchains' objects. Today it fails immediately: `core/` names emscripten 4 times, `world/` names both transports and opens 16 files, and 23 raw allocations sit in `world/`
- [ ] Every wire decoder assembles multi-byte values byte-wise with explicit shifts — one site violates it today (`doc/bugs.md`, `world/OsmVector.cpp:111-112`)
- [ ] No `static_assert` on the size of a struct containing a pointer or a `size_t`; only structs of exact-width and floating members are pinned — `generators/Body.h` (48 B) and `core/ChunkVtx.h` qualify, `core/ChunkVtx.h`'s `Chunk` correctly does not
- [ ] Exact-width integer types everywhere a number is stored — **183 platform-width declarations** outside `world/terrain` when it was last counted; that directory is deleted and its successor `world/tiles/` is **inside** the subject, where it already adds two (`world/tiles/TileGeodesy.h:90-91` `WrapTile(int z, long *x, long *y)`). Recount owed (`long`, `unsigned long`, `long long` as a declaration; the count excludes casts). `long long` is not wrong, it is at least 64 bits on every target, but the width is the platform's choice and `int64_t` says what it is. **Done when** the count is 0 tree-wide — there is no C ABI left to exclude, held by a test in the suite — `long` is a keyword, so no type can forbid it and no `#pragma poison` reaches it; this is a rule a check carries
- [ ] `size_t` names a size, never a count — measured: **923 uses**, of which **4** are cumulative counters (`clients/ServerLog.h:51`, `clients/ServerTelemetry.h:33`, `world/World.h:331`), and none is on a path that can reach 2^32. The distinction is not stylistic: a `size_t` that measures a size cannot be too narrow, because what it measures is bounded by the address space `size_t` spans by definition; a `size_t` that counts events is 32 bits on wasm32 for no reason. The fastest cumulative quantity in the tree is `meshAsked` — 3 643 190 over 228 s in `sim/logs/demo-walk-500-…20260812T043345Z.csv`, **15 979/s, so 2^31 in 37.3 h** — and it is already `long long` (`world/TilePool.h:51-71`)
- [x] The terrain decoder loses its platform coupling — closed the other way round: rather than passing an allocator pair and a diagnostic callback into a surviving C island, the island was deleted. `src/world/tiles/` has **no `malloc`, no `free`, no `stdio.h`** (grep, 2026-08-12): allocation is `std::vector` through the one `operator new`, and diagnosis is `Log::Error` (`world/tiles/TerrainGrid.cpp:31`). *The premise this line was written on — that it is shared with `tiles/`, so the exception survives and the seam moves — was false; the sharing was duplication (`doc/bugs.md`), and `tiles/` links `src/world/OsmVector.cpp` and no terrain decoder at all.* This is what unstuck § I.17's *no `malloc` outside `core/io/`*

### I.19 The platform: SDL3, SDL_GPU, modern C++, this device at 720p60

- [ ] SDL3 as the platform implementation for window, input, timers, threads and filesystem — **behind `Host`, never named by the library.** Recommended against taking SDL3 directly even though it is now a constraint: the link test of § I.18 is only possible if the library names no SDL symbol, a freestanding target has none, and the library needs about eight of SDL3's several hundred entry points. The cost of the seam is the per-resource indirect call already priced; the cost of not having it is that *platform agnostic* stops being decidable
- [ ] **SDL_GPU as the graphics API** — the owner's decision, and it makes `render/` portable by the same argument as the rest: Vulkan, Metal and D3D12 from one source. On this device the backend is Metal
- [ ] `render/` ported from WebGPU to SDL_GPU — measured cost: **9 340 lines of C++ in 36 files, of which 2 739 lines are WGSL** inside raw string literals across 30 of them, one compute pipeline (`stages/ExposureStage.cpp`), and the readbacks (`ReadPixels`, `ReadDepth`, `ReadIrradiance`) that every declared still and every acceptance number in the archive depends on
- [ ] HLSL as the one shader source, translated offline by **SDL_shadercross** to MSL for this device and to SPIR-V/DXIL for the others — SDL_GPU takes a different format per backend by construction, and shadercross is SDL's own answer (SPIR-V or HLSL in; DXBC, DXIL, SPIR-V, MSL, HLSL and JSON reflection out, offline or at run time). The 2 739 WGSL lines convert mechanically as a starting point (Tint emits HLSL) and are then read, not trusted
- [ ] Shaders stay **generated and never authored** across the move — they are code in this tree, compiled by a tool in this tree, which is the same test principle 2 states for every other appearance
- [ ] **Modern C++ = C++20**, one value, no second dialect — recommended rather than C++23 because it is already half-present (`render/` and `clients/` compile at `-std=c++20` today while the gates judge C++17, which `doc/bugs.md` records as a defect) and Apple clang 21 on this host carries it whole. What it buys that is load-bearing, not pleasant: **`std::span`** against § I.17's 40 raw pointer+count parameter pairs and `core/Span.h`'s hand-rolled equivalent · **`<bit>`**'s `std::endian` and `std::bit_cast` against the wire decoder in `doc/bugs.md` · **concepts** to state the generator contract `(Region, Ground) → Yield` as a constraint the compiler checks instead of a comment · **designated initialisers** for the material rows · `[[likely]]` on the posting loop. C++23's `std::expected` is explicitly **not** a reason: `operator*` reads the payload with nobody having looked at the state, which is the same objection `doc/bugs.md` already sustains against `std::optional`, so the house `Try(T *out)` shape survives the standard bump
- [ ] The device's budget as a declared record the library enforces: **A18 Pro, 2 performance + 4 efficiency cores, 5 GPU cores, 8 GB unified, Metal 4** — heap ceiling, worker cap and a 16.67 ms frame budget with p99 ≤ 33 ms as the floor. The development platform is the target, so the budget is measured on the machine the work happens on and needs no simulation
- [ ] The heap ceiling enforced inside `core/io/Heap` — this is § I.17's open line *a declared heap ceiling honoured inside `core/io/Heap`, so an exhaustion is reproducible on a target that has no fixed linear memory*, and it is now the only way to have one at all: `ulimit -v` is rejected on this host, `ulimit -d` answers `Invalid argument`, and a process allocates and touches 4 GB unrefused
- [ ] A frame that exceeds the declared budget is a published column, not a log line
- [ ] **wasm is a port destination, not a constraint** — one host implementation among several, reached when SDL3 reaches it. What that ends: the emcc half of `Makefile` stops being load-bearing, `-sINITIAL_MEMORY`/`-sPTHREAD_POOL_SIZE`/`-sABORTING_MALLOC` stop being the engine's frame, and the six emcc-only translation units become one host's implementation rather than a second program. What it must not end: the exact-width discipline of § I.18, which was learned there and is right everywhere
- [ ] The 720p60 floor stated as a distribution over a moving camera on **this** device — p50/p95/p99, never a mean — and it is the one number that decides whether a port or a stage lands

**Shader source moves out of the C++ and into files, and the port is the only moment it is free.**
*Owner's question, 2026-08-12: is embedding shaders in `.cpp` files really a good idea, given they are
rewritten anyway. **Answered yes-it-must-change, and the decisive argument is not ergonomics — it is
that the artefact's kind changes under SDL_GPU.** Under WebGPU a shader is **source the runtime
compiles**, so a string literal is a perfectly honest representation of what it is: text handed to a
compiler at run time. Under SDL_GPU it is a **precompiled binary artefact** — `SDL_shadercross` runs
offline and emits MSL here, SPIR-V and DXIL elsewhere. A build artefact wants a build step, a build step
wants an input file with a path and a timestamp, and a C++ string literal has neither. Keeping the
current shape after the port would mean a build step whose input is extracted from a compiled
translation unit, which is the tail wagging the dog.*

*Measured, not recalled: **2 776 lines of raw-string shader text in 30 files**, the four largest being
`stages/TerrainDraw.cpp` (546), `stages/BuildingDraw.cpp` (373), `stages/ModelDraw.cpp` (334) and
`Sward.h` (220). And the shape is not "one literal per shader": a pipeline's source is **assembled at
run time by `operator+`** — `BuildingDraw.cpp:396-397` concatenates seven fragments — which is an
include system implemented as string concatenation. That is the second reason files win: `#include` is
the same mechanism with a name, a search path, and a compiler that reports the line number of an error
in the fragment rather than in the concatenation.*

- [ ] **One shader file per pipeline under `src/render/shaders/`, in the port's own commits**, never a second migration afterwards. Cost of doing it now: **zero** — every one of the 2 776 lines is rewritten either way. Cost of doing it later: the same 2 776 lines moved a second time, by hand, after they have stopped being mechanically translatable from WGSL, plus every reviewer's diff of the port polluted by the move
- [ ] **The composition seam survives as `#include`**, because that is what it already is: the fragments that are concatenated today — `kSceneScaleWGSL`, `kSurfaceLightWGSL`, `ShadowSampleWGSL()`, `kVelocityWGSL`, `kHazeWGSL` — become headers under `shaders/` included by the pipelines that use them. What this buys beyond tidiness: an error in `SurfaceLight` is reported once at its own line, instead of once per including pipeline at a line number that exists in no file
- [ ] **The codegen seam is kept and is exactly as strong, and this is the load-bearing check on the whole answer.** `core/FacadeUv.h:67 FacadeUvWGSL()` emits shader constants from the one C++ enumeration, so *a shader constant that disagrees with the C++ enum cannot exist* — and it is not alone: `HazeConstsWGSL`, `SwardConstsWGSL`, `CurveConstsWGSL`, `CloudDensityConstsWGSL` and `SceneScaleWGSL` are the same seam, **six of them**. Under files the emitter writes a **generated header** into the build directory and the shader includes it; the C++ declaration is still the only place the number is written, so the invariant is unchanged. It is **not weaker**, and it is arguably stronger, because the generated header is a file a human can read and a build can diff, where today the emitted text exists only inside a `std::string` at run time
- [ ] The generated headers are **build output and are never committed** — same rule as the corpus (§ I.26.1). A generated file in git is a second copy of a truth that already has one owner, and it drifts the first time somebody edits the wrong one
- [ ] **The offline-compiled artefacts are build output too, and are not committed.** `SDL_shadercross` runs as a build step over `shaders/`, emits MSL for this device into the build directory, and the binary is loaded through `Host::Storage` like every other declared datum (§ I.18). Committing them would put a per-backend binary in the tree for a toolchain we run ourselves — the exact thing the corpus manifest exists to avoid one level out
- [ ] **A shader edit stops costing a C++ recompile**, which is the ergonomic half and is worth stating as a number rather than a preference: today editing one line of `TerrainDraw.cpp`'s 546 shader lines recompiles a translation unit that also carries the pipeline construction, the bind-group layouts and the draw recording. After the move it runs shadercross over one file. *This is the smallest of the reasons and it is listed last on purpose*
- [ ] **Editor and linter support is a real property and not a comfort**: a `.hlsl` file gets syntax checking, completion and a language server; a raw string literal inside a `.cpp` gets none, and a typo in it is found by the runtime compiler at first draw. That moves an error from **build time to run time**, which is the wrong direction by `P.5`
- [ ] **`one class per file` survives intact and gains a sibling rule**: the stage class stays one class in one `.h`/`.cpp` pair and owns its pipeline; the shader is *its* file, named after it, beside it. What is deleted is not a class but a string. The house rule was never *one class and its shader in one file*
- [ ] **A shader on disk is still generated in principle 2's sense, and this is stated explicitly because the opposite reading would be a real defect.** Principle 2's test is *can this be recomputed from something we own* — and a shader is **code in this tree, compiled by a tool in this tree**, which is the same status as every `.cpp`. A `.cpp` on disk is not "authored appearance" and neither is a `.hlsl`. § I.19's existing line already says shaders stay generated across the move; this line says the file form does not touch that
- [ ] **What would make the answer wrong, so a later round can find it**: if SDL_shadercross turned out to be unusable offline and the engine had to compile HLSL at run time, the artefact would be source again and the argument's spine would go with it. Checked: shadercross is documented as an offline tool *and* a run-time library, and SDL_GPU's own model is precompiled-per-backend. If that changes, this decision is re-taken rather than patched

### I.20 The test suite in C++, and its harness

- [ ] `test/` mirroring `src/` one directory for one directory — **the mirror is the layering**: a test under `test/generators/` compiles with `-Isrc/core -Isrc/generators` and links `Core`+`Generators`, so a test that reaches up does not compile. Every test is then a continuous positive-half proof for its own layer, which subsumes the hand-written `test/generators/CoreIsReachable.cpp`
- [ ] One translation unit per claim, its own `main`, no framework and no dependency below it — libc-test's shape. `test/generators/SameRegionSamePlacement.cpp` is 689 lines and ~30 claims behind one `main` today (`doc/bugs.md`)
- [ ] **A test that sweeps a population states the population's size as a claim** — `Report()` fails a test that checked nothing at all, but a test with one guard claim whose sweep then runs over zero subjects is green and teaches nothing, which is the same silence one layer along. `GrownBarkIsAClosedMesh` does it by hand (`!species.empty()`, `Declarations == species.size()`); it must be the rule and not the habit, because a directory that moved, a filter that matched nothing and a generator that returned early all look like this
- [x] `test/Check.h` as the whole reporter — `Check`, `CheckNear` with a tolerance that has no default, `Note` for a measured number, `Covers` for the requirement identifier, `Report` for the exit code. Header-only, C++17 inline variables, two macros and no more, and the reason for the macros stands beside them (`ES.31`: `__FILE__`, `__LINE__` and the source spelling of an expression cannot be had from a function). `test/Check.h`, held by `test/harness/ExpectFail.cpp` — the counters are a `Tally` that counts up and cannot be assigned, so `Test::Failures = 0;` **does not compile**, and `Report()` returns one bit because the trailer carries the count
- [ ] A test never stops at its first failure — it reports all of them and exits with the count, so one round sees every claim that fell. *The second clause is superseded and kept for the record: a POSIX status cannot hold a count and 256 of them aliased onto success. What stands is* **it reports all of them, and the count is read from the trailer** *— the line below is the mechanism*
- [ ] **The verdict is the reporter's printed trailer, and the exit status is only cross-checked against it** — `CHECKS n FAILURES m` is what the harness judges, a disagreement between the two numbers is a failure naming both, and a test that prints no trailer at all did not report. A POSIX exit status is eight bits and the line above asks for a count, so the two collide: measured 2026-08-12 against the uncommitted harness, a test failing **256** claims exits 0 and is printed `PASS` while its own log reads `CHECKS 256 FAILURES 256`, and one failing **77** claims is printed `SKIP` and goes green under `--allow-skip` (`doc/bugs.md`). One mechanism closes four holes — the two aliases, `(void)Report()`, and a `.cpp` that forgot the reporter and therefore emits no trailer. **Built** in `test/run.sh:213-229`, `:318-324` and `test/Check.h:86-95`, and all four holes re-planted and confirmed red 2026-08-12 by a judge who built neither. **Un-ticked on the two lines below, not on the mechanism**: the trailer is authenticated by shape alone, so a file with no reporter that prints a trailer-shaped line is still `PASS`; and the cross-check itself is held by no fixture in the tree
- [ ] **The trailer's counts carry a second witness on an independent path** — every increment of `Failures` prints one line beginning `FAIL ` and every increment of `Skips` one beginning `SKIP `, so the harness requires `grep -c '^FAIL '` = `FAILURES` and `grep -c '^SKIP '` = `SKIPPED` before it believes either. Shape alone cannot authenticate a trailer: demonstrated 2026-08-12, a `.cpp` that includes no reporter, prints a `FAIL` line and a hand-written `CHECKS 1 FAILURES 0 SKIPPED 0`, and returns 0 is reported `PASS` (`doc/bugs.md`). This also catches a counter zeroed by a spelling `Tally` does not forbid — placement new, `memcpy` — because a reset cannot unprint the lines
- [ ] **A missing, doubled, malformed or disagreeing trailer is a red verdict of its own, and the run continues** — one test that cannot be read must not hide the verdict of every test after it, which is `Makefile:409`'s rule for gates in the harness that rule matters most in. Today each of those four cases is a `Die` that exits 2 mid-loop (`test/run.sh:218`, `:222`, `:224`, `:324`). Only the pre-flight directory scan refuses before anything is built, which is right there because nothing has run yet
- [ ] **The build cache is keyed by the root it was built from** — `test/run.sh:41-42`, `:159`, `:287-288` name objects, logs and binaries by path relative to the root, so a git worktree or a `bisect` clone links the other checkout's objects and every number read from them belongs to the other tree. One line: the root's real path folded into the build directory
- [ ] `Test::Covers("R-I.8.4")` emitted **at run time**, not written in a comment, so a claim only counts as covered if the test that carries it actually ran
- [ ] The identifiers of § *How a line is read* applied to all 1436 lines, and the harness fails on an identifier no line carries
- [ ] `test/run.sh` as the harness: discovers, builds, runs each test in its own process, prints one line per test (`PASS`/`FAIL`/`TIMEOUT`/`SIGNAL`/`BUILD`/`SKIP`, name, milliseconds, log path), tabulates, and exits non-zero on anything that is not `PASS`
- [ ] **Every `.cpp` under `test/` carries a declared role** — test, entry point, negative fixture, must-compile fixture, harness tool — and a file whose role no directory declares is a refusal before anything is built, the same shape as an undeclared include set. `test/Millis.cpp` is a legitimate non-test, so the predicate cannot be *every `.cpp`*; it must be a declaration and not an inference. Discovery by `grep '^#include "Check.h"'` is a text scan over source — the instrument class this tree deleted with the Python validators — and it says nothing about a file that forgot the include: today such a file is not built, not run, not counted, and nothing reports it. **Built** 2026-08-12 as role-by-directory in `test/run.sh:92-124`, `:253-268`, and the refusal confirmed: a `.cpp` in an undeclared directory stops the run before the clock is compiled. Un-ticked on the line below
- [ ] **The Makefile-owned role is derived from the Makefile, not restated beside it** — a directory the harness declares as the Makefile's is trusted today, so a real test dropped into `test/compile/` or `test/clients/` is silently not run (demonstrated, `doc/bugs.md`), and *which files the Makefile builds* is stated twice with nothing failing when the two disagree — the same defect as the duplicated `INC_*` sets two lines down. Both directions are checked: every `.cpp` under a non-layer directory is named in the `Makefile`, by path or by the stem the Makefile composes, and every `test/…cpp` the Makefile names lies in a non-layer directory. Green over the tree by hand at this commit — 12 Makefile-owned sources, 12 named, no stray — so it costs about six lines and moves no file
- [ ] **The include set of a layer is declared once and read by both the Makefile and the harness.** `test/run.sh:78-96` restates `INC_CORE`, `INC_GENERATORS` and `INC_DRAW`; they agree today and **nothing fails when they stop agreeing**, so the failure is silent by construction — a harness proving a layering the build does not have is green about a set that exists only in the harness. Step 3's layer archives remove the copy, and until they land the copy needs a reader rather than a promise (`make print-includes`, three lines, and the pattern already exists as `make print-sources`)
- [ ] Its own timeout, because **macOS has no `timeout(1)`** — background the child, poll, kill, and report `TIMEOUT` distinctly from `FAIL`, **leaving no process behind**: a watchdog subshell killed while its `sleep` child runs orphans that child for the whole timeout, one per test (`doc/bugs.md`)
- [ ] A crash reported as the signal it was (128 + n from `wait`), never as a generic failure
- [ ] A test that could not run says so by name and **fails the harness** unless the skip was declared — a silent skip is the defect class this repository keeps finding, wearing a harness's hat
- [ ] Tiers the harness names and can run alone: `build` (compiler only) · `host` (native, no device, no network) · `device` (native Dawn) · `world` (needs `fb-tiles` on :8081) · `port` (the one browser gate)
- [ ] **A sixth tier, `corpus` — a test whose subject is a prepared artefact rather than a tracked file.** § I.26.10 rules that `manifest.json` is the **only** tracked file in a case directory, so a test that reads `scene.gltf` needs the preparer to have run and, the first time, the network. That is a legitimate dependency and it has no spelling: `test/unit/gltf/TheTriangleProjectsToTheOraclesArea` reads `test/render/coverage/triangle/scene.gltf` and, on a tree carrying only tracked files, prints **`FAIL … the Khronos Triangle reads as a .gltf with its buffer beside it`** — measured 2026-08-12 by running it against a directory holding the manifest alone. A missing subject is being reported as a reader defect, and no tier says otherwise. *`host` is the tier it claims by living under `test/unit/`, and `host` promises "no network"*
- [x] The harness's own red demonstrated by a test that is declared to fail — a harness that has never been non-zero is a harness nobody has tested. `test/run.sh:51-55`, `:231-240`, `:335-344`, held by `test/harness/ExpectFail.cpp`, and the declaration carries the **count** (`LAYER/NAME:FAILURES`) so that inverting cannot swallow a crash, a build that fell over, or a failure for a second reason
- [ ] **One declared-outcome fixture per verdict the harness can print, not only `FAIL`** — `SKIP`, `SIGNAL`, `TIMEOUT`, `BUILD`, and the three refusals (undeclared directory, missing or doubled trailer, trailer disagreeing with the status). All eight were confirmed to work 2026-08-12 by planting a `.cpp` per case out of tree; none of the eight leaves anything behind, so **seven of the harness's eight reds are held by nothing that runs**, and a check that has never been red is a check nobody has tested. `EXPECT_FAIL` already has the shape and wants one field more: `LAYER/NAME:VERDICT[:count]`. The three refusals are the awkward ones, because a refusal aborts the run — they become tractable exactly when the line above makes a refusal a per-test verdict
- [ ] `make gates` becomes one line that calls `test/run.sh`, and the eleven `verify-*` targets are deleted **as they are replaced**, one per step, never in one jump. Until then `test/run.sh` is in no gate list (`Makefile:420-422`), so nothing runs the suite unless a person types it
- [ ] The must-not-compile negatives as ordinary tests that shell out to the compiler — fixtures under `test/compile/<layer>/`, one driver per fixture, the toolchain and source root taken from the environment and **refused if absent** rather than defaulted. *The home named here was `test/negative/fixture/` and that name is wrong for the set it must hold: of the 12 compile-judged subjects, **4 must compile** (`GroundSampleIsUsable`, `WaterDepthIsUsable`, `CoreIsReachable`, `DrawIsReachable`) and 8 must not, so `negative/` would misname a third of them. `compile/` names the instrument rather than the claim, which every other directory under `test/` does the other way round; it is right for as long as the fixture is a subject in its own right, and it dissolves when the fixture becomes data for a driver that lives in the layer it defends — `test/core/fixture/` beside `test/core/`, at which point the role rule is "a layer directory is tests, its `fixture/` is data" and no list of directory names survives*
- [ ] A negative demands the **exact** diagnostic and that it is the **only** error emitted — today a typo in a fixture's own include name satisfies the gate (`doc/bugs.md`)
- [ ] Geometric invariants over generated content, which is the *decidable* class `CLAUDE.md` names as the cheapest evidence and the class the tree has **none** of: watertight/manifold **0**, unit-normal assertion **0**, weld/degenerate-triangle check **0**, winding check **0**
- [ ] Every index in range and no degenerate triangle, for every mesh a generator produces
- [ ] The index buffer's length a multiple of three, for every mesh a generator produces — a sweep that steps `i += 3` while `i + 2 < size` drops a trailing one or two indices and then reports `size / 3` triangles, so the count agrees with the truncation instead of with the buffer (`test/generators/draw/GrownBarkIsAClosedMesh.cpp:102`, `:140`). Counted and checked for the **bark** mesh 2026-08-12 (`:152-155`, `:312`); the building, leaf, chunk and impostor meshes are untouched, and no declaration produces a ragged tail, so the check has never been red — see the line below
- [ ] **Every geometric invariant demonstrated red at least once, by a fixture that produces the violation** — a mesh built in the test, not a declaration edited in `src/`, so a check on a population that happens to be clean is still known to fire. This is `Test::Covers` applied to the instrument instead of the requirement: an invariant nothing has ever violated is indistinguishable from an invariant nothing evaluates
- [ ] Every normal unit to 1e-4, for every mesh a generator produces
- [ ] Consistent outward winding by the divergence-theorem volume `Σ v0·(v1×v2)/6 > 0` — a single number that catches a globally inverted mesh, where a per-triangle test cannot
- [ ] Closed, or every boundary edge in a declared plane — a trunk open at `y = 0` is legitimate and an opening anywhere else is not, so the invariant states its exception instead of assuming closure (Ericson, *Real-Time Collision Detection* ch. 12: mesh consistency, welding and T-junctions)
- [ ] No two distinct vertices closer than the declared weld epsilon
- [ ] The normalisation contract held for every declared species — base at `y = 0`, centred in x/z, height exactly 1 (`generators/draw/TreeMesh.h`), which nothing checks today. *Those words have two readings and the box reading is false in all three clauses (`doc/bugs.md`); the contract the grower implements is* **origin at the trunk foot, `BoxMax.Y` = 1 for a standing form and the larger horizontal run = 1 for a lying one, `BoxMin.Y` free to be negative** *— and that is what must be checked and what `TreeMesh.h` must say.* The extent third is built (`test/generators/draw/GrownBarkIsAClosedMesh.cpp:101-107`, `:227-231`, `:313-315`): worst deviation over 31 declarations **5.96e-08 in `dog_rose`**, one ulp at 1.0, the other 30 exact. Un-ticked on three counts — the tolerance is 168 ulps and bypasses `CHECK_NEAR` (`doc/bugs.md`), the origin third is measured and only printed, and the lying branch re-decides `GrowthForm::Lying` so it is a consistency check and not a decidable one
- [ ] The same four invariants over `BuildingMesh` for every declared roof kind, and over `ChunkMesh` for a terrain tile at every ladder level
- [ ] `verify-clients`' three rules become compile errors instead of a Python scanner: `src/api/` as the entry point's whole include set, so an entry point that builds a world has no spelling for it. `main()`'s length ceiling (`F.3`) and the one-scene-builder rule then need no text scan
- [ ] **Every test native, and after the fifth ruling that costs nothing** — there is no second target the suite has to reach, and a port is checked by the port's own gate when it exists. `verify-counters` retires with the exact-width sweep of § I.18, not before, and what replaces it is the defect being unspellable rather than a second place to look
- [ ] A port gate per port, when a port exists: the module links, runs one declared scene headless, and reports the **same drawn-world counters** as the oracle — never the same pixels, since the backends differ

### I.21 Coverage and portability as measured claims

- [ ] Every requirement line classified by what could decide it, so *"every requirement that can be tested"* has a boundary. Measured 2026-08-12 over all **1436** lines by section, ±10 % (the classification is per section with the obvious splits taken, not per line): **A decidable invariant, native, no reference — 367 (26 %)** · **B consistency with a declaration already in the tree — 170 (12 %)** · **C consistency with an external reference that must first enter the tree as a declared table — 707 (49 %)** · **D needs a device — 124 (9 %)** · **E needs motion over a traversal — 62 (4 %)** · **F needs a sense we cannot instrument — 6 (0.4 %, all of § I.15 audio)**
- [ ] **No line is untestable, and that is the finding.** What is not testable is the *verdict* — whether the picture holds against Kingdom Come: Deliverance — and that verdict is not a requirement line, it is the acceptance over the whole set. The eye judges the whole; the suite judges the parts. A round that confuses the two spends itself proving a picture with numbers
- [ ] The C class — 707 lines, half the scope, nearly all of bands III, IV and V — becomes testable the moment its reference is written down as a declared table in the tree. Writing the table down is most of the work of building the feature, so this is a sequencing statement and not an obstacle
- [ ] A declared reference table per band — botanical dimensions for III, building proportions for IV, vehicle data for V — with its source named per row, against which a test bounds what the generator produced. The test then decides *the grown thing matches its declaration*; whether the declaration matches nature is a reading, not a run, and the table is where that reading is recorded once
- [ ] **Requirement-line coverage as the primary number**, because it is what the owner's ruling asks for and it is the only coverage figure that cannot be raised by testing something nobody wanted: the fraction of **ticked** lines whose class admits a test and which name one. Target **100 %** of ticked lines, which is enforceable, unambiguous and not gameable downward — a tick that cannot name a test is either untested or was never true
- [ ] Line and branch coverage as the **instrument**, not the target — `-fprofile-instr-generate -fcoverage-mapping` on a native test build, `llvm-cov` per function, which is how the untested residue *inside* a covered requirement is found. TOOL
- [ ] Per-directory line-coverage floors, argued from where an excuse exists: `core` 90 % and `generators` 85 % (pure functions with no device, no network and no thread — there is no excuse for a gap), `world` 70 % (network and threads), `render` measured and published but not floored (device tier), `clients` excluded (entry points). The industry consensus for "high" sits at 80–90 % and the returns above it are documented as poor; the floors above deviate from it only where the code is pure
- [ ] **The baseline today is not measured and cannot be, because no coverage instrument exists in this tree** — twelve gates, all structural, and no profile build. TOOL, and the first number the instrument prints goes into the record whatever it is
- [ ] A source file under `src/` that no test binary links, published as a count — the cheap floor under the profile instrument, derived from the archives with `nm`, and the case the owner named: *a commit that adds a source file and no test*. The gate fails when the count rises, which makes the policy mechanical rather than a habit
- [ ] A **symbol** in an archive that no test binary ever reaches, published as a count — linkage is the wrong floor and this round proved it: `src/clients/Walker.cpp` is in `APP_SRCS`, so its file links, `-Werror` covers it, the count above stays flat, and nothing in the tree ever constructs a `Walker`. Cheapest instrument that is not a profile build: the entry points' reachable call graph from `main`, which `llvm-cov`'s zero-count functions give for free once § I.21's profile build exists, so this is one report and not a second tool
- [ ] Every translation unit compiles under **every compiler it ships under**, and the count of compilers a source is checked by is published — *portable* is a property of a build that ran, not of the language chosen. Measured 2026-08-12 and the reason this line exists: `tiles/`'s thirteen units are compiled by host clang in the edit loop and by the image's `g++ 14.2.0` when the container is built, and after the C→C++ port **nine of them failed the second compiler and none failed the first** — `#define _GNU_SOURCE` is legal in C and a `-Werror` redefinition in g++'s C++ mode, which predefines it (`doc/bugs.md`). Nothing announced this for as long as the container went unbuilt. The same asymmetry is owed for `src/`: the wasm module and the native oracle are two compilers over one source set and § I.17 already carries the gate for one of them
- [ ] Portability stated as the link test of § I.18 and nothing softer — *"runs on any platform"* is a property of the symbol table or it is a sentence
- [ ] The first-page claims of this file held to the same discipline: a number, an instrument, and a tick only when the instrument says so. *Every point on Earth is a valid start* was un-ticked on measurement (§ I.6, 0.373 % of the surface excluded by Web Mercator); *portable* and *tested* are claims of the same kind and are now lines rather than adjectives

### I.22 External data as declared plugins

*Added 2026-08-12 on two rulings in one session: **the tile source becomes part of the engine** —
`CLAUDE.md` principle 6 is now* no servers of our own *(`fee279f`) — and **the data providers are
plugins, like the generators**. Together those delete a process, a wire format and a cache, and they
promote a static table of three URL templates (`tiles/src/tilesrc.cpp:9-24`) into the second registry
this engine has. § I.16 is what the tile server was; this section is what replaces it, and § I.16's
lines are not struck — each one is either satisfied by a plugin here or is still owed.*

**The generator contract is the model and the parallel is exact.** A generator declares what it can
propose before it proposes anything (`Proposes(areaM2)`), is registered at a `Rank` that refuses a
duplicate at registration rather than racing at run time (`generators/GeneratorSet.h:15-17`), and its
products are separated by who consumes them. A provider is the same shape turned outward: it declares
its domain before it fetches anything, it registers at a rank, and *what it covers* is kept apart from
*what it delivered*.

**The distinction the whole section exists for.** *No provider here* and *no data here* are different
answers and this tree has confused them, at a cost of two rounds. They must be different **types**, not
two enumerators of one: a statement about the world can only be minted by a source that first declared
the place to be inside its own domain, and the exhaustion of the registry is a declaration error rather
than a fact about the Earth.

- [x] **The two contracts are deliberately the same shape, and that is the ruling** (owner, 2026-08-12): a generator is a pure function `(Region, Ground) → Yield`, `const noexcept`; a provider is that idea turned outward — an upstream declaring what it covers, behind one registry. Both **declared**, both **replaceable**, and **neither knowing the engine**. What the sameness buys is not tidiness: `Proposes(areaM2)` and `Covers(Request)` are both pure predicates answerable before any work, so the selector for content and the selector for data are one pattern with one failure mode, and a provider is therefore not merely an interface — it is the generator contract with the arrow reversed. `data/Source.h` (`Covers` `const noexcept`, `Serves` `const noexcept`, `Declaration`), `data/SourceSet.h` beside `generators/GeneratorSet.h`; held by `test/data/AbsenceHandsOver.cpp` and `test/data/UncoveredIsUndeclared.cpp`
- [x] Neither contract may reach the engine: a generator compiles with `-Isrc/core -Isrc/generators` and a provider with no more, so `Renderer`, `World`, `Log` and the streamer **have no name** in either. The layering is the build (§ I.18), and a provider that needs the world to answer *what do you cover* has the arrow the wrong way round. `Makefile` `INC_DATA := -Isrc/core -Isrc/data` and the `verify-data` gate; held by `test/compile/data/{RendererIsNotReachable,WorldIsNotReachable,LogIsNotReachable,GeneratorIsNotReachable}.cpp` against `test/compile/data/CoreIsReachable.cpp`, each required to fail *for the stated reason*. *Ticked on the include half only — the same gate's `nm -u` half passes vacuously and reads three of six object groups (`doc/bugs.md`)*
- [ ] `Data::Source` as the plugin interface, one per upstream — the six that exist are terrarium DEM on S3, versatiles vector, arcgisonline imagery, NOAA GFS weather, Overpass peaks and the baked star catalogue (`tiles/src/tilesrc.cpp`, `wx.cpp`, `peaks.cpp`, `stars.cpp`)
- [x] A source's **declaration** is a value with no I/O in it: data kind, addressing scheme, zoom range, geographic extent, wire format, declared payload size, declared latency class, rank. Everything the selector needs to choose is answerable without touching the network, exactly as `Proposes` answers without allocating. `data/SourceDecl.h` — plus `Version` (the content key covers it), `AncestorFill`, `Cacheability`, `Necessity`, `RetryBudget`; held by `test/data/TheAnswerNamesItsAddress.cpp` (reads `MaxZoom` off two declarations before any transport exists) and published per source at load by `clients/Sim.cpp:478-487`
- [x] `Covers(const Request&) const noexcept -> Coverage` — pure, allocation-free, `Inside` or `Outside`, and it is the **only** producer of the right to mint a world fact. `data/Source.h`, `data/WebTileSource.cpp:12-22`, `data/StarBands.cpp:33-38`; held by `test/data/UncoveredIsUndeclared.cpp` (an uncovered request touches no transport) and `test/data/AbsenceHandsOver.cpp` (an `Outside` source at the answering rank is never begun)
- [ ] Fetching is asynchronous **by shape** — submit yields a ticket, the caller polls, cancel is a real operation — over `Host::Fetch` (§ I.18) and never a blocking call. `world/TilePool.h:110` `BytesBlocking` is today's shape and it is legal *"natively on the frame thread"*, which is the property that cannot survive a browser host
- [x] A fetch's answer is **one state-carrying type**, and *no such place*, *not yet*, *the transport refused* and *an empty body* are four of its states rather than four readings of one empty buffer. `data/Fetched.h` (`Working`/`Settled` × `Bytes`/`Absent`/`Refused`/`Retry`), `data/Delivery.h` (`Delivered`/`Pending`/`Vacant`/`Undeclared`/`Refused`), `data/Transport.h` `Wire`, `world/tiles/TerrainTiles.h` `TerrainBytes`, `world/tiles/TerrainGrid.h` `TerrainGrid::State` — and the thread-local `tMiss` is deleted; held by `test/data/AbsenceHandsOver.cpp`. *Ticked on the four states existing. The `Refused` arm is now four things again and carries no cause, which is a defect in what was built rather than a line that was not (`doc/bugs.md`)* This is the section's opening ruling written as a signature, and the tree has just failed it a second time: `World::TerrainSource::TakeTerrainPng` (`world/tiles/TerrainTiles.h:23`) returns a bare `std::vector<uint8_t>` and its own comment delegates the distinction elsewhere, so the reason travels on a thread-local (`world/TilePool.cpp:401` `tMiss`) exactly as it did through the C function pointer this interface replaced — minus that one's excuse, which was that a C callback had no channel (`doc/bugs.md`). The shape is already written twice in `core/GroundSample.h` and `core/WaterDepth.h` and four more times in `world/tiles/` itself
- [x] A `Delivery` carries **which source answered and at which address**, never only bytes — a request at z15 served from the z14 ancestor must say so, because *what resolution actually answered* is a measurement and today it is an assumption. `data/Delivery.h` (`Answer{SourceId, At, Bytes}` reachable only through the one `TryTake`), `data/Source.h` `Serves`, `data/WebTileSource.cpp:24-31`, carried through the byte cache as `world/TilePool.h` `Landing` and out through `TerrainBytes`; held by `test/data/TheAnswerNamesItsAddress.cpp` (z17 → the z15 ancestor, the URL fetched is the ancestor's). *Ticked on the value and its test. **No run reaches the non-zero case** — `World.cpp kMaxZ = 14` against terrarium's `MaxZoom = 15` makes the difference always zero, so this is unit-tested and un-exercised, which is the honest statement (`doc/bugs.md`)*
- [ ] Status-to-meaning is **per source and declared**, never one global function — `world/TilePool.cpp:49-59` `Classify` maps `204 -> Hole -> Absent -> terminal`, and **204 is minted by `tiles/src/main.cpp:24`, our own server**. No upstream in `tilesrc.cpp` produces it. When the hop goes, the tree's entire absence semantics has nothing behind it, and the three upstreams disagree: an out-of-coverage terrarium tile, an empty vector tile and an imagery tile with no imagery are three different HTTP conversations
- [x] The selector walks the covering sources in declared rank order and **an `Absent` from one hands over to the next** — the terminal absence is the exhaustion of the list, not the first refusal. This is the whole reason to have plugins: a national 1 m DEM over one country falling through to terrarium everywhere else is unspellable today, because `world/TilePool.cpp:596-598` makes the first `Absent` final at the node. `data/SourceSet.cpp:35-119` (`Meaning::Absent` → `Current_ = nullptr; continue`, `Vacant` only when `Next_` passes the end); held by `test/data/AbsenceHandsOver.cpp` — rank 0 absent hands to rank 1's bytes, both absent is `Vacant`, and a `Refused` does **not** hand over
- [x] A duplicate rank within one data kind is refused **at registration**, the rule `generators/GeneratorSet.h:15-17` already states for generators — never a run-time coin toss over who answers first. `data/SourceSet.cpp:8-26` (`Registration::DuplicateRank`, and the registry is held sorted so the walk *is* the rank order); held by `test/data/AbsenceHandsOver.cpp:102-114`, which also checks the refused source is not in the registry afterwards
- [ ] The decoder is registered against the **wire format**, not against the source, so a second elevation upstream in a new format is a decoder registration and not an edit inside `world/TerrainLoader` (Gregory, *Game Engine Architecture* 3e ch. 7: the resource manager's type-to-loader registry is the pattern, and its point is exactly that adding an asset type touches no consumer)
- [ ] Every source's **declared** payload size and latency class published beside the **measured** one on the ordinary telemetry row, so a declaration that has drifted from the wire is a visible disagreement rather than a comment. A declared number with no consumer rots; this is the consumer
- [x] The upstream URL is the source's own business and appears exactly once — `tiles/src/tilesrc.cpp:9-24` is the only place a URL template belongs, and it moves into the plugin whole. `data/TerrariumDem.cpp:35-43` and `data/VersatilesVector.cpp:32-39` are the two `Url` overrides in the tree, behind `WebTileSource`'s protected virtual; held by `test/data/UncoveredIsUndeclared.cpp` (the URL reached is the elevation upstream's own) and `test/data/TheAnswerNamesItsAddress.cpp` (the path fetched is the ancestor's)
- [x] The zoom bound is the source's declaration about itself and exists **once** — `doc/bugs.md` records three copies of `15` across two languages plus a fourth number, `world/World.cpp:27 kMaxZ = 14`, with no stated relation to them. `data/SourceDecl.h` `MinZoom`/`MaxZoom`/`AncestorFill`, written once per source (`TerrariumDem.cpp:18` = 15, measured against a z16 404; `VersatilesVector.cpp:17` = 14, measured against a z15 404); `TilePool.cpp kProviderTerrainMaxZ` and `TerrainTiles::Config::SourceMaxZoom` are both deleted; held by `test/data/TheAnswerNamesItsAddress.cpp:53-54`. `World.cpp kMaxZ` stays and is **a different subject** — the world's split depth, not an upstream's bound — so one declaration each is the whole of the rule
- [x] `world/terrain/`'s `osmmesh_tile_provider` function pointer is **retired into this interface** — the C function pointer with its `void *user` is gone (2026-08-12) but it was retired into `World::TerrainSource` (`world/tiles/TerrainTiles.h:20-24`), a terrain-only abstract class with no registry, no `Covers`, no rank and no `Delivery`, so the line stays open and its subject moves. Two properties of `Data::Source` it must gain and one it must lose: it declares what it covers, it names which source answered at which address, and it stops returning a bare `std::vector<uint8_t>` in which *absent*, *pending*, *refused* and *empty body* are the same value (`doc/bugs.md`). `world/tiles/TerrainTiles.h` `TerrainSource::Take -> TerrainBytes` (four states, address and bytes handed over together through one door), `world/TilePool.cpp` `PoolTerrain` as the adapter, `data/Source.h` as what it delegates to; held by `test/data/TheAnswerNamesItsAddress.cpp`. **Read the tick precisely**: coverage and rank are declared *behind* this seam by `Data::Source`, not on `TerrainSource` itself, which is the arrow this line asked for — the consumer no longer knows an upstream exists. The bare vector and the thread-local are gone
- [x] **The `world/terrain/` C-ABI exception is struck** — `src/world/terrain/` is deleted, `src/world/tiles/` is C++ in `namespace outshine` with PascalCase names and state-carrying answers, and there is **no `.c` file anywhere in the tree**. It paid for two of the three items it promised: § I.17's *a C-ABI status reaches a house type at the boundary* is closed above, and the naming exception has no subject left. **The third did not land**: the two spellings of the Mercator band both survive (`world/tiles/TileMath.h:19` against `tiles/src/tilemath.h:11`, `doc/bugs.md`). Two further things this tick does **not** cover — `CLAUDE.md`'s exception clause still names `world/terrain/` and is now a stale pointer held with confidence, and eleven `fb_*` free functions in `src/` were never inside the exception and are now unexcused (`doc/bugs.md`)
- [ ] Weather is a source under the same contract, with the one property no tile source has: a **validity epoch**. A GFS cycle expires; a DEM tile does not, and a cache that cannot tell them apart either re-fetches bedrock or serves yesterday's wind
- [ ] The star catalogue is a source that declares `WholeWorld` addressing and **no upstream** — 53 KB, generated in this tree, never absent, never negative-cached (`tiles/src/stars.cpp:5-8` already states exactly this and it is a source declaration written as a comment)
- [ ] Peaks are a source whose scheme is a **query**, not a tile address — Overpass takes a bounding box. The addressing scheme is therefore an enumeration with more than one arm from the first day, and a design that assumes `z/x/y` everywhere has to be reopened to admit it
- [ ] Imagery consumed by the engine (§ I.16's open line) becomes a consumer registration against an existing source rather than new plumbing
- [ ] A source declares whether its product is **required for the world to be complete**. A missing imagery tile is cosmetic and a missing DEM tile is not; today both travel the same `Reply::Absent` and only the caller's site knows the difference
- [ ] The registry is declared in JSON in the library tier (§ I.24) — which upstreams exist, at which rank — so adding one is data, and the plugin is the code that knows how to speak to it
- [ ] A client or a test may **narrow** the registry to a named subset, so a test that must not reach the network declares zero sources and gets a refusal by name instead of a hang
- [ ] The `tiles/` sources fold in as decoders and sources — **14 224 lines** when counted 2026-08-12, of which `tiles/src` was 3 273 and `tiles/osmmesh` 1 239. The **decoders** landed the same day (`tiles/osmmesh/` deleted, 977 lines; `tiles/` now links `src/world/OsmVector.cpp` and nothing else of the library) and the **sources** did not: `tilesrc`, `wx`, `peaks`, `stars` are still behind an HTTP hop — and the HTTP surface, the nginx layer, the Dockerfile and the negative cache go with the process
- [x] The declared still (§ I.17's `verify-still`) survives the fold with its subject intact: arrival order is now the executor's rather than an HTTP race, which makes the imposed order *easier* to state and the gate's coverage of 2 orders out of 9! no better. `test/host/DelayedTransport.{h,cpp}` as a decorator over the host seam, driven by `OUTSHINE_ARRIVAL_SEED`/`OUTSHINE_ARRIVAL_SPREAD_MS` in `test/clients/AppWalk.cpp`; the Python proxy, the port and `TILES_BASE` are deleted. **Re-run 2026-08-12 by the architect against the real upstreams, exit 0: `3 imposed ingest orders, one picture bec69fea0a4e6837`, `terrainTris=331260 buildingTris=134990`** — the declared still, byte-identical. Coverage is now measured rather than assumed and it is **3 distinct orders out of 4 seeds**: two seeds collided, so the discriminating power of the instrument is 3/4 and the gate reports it. *The gate turns the content store off to run at all, so the one path a shipping run takes has no imposed order on it (`doc/bugs.md`)*

*Extended 2026-08-12 by the architect on the evidence of the round that built the section. Each line
below is something the built shape showed was missing, and each is ordered after the line it depends
on.*

- [ ] **A refusal names its cause as a value**, because `data/` may not name `Log` and a refusal that cannot say what refused it is the four-readings defect one level down. `Meaning::Refused` is minted today from a status that is discarded in the same expression, so a 401, a 500 past its retry budget, a body over the transport's ceiling, a local file that would not open and a wire that never answered are one enumerator. The cause travels **with** the state — the status where there was one, a declared enumerator where there was not — and the consumer logs what it was handed. **NO SUBSTITUTE**: a log line inside `data/` would break the include set, and a thread-local is what this section deleted
- [ ] **A query cancels what it began.** The ticket is an owning handle, so a `Data::SourceSet::Query` that is dropped, moved from or destroyed leaves no transfer running and no entry in the transport's table — `R.1`, `C.30`, `C.31`, `C.64`. `Abandon` as a free operation the caller must remember is the rule written down; the destructor is the rule carried
- [ ] **The transport declares a wait, so a thread with nothing to do blocks rather than polls.** Asynchronous-by-shape is the right seam and a 1 ms sleep loop is the wrong consumer of it: the pool holds a worker for up to 30 s of wall re-taking the transport's lock a thousand times a second, and the two thread pools together reach 14 threads on 2 performance and 4 efficiency cores (`CP.40`, `CP.41`). One more virtual — wait until any of these tickets settles, with a deadline — collapses them to one waiter and one worker set without giving back the property a browser host needs
- [ ] **TOOL: wall per request, split into wire and wait, as a distribution.** `Ledger::FetchMs` is the sum of `SourceSet::Collect` calls and `FetchBlockedMs` the whole span, so the difference is *sleep* and not *upstream*; neither is per request and neither is a distribution. p50/p95/p99 over a **cold** traversal, with the content store's state named in the row, is what decides the line above — the number does not exist yet, which is a cost and not a boundary
- [ ] **The registry's and the store's counters ride the ordinary telemetry row.** Fifteen exist and none is read: asked, delivered, handed over, vacant, undeclared, refused, retried, from-store and delivered bytes; hits, misses, writes, write failures, swept and swept bytes. The store's hit rate is the one number that says whether the store is doing anything, and without it a run at 131 MB/s reads as a fast network. A ledger with no reader is § I.23's drawer, applied to a counter
- [ ] **An imposed arrival order survives a warm store**, because that is the path a shipping run takes. The order is a property of the *delivery* and not of the wire, so the instrument sits where a store hit and a wire answer both pass — inside the selector, as a declared library facility — and not as a host decorator the store answers in front of. Today the still gate must turn the store off to have anything to order, so the gate and the shipping path are disjoint
- [ ] **`Undeclared` is its own answer at every rung and it ends the run loudly** (§ I.17). No source covering a request is a declaration error that cannot heal; sharing an arm with a wire refusal — which does heal, and is therefore retried — makes it an unbounded retry with an error line per pass and a load that never reaches 1.0
- [ ] **The content store's cap holds during a run**, not only at construction. A sweep before the first write is not a cap; it is the shape the 7 GB store this replaces already had, one level up
- [ ] **The bake that produces the star bands is C++ under `test/`.** The HYG catalogue is measured data and admissible as such, exactly like a DEM; what this tree *owns* is the computation over it — precession to the declared epoch, the magnitude binning, the `<HhBB>` quantisation — and that computation is Python today (`assets/sky/stars/build_stars.py`, 170 lines), against *modern C++, and only C++*. Deleting it is not the alternative: the bands would then be bytes nobody can recompute. The two honest resolutions are port it, or declare the *bands* measured data and lose the epoch — and the first is the one that keeps the epoch a number this tree can move
- [ ] **The egress wire is behind a declared host seam too.** `src/clients/HttpPost.cpp` is library source that includes `<curl/curl.h>` and carries an `EM_JS` arm; `ServerLog` and `ServerTelemetry` are its consumers. It is the same need the ingress seam was built for, in the same tree, in the opposite direction — *the library declares what it needs from a host and calls nothing else* is not true while it stands. One seam or two is a design question; a transport library linked into `src/` is not

### I.23 Constants: one declaration per number

*Added 2026-08-12 on the owner's ruling: **no magic numbers, one const header**. The tension with
`CLAUDE.md`'s* every number carries its origin — derived, measured or `[SET]` — with unit and frame of
reference *is real and is resolved here rather than around: a single header of hundreds of constants is
the classic place where that discipline dies. Measured baseline this round over `src/`, comments and
string literals stripped and the eight trivial values excluded: **1 484 non-trivial numeric literals in
266 files**, against **193 `constexpr` declarations** and **105 `[SET]` tokens that nothing counts**.
Densest: `generators/draw/BuildingShape.cpp` 101 · `clients/SubjectBench.cpp` 97 · `core/ClusterDag.h`
81 · `render/Renderer.cpp` 67 · `render/stages/ModelDraw.cpp` 64.*

**One header cannot be literally one file**, because the layering is the build: a generator translation
unit compiles with `-Isrc/core -Isrc/generators` and nothing else, so a header holding a `render/`
constant beside a `generators/` one would give a generator a name for a render concept and dissolve the
gate. So the rule is **one const header per layer**, each including the layer below it, and the property
that is actually wanted — *one place to look, and no number in two of them* — is held by a check rather
than by a filename.

**"No magic numbers" is not "every constant in one file".** The magic number is the **unnamed literal at
a use site**. A named `static constexpr` member beside its single consumer is not one, and moving it into
a shared header would make it worse: it would be a number two layers can see that only one needs.

- [ ] `core/Const.h` as the one spelling a reader looks in for `core`, with one file per **subject** below it — units, Earth and the tile scheme, sky and ephemeris — and never one file per consumer. A subject has an owner who can say whether a number is right; `RendererConstants.h` has none
- [ ] One const header per layer, each including the one below: `core` · `generators` · `world` · `render` · `clients`. Five places in the program, and the layering already forbids the sixth
- [ ] `core/Units.h` folds in as the first subject file — it is already the shape: exact ratios rather than truncated decimals, derivation as the initialiser, one comment per deviation
- [ ] **A derived constant is written as its derivation**, never as its value: `kDeg2Rad = kPi / 180.0`, `kKtToMs = kNmToM / 3600.0`. The derivation then cannot drift from the number, because it *is* the number. `core/Units.h:11 kRad2Deg = 57.29577951308232` and `:22 kMsToKt = 1.9438444924406` are the two that are not, and one of them says so
- [ ] **An origin that is not a derivation is spelled, not commented**: `Const::Set(v)` for a decision and `Const::Measured(v)` for an instrument's reading, two `constexpr` identity functions in the const namespace. Then *every number carries its origin* is a property a 20-line test decides — a bare floating literal in a `const/` header is a failure — instead of 105 free-text tokens nothing reads. There is no `Derived` spelling because derivation is visible in the initialiser
- [ ] A constant's **name ends in its unit token**, from a declared suffix table with no ambiguity in it. The tree has one today: `Ms` is metres-per-second in `clients/Walker.h:17` and `core/Units.h:22`, and milliseconds in `clients/FrameTelemetry.h:33` (`doc/bugs.md`). `MPerS` and `Ms` resolve it and `core/Units.h:15 kMPerDeg` shows the spelling already exists in the same file
- [ ] A constant with **exactly one consumer does not enter a const header** — it stays a `static constexpr` member beside its consumer, which is what `SubjectBench::kFovDeg` and `Renderer::kNearM` already are. This is the anti-junk-drawer rule and it makes the header shrink under use rather than grow
- [ ] A constant with **zero consumers fails the check** — a dead constant is dead code, and zero-consumer is the mechanically decidable symptom of a drawer. `CLAUDE.md` carries the dead-code rule; the Guidelines do not
- [ ] The const headers are **not included by any JSON reader**, which is the line that stops a tuning value being smuggled in as a constant (§ I.24)
- [ ] The literal ratchet as a test, not a ban — population: floating literals and integers outside `{0, ±1, 2, 3, 4}`, in `src/`, excluding `const/` headers and `static_assert` arguments; per-file counts published; **the test fails when a file's count rises**, exactly as the hardening ledger's counts do. Baseline is the 1 484 above and it is a first reading, not a target
- [ ] The ratchet states its own weakness in its own output: a count is gameable by folding two literals into one expression, so the per-file count is published for a human to read the diff against, and the number is never presented as a proof
- [ ] Constants per file and consumers per constant published by the same test, so *"is the header a drawer"* is a number

### I.24 Settings in two tiers, and what makes the first one untouchable

*Added 2026-08-12 on the owner's ruling: **the library carries JSON settings that generators and
providers require — defaults, and values a consumer should not change; everything else is set by the
client or the test.** The split half exists already and has never been stated: `assets/world/*.json`
(ground materials, vegetation classes, 34 species files) is the library tier and `mods/*/mod.json` is
the client tier, and nothing in the tree says so or enforces it.*

**The tier test, and it has to be sharper than *should not*.** A value belongs to the library when a
wrong value there is a **defect in the engine**; it belongs to the client when a wrong value there is a
**defect in the run**. Beech leaf length wrong → the engine grows a wrong beech → library. Camera at the
wrong latitude → the run looked at the wrong place → client. An upstream's zoom range wrong → the engine
asks for tiles that do not exist → library. Render width 640 → the run measured something other than
720p → client.

- [ ] Two readers, two types, and no key path from one into the other — the library tier is not *merged with* the client tier, it is a different object
- [ ] The library tier is `const` at the type level: constructed once, handed out as `const&`, **no setter exists**. *Untouchable* is then `C.12` and `Con.*`, not a sentence in a document
- [ ] The client tier is an **enumerated** surface, not an open one: the scenario schema of § I.4 names every settable key and a key it does not name is refused with its path — so *"everything else"* is a list rather than a hole
- [ ] A test overrides the library tier by **substituting a whole table, never by patching a key** — `FromSubstitute(tables, why)` beside `FromDeclared(storage, root)`, with `why` required and a run refused when it is empty. A patched key is invisible in the row it produced; a substituted table is a declared act
- [ ] The substitution's `why` enters the run identity, so a run built on a substituted table **cannot enter the archive looking like a shipping run** — the same defect class as § I.17's `client=gpu_walk` string literal
- [ ] The `why`-or-refuse shape is the one `clients/Scene.h:30-37` already uses for a render size that is not the budget's 1280×720 — this is that pattern generalised, not a new one
- [ ] **A number is a constant if changing it is a code change, and a setting if changing it is a data change. Nothing is both**, and the check is mechanical: no `constexpr` in a `const/` header may share a name with a key in any library-tier schema
- [ ] **No constant is a default for a setting.** A default lives in the library JSON where the owner ruled it lives; a constant has no alternative value at all. That gives every number exactly one owner and makes the previous line maintainable
- [ ] The library tier is read through `Host::Storage` (§ I.18) and never `fopen` — it is the same call on every target, and it deletes the three `fopen` sites in `world/`
- [ ] Every generator and every provider states **which library tables it requires**, so a missing table is a named refusal at load instead of a default nobody declared. § I.4's *declared strata list per ground class, with no global default, so an unclassified place grows nothing* is the same rule for one table
- [ ] A library table that no generator and no provider requires is reported — the drawer check of § I.23, applied to data
- [ ] The library tier's location is `assets/` and it ships with the library; the client tier's is the scenario. A test that declares neither gets a refusal naming both
- [ ] **A value lives in exactly one tier**, and a key present in both is refused at load with its path — the ruling's *everything else is set by the client or the test* is only enforceable if the two sets are provably disjoint, and disjointness is a check over two schemas rather than a habit

**A scenario carries its own data.** *Owner's ruling, 2026-08-12: a scenario must be able to provide
its own glTF assets, and to give a provider — elevation above all — data from a file inside the
scenario, so a run has correct terrain, depends on no network, and goes at the machine's full speed.
This was done once for the FlightBox gym.* **It needs no new mechanism, which is the evidence the
registry is the right shape:** a scenario-declared source is a `Data::Source` like any other — it
declares what it covers, it is registered at a rank, and everything else follows.

- [ ] A scenario declares sources of its own, registered above the network ones, so a covered request never reaches a wire
- [ ] A scenario declares whether the network may be reached at all; with it refused, the provider list is exactly what the scenario carries and exhaustion is the terminal absence — the same rule, not a second one
- [ ] A file-backed elevation source: the scenario names a file, the source declares the box and the zoom range it holds, `Covers` answers from the declaration without touching disk
- [ ] A scenario carries its own glTF assets, addressed relative to the scenario, so a subject is declared where the scene is declared
- [ ] A run with no network and no store reaches its verdict at the machine's speed, and **the same scenario with the network available produces the same answer** — that identity is the test, and it is the same shape as *cache on and cache off differ only in timing*
- [ ] A scenario whose declared file is missing, unreadable or does not cover what the scene asks for is refused **by name and by path**, never silently filled from the wire — a fallback here would make a deterministic run quietly non-deterministic

### I.25 Scenario axes, and the scenario with no world

*Added 2026-08-12 on the owner's ruling: **it must be possible to declare a scenario with no world at
all — one tree, one building, one car — and the engine must be flexible enough to define what a test
needs, like a headless Blender.** The question put was whether* world present or absent *is a third
dimension beside camera × clock.*

**It is not a third dimension, and treating it as a boolean is what produced today's special case.**
Camera (`Fixed` · `Keyframed` · `Driven`) and clock (rate 0 · timeline · rate 1) are axes of the
**observer**. World-or-not is the **subject**, and choosing it *removes fields from the observer's axes*
rather than adding a dimension: a studio scenario has no latitude, because there is no place, and no
civil time, because the light is declared rather than computed from an ephemeris. A boolean would have
kept those fields and left them meaningless — which is measurably today's state, at eighty dead fields
across ten declared scenes (`doc/bugs.md`).

**A studio stage is a declared `Ground`, and that is the whole trick.** `Ground` is already the entire
interface between the world and every generator — *"height, slope, class with edge distance and
runner-up, water level and the declared tables — resolved values, never a callback"* (§ I.9). A studio
stage declares those values directly instead of resolving them from a tile, so **every generator becomes
benchable with no second code path**, and a lone building comes out of `Buildings` rather than out of a
bench that had to learn what a building is.

- [ ] `Stage` as an enumeration with a record per arm (`Enum.2`, and `I.23`'s *each kind reads its own parameter object rather than a shared flag soup*, which `clients/Scene.h:39-41` already states for `Run`) — `World` carries the standpoint, `Studio` carries substrate, key light, backdrop and subject
- [ ] A `Studio` scenario **has no latitude to declare**, so the Mercator band refusal is not merely unreachable there, it is unspellable — and `SceneRunner.cpp:32 kSubjectGroundAslM = 100.6` has no home
- [ ] A `Studio` scenario has **no civil time and no met wind**: the key light is declared as elevation, azimuth and irradiance, and the wind as a value, because a still life judges form and an ephemeris there is a number nobody chose
- [ ] The declared key light's irradiance is **W/m² perpendicular to the beam**, the same convention `IrradianceStage`'s `sunDirectNormal` already carries and the same one Blender's Sun Strength is (§ I.26) — so the studio light and the oracle's light need no conversion between them, and the commonest single error of this class, perpendicular against on-the-horizontal, has one spelling in the tree instead of two
- [ ] The studio's **ambient is declarable as a uniform environment radiance**, not only as a sky model, because the oracle's default world is one and because it is the only ambient with a closed form. `render/stages/IrradianceStage.h`'s `skyIrr` is *diffuse on horizontal*, so a uniform environment of radiance `L` enters as `π·L` and the conversion is stated once, here
- [ ] The four things a studio must declare, because `SubjectBench` had to invent all four in C++: **substrate** (a ground-material class, or the 18 % neutral), **key light**, **backdrop** (a card, a declared sky, or nothing), **subject**
- [ ] The studio's `Ground` is declared in full — height, slope, class with edge distance and runner-up, water level, and which library tables are in force — so a generator run on a studio stage cannot tell it apart from a region, and a difference between studio and world output is therefore a defect rather than a category
- [ ] The **subject is a generator invocation**, not a species name: generator, seed, and the parameters that generator declares. `clients/Scene.h:77-82 SubjectRun` carries `Template` and `Species` and nothing else, which is exactly why a building or a car has no bench today
- [ ] Exactly one subject stands at the origin, and the count is one because a bench that frames two things is measuring composition rather than form
- [ ] `SceneRunner::BringUp`'s special case is deleted — *"whether a scene needs a world at all is the scene's own statement"* is the comment at `SceneRunner.cpp:150-151` and the code below it asks `Runs().front().What == Kind::Subject`. The stage is the statement
- [ ] `SubjectBench`'s **measurement survives whole**: the full view × light matrix, the three-render depth-buffer `Fill` that separates frame from card from floor (measured 64.7 % against 13.0 % for the colour-difference alternative it replaced), the turn-based readback discipline, and the 30° lens with its reason. What is replaced is exactly the part that made it vegetation-only — `Select`/`SelectTree`, `Stand(lat, lon, 100.6)` and `kSunElDeg` as a C++ constant
- [ ] `fovDeg` declared once — `mods/demo/mod.json` and `SubjectBench.h:62` both say 30 and the C++ one acts (`doc/bugs.md`)
- [ ] A studio scenario **needs no network and no tile source**, which is what puts a generator test in § I.20's `host` tier instead of its `world` tier — the single largest effect this section has on the suite
- [ ] Camera and clock keep their axes unchanged across both stages: a studio scenario can be a still, a turntable film or an interactive model viewer with no new mechanism, because those are the observer's axes and the stage did not touch them
- [ ] A `World` stage and a `Studio` stage produce the same telemetry schema, so a bench row and a walk row are comparable — a bench is a layer over the system and never a mode inside it (§ I.11)
- [ ] More than one subject kind demonstrated before the section is ticked: one tree, one building, one vehicle, from three different generators, through one declaration

### I.26 glTF, and the first check against something outside this tree

*Added 2026-08-12 on the owner's ruling. This file's own measurement rule ranks* correctness — checked
against something outside *above* consistency — two parts of this tree agree*, and says another digit of
internal agreement is worth less than the first external check. **This repository has never had one.**
Rendering a glTF scene here and the same scene in Blender is that check. Blender 5.2.0 LTS is on this
host (build 2026-07-14) and runs headless.*

**Four rungs, and each isolates one thing.** The order is the design, because a light comparison whose
camera is half a pixel out produces a residual at every silhouette that nobody can attribute.

| Rung | What it compares | Needs |
|---|---|---|
| 1 | **coverage** — a binary mask, no light at all | the reader, nothing else |
| 2 | **depth** — linear view-space range | an existing readback |
| 3 | **direct diffuse radiance** — linear, pre-tone-map | a linear tap that does not exist |
| 4 | **shadow and indirect** — as a bias curve, never a verdict | rung 3 |

**Rung 3 is three-way, not two-way.** For a flat facet under one directional light the answer is closed
form — `L = ρ·E·cos θ / π` — so the referee is arithmetic and Blender is the tie-breaker on what
arithmetic cannot reach. That matters because Cycles is not ground truth everywhere: its own Principled
BSDF carries open energy-conservation issues and the Glass BSDF fails the white-furnace test
(blender/blender #158426, #159635), so the oracle is pinned to the lobe it is known-good on.

**Blender's default lighting, read and not recalled.** *Owner's ruling, 2026-08-12, two clauses: **match
Blender rather than making Blender match us** — our scenario declares whatever Blender's defaults
already are, so the reference is configured by* not *being configured — and **Blender is open source, so
read what Cycles computes** rather than inferring it from renders. Everything below was queried from the
shipping binary with `blender --factory-startup --background` (**5.2.0 LTS, hash `fbe6228777e7`, built
2026-07-14**) or read in the source that performs the conversion. The app bundle ships the Cycles kernel
headers whole at `…/Blender.app/Contents/Resources/5.2/scripts/addons_core/cycles/source/kernel/`, which
is the exact kernel this binary runs; host-side paths are `intern/cycles/…` at tag `v5.2.0`.*

| Quantity | Factory value | Where the number is |
|---|---|---|
| key light | `POINT`, power **1000 W**, colour (1,1,1), radius **0.1 m**, at **(4.076245, 1.005454, 5.903862)**, `normalize` on, `exposure` 0 | startup scene, queried |
| what "Power" becomes | `strength = colour · energy · 2^exposure` — the only place a watt enters Cycles | `blender/light.cpp:58` |
| point radiance | `area = 4πr²`, `invarea = normalize ? 1/area : 1`, `eval_fac = invarea/π` → **2533.0 W·m⁻²·sr⁻¹**, total flux 1000 W, intensity **79.577 W/sr** | `scene/light.cpp:132-145` |
| its irradiance at the origin | **1.51627 W/m²** perpendicular at d = 7.244467 m. A uniform sphere fully above the horizon gives *exactly* the point-source value, so there is **no area-light approximation** to carry | derived from the two rows above |
| world | `Background` node, colour **0.05087608844041824** linear on all three channels, strength **1.0** | startup scene, queried |
| is the world a light? | **Yes, by default.** `sampling_method` is `AUTOMATIC` and `sample_as_light = (method != NONE)`; a `BackgroundLight` is created with MIS on whenever a world exists | `blender/light.cpp:90-93,136` |
| what that ambient is worth | a uniform environment of radiance `L` puts exactly **`ρ·L`** out of an unoccluded facet — no π — hence **10.5 %** of the key at normal incidence at the origin, and **all** of the light on the unlit side | derived |
| camera | 50 mm, sensor 36 mm, fit `AUTO`, clip 0.1/100 m, at (7.358891, −6.925791, 4.958309), Euler XYZ (1.109319, 0, 0.814928); `AUTO` fits the larger raster dimension, so at 1920×1080 **hfov 39.5978°**, vfov 22.8952° | startup scene; `blender/camera.cpp:415-425`, `:674` `fov = 2·atan((0.5·sensor)/lens/aspect)` |
| subject | cube of ±1 m, 6 flat-shaded quads, 8 vertices | startup scene, queried |
| engine | **`BLENDER_EEVEE`** — Cycles is *not* the factory renderer | startup scene |
| view transform | **AgX**, display sRGB, exposure 0, gamma 1 | startup scene |
| sampling | 4096 samples, adaptive on at 0.01, **denoising on**, Blackman-Harris **1.5 px**, `max_bounces` 12 / diffuse 4, `film_exposure` 1.0, light tree on, `sample_clamp_indirect` 10.0 | startup scene |
| a newly added lamp | every type defaults to **10 W**; `SUN` angle **0.00918043 rad = 0.526°** | queried |
| **Sun lamp units** | `SunLight::area = π·sin²(angle/2)`, `eval_fac = 1/area`, so the disk's irradiance on a perpendicular surface is `strength` **exactly and independently of the angle** — Blender's Sun Strength *is* W/m² perpendicular to the beam | `scene/light.cpp:298`, `:316` |
| Diffuse BSDF | `max(dot(N,ω),0)·(1/π)` times the closure weight — **exactly Lambertian**, and the closure carries no roughness parameter at all | `kernel/closure/bsdf_diffuse.h:46` |
| pixel filter | box is the constant 1; the importance table spans `[0, width/2]` symmetric, and `raster = (x,y)` **before** the table offset is added — so **the integer raster coordinate is the pixel centre** | `scene/film.cpp:26-29`, `:74-81`; `kernel/camera/camera.h:458-464` |
| narrowest filter reachable | `pixel_filter_type='BOX'` with `filter_width` at its RNA minimum **0.01 px** → every sample within **±0.005 px** of the pixel centre | `cycles/properties.py:875-888` |
| transparent film | applies only to a ray carrying `PATH_RAY_TRANSPARENT_BACKGROUND`, i.e. camera rays, and writes alpha — **an exact coverage channel that does not touch the lighting** | `kernel/integrator/shade_background.h:103-107` |

- [ ] **The factory values the harness does not set are declared once, not re-queried and gated.** *Relaxed 2026-08-12 on the owner's ruling — **we do not aim to be bit-identical with Blender, so the exact version does not matter much**, and a gate that refuses a run because a factory default moved defends an exactness we are not claiming. What the table above is, after the relaxation: **the derivation of the numbers we do set**, read in the source once so that nobody re-derives them from memory. It is documentation of a reading, not a contract with a binary.* The `blender --factory-startup` flag stays, because *configured by not being configured* still removes a whole class of accidental divergence — it just is not policed
- [ ] The deviations from the factory startup are a **closed, reasoned list**, and there are six: **engine** `BLENDER_EEVEE → CYCLES` (unavoidable — the oracle is the path tracer) · **samples** fixed and `use_adaptive_sampling` off (4096 with an adaptive threshold is not a reproducible number) · **denoising off** (a denoiser is an estimator with no error bar) · **pixel filter** `BOX` at 0.01 px for the geometric rungs (§ below) · **output** OpenEXR float32, which ignores the view transform by Blender's own colour-management rule and thereby deletes AgX in one move · **resolution** 1280×720, this engine's budget. Nothing else is touched, and in particular **the world colour, the world strength, `film_exposure`, `scale_length` and the sensor stay at their factory values**
- [ ] **The world stays Blender's factory world on every rung** — `0.05087608844041824` linear at strength 1.0, sampled as a light. It is the half of *default lighting* that costs nothing to match and it is the half most likely to be wrong here, because our ambient is a hemisphere over geodetic up and Blender's is a full sphere (§ II.8)
- [ ] **The key light is Blender's `SUN` lamp for the light rungs, and this is a declared deviation from the literal factory default, with its reason.** Blender's factory key is a point light and `src/render/` has no punctual light of any kind — `Gpu.h:22 SceneLight` is one irradiance pair, one cascade buffer and one shadow atlas — so matching the default literally would put the first external check this repository has ever had behind three unbuilt features on our side. The sun costs **no conversion at all**: Strength is W/m² perpendicular to the beam by `scene/light.cpp:298,316`, which is the unit `IrradianceStage`'s `sunDirectNormal` already carries, leaving exactly one unknown in the comparison — which is the unknown rung 3 exists to settle
- [ ] **The factory point light is not dropped, it is a rung** — scene 8 below is the 1000 W point at its factory position, and it is the rung that first requires the punctual light § II.8 already owes. The ladder drives that feature rather than the feature blocking the ladder
- [ ] **`KHR_lights_punctual` is refused as the light channel for rungs 1–7 as a simplification, not as a necessity** — and the reason on the previous version of this line was incomplete. The 683 lm/W factor is real and is in the shipped importer (`io_scene_gltf2/blender/com/conversion.py:10 PBR_WATTS_TO_LUMENS = 683`), but `blender/imp/light.py:57-77` applies it **only in mode `SPEC`**; `COMPAT` multiplies point-likes by 4π alone, and **`RAW` passes the number through unchanged**, in which case a glTF directional `intensity` becomes Blender Sun Strength one-to-one and is therefore W/m² perpendicular by the row above. `SPEC` is the default (`io_scene_gltf2/__init__.py:198-206`), so the factor is opt-out, not unavoidable
- [ ] `import_settings['export_import_convert_lighting_mode'] = 'RAW'` recorded on the row of any comparison that does let a light cross the glTF boundary, because the mode is the unit
- [ ] **The anti-aliasing objection is dissolved for the geometric rungs, and it is dissolved by the source rather than by tolerance.** With `BOX` at 0.01 px, Cycles evaluates the same predicate a centre-sampling rasteriser does — *is the pixel centre inside the primitive* — because the integer raster coordinate is the centre and the sample never leaves ±0.005 px of it. The residue is an **instrument floor of 0.005 px**, published beside the result, with an expected disagreement of ≈ 0.01 × the silhouette length in pixels; on a subject filling 30 % of 1280×720 that is **≈ 30 pixels at risk and ≈ 15 expected to flip**, and a run reporting more than the floor has found something
- [ ] Rung 1 therefore has **two** products and they answer different questions: the **binary mask** at 1 spp / box 0.01 against our centre-sampled coverage, whose acceptance is the floor above; and the **alpha coverage** at N spp / box 1.0, which is the analytic pixel-area fraction and gives a **signed sub-pixel edge offset** (`α = 0.5` is the edge through the centre) — the boundary metric with an error bar instead of a count
- [ ] Our side of rungs 1 and 2 needs **no colour tap at all**: `render/Renderer.h:66 ReadDepth` already returns reversed-Z float depth with its range conversion stated, so coverage is *depth ≠ far* and range is `kNearM/depth/cos(off-boresight)`. The two cheapest rungs are unblocked today and depend only on the reader and the studio scenario (§ I.25)
- [ ] **Indirect light stops being a confound and becomes a setting**: `diffuse_bounces = 0` makes Cycles' answer *exactly* the direct term, so rungs 5–8 are compared against closed form with no bounce to argue about, and rung 9's product is the 0-bounce against 4-bounce difference — the bias curve, which is the only actionable form
- [ ] Scenes 1–8 contain **one convex subject and no second surface**, so interreflection is identically zero by geometry and not merely by a setting. The first scene with a second surface is 9, and that is the scene where indirect is the subject
- [ ] Blender's own residual against the closed form is published beside ours on every radiance rung — the oracle states its error before it judges ours — and for the Diffuse BSDF that residual is expected to be Monte-Carlo noise alone, because `bsdf_diffuse.h:46` *is* the closed form

**What decides a rung, and why a Blender point release cannot move it.** *Owner's ruling, 2026-08-12:
**we are not aiming for bit-identity with Blender.** That relaxation is only safe if the tolerances are
written down and are visibly far above point-release noise — otherwise "we do not need exactness" is a
sentence that quietly absorbs a real regression. So the tolerances are stated here, in the document,
with the distance to the noise floor beside each.*

- [ ] **A release version pins the oracle, never a commit SHA.** `Blender 5.2 LTS` is the pin; the build hash is recorded for the record and is not a condition of the run. A build from a different point release is a valid oracle
- [ ] **The version is recorded on every comparison row, and that survives the relaxation intact** — not as an exactness claim but as the **attribution** of a red: when parity fails, one line answers whether our renderer moved or the oracle did, and without it that question costs a bisection
- [ ] **The version is part of the oracle cache key**, one string, and it prevents exactly one defect: the pin is bumped, scene and recipe unchanged, and the cache serves a render from the old Blender while the manifest claims the new one — **the manifest lying about its own output**. It is the same hash-is-the-filename discipline § I.22 already applies to a provider
- [ ] **The tolerances, and each is at least an order above point-release noise.** Rung 1 coverage: boundary p95 ≤ 0.5 px against an instrument floor of 0.005 px — **a factor of 100**. Rung 2 depth: p99 ≤ 1e-4 relative, against a float32 mantissa floor near 6e-8 — **three orders**. Rung 3 radiance: median relative difference ≤ 1 % against the closed form, where Monte-Carlo noise at a declared sample count is the only Blender-side term and is itself published. **Nothing at that scale moves because a point release changed a sampler**, and if a point release *did* move a number by 1 % it would be a finding about Cycles worth having
- [ ] **The 0.005 px instrument floor survives the relaxation untouched, because its justification is geometric and not empirical**: Cycles' box filter is the constant 1, the integer raster coordinate is the pixel centre, and `filter_width` floors at 0.01 px by its own RNA minimum. That argument does not mention a version
- [ ] **`Diffuse BSDF` at roughness 0, never Principled — and the relaxation *reinforces* this rather than loosening it.** Principled was rewritten in Cycles 4.0; a design leaning on it would be genuinely version-sensitive in exactly the quantity rung 3 measures. The Diffuse BSDF is `max(dot(N,ω),0)·(1/π)` times the closure weight and has no roughness parameter at all, so it is the one lobe whose value is a closed form rather than a model choice
- [ ] **Light and material never crossing the glTF boundary is kept and is not a version argument** — it is about importer semantics, and the lighting-mode factor that motivates it (`SPEC` versus `RAW`) is a mode, not a release
- [ ] **No acceptance anywhere is framed as pixel identity.** Every rung's product is a distribution, a residual shape or a bias curve, and a rung whose acceptance could only be written as *the images are equal* is a rung that was designed wrong
- [ ] **Cycles on Metal is not bit-reproducible at 4096 spp, measured, and this is evidence for the ruling above rather than a caveat under it.** Two runs of `render/coverage/triangle`'s `coverage` recipe at seed 0, everything else identical: **11 of 921 600 pixels differ (1.2e-5)**, max |Δ| **2.9e-11**. That delta is exactly `2^-35`, which is **one float32 ulp in the binade `[1.22e-4, 2.44e-4)`** — so the disagreement is confined to samples about four orders below the subject's own level and cannot be anything but the last bit of a near-black accumulation. **Alpha is bit-identical**, and the 1 spp `default` mask is bit-identical entire
- [ ] **What the derived cache key therefore promises: a valid render of that recipe, never identical bytes.** Two cache states can differ in the last bits of eleven pixels and both be correct, so a key is a statement about *provenance* and not about *content*. **Coverage scoring is unaffected by construction** — the mask reads alpha, alpha is bit-identical, and the radiance rungs' tolerances (≥ 1 % relative, § above) sit **nine orders** above 2.9e-11. *Stated here because the alternative reading — treating non-determinism as a defect to chase — would put a bit-exactness contract under an estimator that has none, and it is the exact shape `CLAUDE.md`'s determinism rule is **not** about: the rule binds our mathematics, and the oracle is a Monte-Carlo integrator by choice*
- [ ] **The derived cache key covers both Blender versions and neither alone is enough** — the **declared** version, so a manifest bump on an unchanged host misses, and the **observed** version and build hash, so a host that moved under an unchanged manifest misses. Plus every subject's file digest and the converted glTF's digest · the whole declared scene, camera, light, world and material · the whole render recipe · a derivation version · and the **product** — `exr`, `png`, `raw` — separately. Built the way `Data::ContentKey` builds one, newline-separated, and stored in the one global content store (§ I.22): `hash = filename`, no index, no sidecar, writes landing on a temporary and renamed so a name never precedes its bytes. **There is no second cache**

**IoU is reported and never enforced, because it is shape-driven.** *Owner's ruling, 2026-08-12, with
the derivation re-run here and confirmed exact. For two masks whose boundary is displaced by a mean δ,
the symmetric difference is a band of area ≈ P·δ, so `IoU ≈ 1 − P·δ/A` to first order — and `P/A` is a
property of the **subject's shape and size**, not of the renderer.*

*At 1280×720 with an equilateral triangle filling a quarter of the frame: `A = 230 400 px²`,
`s = √(4A/√3) = 729.5 px`, `P = 3s = 2 188 px`, so `P/A = 0.009498 /px`.*

| δ | IoU | what it is |
|---|---|---|
| 0.005 px | 0.99995 | the instrument floor |
| 0.05 px | 0.99953 | nothing |
| **0.105 px** | **0.999** | the old threshold, back-solved |
| 0.5 px | 0.9953 | **a pixel-centre convention bug** |

- [ ] **The two findings the arithmetic produces, and both are reasons not to gate on it.** A fixed `IoU ≥ 0.999` means `δ ≤ 0.105 px` on that triangle and `δ ≤ 0.053 px` on a subject a quarter the area — `P/A` doubles when linear size halves — so **one constant drifts 2× in strictness as the subject changes**, silently. And a **half-pixel convention error scores 0.9953**, which reads as near-perfect to anyone who has not done this arithmetic. A number that hides the defect it was chosen to catch is worse than no number
- [ ] **Boundary displacement p95, in pixels, is the deciding instrument on every geometric rung.** It is shape-independent and it **names** the defect rather than scoring it: 0.05 px is nothing · **0.5 px is a pixel-centre or raster-convention error** · ~3 px is a projection error · a radial trend is focal length · a shear is handedness. IoU is published beside it for continuity with the literature and decides nothing
- [ ] **Three subject classes, three instruments, and the class is a declared property of each rung rather than a global threshold:**

| Subject class | Instrument | Number |
|---|---|---|
| **opaque, all geometry ≥ 1 px** | boundary p95 | **≤ 0.1 px** — 20× the 0.005 px floor: room for float32 and for a tessellation ordering, none at all for a convention error |
| **opaque, sub-pixel geometry present** | boundary p95 | **≤ 0.5 px, reported and not enforced** — a rasteriser drops a triangle no sample centre hits and a 128-spp path tracer finds it. That is a **sampling-policy difference**, not an error, and gating on it would make the ladder punish us for being a rasteriser |

- [ ] **Two classes decide a *parity* rung, and foliage is not a third class — it is a different question with a different ladder** (§ I.26.7). *A foliage row stood here as a weak-instrument exemption, was deleted on a foliage-exclusion rule, and the rule was then overruled. Both versions were wrong in the same way: they treated a forest as a **subject** of the parity comparison, badly measured. It is not. A forest is measured by **frame time and by eye in motion**, and it does not appear in this table because no coverage instrument applies to it at all — not because it is exempt from one*
- [ ] **Which class each rung is, stated in its manifest, because it is the rung's own property**: rungs 1–4, 6–11, 13–15 and 17 are **opaque ≥ 1 px** and carry the 0.1 px number · rung 5 is opaque ≥ 1 px **at sponge level ≤ 2**, where the smallest feature is `729/9² ≈ 9 px`, and **level 3 is deliberately the sub-pixel arm** at `≈ 1 px` · rungs 16 (`Fox`'s limbs and tail at distance), 18 (`SciFiHelmet`'s greebles), 19, 20 and 21 are **sub-pixel present** and report rather than enforce · rung 12 is **format conformance** and carries neither number (§ I.26.5)
- [ ] **Rung 20 is the one rung where a sub-pixel report is the product rather than a concession** — a Julia isosurface at rising subdivision *is* the sub-pixel regime, and the number worth having is where boundary p95 leaves the 0.1 px band as subdivision rises. That curve is a measurement of our rasterisation policy against a path tracer's, on a subject built to force it

**Radiance: the oracle is configured DOWN to the physics the engine claims on that rung.** *Owner's
ruling, 2026-08-12, and it is the clause that stops the ladder growing a permanent red.*

| Oracle configured to | Realistic median relative difference |
|---|---|
| direct, Lambertian, no bounce | **≤ 1 %** — a closed form; both sides should hit it |
| + an analytic sky | **2–3 %** |
| full GI, against a renderer with none | **10 %+ in the open, 100 %+ in shadow** |

- [ ] **`diffuse_bounces = 0` stays until this engine implements bounce, and the last row of that table is not a defect.** A number produced against physics we do not model measures **our ambition, not our correctness**, and it would sit in the tree for months looking like a failing gate — which is how a suite learns to be ignored. When bounce lands, the oracle's bounce count rises with it, in the same commit
- [ ] Rung 4's shadow-and-indirect product keeps its existing shape and this makes it consistent rather than changing it: it is a **bias curve, never a pass/fail**, of the form *our screen-space occlusion removes 0.6× of what one Cycles bounce removes over this geometry* — which is exactly *configured down, difference reported* stated for the one rung where the difference is the subject
- [ ] **Every tolerance above is published with the instrument floor beside it**, and none of them is within an order of magnitude of point-release noise — that is what makes the version relaxation safe rather than convenient

**The ten scenes, one new thing per rung.** *Owner's ruling, 2026-08-12: about ten glTF scenes of
ascending complexity, tests under `test/render/`, references rendered in Blender, our pipeline developed
to match. The value of ascending complexity is that a red names its own step, so a rung that adds two
things has thrown that value away. Every `.glb` is emitted by a script in this tree and never authored
(principle 2). The camera is set on the Blender side from the glTF `yfov` by
`sensor_fit='VERTICAL'`, `lens = sensor_height / (2·tan(yfov/2))`, which reproduces `yfov` exactly by
`blender/camera.cpp:674` — never by the importer.*

| # | Scene | The one thing it adds | Judgeable on | First needs |
|---|---|---|---|---|
| 1 | one triangle, ~30 % of frame, declared camera | the reader, the projection, the raster convention | coverage | reader · studio scenario |
| 2 | one quad, rotated off both axes | depth that varies across the frame | coverage · depth | — |
| 3 | the ±1 m cube, 12 triangles, flat normals | indices, winding, back-face culling, a silhouette from six planes | coverage · depth | — |
| 4 | UV sphere, 32×16, normals declared smooth | a curved silhouette — every boundary pixel now has its own sub-pixel edge offset | coverage · depth | — |
| 5 | scene 3 lit: one `SUN` at declared irradiance + factory world | the first radiance number | + direct radiance | **linear tap** · declared studio light |
| 6 | scene 4 lit by the same | the whole `cos θ` sweep from 0° to 90° in one image | + direct radiance | — |
| 7 | three spheres: two `baseColorFactor`s and one `emissiveFactor` | albedo linearity and the emissive channel | + direct radiance | material factors through glTF |
| 8 | scene 3 under Blender's **factory point light**, 1000 W at its factory position | inverse-square falloff — *the literal default lighting* | + direct radiance | **a punctual light in `render/`** (§ II.8) |
| 9 | cube on a ground plane, sun at 30° elevation | a cast shadow, and the first non-zero interreflection | + shadow and indirect, as a bias curve | shadow under a declared studio light |
| 10 | scene 9 plus a generated checker `baseColorTexture` at 1 texel per pixel | texture sampling and the base-colour sRGB decode | coverage · direct radiance | glTF texture path |

- [ ] Scene 1's triangle declared so the mask is not decided by a tie: no edge axis-aligned, no edge through a pixel centre at the declared camera, and vertices exactly representable in float32. A deliberate tie is scene 1's **second** fixture, run as a declared expected-difference rather than as a pass
- [ ] **The ~30 % holds, the roll is what buys it, and the roll's *sign* is the whole of it — `+22.5°`, derived and not set.** *A round measured a unit right-isoceles triangle at **28.1 %** of a 16:9 frame and concluded the 30 % was unreachable; the ceiling it found is real only at **zero** roll, where the legs are axis-aligned, which is the one orientation scene 1's no-tie rule forbids.* The arithmetic, over the usable frame after an 8 px margin (1264 × 704 of 1280 × 720): the unit triangle `{(0,0),(1,0),(0,1)}` rotated by `θ` in a y-up raster has bounding box `w(θ) × h(θ)`, fits at `k = min(1264/w, 704/h)` px per unit, and covers `0.5·k²/921 600`.

| roll | bbox, units | fits at | frame fraction |
|---|---|---|---|
| 0° | 1.000 × 1.000 | 704 px | 26.9 % |
| **−12°** | 0.978 × 1.186 | 593.6 px | **19.1 %** |
| +12° | 1.186 × 0.978 | 719.7 px | 28.1 % |
| **−22.5°** | 0.924 × 1.307 | 538.8 px | **15.8 %** |
| **+22.5°** | 1.307 × 0.924 | 762.0 px | **31.5 %** |
| +45° | 1.414 × 0.707 | 893.8 px | 43.3 % |

- [ ] **Two rolls of equal magnitude and opposite sign are indistinguishable on the no-tie criterion and differ by up to 1.65× in area**, because the frame is 16:9 and the shape is not: the edges lie at `θ`, `45°+θ`, `90°+θ`, so `−θ` and `+θ` put the same three angles the same distance from the raster axes, while only one of them lays the long bounding extent along the long frame axis. **The sign is therefore a derivation with a stated criterion — *the wider bounding extent goes across the wider frame axis* — and never a `[SET]` number.** *A camera that took the other branch lost a third of the subject with nothing to notice it (`doc/bugs.md`)*
- [ ] **`+22.5°` is where both constraints peak together, so nothing trades.** The minimum edge-to-axis distance is maximised at exactly `θ = 22.5°`, where all three edges sit 22.5° off — against 12°, 33° and 12° at the roll first written down — and that same roll is the smallest one clearing 30 %. Distance follows from the binding axis: half-height `0.46194 m` at `tan(yfov/2) = 0.24` exactly, projected to 352 of 360 px, gives **`d = 1.968493 m`** and **`1.312329e-3 m/px`**, with the half-width then 497.8 px inside the 632 available
- [ ] **The subject's frame fraction is a declared, computed, refused-on-mismatch property of every geometric rung, not a note beside the camera.** It is what the § below's `IoU ≈ 1 − P·δ/A` is applied under and what § I.26's *≈ 30 pixels at risk* is counted over, so a camera that silently frames the subject smaller tightens both without saying so. Right: the manifest declares it `derived` with its derivation, the runner recomputes it from the projected geometry, and a deviation beyond a stated tolerance is a **refusal naming both numbers**. *Written as a requirement because a declared camera got the frame fraction wrong by 1.65× and nothing in the tree could notice (`doc/bugs.md`)*
- [ ] Scenes 1–4 are judgeable on coverage and depth alone and are **valid geometry tests long before any light model agrees** — that is the whole reason the ladder is ordered this way, and it means the reader, the projection and the raster convention are settled before the first radiance number is asked for
- [ ] Scene 4 is where the boundary-distance distribution earns its place: three straight edges (scene 1) cannot separate a focal-length error from a principal-point error, and a circle can
- [ ] Scene 6 is the rung that **measures our ambient's shape** rather than assuming it: under a full uniform sphere environment every facet of a free-floating sphere should return `ρ·L` regardless of orientation, and `render/stages/SurfaceLight.h:89` weights the sky by `(1 + n·up)/2`, which is right for a dome over dark ground and wrong for a sphere. The residual is expected to be a clean `(1 + n·up)/2` in the elevation direction, and finding exactly that shape is the rung passing, not failing — what it produces is the size of a declared model difference
- [ ] Scene 7's emissive rung is the first consumer of `Material`'s emissive field, which § II.8 records as declared and unreached — *"`Material` has the field and `SurfaceState::Emits()` derives from it; nothing emits"*
- [ ] Scene 8's acceptance is the falloff **exponent** and not only the level: fitting `E(d) ∝ d^-n` over the cube's six faces separates a wrong constant from a wrong law, and a wrong law is the failure a single-distance test cannot see
- [ ] Scene 10 is valid **only at 1 texel per pixel**, declared as a scene constraint and not as a caveat — away from 1:1 Cycles uses its own mip and filter policy and we use ours, and the comparison would measure the choice rather than the implementation
- [ ] Each scene is one directory under `test/render/<feature>/<case>/` whose single tracked file is its `manifest.json` (§ I.26.10) — the scene, the recipe and the acceptance in one declaration, with the Blender side driven from it rather than from a per-scene script, and the requirement identifiers it covers named in `covers` (§ I.20). *Restated 2026-08-12: "its `.glb`, its Blender script and its expected values" described three tracked artefacts per scene, and a per-scene script is a place a scene can differ from every other scene in a way nothing compares*
- [ ] The ladder's own acceptance: **rung `n` is not run until rung `n−1` is green**, which is what makes a red name its own step. A harness that runs all ten and reports eight reds has produced one finding, not eight

- [x] glTF 2.0 reader, **both containers decided by the bytes and never by the file name** — `.glb` with its `BIN` chunk, `.gltf` with its buffer beside it (`src/gltf/Document.cpp`, `src/gltf/Types.cpp`; held by `test/unit/gltf/AGlbCarriesWhatItDeclares` and `test/unit/gltf/TheTriangleProjectsToTheOraclesArea`). A `.glb` named `.gltf` is still a `.glb`, and a reader that trusted the extension would report a JSON parse failure at byte 0 for a file that is fine. An unknown GLB chunk is skipped, which is what the format asks so a future chunk does not make a readable file unreadable
- [x] **All six component types with the format's own signed asymmetry** — `−128` and `−127` both normalise to `−1`, because a signed component divides by its positive maximum and clamps (`src/gltf/Document.cpp` `Normalise`, asserted to 1e-12 in `test/unit/gltf/AGlbCarriesWhatItDeclares`) — and `byteStride` honoured, proven with `0xCDCDCDCD` **live in the padding**: a reader stepping 12 bytes instead of 16 reads the padding as a float, which is a visibly wrong answer instead of a wrong answer that happens to be zero. u8, u16 and u32 indices all decode to the same six values; `min`/`max` cross and are asserted to reach the decoded extremes exactly. *Five of the six component types are asserted by a fixture; `Int16` (5122) is implemented and carried by none, which is the cheapest gap in this section to close*
- [x] **An attribute is a name, not a slot** (`src/gltf/Types.h`, `src/gltf/Types.cpp` `Primitive::Find` and `MissingSemantics`; held by `test/unit/gltf/TheTriangleProjectsToTheOraclesArea`, which reaches its subject's positions through `Find("POSITION")` and gets `NORMAL` back as the name that is missing): the reader answers *what is in the file* and never *what shape does it become*, so `TANGENT`, `TEXCOORD_1`, `COLOR_0` and `JOINTS_0` cross with no line each and the vertex-layout question (§ I.26.6) stays open where it belongs. *This is the shape the four narrowings below were superseded in favour of, and it is the reason the reader is wider than this section first asked for.*
- [x] **All seven primitive modes cross, and refusing them at the reader was the wrong boundary** (`src/gltf/Types.h` `PrimitiveMode`, held by `test/unit/gltf/AGlbCarriesWhatItDeclares`). *Superseded 2026-08-12: this line read `TRIANGLES` only, which is right about the picture and wrong about the layer* — a reader that refuses `mode: 3` cannot report what a file contains, and `MeshPrimitiveModes` becomes unreadable rather than readable-and-declined. **The refusal moves down to the consumer**, one line below, and § I.26.6's `POINTS`/`LINES`/`STRIP`/`FAN` **REFUSED** is unchanged: we still draw none of them
- [ ] **The mesh consumer refuses a non-triangle primitive by name, and nothing else in the engine may read `PrimitiveMode`.** It is the same division as the `POSITION`/`NORMAL` line below: the reader records, the consumer refuses. Nothing enforces it today because nothing consumes the reader at all — which is the next line and not a second defect
- [ ] **Nothing consumes the reader yet, and the bridge is its own feature.** `Document::ReadElements` hands out `Count × components` doubles per accessor and no file in `src/` builds a `core/ChunkVtx.h` run from one, so rung 1 cannot run however green the reader is. **Every permissiveness the reader earned above is paid for at that bridge, by name**: a non-triangle mode · a primitive with no `NORMAL` · an index ≥ the vertex count · a component count that is not the layout's · a `matrix` that does not invert. *Written as its own line because the reader's own header states the division — "nothing here answers what shape does it become" — and a division stated on only one side is half a design*
- [x] **Sparse accessors, both arms** (`src/gltf/Document.cpp` `ApplySparse`; held by `test/unit/gltf/ASparseAccessorResolves`): over a base `bufferView`, and with **no `bufferView` at all**, where the accessor reads as zeros and the overrides are written into them. The second arm is the one a reader gets wrong, because *no bufferView* looks like *nothing to read* until a morph target comes out empty. *Supersedes this section's original **sparse accessors refused** and § I.26.6's `REFUSED` row alike — the ruling is under that table*
- [x] **Node `matrix` and the TRS triple, and a file that carries both is refused** (`src/gltf/Types.h` `Node`, `src/gltf/Transform.cpp` `FromTrs`/`FromColumnMajor`, `src/gltf/Document.cpp` `WorldTransform`; held by `test/unit/gltf/AMatrixNodeAndItsTrsAgree`). Composition is `T * R * S` in the format's order, the hierarchy is walked to the scene root, a node that reaches itself is refused, and a node that is the child of two nodes is refused because a glTF hierarchy is a forest. **A matrix node and its TRS twin place the same vertex 1.78e-15 m apart**, against an expected world matrix written in whole integers so the test does not check the reader against itself
- [x] **`scenes` and the default scene cross as the file states them** (`src/gltf/Document.cpp`; held by `test/unit/gltf/AGlbCarriesWhatItDeclares`, which asserts the default scene is the one the file names). *Two refusals on this arm are implemented and held by no claim — a `scene` index the file does not carry, and a scene naming a root node it does not carry — so they belong beside `Int16` on the list of what the next fixture should carry. And § I.26.6's **multiple scenes REFUSED** is a statement about what we render, not about what the reader may report — the same division as the primitive modes above.*
- [x] **Every refusal is a sentence that names the file and what was missing, and there is no partial document** (`src/gltf/Document.cpp` `Refuse`, `Document::Error()`; held by `test/unit/gltf/AFileThatCannotMeanAnythingIsRefusedByName`, **13 subjects each asserting the wording of its own refusal**). Asserting only that a file was refused would pass a reader that refused everything for one reason, which is the vacuous shape this repository keeps finding. A file that read nine of its ten accessors would hand its caller a subject that is silently not the declared one, so a refused read leaves `out` **empty** rather than a run of zeros a caller could mistake for a mesh at the origin. *The class-level claim is nevertheless false in one place — see `doc/bugs.md`, `extensionsRequired`*
- [ ] `POSITION` **and** `NORMAL` required, never derived — our vertex layout has no spelling for a mesh without a normal, and generating one would put our smoothing decision inside a comparison whose subject is somebody else's geometry. An oracle comparison must contain no repair. *The **mechanism** is built and the **requirement** is not: `Gltf::MissingSemantics(primitive, {"POSITION", "NORMAL"})` (`src/gltf/Types.cpp`) returns the names a primitive lacks and derives nothing, and `test/unit/gltf/TheTriangleProjectsToTheOraclesArea` asks it of Khronos's `Triangle` and gets back exactly `NORMAL`. Nothing calls it outside that test, so a document with no normal is still read happily — the enforcement is the consumer's and the consumer does not exist*
- [ ] **The rule stands unrelaxed for the coverage rung, and the question was asked properly rather than assumed.** *Khronos's `Triangle` carries `POSITION` alone, and the case for relaxing is real: coverage is decided from Cycles' alpha on the oracle side and from `depth ≠ far` on ours, so **the comparison needs no normal at either end**.* It is refused anyway, on the cost of the two ways to accept it: **deriving** one is the repair this line forbids, and it puts a smoothing decision inside the rung whose whole subject is the reader · **admitting** a normal-less vertex means a second vertex layout in the engine, bought for one fetched asset, at the rung whose job is *the reader, the projection, the raster convention* — a shape change to `core/ChunkVtx.h` earned by a test fixture. **The third way costs nothing and is already required**: rung 1's subject is generated here (§ I.26.5 row 1), so it carries a normal because we write one
- [ ] **The synthetic floor's generator emits `POSITION` and `NORMAL` for every fixture it writes**, which makes the reader's requirement and the fixture supply consistent by construction — triangle, quad, cube, sphere, sponge and Julia set all come out readable, and a generated fixture that omitted the normal would be refused by our own reader before any oracle ran
- [ ] **Khronos's `Triangle` is therefore the schema's worked example and never rung 1's subject**, and it is out on two counts at once: no `NORMAL`, and it is a *fetched* file where § I.26.5 row 1 requires a generated one so that a coverage failure at rung 1 has exactly one cause. *Recorded because the manifest under `render/coverage/triangle/` says so in its own notes and a later round would otherwise read the directory name as the ruling*
- [x] **Both glTF cameras, with the two conventions stated once and checked by where a point lands** (`src/gltf/Camera.h`, `src/gltf/Camera.cpp`; held by `test/unit/gltf/ACameraCrossesWithItsConventions`, 24 claims): perspective and orthographic · **`aspectRatio` absent means the viewport supplies it and `zfar` absent means an infinite frustum** — absent rather than substituted, so no large number stands in for infinity and the infinite arm is a different matrix · near at NDC z = −1 and far at +1 · the frustum's top edge at NDC y = +1 and therefore at raster row −0.5, the integer coordinate being the pixel centre · the view transform as the **inverse** of the camera node's world transform, with a point 1 m ahead landing at view z = −1. *Asserting the projection's entries against the algebra that produced them would prove the typing was careful and nothing else; each of these is a sign a reader can get wrong on its own*
- [ ] `cameras.perspective` with `yfov`, `znear`, `aspectRatio` — and what our reversed-Z infinite projection does with `zfar` stated in the same header rather than silently ignored. *Half built 2026-08-12 and the half that is owed is the deciding one. The **file's** side is done and ticked on the line above; the second clause is an **engine-side** statement — `src/gltf/Camera.h` says only that glTF's `[-1, +1]` is not `core/Mat4.h`'s reversed-Z and that converting is the renderer's business, which is the right division and not the answer. Nothing converts one to the other, because nothing consumes the reader*
- [ ] `pbrMetallicRoughness` factors, `emissiveFactor`, `doubleSided`, `alphaMode`; `baseColorTexture` as PNG only at first, because a second image decoder buys no comparison
- [ ] **`KHR_lights_punctual` refused as the light channel** — REFUSED, and this is the load-bearing decision of the section. Blender's glTF importer converts light intensity through a lumens-per-watt factor that is **683** and is under an open, Khronos-PBR-TSG-endorsed proposal to become **177** (glTF-Blender-IO issue #2554, open at the time of writing): a factor of 3.86 sitting inside the oracle's importer, in exactly the quantity rung 3 exists to measure. The light is declared beside the glTF in W/m² and applied by script on both sides, so nothing about it crosses the glTF boundary. **The reason above was read in the source afterwards and is narrower than it was written**: the factor is applied only in mode `SPEC`, which is merely the default — see the `RAW` line above, which is why this refusal is a simplification and not a necessity
- [ ] Skinning, morph targets and `animations` out of scope — we carry our own animation shape already (`clients/Animation.h`, glTF's two-table form with two declared deviations)
- [ ] **Rung 1, coverage**: both sides to a binary mask, compared by IoU **and** by the boundary-distance distribution (for each boundary pixel, distance to the nearest boundary pixel of the other mask; p50/p95/p99 in pixels). IoU alone cannot see a half-pixel camera offset on a large subject; the distribution can, and it localises it
- [ ] Rung 1's failure signatures declared with the metric, which is what makes a difference **attributable**: a constant offset is the camera origin or principal point · a radial trend is the focal length or a projection convention · a uniform scale is the `yfov`/aspect interpretation · a shear is a row/column order or a handedness. Four distinguishable shapes, one metric
- [ ] Rung 1 acceptance: **boundary p95 ≤ 0.1 px** at 1280×720, both sides at one sample per pixel with the narrowest reconstruction filter. *Tightened 2026-08-12 from the 0.5 px this line first carried — right instrument, 5× too loose for a subject with no sub-pixel geometry, where 0.5 px is the size of the convention error the rung exists to catch. IoU is reported beside it and enforces nothing (§ below).*
- [ ] **Rung 1 becomes the acceptance criterion for the SDL_GPU port** (§ I.19), which today has none: the same glTF through the old backend and the new one must give the same mask. It is the cheapest port gate that exists and it is a picture claim rather than a counter claim
- [ ] **Rung 2, depth**: linear view-space range both sides, compared inside the intersection mask, p99 ≤ 1e-4 relative. A bias attributes to the near plane or the reversed-Z convention; growth with distance is the float32 floor and is published as the instrument's own floor beside the result
- [ ] **A scene-referred linear float readback ahead of `ExposureStage`** — the one thing rung 3 needs that does not exist. `render/Renderer.h:59` `ReadPixels` is *"already sRGB-encoded"* and there is no other colour tap, so the physics of this renderer is currently unreadable by anything, including us (`doc/bugs.md`)
- [ ] **The linear tap priced, because it is the whole blocker between rung 2 and rung 3 and it is small.** It is a *second reader of a texture that already exists* — the offscreen HDR scene target (`render/Gpu.h:13 HdrFormat`, RGBA16F) — not a new pass, not a new format and not a new pipeline state. Cost: one copy-to-buffer of **W·H·8 B = 7.37 MB at 1280×720** on the frames a test asks for, one staging buffer, and one method shaped exactly like `ReadDepth` — poll, `ReadState`, no waiting variant. **Zero cost on a frame nobody asks**, so it does not touch the 720p60 floor and needs no quality tier. It lands as its own step **between § I.25's studio scenario and the glTF work**, and it is the cheapest of rung 3's three prerequisites
- [ ] What the tap makes decidable the day it exists, before any Blender comparison runs: whether `render/stages/SceneScale.h:17 kSceneExposure = 11.0` is an exposure or a physics scale. Its own stated derivation terminates in *"the value whose sRGB output is 0.70"* — a display code on the path of every physical quantity (`doc/bugs.md`) — and no reader in this tree can currently see the quantity it scales
- [ ] The tap is `device`-tier and native, so it costs no browser gate and no second toolchain (§ I.20)
- [ ] **Rung 3, direct diffuse**: one directional light at declared irradiance in W/m² perpendicular to the beam · Blender's `Diffuse BSDF` at roughness 0, which is **exactly Lambertian** by the Blender manual, and explicitly **not** the Principled BSDF, which at metallic 0 still carries a specular lobe at IOR 1.5 (F0 = 0.04) and whose diffuse lobe becomes energy-preserving Oren-Nayar above diffuse-roughness 0 · constant albedo, no texture, no sky, no indirect · both sides written as linear OpenEXR
- [ ] Rung 3 acceptance: median relative difference ≤ 1 % against the closed form `ρ·E·cos θ / π` on unshadowed facets, with the **sign** of the residual reported, and Blender's own residual against the same closed form published beside ours — the oracle states its error before it judges ours
- [ ] Rung 3's residual shape read as an attribution: a constant factor says the scene scale is doing physics' work · a `cos θ` or `sin(elevation)` dependence says the irradiance convention — perpendicular-to-beam against on-the-horizontal, the commonest single error of its class, and `IrradianceStage` already names both (`sunDirectNormalY`, `skyDiffuseHorizY`) · a per-channel difference says the three channels are scaled apart somewhere
- [ ] What rung 3 settles, stated before it is run: whether `render/stages/SceneScale.h:17 kSceneExposure = 11.0` is an **exposure** — legitimate, and belonging in the exposure stage — or is doing physics' work, which nothing in this tree can currently decide because its own derivation anchors it on *"the value whose sRGB output is 0.70"*
- [ ] **Rung 4, shadow and indirect**: reported as a **bias curve and never a pass/fail**, because a raster engine and a path tracer disagree there by construction. The product is a number of the form *our screen-space occlusion removes 0.6× of what one Cycles bounce removes over this geometry* — actionable, where a red is not
- [ ] The pinned set, both sides, published with every comparison: linear Rec.709/sRGB primaries · OpenEXR float32 output, which **ignores the view transform** by Blender's own colour-management rule and thereby deletes AgX, Filmic and our ACES fit in one move · Blender `Standard` view transform for any PNG a human looks at, since AgX is the 4.0+ default and is a heavy S-curve · `film.exposure = 1.0` · 1280×720 at 1 spp with the narrowest filter · fixed Cycles seed, declared sample count, **denoising off** · declared bounce count per rung · camera set by script from the glTF `yfov`, never by the importer · `use_auto_smooth` off so Blender does not re-derive a second geometry · `scale_length = 1.0` · **the Blender version recorded on the row**, because the oracle's version is part of the measurement
- [ ] Our own sky exported as an equirectangular EXR and set as Blender's world, which makes the environment identical by construction and isolates the surface response — the only way an atmosphere comparison measures an implementation rather than which model each side chose (Blender's Nishita sky is a different model with different aerosol parameters)
- [ ] What a Blender comparison **cannot** judge, declared in the same header so no round reports an unactionable red: anti-aliasing and reconstruction (Cycles integrates the pixel footprint, we resolve jittered samples — different everywhere at a silhouette) · texture filtering away from 1 texel per pixel · indirect light, which we do not have by design and where the comparison measures the size of a known absence · our TAA, impostors, LOD selection and grass field, which have no counterpart · anything about display beyond the working space
- [ ] **That list re-decided against the source, because three of its seven entries dissolve and the rest get sharper.** *Anti-aliasing* — **dissolved for coverage**, `BOX` at 0.01 px makes it the same predicate, floor 0.005 px; **survives for the shaded image**, where our TAA resolves eight jittered frames and the rung is therefore run with TAA off. *Indirect* — **dissolved as a confound**, `diffuse_bounces = 0`; **retained as the subject** at rung 9. *Display* — **dissolved**, EXR float32 ignores the view transform and our side reads the linear tap, so AgX never enters. *Texture filtering* — **survives, narrowed to a scene constraint**: valid at 1 texel per pixel, undefined away from it. *Our atmosphere* — **survives**, and gains an exact substitute: Blender's factory world is a *uniform* environment of a known radiance, which is closed-form on both sides, so an ambient comparison is possible even though a sky-model comparison is not. *TAA, impostors, LOD, grass* — **survive**, no counterpart, off for every rung. The one entry the list was missing is ours and not Blender's: **our ambient is a hemisphere over geodetic up with two bounce constants**, so it cannot match a full-sphere environment except where `n·up = 1` — measured at rung 6 rather than assumed
- [ ] The comparison is a **test in the suite** (§ I.20), tiered `device`, with the Blender binary taken from the environment and **refused if absent** rather than skipped — a silent skip is the defect class this repository keeps finding
- [ ] The reference `.glb` files are generated by a script in this tree, never authored — principle 2 applies to a test fixture exactly as it applies to a texture, and a hand-modelled cube is a file nothing can recompute
- [ ] The first comparison to run is **rung 1**, because it is *decidable* in this file's own sense — no light model has to agree for it to mean something — and because every later rung is confounded by its failure. It costs the reader and one mask difference
- [ ] The first comparison that **settles** something is rung 3, and what it settles is whether this engine's light transport is right at all: the first correctness-class number in the archive, against 100 % consistency-class ones today

**The ten scenes above are ten fixtures, not the ladder.** *Superseded 2026-08-12 by the twenty-one-rung
ladder below, on the owner's ruling that the ladder is built from concrete assets and ordered easy to
hard. Nothing above is withdrawn: every one of the ten is still a fixture we generate, and every bullet
above that says "scene N" means **fixture N**, which the ladder's own table maps to its rung. The four
rungs of the comparison — coverage, depth, direct radiance, shadow and indirect — are unchanged and are
what a rung is judged **on**; the ladder is what is judged, in order.*

### I.26.1 The corpus, and why it is a feature inventory before it is a set of scenes

*Owner's ruling, 2026-08-12, in four parts: **the corpus is built by a Python script from online
sources** rather than committed · **pin the strongest thing each source offers** — a commit SHA where
there is a repository, a content hash where there is only a file · **Outshine must implement almost
everything the glTF samples provide to be a full-featured game-engine renderer** · and the decision rule
per feature is **does Kingdom Come: Deliverance or GTA 5 use it**. The first three are mechanism. The
fourth is what makes a percentage mean anything, because a score dragged down by
`KHR_materials_dispersion` was never telling anyone anything.*

**Principle 2 is not in tension with this, and the reading is now written down.** Principle 2 forbids
**authored appearance in our content** — a file somebody painted that nothing can recompute. It does not
forbid the engine from **sampling a texture a glTF supplies**. Implementing the feature is required;
shipping painted assets in the world is not. The samples are how a feature is **proven**, never what the
world is **made of**, and the two never meet: nothing under `test/corpus/` is reachable from a scenario
that builds a world. *Stated because it was about to become a requirement either way, and because the
opposite reading would forbid the only external check this repository has.*

- [ ] The corpus is **fetched and built, never committed** — `test/corpus/prepare.py`, one script, the door `CLAUDE.md` leaves open for a script that prepares data offline. The recipe is in git, the bytes are not, and *can this be recomputed from something we own* is answered by the script. Three jobs, each independently invocable and each idempotent — **fetch** a subject at its pin · **convert** a `.blend` to glTF through Blender · **render** the oracle — plus `all` and a `dry-run` that prints what a cold run costs and touches nothing. It compares, scores and decides **nothing**; that is C++, in the test
- [ ] **The corpus has no directory of its own and the products land in the case directory** — `test/render/<feature>/<case>/`, beside the declaration that named them (§ I.26.10). *Superseded 2026-08-12: the earlier form put fetched bytes under `test/corpus/` and the comparison somewhere else, which is the split § I.26.10's own last line calls "a comparison whose halves live in different places".* `test/corpus/` holds the **preparer**, which is source and is tracked. Nothing fetched or rendered is engine data, so nothing reaches `src/assets/` (§ I.24's tier rule: `assets/` ships with the library)
- [ ] **One manifest per case, not one for the corpus** — `test/render/<feature>/<case>/manifest.json`, the only tracked file in the directory, and the preparer reads only it. *Superseded 2026-08-12 on the owner's declarative ruling: a test is a directory, the glTF is the declaration, the directory is self-describing. A single `test/corpus/manifest.json` made a case's subject, its licence and its comparison three files apart, and it made the corpus a second global registry that every case had to be kept in step with.* Per subject: source kind, pin, per-file licence with its SPDX identifier, the name each file lands under and its role. A subject not in a manifest is not fetched, and a file in a manifest with no licence field is a **refusal at parse**, not a warning
- [ ] **An unknown key is a refusal, not a shrug** — a key nobody reads is a setting that silently did not apply, and every refusal names the subject, what was expected and what was observed. This is what makes the merged manifest safe to grow: a field added to the schema and not to the reader cannot pass unnoticed
- [ ] **Three source kinds, because the strongest available pin differs per host** — `khronos-sample-assets`, `blender-download`, `blender-studio`, stated in the manifest and never assumed. *Superseded 2026-08-12: the earlier form was `repo` fetched by **sparse checkout** at a commit SHA, and `file` pinned by URL plus SHA-256. **Pinned raw URLs with a per-file SHA-256 are strictly stronger and need no git at all** — a sparse checkout pins the tree and then trusts whatever the working copy holds afterwards, where a per-file hash is verified on every byte that enters, is the store's own key, and makes the content store verify itself.* For the Khronos kind the commit SHA sits **inside** the URL and is cross-checked against the manifest's declared pin, so the file carries two independent pins and a URL naming a different commit is a refusal
- [ ] **A pin is not an allow-list and the two are separate mechanisms.** The pin says *these bytes*; the allow-list says *this host is a source at all* — it has an index that can be walked, a licence convention that can be read per file, and no account. Today it is `raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets` at a pinned commit, **`download.blender.org/demo/` entire**, and `studio.blender.org`; a redirect off the list is refused too. **`/demo/` entire and not an enumeration of its subdirectories** — an enumeration of `{cycles, eevee, bbb, asset-bundles}` was already stale against two requirements in this same section the day it was written: `demo/test/pabellon_barcelona_v1.scene_.zip` is rungs 19 and 21's scene and `demo/sprite_fright_030_0020_A.zip` is the forest rung's motion arm (§ I.26.7), and neither is under those four. Widening loses nothing, because **nothing under `/demo/` is refused by path** — `lone-monk`, `mr_elephant`, `tree_creature` and `loft.blend` are refused by the named-subject table in `licence.py`, which is the mechanism that reads licences and the only one that can
- [ ] **`download.blender.org`'s open-movie production trees are not under `/demo/` and that was checked rather than assumed**: `/peach/`, `/durian/`, `/mango/` and `/institute/` all answer *"Please visit www.blender.org for finding links…"* and `/demo/movies/` holds rendered video only. What *is* in the open, at the `/demo/` root, is a complete Sprite Fright shot and Big Buck Bunny's whole production tree (§ I.26.4)
- [ ] **The Blender side's licences come from a `README.txt` beside the file where one exists, and from the publishing page otherwise — per file, never per host.** *This is where the first pass went wrong by generalising, and the per-file readings are worth more than the web cards: `/demo/eevee/*/README.txt` states a licence the demo page does not.* Verified, one fetch each:

| Asset | Licence, at the source | Verdict |
|---|---|---|
| `demo/eevee/archiviz/` | `README.txt`: **CC0 1.0 Universal** — Marek Moravec, bed by Lech Sokolowsk | in |
| `demo/eevee/ember_forest/forest.blend` (74.3 MB) | `README.txt`: **Creative Commons Attribution** — Mike Pan | **in — the forest rung** (§ I.26.7). *Struck from the ladder mid-round on a foliage-exclusion rule and restored when the owner overruled it: a forest is the hardest workload this renderer will meet, and excluding it protected nothing* |
| `demo/eevee/race_spaceship/` | `README.txt`: **CC-BY** — Alessandro Chiffi / ONdata | in |
| `demo/eevee/wasp_bot/` | `README.txt`: **CC 4.0 Attribution** — Emiliano Colantoni | in |
| `demo/cycles/monster_under_the_bed_…blend` (78.1 MB) | page: **CC-BY** — Metin Seven, concept Blake Stevenson | in · **geometry only**, it is an SSS demo and subsurface has no glTF spelling |
| `demo/test/pabellon_barcelona_v1.scene_.zip` (24.7 MB) | page: **CC-BY** — eMirage | in |
| `demo/test/classroom.zip` (70.3 MB) | page: **CC0** — Christophe Seux | in |
| `demo/cycles/flat-archiviz.blend` (368 MB) | page: **CC-BY** — Flavio Della Tommasa | in |
| `demo/eevee/temple/`, `tiger/`, `wanderer/` | `README.txt`: **CC-BY-SA** | in · share-alike attaches to a published comparison image, so **CC-BY and CC0 are preferred where a choice exists** |
| `demo/eevee/mr_elephant/` | `README.txt`: **CC-BY-ND**, *"only for sharing as a demo"* | **out** — no-derivatives, plus an added use restriction |
| `demo/cycles/lone-monk_…blend` (36.7 MB) | page names the author and **no licence**; no `README.txt` beside it | **out** — same rule that removed the Junk Shop |
| `demo/cycles/loft.blend` (561 MB) | on no page at all, no `README.txt` | **out** |
| `demo/eevee/tree_creature/` | `README.txt` is **404**; the page names no licence | **out** |

- [ ] **`lone-monk` is out and it is the most painful of the exclusions, which is why the rule holds.** It is the best-known Cycles demo on the host, it is exactly rung 18–19's shape, and the page that publishes it names Carlo Bergonzini / Monorender and stops there. *An asset whose licence we cannot state is out* — and the rule earns its keep precisely when it costs something. If a licence turns up in the file or from the author it comes straight back in
- [ ] A fetch whose bytes do not match the pin is a **refusal naming the asset, the expected pin and the observed one** — never a warning, never a retry that takes what it got. Without this, "the corpus" is whatever the internet held that day and every acceptance number under `test/render/` is irreproducible, which is the arrival-order defect one level further out
- [ ] **A pinned corpus does not silently improve either, and that is correct.** When Khronos fixes an asset we move the pin **deliberately, in its own commit**, and re-take every acceptance number that asset carries. The manifest records the pin's date and the reason it moved, so the first time a comparison shifts after a pin move nobody has to guess whether the renderer changed or the asset did
- [ ] **The licence field is derived, never transcribed.** Each Khronos model carries `Models/<Name>/metadata.json` with a `legal` array of SPDX identifiers at the pinned SHA; the script reads it and **refuses any asset whose set of SPDX identifiers is not a subset of the declared allow-list**. That makes the audit mechanical and makes a licence change at the upstream a refusal rather than a surprise. Measured at SHA `2bac6f8c…`: 148 models, `CC0-1.0` ×92, `CC-BY-4.0` ×64, and thirteen other identifiers of which nine are restrictive
- [ ] The allow-list is `CC0-1.0`, `CC-BY-4.0`, and the `LicenseRef-LegalMark-*` marks, which are Khronos's, Cesium's, UX3D's and DGG's statements that a **logo is non-copyrightable** and carry no obligation. Everything else is out by construction
- [ ] **`NOTICE.md` is generated by the script from the manifest**, listing every CC-BY asset with its author and year, because attribution is a licence condition and a condition nobody can find is a condition nobody meets
- [ ] **No authenticated URL is ever in the manifest** — if a source needs an account, it is not a source. This is what rules out the Blender open movies' production files (§ I.26.4)
- [ ] **With no network the corpus is absent, and absence is a named refusal in the test that needs it** — never a silent skip, never a pass over nothing. The test declares the corpus assets it requires by manifest id; a missing one refuses with the id and the path it looked in, which reaches the harness's existing undeclared-skip machinery (§ I.20). *The mirror of § I.24's ruling that a scenario carrying its own data runs offline at full speed: once fetched, the corpus needs no network, and the same test gives the same answer with the wire cut*
- [ ] **The `.gitignore` is one tracked file at `test/render/`, and it states the invariant rather than an enumeration of products.** Four lines — `*` · `!*/` · `!.gitignore` · `!manifest.json` — which tracks exactly the manifests at any depth and nothing else, verified against git. *The preparer's first form wrote a per-directory `.gitignore` naming what that run produced, including itself. It is not wrong and it churns nothing in git, because it ignores itself — but it states a **consequence** and must be rewritten to stay true, and it cannot be stated once for the suite because a fetched file's name comes from the manifest's own `as` field and is arbitrary per case (`Triangle.bin`). The inverted form states `manifest.json` is the only tracked file, which **is** § I.26.10's rule, so it cannot go stale: a new product is ignored by construction and a new **tracked** file has to be un-ignored deliberately, which is the review moment worth having.* `!*/` is load-bearing — without it git does not descend into a case directory at all and the manifest is never seen
- [ ] **What the script costs cold, published in its own `dry-run`** so nobody discovers it after starting: the Khronos side of the ladder's assets is **≈ 145 MB** over 21 assets (`MetalRoughSpheres` 11.0 MB · `SciFiHelmet` 29.6 MB as `glTF/` + six PNGs, it has **no** `glTF-Binary/` · `ABeautifulGame` 42.0 MB as `.glb` · `NormalTangentTest` 1.8 MB · the rest under 600 KB each), against **1 397 MB for the whole repository**, which is why the assets are fetched file by file and the repository is never cloned. The Blender side is **one 24 MB archive** for the pavilion, and an archive is **read, never downloaded**: the end-of-central-directory record comes out of the last 64 KiB by HTTP range, the central directory follows, and a member is fetched by the byte range its local header points at, with the declared SHA-256 taken over the **member** rather than the archive. Measured on `download.blender.org/demo/bbb/blender.zip`: 830 709 844 B, 598 members listed in **0.34 s**, one 140 KB member extracted in **0.27 s** — which is what makes a 254 MB Sprite Fright shot and an 830 MB production tree usable sources at all. Blender conversion is one headless run per `.blend`

### I.26.2 The .blend to glTF conversion, and the six settings that are load-bearing

*The argument for exporting ourselves holds and is worth stating once: **Blender renders the reference
from the original `.blend`**, so Cycles never reads a glTF, the import ambiguity exists on one side only,
and every difference is ours. With a third-party `.glb` we would be comparing two importers as much as
two renderers. **Where it breaks is exactly where the export is lossy**, and the exporter's own defaults
say where.*

- [ ] The conversion is part of the corpus build — `blender --background <file>.blend --python export_gltf.py` — with **every export setting written out explicitly**, never left at its default, because three of the defaults are wrong for us and one is exclusive with another
- [ ] `export_yup=True` — glTF's convention, and it is the default (`io_scene_gltf2/__init__.py:673`). Our reader therefore reads +Y up and the axis swap happens once, in the exporter, where Khronos's own assets already carry it
- [ ] **`export_apply=True`, declared with its cost.** The default is `False` and its own description reads *"Apply modifiers (excluding Armatures) to mesh objects — WARNING: prevents exporting shape keys"* (`:679`). Without it a Subdivision, Array or Solidify modifier does not reach the glTF and the exported mesh is not the mesh Cycles rendered — the comparison would then measure the modifier stack. **With it, shape keys are lost**, so a scene that is a morph-target test must be exported a second time with `export_apply=False`. The two are exclusive and the manifest says which arm each asset takes
- [ ] `export_cameras=True` — the default is `False` (`:612`). For a *downloaded* scene the direction of § I.26's camera rule reverses: the `.blend` is authoritative, so the camera must cross the boundary and our side reads it. For a *fixture we generate* the existing rule stands — the camera is set on the Blender side from the glTF `yfov`, never by the importer
- [ ] `export_lights=False` — the default, and § I.26's refusal of `KHR_lights_punctual` as the light channel stands for every rung it already covers. **The consequence is the placement ruling**: a downloaded scene's lighting lives in its world, its area lights and its shader emission, none of which crosses, so **a downloaded scene is judgeable on coverage and depth and not on radiance** until its light rig is re-declared beside it. That is a property of the boundary, not a caveat about the asset
- [ ] `export_animation_mode='SCENE'` for the film rung — *"Export baked scene as a single animation"* (`:734`) — because with the default `'ACTIONS'` a scene becomes several animations and *the scene at time t* stops being one statement. `export_force_sampling=True` (the default, `:712`) and `export_frame_step=1` (`:704`), so every frame is a sample and nothing is inferred from a curve
- [ ] **`export_optimize_animation_size=False`**, against its `True` default (`:797`): the default *"removes duplicate keyframes"*, which leaves a non-uniform time grid. Off, the glTF's own input accessor **is** the frame grid, so a comparison time is a keyframe rather than an interpolation, and the time contract (§ I.26.3) is checkable by reading the file
- [ ] `export_image_format='AUTO'` and `export_texture_dir` set, so textures land beside the `.gltf` as PNG and no second image decoder is needed — the same restriction § I.26 already places on `baseColorTexture`
- [ ] **What has no glTF equivalent at all, named so no round reports it as a defect**, and this is what decides which `.blend` files are usable: **procedural shader node graphs** — the exporter handles a restricted Principled BSDF graph with image textures, and a Noise, Voronoi or ColorRamp chain becomes a flat factor with no error · **hair and curves**, which have no glTF primitive · **volumes** — smoke, fire, atmospheric · **particle systems** · **compositing**, which is not in the scene graph · **light rigs**, refused above · **Geometry Nodes**, which need `export_gn_mesh` (`:436`, *Experimental*, default `False`) even with `export_apply` on
- [ ] **A `.blend` that loses what makes it a test is out of the manifest, not exported and repaired.** Named refusals with their reason: the **Barbershop** benchmark (CC-BY, 280 MB) is a hair-and-particle scene and hair does not cross — exporting it produces a bald room · every **Geometry Nodes** demo, whose subject is the node graph · every **simulation** demo, whose subject is a cache. *An oracle comparison must contain no repair* — § I.26 already states this for normals, and it is the same rule

### I.26.3 Rung 6, the time contract — the cheapest thing that would otherwise poison every rung above it

*Owner's ruling, 2026-08-12: **motion enters as early as it honestly can, and rung 6's product is the
time contract, not the animation.** On a short film a timing disagreement and a rendering disagreement
are indistinguishable — both read as "frame 40 is wrong". On one cube with one TRS track they are not.*

- [ ] **Sample times are declared, never derived from a frame rate either side assumes.** glTF's animation sampler input is **seconds**; Blender's exporter writes `seconds = frame / (fps · fps_base)` (`blender/exp/animation/keyframes.py:13`, and the same expression at four more sampler sites). The declared run's `fps` must equal Blender's `fps · fps_base` and the manifest records both numbers beside the asset
- [ ] **Where time zero sits, stated — and Blender's default puts it in the wrong place.** The exporter divides the **absolute** Blender frame number by fps and subtracts no start frame, so a scene with the factory `frame_start = 1` writes its first keyframe at `t = 1/fps` — **0.0416667 s at 24 fps**, not 0. Our runner's first frame is `FrameAt_ = 0`, hence `t = 0`. The contract: **`frame_start = 0` is set on the Blender side before export**, or the offset is declared in the manifest and applied by the reader. An off-by-one here looks exactly like an animation that is slightly too fast, which is the hardest residual shape to attribute
- [ ] `InterpolationTest` is a **check inside rung 6**, not a rung beside it: it carries `LINEAR`, `STEP` **and** `CUBICSPLINE` in one image (measured at the pinned SHA — it is the only asset in the corpus that carries all three), so a mode mismatch appears as a **shape** rather than as a drift, and it costs nothing extra once time exists
- [ ] **The engine side of the contract already holds and this line records that it does, so nothing is rebuilt that works.** `clients/SceneRunner.cpp` advances an **integer** `FrameAt_` (`:347`), applies keyframes at `MotionApply((double)FrameAt_)` (`:335`) and derives seconds as `(double)FrameAt_ / m.Fps` (`:403`); `clients/Scene.h:58-59` states the rule — *"`Fps` only enters DERIVED seconds and the frame-budget verdict. It never enters the frame count, which is what keeps a run reproducible."* **Nothing accumulates `dt`**, so the drift that would surface only at the film's length does not exist today. What is missing is not the arithmetic, it is the **test that holds it**
- [ ] **The contract's own acceptance, decidable with no oracle at all**: for every `n` in a declared run, the engine's reported animation time for frame `n` equals `n / fps` **exactly** in double precision, and equals the glTF sampler input at the corresponding keyframe **exactly**. This is its own test in `test/clients/`, not only a property of a comparison — because if it fails, rung 6's picture comparison fails too, and only the separate test says which of the two it was
- [ ] `clients/Animation.h`'s declared deviation — *keyframes are frames, not seconds* — is the reason this contract needs writing down rather than the reason it is broken: the deviation is sound and it is exactly what removes the accumulation, but it means **the conversion to glTF's seconds happens in one place** and that place is named here
- [ ] Every motion rung above 6 inherits the contract and none of them re-establishes it. A motion rung that disagrees on time is a **red on rung 6**, reported as such

### I.26.4 Rung 21, the film — and the honest finding is that no open movie is fetchable

*Owner's ruling, 2026-08-12: **an animated `.blend` short film is the ultimate scenario, engine and
renderer test for non-interactive mode**, placed above the ten rungs rather than inside them. The shape
of the test is right and the reason is right: it is the first thing that makes **motion decidable**
against a reference. `CLAUDE.md` holds that popping, ghosting, a hitch on stream-in and a scatter that
ends at a radius are only judgeable in motion, and that has meant judged **by eye**, because there was
no reference for a moving image. A film gives frame n a reference for every n, and a pop stops being an
impression: it is a discontinuity in a difference curve that is otherwise smooth, on a **named frame**.*

- [ ] **The availability finding this section first carried was wrong, and the corrected one is that the licence was never the constraint — access is, and only for some of it.** *Corrected 2026-08-12. The first reading checked `/peach/`, `/durian/`, `/mango/`, `/institute/` and `/demo/movies/`, found redirects and rendered video, and concluded about the whole host. Walking the tree properly: **`download.blender.org/demo/` carries production files in the open**, including a complete Sprite Fright shot and Big Buck Bunny's entire `.blend` tree — see the two lines below. The method failure is recorded in `doc/bugs.md`, because it generalises.*
- [ ] **Blender Studio's licence is stated by the publisher and is not gated**: *"Unless notified otherwise, all digital content (webpages, video, artwork, 3D data) is available under the Creative Commons Attribution 4.0"* (`studio.blender.org/terms-and-conditions`), and *"Blender Studio content is generally released under Creative Commons Attribution License (CC-BY), with copyright © Blender Foundation"* (`studio.blender.org/remixing/`). What a membership buys is **access**, not permission — *"Members get access to all services … which includes previous Open Movies"*. So the gated production archives are **CC-BY 4.0 assets we cannot currently fetch**, which is a different fact from the one first recorded, and it is the owner's decision rather than a limit
- [ ] **`sprite_fright_030_0020_A.zip` is a complete open-movie shot in the open, and it is the film rung** — `download.blender.org/demo/sprite_fright_030_0020_A.zip`, 254 374 945 B, no account. Listed over HTTP range on its ZIP central directory without downloading it: **100 entries, 47 `.blend`**, three animation takes (`030_0020_A.anim.{A,B,C}.blend`, 185–210 MB each), `030_0020_A.lighting.blend`, and a linked library tree — `lib/char/sprite/sprite.blend`, `lib/char/spider/spider.blend`, `lib/sets/mushroom_grove/`, `lib/sets/sprite_village/`, `lib/env/plants.blend`, `lib/env/fungi.blend`. **Recorded because the availability claim it refutes was mine and wrong** — not because the shot is wanted: it is a vegetation-and-hair shot, and § I.26.4 rules those out of this ladder on the owner's decision. What it proves is that the *host* publishes production files in the open, which is a fact the next round should not have to rediscover
- [ ] **`bbb/blender.zip` is Big Buck Bunny's whole production tree** — 830 709 844 B, **598 entries, 591 `.blend`**, `blender/2d/` and `blender/3d/` with left/right stereo pairs, largest 55.9 MB. **And the intuition that old means cleaner is right in its premise and wrong in its conclusion**: it is a 2008 file, so it carries no Cycles node graphs at all — because it carries **Blender Internal** materials, a renderer removed in 2.8. Its appearance does not survive to Cycles *or* to glTF; what survives is geometry, armature animation and camera. It is therefore a **coverage, depth, skinning and motion** asset and explicitly **not** a shading or radiance one, and a reference render of it would be Cycles shading an auto-conversion nobody authored
- [ ] **The two archives' own `readme.txt` files are unread and that is a task, not a finding** — `030_0020_A/readme.txt` and `blender/README.txt` are each one entry in a 254 MB and an 830 MB archive, reachable by one ranged read of their local header. The licence basis recorded above is the publisher's stated terms, which is a licence stated at the source; the readmes may narrow it and must be read before either asset is fetched for real
- [ ] **The film rung is the declared path over Barcelona Pavilion with `Fox` in it, and it wins on the timing argument alone.** *Decided 2026-08-12 after the vegetation exclusion that first decided it was overruled — so the reason had to be re-earned and it is a narrow one. The Sprite Fright shot is **CC-BY 4.0, confirmed in the archive's own `readme.txt`** — "This collection of files is provided under the CC-BY license by the Blender Studio" — it is fetchable without an account, and its vegetation is now an argument for it. What still beats it: **`frame n` is unambiguous by construction** when the motion is ours. A third-party shot puts the film's timeline, Blender's frame numbering, the exporter's absolute-frame conversion (§ I.26.3) and our runner's frame index in one comparison, and a red anywhere in that chain reads as "frame 40 is wrong". At the summit — the one rung where everything is on at once — that is the one confound worth paying to remove*
- [ ] **The Sprite Fright shot is not discarded; it is the forest rung's motion arm** (§ I.26.7), where its vegetation is the point and where **no per-frame parity number is computed**, so the timing chain that disqualifies it as a parity summit does not matter. `lib/scripts/rigged_particle_hair.blend` remains a named export risk there, not a disqualification
- [ ] **Big Buck Bunny is out of the ladder, recorded so nobody re-proposes it**: 591 `.blend` of Blender Internal materials, a renderer removed in 2.8. Geometry, armature animation and camera survive; **appearance does not survive to Cycles or to glTF**, so the oracle would be shading an auto-conversion nobody authored. Its `blender/README.txt` returned empty on a ranged read and the publisher's CC-BY 4.0 terms are what stand for it
- [ ] **The whole Blender Studio catalogue is in scope and is enumerated rather than sampled** — 22 films and projects are listed at `studio.blender.org/films/` and `/projects/`: Elephants Dream · Big Buck Bunny · Sintel · Tears of Steel · Cosmos Laundromat · Caminandes 2 and 3 · Glass Half · The Daily Dweebs · Hero · Agent 327 · Spring · Coffee Run · Sprite Fright · Gold · DOGWALK · Storm · Impulse Purchase · Charge · Wing It! · Singularity · Overgrown. **All CC-BY 4.0 by the publisher's terms; most are behind a membership and a few are not.** Which is which is a fetch check per film, and it is a task rather than a finding — the two already found in the open (`demo/sprite_fright_030_0020_A.zip`, `demo/bbb/blender.zip`) were found by walking `download.blender.org/demo/`, and that walk has not been repeated for the other twenty
- [ ] **The reference is rendered once and stored, pinned like the corpus** — a directory of OpenEXR float32 frames with its own SHA-256 manifest, the Blender version and every Cycles setting on the row. Re-rendering per run would put a two-figure minute cost on every test invocation and would make the reference a moving target
- [ ] **What that costs, measured on this host rather than estimated.** Cycles 5.2.0 LTS, factory startup cube, 1280×720, 128 spp, adaptive sampling off, denoising off, `diffuse_bounces = 0`, seed 0, OpenEXR float32: **2.087 s/frame** on the Metal device (Apple A18 Pro, 5 GPU cores) and **11.6 s/frame** on the CPU device, both warm. **The first Metal frame after a cold kernel cache costs 200.9 s** — Cycles compiles its Metal kernels once — so a per-frame timing that does not warm the cache attributes 200 s of compilation to the scene. That is the **instrument's floor**, not the film's cost: it is a six-quad scene. A 10-second segment at 24 fps is **240 frames**, so the floor is **8.4 minutes** and a furnished interior will be a multiple of it that is *not yet measured* — one timed frame of the pavilion is the tool, and it costs one run
- [ ] **The segment is declared in frames, not in seconds**: `frames` and `fps` on the row, `frame_start = 0`, and the budget rule is `N · t_frame ≤ 2 h` with `t_frame` measured on the actual scene before the run is declared. A segment that does not fit is shortened, never sampled sparsely — a gap in the series destroys the discontinuity test, which is the half worth having
- [ ] **The acceptance is a per-frame difference series with a stated shape, and never "the frames match"** — they will not, and § I.26 already says a raster engine and a path tracer disagree by construction. Two products: a **bounded median** difference per frame inside the intersection mask, and — the valuable half — **no discontinuity in the series where the scene is continuous**. A pop is a jump in a curve whose neighbours are smooth, and the frame index localises it
- [ ] The discontinuity test's own statistic declared with it, so *jump* is a number: the **second difference** of the per-frame median over a window, against the distribution of the same statistic over the frames either side. A LOD switch, an impostor swap, a shadow-cascade boundary crossing and a tile becoming visible all produce it, and each has a different signature in *which* pixels moved — which is why the series is kept per-frame and not reduced to one number
- [ ] **What must exist before it can run at all: every prerequisite of rungs 1–20 plus animation**, and that is the point of a summit rather than a defect in it. Named: the reader · the studio scenario · the linear tap · a punctual light list · shadow under a declared light · texture sampling · alpha `MASK` and `BLEND` · skinning · the time contract of § I.26.3 · a `Keyframed` camera over a timeline clock, which § I.25 already carries. **If it goes red before the rungs below are green it names nothing**, so the ladder's own rule — rung n waits for n−1 — is what gates it, not a schedule
- [ ] **It is a declared run and not part of the suite's default tier**: `device`-tiered, named, and never triggered by an ordinary `make test`. A test nobody can afford to run gets skipped, and a skip is the defect class this repository keeps finding

### I.26.5 Not a ladder but a matrix with a dependency order, and the integration scenes sit at the joins

*Owner's ruling, 2026-08-12: **"21 rungs" is the wrong shape.** Once every feature KCD or GTA 5 uses
gets its own basic isolated case (§ I.26.8), the structure is a **feature matrix with a dependency
order**, not a line. The table below is the matrix's **spine** — the path a first implementation walks
— and it is no longer the whole thing.*

**The ladder grows wide before it grows tall.** Every basic case for a feature **precedes** the complex
scene that combines it, and the complex scenes are **integration** rather than difficulty: the forest
integrates instancing, alpha test, two-sided transmission, LOD and overdraw, each of which has its own
isolated case first. So the order is:

```
  basic cases          integration scene        what a red means
  ───────────────      ─────────────────        ────────────────
  A: alpha modes   ┐
  B: overdraw      ├──▶ forest (§ I.26.7)  ──▶  a combination, because every part is green
  B: LOD, A: inst. ┘
  A: metal-rough   ┐
  A: transmission  ├──▶ built world        ──▶  a combination
  B: cascades, SSR ┘
  A: skinning      ┐
  A: interpolation ├──▶ film (§ I.26.4)    ──▶  a combination
  spine rungs 1–20 ┘
```

- [ ] **An integration scene is never run before every basic case it combines is green** — the ladder's own rule, restated for the matrix. Rung *n* waiting for *n−1* was the linear form of it; the general form is *a join waits for its inputs*, and it is what keeps a red naming one thing
- [ ] **A basic case is owned by exactly one feature and a feature is owned by exactly one basic case.** Two cases for one feature means neither is the answer when they disagree; one case for two features is the bundling the one-new-thing rule exists to prevent — which is why `DamagedHelmet` was refused for carrying five
- [ ] **The spine below is the walk order for a first implementation, not the matrix.** It is kept because somebody has to start somewhere and because rungs 1–4 genuinely must come first — the reader, the projection and the raster convention are prerequisites of every case in both kinds

**The spine, twenty-one rungs, easy to hard**

*Owner's ruling, 2026-08-12, in two steps: **implement from easy to hard, motion last** — then refined,
**simple animations can be in between; difficulty decides the stages**. The ordering principle is
therefore **how many independent things can be wrong at once**, and each rung adds exactly one. Motion
enters at rung 6, as early as it honestly can, because a moving cube whose static form does not match
teaches nothing — and because motion adds a **time axis to every comparison**, so a residual under an
unsettled static rung is unattributable between a geometry error and a sampling-time disagreement. Every
animated rung sits above the static rung it depends on. The synthetic floor is **generated here**
(principle 2); everything else is fetched under § I.26.1's manifest.*

| # | Asset | Source · licence | The one thing it adds | Judgeable on | Fixture |
|---|---|---|---|---|---|
| 1 | triangle | **ours**, generated | the reader, the projection, the raster convention | coverage | 1 |
| 2 | quad, off both axes | **ours**, generated | depth varying across the frame | coverage · depth | 2 |
| 3 | ±1 m cube, 12 tris | **ours**, generated | indices, winding, back-face culling | coverage · depth | 3 |
| 4 | UV sphere 32×16 | **ours**, generated | a curved silhouette — every boundary pixel its own sub-pixel offset | coverage · depth | 4 |
| 5 | **Menger sponge, level 1–2** | **ours**, generated | genus and depth complexity at *known* topology | coverage · depth | — |
| 6 | **`BoxAnimated`** + `InterpolationTest` | Khronos · CC-BY-4.0 / CC0-1.0 | **time, and nothing else** — one object, TRS, no light | coverage · depth | — |
| 7 | cube lit by one `SUN` | **ours**, generated | the first radiance number | + direct radiance | 5 |
| 8 | sphere lit by the same | **ours**, generated | the whole `cos θ` sweep in one image | + direct radiance | 6 |
| 9 | albedo + emissive spheres | **ours**, generated | channel linearity and the emissive path | + direct radiance | 7 |
| 10 | **`SimpleTexture`** → `TextureCoordinateTest` | Khronos · CC0-1.0 | uv, sampling, base-colour sRGB decode, at 1 texel per pixel | coverage · direct radiance | 10 |
| 11 | **`NormalTangentTest`** | Khronos · CC0-1.0 | tangent-space handedness | + direct radiance | — |
| 12 | **`AlphaBlendModeTest`** | Khronos · CC-BY-4.0 | **format conformance**: `OPAQUE`, `MASK` with `alphaCutoff`, `BLEND` and its ordering | conformance — **not** a coverage rung | — |
| 13 | **`AnimatedMorphCube`** → `MorphStressTest` | Khronos · CC0-1.0 / CC-BY-4.0 | vertex-level animation over a rung already green | coverage · depth | — |
| 14 | cube under the factory point light | **ours**, generated | inverse-square falloff — the literal default lighting | + direct radiance | 8 |
| 15 | cube on a plane, sun at 30° | **ours**, generated | a cast shadow and the first non-zero interreflection | + shadow and indirect | 9 |
| 16 | **`RiggedSimple` → `RiggedFigure` → `Fox`** | Khronos · CC-BY-4.0 (+CC0-1.0) | skinning, at rising joint counts | coverage · depth | — |
| 17 | **`MetalRoughSpheres`** | Khronos · CC-BY-4.0 | the full roughness × metalness sweep | + direct radiance | — |
| 18 | **`SciFiHelmet`** | Khronos · CC0-1.0 | one real asset's material stack | coverage · direct radiance | — |
| 19 | **`ABeautifulGame`** → Barcelona Pavilion | Khronos · CC-BY-4.0 · eMirage · CC-BY | scene scale — draw counts, occlusion, many materials | coverage · depth | — |
| 20 | **quaternion-Julia isosurface, high subdivision** | **ours**, generated | the stress rung: triangle count with *unbalanced* spatial distribution | coverage · depth | — |
| 21 | **the film** (§ I.26.4) | Barcelona Pavilion + `Fox` + our camera path | everything at once, over time, with a per-frame reference | the difference series | — |

- [ ] **`DamagedHelmet` is refused twice over and rung 18 is `SciFiHelmet` instead.** Its normal, occlusion, emissive, base-colour and metallic-roughness textures are **CC BY-NC 4.0** (theblueturtle_, 2016, in the model's own `metadata.json` at the pinned SHA) — non-commercial, which is not a free cultural work and which would put a use restriction on this repository's test suite. And it bundles **five** new things at once, which is exactly what the one-new-thing rule exists to prevent. `SciFiHelmet` is CC0-1.0 entire, carries `POSITION · NORMAL · TANGENT · TEXCOORD_0` with base-colour, metallic-roughness, normal and occlusion textures, and adds **occlusion** alone over rung 11 — one thing
- [ ] **`Sponza` is refused on licence, and the refusal is not close.** The Khronos copy is *"© 2016, Crytek. Cryengine Limited License Agreement"* — a proprietary EULA, not a Creative Commons licence. Rung 19's scene-scale role is carried by `ABeautifulGame` (CC-BY-4.0, ASWF and Ed Mackey, 42.0 MB) and by **Barcelona Pavilion** (eMirage, CC-BY, 24.7 MB, `download.blender.org/demo/test/pabellon_barcelona_v1.scene_.zip`), which is the better of the two for us: it is **architecture**, which is what GTA 5 is the reference for, and it is hard-surface and image-textured, so it survives glTF export
- [ ] **`BrainStem` is refused on licence** — *"© 2017, Smith Micro Software, Inc. Poser EULA"*. Its skinning role is carried by `RiggedSimple` → `RiggedFigure` → `Fox` at rung 16, which is a **rising joint count** and therefore a better ladder than one asset
- [ ] **`BoxTextured` is refused and `SimpleTexture` takes rung 10.** `BoxTextured` is *"CC-BY 4.0 International with Trademark Limitations"* plus a Cesium trademark, and it is on Khronos's own `Models-issues.md` list of models with licence or ownership issues — together with `AntiqueCamera`, `BoxTexturedNonPowerOfTwo`, `CesiumMan`, `CesiumMilkTruck`, `PrimitiveModeNormalsTest` and `RecursiveSkeletons`, all seven of which are out by that list alone. `SimpleTexture` and `TextureCoordinateTest` are CC0-1.0 and carry the same uv path
- [ ] **`Duck` is refused**: SCEA Shared Source License 1.0 (Sony, 2006) — not a Creative Commons licence and not on the allow-list
- [ ] **The fractal is confirmed as the right stressor, and it is two generators rather than one — the instinct was right about *what* and wrong about *which*.** A subdivided sphere is the **best case for every spatial partition**: convex, genus 0, uniform vertex density, one component, near-spherical cluster bounds at any cut, depth complexity 2. It cannot fail a cluster build. What it cannot produce, and what our one cluster DAG is judged on (`core/ClusterDag.h`), is **unbalanced occupancy** and **topology that survives simplification**
- [ ] **A Menger sponge is rung 5 because its topology is *known*.** At level n it is 20ⁿ cubes with a genus that is closed-form, so *"the LOD closed three holes"* is a **number** rather than an impression — and screen-space error alone cannot see a closed hole, because the silhouette barely moves while the shading changes completely (`Real-Time Rendering` 4e, ch. 19 on LOD error metrics). It also gives **depth complexity** along its axes, which a convex body cannot measure at all, and **exact coplanarity at many depths**, which is the deliberate-tie fixture § I.26 already asks scene 1 for
- [ ] **A quaternion-Julia isosurface is rung 20 because its distribution is *unbalanced*.** A 4D Julia set sliced to 3D and meshed at rising grid resolution gives dense filigree in some regions and emptiness in others, with wildly uneven triangle size — which is close to the worst case for a surface-area-heuristic partition, where a sphere is the best (`Real-Time Collision Detection`, ch. 6 on BVH construction and the SAH). The measurable product is the ratio of **cluster bound volume to enclosed geometry**, taken on both bodies: the sphere gives the floor, the Julia set gives the ceiling, and our DAG sits between them
- [ ] Both fractals are **functions, not files** (principle 2), and both give arbitrary triangle counts at a declared parameter — which is what makes rung 20 a *stress* rung with an axis rather than one big model
- [ ] The synthetic floor — triangle, quad, cube, sphere, sponge, Julia — is generated by the same code that emits the ten fixtures, so a coverage failure at rung 1 has **one** cause: we control every vertex position exactly and none of them came from a file

### I.26.6 What the corpus requires of this engine, one line per capability

*Owner's ruling, 2026-08-12: **on every feature ask whether Kingdom Come: Deliverance or GTA 5 uses it;
if so Outshine must implement it, and if not it is a `REFUSED` line with that reason.** Both references
are already `CLAUDE.md`'s, so this is the existing standard applied to a list rather than a new one. The
capability is the requirement; **glTF's spelling of it is how the corpus proves we have it.** Where a
reference reaches the effect by another means that is written down rather than rounded — KCD is a
CryEngine fork that predates several of these extensions, so "KCD does fuzzy cloth, but not via a sheen
BRDF" is a finding about what we owe, not a technicality. The corpus was measured at SHA `2bac6f8c…`
by parsing every `.gltf` in it, so the counts below are the corpus's own and not a recollection.*

| Capability | Proven by | KCD / GTA 5 | Verdict |
|---|---|---|---|
| indexed `TRIANGLES` | `Box` — 144 of 146 | both | required |
| non-indexed | `TriangleWithoutIndices` — 3 of 146 | both | required |
| `NORMAL` | 133 of 146 | both | required |
| `TANGENT` as an attribute | `SciFiHelmet` — 34 of 146 | normal mapping in both | required |
| `TEXCOORD_0` | 118 of 146 | both | required |
| `TEXCOORD_1` | `MultiUVTest` — 9 of 146 | second parameterisation for occlusion and decals | required · `UNSURE` on which reference uses it for what |
| `COLOR_0` | `BoxVertexColors` — 7 of 146 | CryEngine vegetation bending and terrain blending | required |
| skinning (`JOINTS_0`/`WEIGHTS_0`/`skins`) | `RiggedSimple` — 6 of 146 | both, every character | required |
| morph targets | `AnimatedMorphCube` — 4 of 146 | facial animation; CryEngine's Facial Editor is morph-target based | required |
| node hierarchy, TRS and `matrix` | all | both | required |
| GPU instancing | `SimpleInstancing` (`EXT_mesh_gpu_instancing`) | vegetation in both; GTA 5 instances transparent geometry in 11 draw calls | required |
| vertex quantization | `KHR_mesh_quantization` | both ship quantized vertex streams; our 32 B vertex is already a budget | required |
| perspective camera | 12 of 146 | both | required |
| orthographic camera | `Cameras` — 1 of 146 | every cascade is an orthographic projection | required |
| base colour factor and texture | 108 of 146 | both | required |
| **metallic**-roughness factor and texture | `MetalRoughSpheres` — 63 of 146 | both | required · **`Material` has no metalness field** |
| normal texture | 55 of 146 | both | required |
| occlusion texture | 46 of 146 | both | required |
| emissive factor and texture | 27 of 146 | GTA 5 at night | required · declared and unreached today |
| alpha `MASK` | 16 of 146 | GTA 5 discards below 0.75; KCD foliage | required · **load-bearing for vegetation** |
| alpha `BLEND` | 13 of 146 | both | required |
| `doubleSided` | 40 of 146 | foliage in both | required |
| sampler wrap, filter, mip | 92 of 146 | both | required |
| `KHR_texture_transform` | 15 of 146 | atlases and scrolling uv in both | required |
| `KHR_materials_emissive_strength` | `EmissiveStrengthTest` — 5 | HDR emissive at night | required |
| `KHR_lights_punctual` | `DirectionalLight`, `PointLightIntensityTest` — 10 | both | required · **a subsystem, not a feature** |
| `KHR_materials_specular` | `SpecularTest` — 9 | dielectric F0 in any PBR | required |
| `KHR_materials_ior` | 17 | water and glass in both | required |
| `KHR_materials_transmission` | 33 — the most-used extension in the corpus | glass in both | required |
| `KHR_materials_volume` | 25 | GTA 5's water absorbs with depth, which is the same integral | required · the glass case is the finding |
| `KHR_materials_clearcoat` | `ToyCar` — 13 | GTA 5 car paint; CryEngine's car paint does colour shifting over a coat | required |
| `KHR_materials_sheen` | `SheenCloth` — 10 | **CryEngine's Cloth shader has a fuzzy layer with its own gloss** — the effect, by another means | required |
| `KHR_materials_anisotropy` | 7 | **CryEngine's Hair shader has had anisotropic highlights with directionality maps since 3.6**; brushed metal and rims in GTA 5 | required |
| `KHR_materials_variants` | `MaterialsVariantsShoe` — 7 | GTA 5 vehicle liveries and modkits | required · cheap, a material-row swap |
| diffuse transmission | `DiffuseTransmissionTest` — 6 | **CryEngine's Vegetation shader's headline feature is translucency (light transmittance)** | required as a **material model**, and `Material::Transmission` is half of it. Proven by the extension's flat test asset as format conformance — **never by a foliage comparison**, which Band III owns |
| `KHR_node_visibility` | `CubeVisibility` — 2 | both | required · trivial |
| `KHR_texture_basisu` | 1 | both ship block-compressed textures; ASTC and BC are native on this device | required |
| `KHR_animation_pointer` | `AnimatedColorsCube` — 5 | animated emissive and uv in GTA 5 | required · `clients/Animation.h` is the mechanism, the glTF spelling is later |
| `LINEAR` · `STEP` · `CUBICSPLINE` | `InterpolationTest` — the only asset with all three | both | required · `core/Keyframes.h` and `core/CatmullRom.h` already carry them |
| `KHR_materials_iridescence` | 10 | neither — no thin-film anywhere in either | **REFUSED** |
| `KHR_materials_dispersion` | 4 | neither | **REFUSED** |
| `KHR_materials_unlit` | 5 | the effect is emissive-only, which `Material::Emission` already spells | **REFUSED** · no second material model |
| sparse accessors | `SimpleSparseAccessor` — 1 | a file compaction, no runtime capability | **BUILT, and outside the 39** — the `REFUSED` verdict is overturned by the ruling below the table |
| `POINTS`/`LINES`/`STRIP`/`FAN` | `MeshPrimitiveModes` — 2 of each at most | debug drawing only; the picture is triangles | **REFUSED** — we draw none of them. The reader carries `mode` all the same; the refusal is the consumer's (§ I.26) |
| multiple scenes | `MultipleScenes` — 1 | neither | **REFUSED** — we render one. The reader reports the whole `scenes` array and the file's default; choosing one is the consumer's (§ I.26) |
| `KHR_draco_mesh_compression` | 1 | neither ships Draco; both use their own formats | **REFUSED** — a large vendored decoder for no picture |
| `EXT_meshopt_compression` | 1 | as above | **REFUSED** |
| `EXT_texture_webp` | 1 | neither | **REFUSED** — a second image decoder buys no comparison |
| `KHR_xmp_json_ld` | 2 | metadata, not rendering | **REFUSED** |
| `KHR_materials_pbrSpecularGlossiness` | 1 | archived by Khronos | **REFUSED** |

- [ ] **The denominator is the required rows of the table above and the percentage is read against it** — *features the references use* — never against every glTF extension that exists. *Recounted 2026-08-12 against the table's own rows: **39 required and 11 refused**, 50 rows in all. The line read 40 and 11, which is one more required row than the table has ever carried; the refusal count is right. Every percentage in this section is therefore read against 39, and the two lines below are the first to say so.* The sparse-accessor row is **outside** the denominator in both directions — it is not a picture capability, so it enters no numerator either
- [ ] **The sparse-accessor `REFUSED` is overturned, and the reason it was refused is the part worth keeping.** *"A file compaction, no runtime capability"* is **true and was the wrong test**: nothing in this section's own rule — *does KCD or GTA 5 use it* — can answer a question about a file's encoding, because neither ships glTF at all. What decides it is the cost of the two answers: refusing means refusing `SimpleSparseAccessor` **whole**, and § I.26.6's closing line requires every capability to be proven by a named corpus asset, so a refusal here deletes an asset from the corpus rather than a feature from the picture. Implementing it cost **37 lines** (`src/gltf/Document.cpp` `ApplySparse`) and **cannot produce a wrong picture** — it either resolves the overrides or refuses the file. Built and ticked under § I.26 above
- [ ] **Today the engine reads 10 of the 39 through glTF — 25.6 %** — and the baseline that stood here was `0 of the 40, because there is no glTF reader`, which was true when it was written and is now stale by one commit (`e658b21`). **The recount, row by row against `src/gltf/`, so the next round can check it rather than trust it.** Read: indexed `TRIANGLES` · non-indexed · `NORMAL` · `TANGENT` · `TEXCOORD_0` · `TEXCOORD_1` · `COLOR_0` · node hierarchy with TRS and `matrix` · perspective camera · orthographic camera. **Not read, and the reason is one fact each**: skinning and morph targets need `skins`, `targets` and `mesh.weights`, and none of the three is parsed · GPU instancing, quantization, `basisu` and every `KHR_materials_*` need an `extensions` object, and the reader parses none · everything from base colour to `doubleSided` needs `materials`, `textures`, `images` and `samplers`, and `Primitive::Material` is an index into an array that is never read · `KHR_animation_pointer` and the three interpolations need `animations`. **The five attribute rows — `NORMAL`, `TANGENT`, `TEXCOORD_0`, `TEXCOORD_1`, `COLOR_0` — are read for one reason, and it is a design decision rather than five features**: an attribute is a name and not a slot, so every semantic a file carries crosses at once
- [ ] **6 of those 10 are held by an asserted claim and 4 cross untested**, which is the number worth watching rather than the 10. Held: indexed `TRIANGLES`, `NORMAL`, `TEXCOORD_0`, the node hierarchy, and both cameras. Untested: **non-indexed** (four primitives in the fixture carry no `indices` and nothing asserts `Indices == -1`), `TANGENT`, `TEXCOORD_1`, `COLOR_0` — all four cross only because the attribute path is generic, and a generic path that no case names is a capability nobody has looked at. The closing line of this section already requires a named corpus asset per capability; these four are where it bites first
- [ ] **As raw capability, independent of the boundary, the engine expresses 18 of the 40 — 45 %** *(the denominator is 39, so the fraction is 18/39 = 46.2 % if the numerator still holds. It was not re-counted this round and is deliberately left standing rather than restated — a numerator nobody checked, divided by a denominator somebody did, is a worse number than the one it replaces)* — and the number is generous by construction, because it counts *the engine can express this quantity*, not *a glTF value reaches it*. Present: indexed triangles · `NORMAL` · a tangent frame (`core/TangentFrame.h`) · `TEXCOORD_0` (`core/ChunkVtx.h`) · instancing (`render/stages/ModelDraw.h`) · perspective and orthographic projection · base colour · roughness · alpha `MASK` and `BLEND` (`Material::Coverage`) · `doubleSided` · sampler settings · transmission and ior (`core/Material.h`) · `LINEAR`, `STEP` and `CUBICSPLINE`. **Absent: metalness has no field at all**, and neither do occlusion, clearcoat, sheen, specular, anisotropy or volume
- [ ] **`Material` gains a metalness field**, because it is the one required quantity of the metallic-roughness model that has **no spelling** in this tree: `core/Material.h` carries albedo, roughness, coverage, transmission, ior and emission and nothing that separates a conductor from a dielectric. It switches no pipeline state, so it is a material row entry by the core's own rule, and rung 17 is what first requires it
- [ ] **A light list, and it is a subsystem rather than a feature.** `render/Gpu.h:22 SceneLight` binds one irradiance pair, one cascade buffer and one shadow atlas — *"one light, one scale, one set of cascades, so no surface can end up lit by a second sun"*. That invariant is **right for the sun and wrong as the whole lighting model**; § II.8 already owes point and spot lights as a list, and rungs 14 and 21 are what pull it. The shape to prefer keeps the sun's uniqueness unspellable while making the list ordinary: the sun stays a distinguished binding, the list is a second one, and no surface can bind two suns because there is still only one field for it
- [ ] Skinning, morph targets, `COLOR_0` and `TEXCOORD_1` are **four new vertex-stream capabilities and one new vertex layout question**: our layout is `pos3 · uv2 · nrm3` at 32 B with a declared 24 B second (`core/ChunkVtx.h`), and neither has a spelling for joints, weights, a second uv or a colour. That is a design question this section raises and does not answer — a fifth layout per capability is the wrong answer, and so is one fat layout
- [ ] `Material` gains an **occlusion strength** entry with the same argument as metalness — it is a number, it switches nothing, and 46 of 146 corpus assets carry an occlusion texture. It is **not** the same quantity as `render/stages/AoStage`'s screen-space term and the two must not be summed silently
- [ ] The § I.26 reader line's *"Skinning, morph targets and `animations` out of scope"* is **superseded by this section**: all three are required capabilities, pulled by rungs 6, 13 and 16 respectively. *The earlier line was right about the reader's first subset and wrong as a scope statement, and the difference is the ruling above*
- [ ] Every required capability is **proven by a named corpus asset and only by that asset** — a capability with no asset behind it is untested scope, and an asset that proves nothing is bytes we fetch for no reason

### I.26.7 The forest rung — a scenario test, not a render test, and that is what it means to measure cost

*Owner's ruling, 2026-08-12: **our renderer must be good at drawing vegetation**, and a forest scene in
glTF is how that is built and proven. It reverses a foliage exclusion I had applied one round earlier,
and the correction is worth stating plainly: **excluding forests protected the parity ladder from a
subject that was never on it, at the cost of the hardest workload the renderer will ever meet.***

**This is the rung where the ladder changes what it is measuring, and the change is large enough that it
changes which suite the rung lives in.** Rungs 1–21 ask *is the picture right* against an oracle. The
forest rung asks *does it hold 60 Hz and does it look right while moving* — so by § I.26.9's placement
rule it is a **scenario test**, declared as camera × clock over a studio stage (§ I.25), and it is the
first **performance** acceptance anywhere in this section. No parity number, no IoU, no radiance. *It was
first filed under the render suite, which was wrong in a way worth keeping visible: a case with no score
inside a suite whose contract is a score is precisely the hollow case the empty-image guard exists to
catch, and this one would have been the first.*

**A forest is the only subject that exercises six things nothing else on the ladder touches.**

| What only a forest exercises | Why nothing else covers it |
|---|---|
| **instancing at scale** — thousands of copies of a handful of meshes | every Khronos feature test is one object; `SimpleInstancing` is a proof of syntax, not of scale |
| **alpha test and alpha-to-coverage on leaf cards** | `AlphaBlendModeTest` is flat quads at one depth |
| **overdraw** — the fill-rate killer on 5 GPU cores | a closed opaque mesh generates none; depth complexity in a canopy is tens |
| **two-sided thin-surface shading, leaves lit from behind** | needs `doubleSided` *and* diffuse transmission on the same primitive |
| **LOD transition and popping** | a still rung structurally cannot see it |
| **density culling at distance** | there is nothing to cull in a single-subject scene |

- [ ] **The instrument, and it is not the parity ladder's**: **frame time p50/p95/p99 over a moving camera at 720p60**, never a mean — `CLAUDE.md`'s existing rule, and this is the first rung that can apply it to a real workload · **by eye and in motion** for popping, ghosting and LOD transition · **Blender's render of the same scene as a reference for the eye only** — canopy translucency and occlusion, what the light does under a crown — and **never as a coverage or radiance number**
- [ ] **No IoU, no boundary p95, no radiance parity on this rung.** Our alpha-to-coverage against Cycles' stochastic transparency compares two sampling policies, and a per-pixel mask difference there measures the choice rather than the implementation
- [ ] **Blender's render of the same scene is a reference for the eye and never a number** — canopy translucency, what the light does under a crown, whether the crown reads as a crown. It is opened beside ours by a person; nothing reads it
- [ ] **It decouples the renderer from the generator, and that is a scheduling argument with teeth.** The vegetation **rendering path** — instanced leaf cards, alpha test, two-sided transmission, LOD, density culling — can be built and proven against a downloaded forest **before the tree generator exists**. Afterwards it is a **known-good target**: when the generator's first canopy looks wrong, the renderer is already exonerated and the defect is attributable in one step instead of two
- [ ] **It is a class claim and therefore an acceptance gate, not an optional extra at the end.** Kingdom Come: Deliverance's identity *is* its forest. A renderer that cannot hold 720p60 in one has failed the CryEngine-class claim whatever its parity numbers say — so **the renderer stage is not done until the forest rung is green**, and it is a gate on that stage rather than a rung somebody gets to later
- [ ] **Its dependency is rung 12 and nothing further**: it needs instancing and alpha test to exist and to have passed as isolated render cases, and it needs nothing else from the parity ladder — not radiance, not skinning, not the film. *Being in another suite does not free it from the dependency order; a join still waits for its inputs (§ I.26.5)*
- [ ] **The subject is Poly Haven's `pine_forest` collection, and it is the cleanest licence in the corpus: CC0.** *"All assets on this site … are licensed as CC0"*, *"You can use our assets for any purpose, including commercial work"*, *"You do not need to give credit"*, *"You can redistribute them"* (`polyhaven.com/license`). Sixteen models tagged `collection: pine_forest`, **published natively as glTF at 1k/2k/4k** — so no `.blend` export step exists on this rung at all and § I.26.2's lossy-export risk does not apply
- [ ] **Poly Haven's trees are scan-derived and enormous, and the selection follows from that rather than from taste**: measured through the API at glTF/1k, `pine_tree_01` is **958 MB** and `fir_tree_01` **487 MB** — the whole collection is **1 874 MB**. So the rung takes the **saplings, ferns, grass, moss, roots, stumps and rocks** (each under 26 MB, ≈ 82 MB together) and builds density by **instancing**, which is what the rung is measuring anyway. One hero tree is fetched separately and only if a large-mesh arm is wanted
- [ ] **Poly Haven is a third source kind, `api`** — an asset id resolved through `api.polyhaven.com/files/<id>` to a URL and a declared byte size, then pinned by **URL plus SHA-256** like any `file`. The API is how the URL is *discovered*, never what is trusted: a manifest entry records the resolved URL and hash, so a rebuilt corpus does not depend on the API answering
- [ ] `demo/eevee/ember_forest/forest.blend` (74.3 MB, CC-BY, Mike Pan) is the **second forest and the export arm** — a real authored forest scene that must survive § I.26.2's conversion, where the Poly Haven collection needs no conversion. Two forests from two pipelines is what separates *our vegetation path is slow* from *the exporter dropped the leaf cards*
- [ ] The forest rung's **motion arm** is `sprite_fright_030_0020_A` (CC-BY 4.0, confirmed in its own `readme.txt`), because popping and LOD transition need a moving camera through a dressed set, and it needs no per-frame parity number to produce that judgement
- [ ] **A declared density sweep, because one forest is one data point**: instance count per hectare swept over a declared range with the frame-time distribution at each step, so the product is *where 720p60 breaks* rather than *it was 47 ms once*. That curve is the rung's real output and it is what a later generator is tuned against

### I.26.8 What KCD and GTA 5 do, mapped to a subject or named as a gap

*Owner's ruling, 2026-08-12, restated because it had narrowed in transit: **the renderer must be good at
everything KCD and GTA 5 do** — not at everything the glTF format exercises. `CLAUDE.md` already carries
the rule; § I.26.6 applied it to a **feature list** and that list was bounded by what a file format can
carry, which silently dropped every workload-shaped requirement. This section is the workload half.*

**Every feature gets a basic, isolated, synthetic case before any complex scene exercises it.** *Owner's
ruling, 2026-08-12: **a basic glTF test case for every GL feature KCD or GTA 5 uses**, and "basic" is
the operative word. It is the one-new-thing rule extended across the whole feature surface instead of
across the first ten rungs only — **a forest that fails tells you nothing if instancing, alpha test and
shadow cascades were never isolated first**.*

**Two kinds of case, and the second is the one with no assets to find.**

| | |
|---|---|
| **A · format features** | glTF expresses them; a Khronos sample usually exists and is **fetched** |
| **B · renderer techniques** | glTF cannot express them, so no sample can be found — the case is **authored in Blender and exported**, minimal by construction |

- [ ] **For kind B the case is authored, and that closes the escape hatch this section first left open.** *"No clearable asset expresses this" is **no longer an admissible reason to skip a feature** — if no asset exists we author one. The only remaining admissible exclusion is **"neither KCD nor GTA 5 does this"**, stated per feature with its reason.*
- [ ] **Authoring in Blender means the oracle comes free**, which is the whole reason kind B is tractable: the scene is built in the reference renderer, so the reference render already exists, and the exported glTF is the subject our side reads. A found asset would have to be *hoped* to isolate the right thing; an authored one **is** minimal because we made it so
- [ ] The authoring scripts are **offline data preparation committed beside what they produce** — `CLAUDE.md`'s one open door, the same one the corpus fetcher goes through. A `.py` per case, its `.blend` and `.gltf` derived and untracked (§ I.26.10)
- [ ] **The glTF file is the *subject*; the feature under test may be ours entirely.** A basic SSR case is still a basic glTF scene — the file carries a sphere and a plane, the reflection technique is 100 % ours, and Cycles renders what the reflection should look like. Nothing about kind B requires glTF to know what the technique is

| Workload | Kind | Basic case | Integration scene |
|---|---|---|---|
| PBR metal-rough, normal · occlusion · emissive maps | A | `MetalRoughSpheres` · `NormalTangentTest` · `SciFiHelmet` | built world |
| alpha `OPAQUE`/`MASK`/`BLEND` | A | `AlphaBlendModeTest` | forest |
| instancing | A | `SimpleInstancing` — syntax; **authored count sweep** — scale | forest |
| skinning · morph · interpolation modes | A | `RiggedSimple` · `AnimatedMorphCube` · `InterpolationTest` | film |
| `KHR_lights_punctual` | A | `DirectionalLight` · `PointLightIntensityTest` | built world at night |
| cameras and projection | A | `Cameras` (perspective **and** orthographic) | every rung |
| `KHR_texture_transform` | A | `TextureTransformTest` | built world |
| vertex colours · second uv set | A | `BoxVertexColors` · `MultiUVTest` | forest |
| clearcoat = **car paint** | A | `ToyCar` · `ClearCoatCarPaint` (CC0) | vehicle |
| transmission + volume + ior = **glass** | A | `TransmissionTest` · `CompareVolume` · `IORTestGrid` (CC0) | built world · vehicle |
| sheen = **fabric** | A | `SheenCloth` (CC0) | character |
| anisotropy = **brushed metal** | A | `AnisotropyDiscTest` (CC0) | vehicle |
| `emissive_strength` | A | `EmissiveStrengthTest` | night |
| `specular` | A | `SpecularTest` | built world |
| **cascaded shadow maps and cascade transitions** | **B** | **authored**: one long ground plane, one occluder, sun low, camera dollying so a shadow crosses a cascade boundary | built world |
| **SSR and reflection probes** | **B** | **authored**: reflective sphere over a plane with a second object off-screen — off-screen is what separates SSR from a probe | built world |
| **water reflection and refraction** | **B** | **authored**: a flat surface with ior 1.33 over a textured floor, a partly submerged rod for the refraction break | river · sea |
| **decals** | **B** | **authored**: one projected quad on a curved surface, to isolate depth bias and normal reorientation | built world |
| **volumetric fog and god rays** | **B** | **authored**: a light through a slotted occluder into a fog volume | forest · built world |
| **motion blur** | **B** | **authored**: a wheel at declared angular speed, one shutter interval | vehicle |
| **depth of field** | **B** | **authored**: three subjects at declared distances, one declared focus and f-number | any |
| **bloom and tonemapping** | **B** | **authored**: an emissive disc at declared luminance against black | night |
| **TAA** | **B** | **authored**: a static subpixel-detail chart plus the same in motion — the only way ghosting is separable from softening | forest |
| **wet surfaces** | **B** | **authored**: one plane, roughness and specular swept as a wetness parameter | weather |
| **LOD transition** | **B** | **authored**: one subject, declared LOD ladder, camera dollying through each switch distance | forest |
| **overdraw** | **B** | **authored**: N alpha-tested quads stacked at declared depth complexity | forest |
| **hair and cloth** | **B** | **authored**: a card-based hair clump and a draped cloth — geometry we emit, since glTF carries no hair primitive | character |
| **particles: fire, smoke, dust** | **B** | **authored**: a declared quad emitter, fixed seed, fixed times — the particle system is ours, the case pins its output | weather |
| **large-scale terrain** | **B** | **authored**: a declared heightfield meshed at kilometre scale, since no clearable asset was found | world |
| **crowds of skinned characters** | **B** | **authored**: `Fox` instanced at declared counts — a throughput claim one character cannot make | crowd |
| **day/night, dynamic sun and moon · night sky** | — | **excluded, and the reason is the admissible one**: these are not features a subject can carry, they are our own scenario axes (§ I.25) — a time sweep is a run, not a file. The features *under* them (punctual lights, emissive strength, tonemapping) each have a case above |
| `KHR_materials_iridescence` · `dispersion` · `unlit` | — | **excluded: neither KCD nor GTA 5 does this.** No thin-film, no spectral dispersion, and unlit is emissive-only which `Material::Emission` already spells |

- [ ] **The count that matters: 14 kind-A cases fetched, 17 kind-B cases authored, 2 exclusions by scenario axis, 3 exclusions by neither-reference-does-it.** No entry is a gap any more, which is the change this ruling makes — a gap was a place the ladder quietly stopped, and an authored case is a task with a size
- [ ] **Kind B is where the real cost sits and it should be stated rather than discovered**: 17 minimal `.blend` files, each with a script, an export and a reference render. They are small scenes — the Cycles floor measured in § I.26.4 is 2.087 s/frame on a six-quad scene — so the reference cost is minutes, and the cost is **authoring judgement**, not compute
- [ ] Every kind-B case declares **what would make it fail**, because a minimal scene with no failure signature is a picture nobody can read: the cascade case fails as a visible seam at a declared distance · SSR fails by missing the off-screen object · decals fail by z-fighting or by a normal that did not reorient · TAA fails as ghosting behind the moving edge · LOD fails as a pop at a named distance
- [ ] **Where the technique has no counterpart in Cycles at all — TAA, SSR as an approximation, LOD selection — the oracle renders the *ground truth* and the case reports the *approximation error***, which is § I.26's bias-curve shape applied to techniques instead of to bounces. A path tracer's reflection is the correct answer SSR is approximating, so the comparison is meaningful even though parity is impossible

### I.26.9 Three suites, split by instrument — and only one of them mirrors `src/`

*Owner's ruling, 2026-08-12: **separate the tests into declarative render tests, declarative scenario
tests, and unit tests.** It governs the two sections below, and it supersedes their `<area>` mirroring
for two of the three kinds. **The split is by instrument, not by shape** — that is what makes it hold,
because two declarative suites look alike and are decided by entirely different questions.*

| Kind | Declaration | What decides it | Mirrors `src/` |
|---|---|---|---|
| **render** | the `.gltf` | agreement with the oracle — boundary p95, radiance median | **no** — organised by feature |
| **scenario** | the scenario: camera × clock × world-or-studio (§ I.25) | frame time p50/p95/p99 over motion · determinism · residency · streaming · memory | **no** — organised by declared run |
| **unit** | the code | a stated invariant, nothing rendered | **yes, exactly** |

- [ ] **The placement rule, so this does not drift: *what would fail this test?*** — *"our pixels disagree with Cycles"* is a **render** test · *"the frame floor broke, the run was not deterministic, or memory grew"* is a **scenario** test · *"the code computed the wrong thing"* is a **unit** test. **A case that seems to fit two is testing two things and is split**
- [ ] **Only the unit tree mirrors `src/`, and it must, because its organising axis *is* source location.** It is the tree that carries `CLAUDE.md`'s property: each directory compiles with its own include set, so a name it must not reach has no spelling and a breach is a **compile error**. Every unit test is a continuous proof that its layer's include set is exactly what it claims
- [ ] **The declarative suites carry none of that and restoring the mirror over them would destroy it.** A render case links the whole library by construction — it needs the reader, the renderer and the readback at once — so a `test/render/core/` directory would compile with a *wider* include set than `test/core/` and the mirror would stop meaning anything. *Written down because "restore the mirror everywhere" is a tidy-looking change that dilutes a proof into a convention, and the dilution is invisible afterwards*
- [ ] The two declarative suites are organised by what they declare: **render by feature** — the § I.26.8 matrix is the directory structure — and **scenario by declared run**, so a run is found by the thing it runs rather than by the source file it happens to exercise
- [ ] **`GrownBarkIsAClosedMesh` and its whole class are unit tests and keep the mirror**: closed, wound, unit-normal and in-range are decidable geometric invariants over a generated mesh, with no reference and nothing rendered. *They are the shape the unit tree is for, and they are already in the right place — this line exists so a later round does not re-file them as render tests because they concern geometry*
- [ ] **§ I.26.3's time contract is a unit test**, not a render one: *for every `n`, the engine's animation time equals `n / fps` exactly, and equals the glTF sampler input at the corresponding keyframe*. Arithmetic, no oracle, no pixels — and it keeps the mirror at `test/clients/`

**The re-filing this ruling forces, and the forest was not the only one.**

- [ ] **The forest rung moves to the scenario suite** (§ I.26.7), and it was filed wrong: its instrument is **frame time over a moving camera plus judgement by eye**, and there is **no oracle comparison in it anywhere** — Blender's render is a reference *for the eye* and never a number. A case with no score, inside a suite whose entire contract is a score, is the hollow case the empty-image guard exists to catch, and it would have been the first one
- [ ] **Re-checked across the whole matrix, seven more move**, by the same test — none of their acceptances is agreement with the oracle: **overdraw** (fill cost at declared depth complexity) · **LOD transition** (a pop at a named distance, only visible in motion) · **TAA** (ghosting against softening, and § I.26 already runs every parity rung with TAA off, so it has no oracle counterpart by construction) · **particle determinism** (a fixed seed pinning its own output) · **crowds of skinned characters** (a skinning-throughput claim) · **large-scale terrain** (LOD, cascade range and streaming together) · and the **forest density sweep** (where 720p60 breaks)
- [ ] **The film rung splits in two rather than choosing**, because it genuinely tests two things: the **per-frame difference series against the stored oracle** is a *render* case, and the **discontinuity test plus the frame-time distribution over the same segment** is a *scenario* case. *They share a scene and a camera path and answer different questions, and merging them would produce a verdict nobody can attribute — which is the same objection as a single blended score, one level up*
- [ ] What stays in the render suite is everything whose failure is *our pixels disagree with Cycles*: the spine's twenty-one rungs, every kind-A format case, and the kind-B technique cases with an oracle counterpart — cascades, SSR, water, decals, volumetrics, motion blur, depth of field, bloom and tonemapping, wet surfaces, hair and cloth as geometry
- [ ] **Each suite states its own verdict shape once, and they are three different shapes**: render is *every named metric within its own threshold and direction* (§ I.26.10) · scenario is *the frame-time distribution within its declared floor, the run bit-identical across two invocations, residency and memory within declared ceilings* · unit is *the invariant held*. **A suite that borrowed another's verdict shape would be reporting a number that does not decide it**

### I.26.10 A render test is a directory, and in the minimal case that directory is one `.gltf`

*Owner's ruling, 2026-08-12: **the render tests are declarative. The glTF is the declaration. One runner
renders oracle and outshine and scores them.** Taken to its strongest form: **a test is a directory and
the directory is self-describing.** It supersedes the earlier "one directory with its Blender script and
its expected values". `<area>` still mirrors `src/` as always.*

*Superseded 2026-08-12, second time, and the trade is written out because the reason the first form was
chosen is still true: this section's own first draft required a `manifest.json` per case, that draft was
struck because it made adding a case a writing task, and the strike went one step too far — **`case.json`
optional plus a global corpus manifest put a case's subject, its licence and its comparison in three
files**, which is the split the last line of this section already calls out. The merged form keeps the
cheap-case property by making the manifest a **delta over declared defaults**, not by making it absent:
a tracked-`.gltf` case with the derived camera and the default recipe is `schema`, `schemaVersion`, `id`
and `covers` — four lines. The 190-line one under `render/coverage/triangle/` is long because it is a
**fetched** case with a **declared** camera and **two** recipes, and every one of those is a deviation
that has to be written down somewhere.*

```
test/render/<feature>/<case>/
    manifest.json     tracked              THE DECLARATION — the only tracked file
    scene.gltf        tracked or fetched   the subject
    0-oracle.png      always written       for the eye, in browsing order
    1-outshine.png    always written       for the eye
    2-diff.png        always written       for the eye
    oracle.exr        always written       float32 — this is what the score reads
    oracle.raw        always written       float32, flat — what the C++ runner reads
    outshine.exr      always written       float32
    provenance.json   always written       what actually ran
```

- [ ] **A four-line `manifest.json` beside a `.gltf` is a complete, valid, scoring test case** — camera, thresholds and render recipe all resolve from declared defaults, and every field beyond those four exists only to override. *The writing cost the first strike was protecting is preserved and the measure of it is the diff: if the minimal case is ever more than the schema, the id and what it covers, this line has been broken*
- [ ] **A recipe other than `default` names its products `oracle.<recipe>.exr`, `0-oracle.<recipe>.png` and `oracle.<recipe>.raw`**, so the eye's files still sort to the top, and `1-outshine.png`, `2-diff.png`, `outshine.exr`, `outshine.raw` and `provenance.json` are a **reserved set the preparer refuses to name** — a collision has no spelling rather than a rule against it
- [ ] **`oracle.raw` is a flat float32 dump beside the EXR, because C++ has no EXR reader**, SDL3 provides none, and vendoring OpenEXR to compute an IoU would buy nothing. Header: `"OSRAWF32"` · a natively-written `0x01020304` a reader compares against its own to learn the byte order · version · width · height · channels · header length · row order · NUL-terminated channel names; then row-major channel-interleaved samples, uncompressed. Scene-referred linear, the same values the EXR carries. 1280×720 RGBA is **14 745 644 B**. *The samples come back through the EXR rather than through `Render Result`, which refuses pixel access in background mode*
- [ ] **The runner is one program over a directory** — read the glTF, resolve camera and recipe, render both sides, compute the named metrics, print a verdict. Still one process and one real verdict per case (§ I.20); what is shared is the code, never the process
- [ ] **The camera comes from the glTF wherever the scene declares one**, which is what makes such a scene fully self-describing — glTF carries `cameras` and a node that references one. A declared camera is used verbatim and no framing rule runs
- [ ] **Where the scene declares no camera, framing is derived by one rule stated here and never per test**, so two cases with the same subject get the same picture and nobody tunes a viewpoint into a pass

**The framing rule, and it is deterministic by construction rather than by care.**

| Step | Rule |
|---|---|
| bounds | world-space AABB over every rendered primitive, node transforms applied |
| centre | `c = (min + max) / 2` — **from min/max only, never a vertex mean** |
| radius | `r = ‖max − min‖ / 2`, the bounding sphere's radius, so the framing does not change with view direction |
| direction | fixed unit vector in glTF's +Y-up frame: **azimuth 35°, elevation 20°**, `[SET]` — off every axis, so no face is edge-on and no silhouette is degenerate |
| distance | `d = r / sin(yfov / 2) / fill`, with `yfov` **39.6°** and `fill` **0.6** `[SET]` — the subject's bounding sphere spans 60 % of the frame's vertical extent |
| clip | `znear = max(d − r, r/1000)`, `zfar = d + r` |

- [ ] **Load order cannot move that camera, and the reason is arithmetic rather than discipline**: `min` and `max` are exact in IEEE-754 and both commutative and associative, so the AABB is identical whatever order primitives arrive in. **A centroid would not be** — floating-point addition is not associative, so a vertex mean shifts in the last bits when the loader changes, and every boundary-displacement number in the suite would shift with it. That is why the centre is defined off the extremes and the rule says so
- [ ] The framing rule's constants live in the const headers of § I.23 like every other number — one declaration, no per-test copy, and changing one moves every derived-camera case at once, which is the intended blast radius
- [ ] **Degenerate bounds are a refusal, not a fallback**: an empty AABB (no rendered primitive) or `r = 0` (every vertex coincident) refuses by name. A fallback camera here would manufacture exactly the empty picture § I.26.11 exists to catch
- [ ] **Thresholds default per instrument class** (§ I.26 — opaque ≥ 1 px, sub-pixel present) and a case overrides only where it earns it, with the reason in its `manifest.json`
- [ ] **A threshold restated in a manifest rather than inherited is a defect, even when the value it restates is right today.** `render/coverage/triangle/manifest.json` copies rung 1's boundary p95 as `0.5` and cites § I.26 for it, four months after § I.26 tightened that number to `0.1` in the same file — so the copy is a **stale quotation that reads as an authority** (`doc/bugs.md`). Right: a manifest's `acceptance` block carries only what it **overrides**, with the reason, and an entry equal to the default is refused at parse rather than accepted as agreement
- [ ] **The default is not weaker against tampering than 200 scattered numbers — it is much stronger, and this is worth stating because it looks like the opposite.** One declared default in one file, read by every case, is **visible when edited**: the diff is one line and it moves the whole suite at once, so nobody quietly relaxes case 137. Two hundred per-case numbers are two hundred places a threshold can be nudged to match a result and have it read as ordinary work. *`CLAUDE.md`'s "a number stated before and after that nobody moves" is served better by one number than by two hundred*
- [ ] **The render recipe defaults the same way** — engine, device, samples, adaptive off, denoising off, seed, `diffuse_bounces`, pixel filter and width, resolution, output format — declared once in § I.26 and overridden per case only where the rung needs it, as rung 9 needs bounces on
- [ ] **The Blender release is part of the oracle cache key and not of any case file** (§ I.26), so the version cannot drift per case and the cache cannot serve a render the recipe no longer describes
- [ ] What `manifest.json` may carry beyond its four required fields: the **subjects** and their pins and per-file licences where the `.gltf` is fetched (§ I.26.1) · threshold overrides with their reason · a recipe override with its reason, or a **map** of named recipes where one scene needs two renders, as rung 1 needs a binary mask and an alpha coverage · an explicit camera where the glTF has none and the derived one is wrong for the question · the **requirement identifiers the case covers** (§ I.20) · and a **closed-form expected value** where one exists, because rung 3's `ρ·E·cos θ / π` is a value and a test that only checks a difference cannot tell a right answer from two wrong ones that agree
- [ ] **Every acceptance entry is an object carrying its origin, never a bare float** — `{"value": …, "unit": …, "origin": "SET" | "derived" | "measured"}`, and a `derived` number without its `derivation` is a refusal. *A bare float has no spelling in the file, so `CLAUDE.md`'s "every number carries its origin" is carried by the shape rather than counted by a checker*
- [ ] **The acceptance is stated before the run and read from there by the test**, so a number cannot be edited to match a result it failed
- [ ] **The reference is derived, never committed** — 720p RGBA float32 is `1280·720·4·4 B = 14.75 MB` a frame, and a 240-frame film segment is **3.54 GB** on its own. What is tracked is the declaration; the pixels come from it and cache by hash
**Both pictures always land in the case directory, and that is where a human goes to look.**
*Owner's ruling, 2026-08-12: **the reference and outshine images are always placed in the test folder
containing the glTF, so I can see the progress.** It supersedes this section's earlier "ours lands
beside it", which left the writing optional and the naming unstated.*

- [ ] **Both images are written on every run, pass and fail alike.** *A picture that only appears on a failure cannot show progress, which is the stated purpose — and a suite that shows nothing while it is green is a suite whose improvement nobody can see between two reds*
- [ ] **PNG always, and the float pair always beside it**: the EXR float32 pair is what every metric reads and what settles a radiance question; the PNGs exist **purely so the directory can be opened and looked at**. Neither replaces the other and neither is optional
- [ ] **The names sort for browsing and the order is part of the contract**, not alphabetical accident: `0-oracle.png` · `1-outshine.png` · `2-diff.png`, beside `scene.gltf` and the two `.exr`. The purpose is visual comparison in a file browser, so *oracle first, ours second, difference third* is what the numbering buys
- [ ] **Untracked but permanent — and the distinction is the whole line.** ~200 cases at 720p is **≈ 100 MB churning per run**, so the images stay out of git; *untracked* means **not in git**, never **not there**. **A cache hit must still materialise the oracle into the directory**, and an incremental run must never leave a case image-less — a directory with a stale picture, or none, is worse than one with no pictures at all, because it is silently wrong about what it shows
- [ ] **Motion cases get a numbered frame sequence plus one encoded file for the eye**, under the same rule: the sequence decides, the encode is for looking, and both are always written
- [ ] **This is where the by-eye judgement lives, and saying so makes it a property of the layout instead of a habit.** `CLAUDE.md` holds that appearance is judged by eye and in motion and that a number never decides whether it looks right — but nothing until now said *where a person goes to do that*. **It is this directory.** A scenario case (§ I.26.9) whose verdict is a frame-time distribution writes its pictures the same way and for the same reason, because its acceptance is half by eye and that half needs a home too
- [ ] A comparison whose halves live in different places is one somebody assembles by hand every time, and nobody does it twice
- [ ] **EXR float32 decides and PNG is for looking**, and both are written: a PNG carries a transfer function and cannot settle a radiance question. `diff.png` is a viewing aid and no acceptance reads it
- [ ] **The case declares both jobs in one file and that is the ruling, not a compromise** — *what may be fetched and under what licence* and *what this comparison is*. *Superseded 2026-08-12: the earlier line split them across a corpus manifest and a case file, so a URL and the acceptance it produces sat in two places that nothing kept in step, and a case could name a corpus id that had quietly moved.* What survives the merge, and it is the whole of what the split was buying: the **allow-list and the licence table are code, not manifest** (`fetch.py`, `licence.py`) — a manifest can name a URL, and a URL off the allow-list or a licence off the allow-list is a refusal it cannot talk its way out of
- [ ] **`provenance.json` records what actually ran** beside what was declared — the observed Blender version and build hash against the declared one, the store's hits, misses and writes, and the world colour and strength Blender was observed to have. A **version difference is a printed notice and never a refusal**; a **scene SHA-256 mismatch is always a refusal**. *That asymmetry is the owner's no-bit-identity ruling made operational: the oracle's version is attribution, the subject's bytes are the measurement*

**The verdict is named metrics, never one blended score.**

- [ ] **Each metric carries its own threshold and its own direction, and pass is every metric within its own** — `boundary_p95_px` passes **at most** 0.1, `iou` passes **at least** its floor, `radiance_median_rel` passes at most 0.01, `coverage_fraction` passes at least its minimum. *A single blended score would be declarative and still wrong: it hides which metric failed, and a red nobody can attribute is a red nobody acts on*
- [ ] **Direction is an enumeration and never a boolean or a sign convention** (`Enum.2`) — `AtMost` · `AtLeast` — because *lower is better* encoded as a flag is the class of mistake that inverts a whole suite silently and passes every test while doing it
- [ ] The runner prints **every metric with its value, its threshold and its direction**, not only the failing one, so a case that passes narrowly is visible before it starts failing

### I.26.11 Two hundred cases, so the suite is generated rather than typed

*Owner's calibration, 2026-08-12: **"I expect hundreds of tests."** That is a design ruling, not a
length target. A suite of ~200 cases has failure modes a suite of ~30 does not, and all three of them
are structural: boilerplate that stops anyone adding case 31, an oracle cost that makes the suite
unrunnable, and empty cases that sit green. **This section is the answer to all three, and it is
deliberately not a list of 200 lines** — a hand-typed enumeration would have holes and no way to prove
it does not.*

**The count, derived rather than asserted.** Core glTF behaviours ≈ 80 · extension cases ≈ 28, one per
ratified extension we require plus its edge cases · technique cases ≈ 70, kind B of § I.26.8 at several
cases each · integration scenes ≈ 20. **≈ 200 for the renderer stage alone**, before vegetation and
buildings have their own.

- [ ] **The axes are declared and the enumeration is mechanical from them**, which is what makes 200 provable instead of typed: **feature** (the § I.26.8 matrix) × **kind** (A fetched · B authored) × **instrument** (boundary p95 · depth · radiance · frame time · by eye) × **subject class** (opaque ≥ 1 px · sub-pixel present) × **motion** (still · moving). A case is a point in that space and a hole in the enumeration is a query, not an act of memory
- [ ] **Adding a case is adding a directory, never writing a file.** Roughly 90 % of render cases ask one question — *load this glTF, place this camera, render, compare against the oracle at these thresholds* — so the C++ is **identical across all of them and only the manifest differs**. One parametrised parity test, instantiated by the harness once per test directory
- [ ] **Still one process and one real verdict per case**, which is `test/run.sh`'s existing requirement and is not relaxed: the harness enumerates the directories and runs the parity binary once per directory with the directory as its argument. What is shared is the **code**, not the process, and a crash in case 137 fails case 137
- [ ] What the render runner reads and nothing else: the `.gltf`, the resolved camera, the resolved recipe, the subject class and the resolved thresholds — from the declaration and the defaults (§ I.26.10). **It contains no scene-specific branch at all**; a case needing one is not a render case
- [ ] **A case whose question is not "does it match the oracle" is not in this suite at all** (§ I.26.9). *This line first read "it writes its own `.cpp`" — an escape hatch inside the render suite, which is exactly how a scoreless case ends up in a suite whose contract is a score. The escape is a **different suite**, not a different file.* § I.26.3's time contract is arithmetic against `n/fps` and is a **unit** test; § I.26.7's forest is a frame-time distribution and is a **scenario** test
**The oracle cache, justified against the numbers this round measured rather than against a feeling.**
*The line here first read "200 Cycles renders at 720p is **hours**". **That is contradicted by this
round's own measurement** — 200 × 2.087 s = **417 s ≈ 7.0 min** — and a claim a round measures and then
contradicts is the precise failure `doc/bugs.md` records twice already. Corrected, with every number
labelled.*

| | value | measured or derived |
|---|---|---|
| warm frame, Metal, factory cube, 720p, 128 spp | **2.087 s** | **measured** (§ I.26.4) |
| cold Metal kernel compilation, once per cache generation | **200.9 s** | **measured** |
| 200 still cases, warm, cube-cost | **417 s ≈ 7.0 min** | **derived** from the two above |
| first run of those 200 on a cold cache | **≈ 10.3 min** | **derived** |
| one case alone, cold against warm | **203 s against 2.1 s ≈ 100×** | **derived** |
| film segment, 240 frames, cube-cost | **8.4 min** | **derived** |
| pavilion, forest, scan-derived subject | **unknown multiple of the cube** | **projection — not measured**, and one timed frame is the tool |

- [ ] **The cache's honest justification is three things, and bulk stills are not among them**: the **cold-start cliff**, where one case costs 203 s instead of 2.1 s and every fresh checkout, every container and every Blender upgrade pays it · the **film**, whose frame count multiplies directly and whose cost scales with a sequence rather than a frame · and **scene cost**, which is a projection and is labelled as one — the 2.087 s subject is a six-quad cube, the cheapest that exists, and the pavilion and the forest are unmeasured
- [ ] **For the two hundred stills the cache is a convenience and the design says so.** Seven minutes warm is affordable; it is not the reason the cache exists, and inflating it into one would make the next round distrust every number beside it
- [ ] **The strongest reason is not performance at all: a cached oracle keyed by recipe and release cannot change underneath a comparison.** Without it, a reference re-rendered per run can move for a reason nobody chose — a driver, a sampler, a point release — and every difference becomes unattributable. *That is a correctness argument, and it would justify the cache at zero render cost*
- [ ] Hash-keyed in the content store § I.22 already has, computed once, invalidated only when the recipe or the Blender release changes. **That is why the release is in the key** and not merely on the row
- [ ] **The tier split, stated with what the fast tier actually costs rather than with an assertion that it cannot afford one.** With a warm cache the fast tier costs **2.087 s per oracle it would have to render, derived** — real but survivable, so *cannot afford it* was the wrong reason. The right one is that **the fast tier must not invoke Blender at all**: an external binary on the fast path is an unbounded dependency whose cost is a **100× cliff** the moment the kernel cache is cold, and a tier whose runtime depends on whether somebody upgraded Blender is not a fast tier. So a fast-tier cache miss is a **named refusal**, never a silent skip and never a render nobody asked for; the `device` tier renders and populates
- [ ] **A cold cache is a declared, separately-invoked run** with its cost published — `N × t_frame` plus the **200.9 s measured** Metal kernel compilation once (`doc/bugs.md`). For 200 cube-cost stills that is **≈ 10.3 min derived**; for the film and the complex scenes it is a projection until one frame of each is timed, and the run prints which of its numbers are which
- [ ] **What stops a generated suite going hollow is the empty-image guard and not a count of declared numbers** (§ I.26.10): under defaults, declaring nothing *is* a complete specification, so "a directory with no acceptance numbers is a refusal" would refuse every correct minimal case. The guard that actually holds is **zero coverage on either side is a failure, an absent or failed oracle is a failure, and the trailer distinguishes agreed from nothing-to-compare**
- [ ] **The suite publishes its own coverage of the matrix as a count** — features with a case, features without, cases with each instrument — so *"every feature KCD or GTA 5 uses has a basic case"* is a number somebody can read rather than a claim somebody made. Without it, 200 directories are 200 opportunities for a feature to have quietly no case at all

---

## Band II — World

### II.1 Elevation and terrain

- [x] DEM tile fetch, decode and stitch
- [x] Terrain mesh per quadtree node, LOD by screen-space error
- [x] Height at a point on the CPU with no device present
- [x] The height oracle answers on the *drawn* surface, so physics and picture cannot disagree
- [x] `GroundSample` as a tri-state return type — Resolved, Pending, Hole — so a caller cannot place on a sentinel
- [ ] Slope and aspect published as first-class ground quantities everywhere they are used
- [ ] Curvature, for a convex ridge to read differently from a hollow
- [ ] Vertical accuracy of the source stated per place — the chain is faithful; Badwater is 10.9 m off on flat ground and that is the DEM's error
- [ ] Hydro-flattening: a lake polygon carved to a constant elevation at or just below the surrounding terrain
- [ ] A river polygon carrying a monotone downstream gradient, as the engine already enforces for water lines
- [ ] Terrain carved under a road so the carriageway does not ride a raw DEM ripple
- [ ] Terrain carved for a building pad, so a house does not float or bury
- [ ] Cliff and overhang — a heightfield cannot carry one; a declared vertical face is the substitute
- [ ] Cave and tunnel volume — REFUSED as terrain, owed to a declared mesh volume instead
- [ ] Erosion as a function over the DEM — Ebert/Musgrave et al. ch. on terrain; the reference paints this by hand and we cannot

### II.2 Classification

- [x] Class grid from OSM vectors, arbitrated in a declared order (`world/ClassBuilder`)
- [x] Edge distance to the nearest boundary of the winning class
- [x] Runner-up class at a point, so a boundary knows what it blends towards
- [x] Class as a state, not a default: `no row` where OSM has no datum (`generators/Cover`)
- [x] Unmapped substrate that is drawn and grows nothing — the retired global `meadow` default is now unspellable
- [x] Twelve declared land templates plus the unmapped substrate row
- [x] Way width per street kind, 1.5 m path to 45 m
- [x] One predicate, two evaluators: the edge test a fragment runs is the edge test a CPU query runs
- [x] Three tiers over the vectors: AABB on the CPU, source polygon on the CPU, refinement on the GPU one-way
- [ ] Runner-up and edge distance consumed for a height-driven layer blend — available, nothing reads them for this
- [ ] Per-place default where OSM is silent, which needs a climate model this engine does not have
- [ ] OSM layer names spelled once rather than in three files

### II.3 Ground materials and surface

- [x] Seventeen ground materials with linear albedo whose chromaticity is sourced (Munsell renotation, ECOSTRESS spectra) and whose luminance is locked to a broadband value
- [x] Roughness, specular scale, grain size, height amplitude, coarse and fine detail scale per material
- [x] Litter class per material, overridable per template — beech litter under spruce is a defect the botanist calls
- [x] Litter coverage, contrast, edge reach, constructed-edge flag
- [x] Slope maximum per material, so a class cannot sit on a wall
- [x] Sward closure folding the grass colour into the terrain albedo beyond the blade fade
- [x] Alpine limit: a rock template selected by slope band and elevation
- [ ] High-frequency detail as a noise function, explicitly greyscale, cut at a declared range — the reference's rule, and the only legal form a detail map takes here
- [ ] Height-driven blend between classes so pebbles poke through dirt instead of cross-dissolving
- [ ] Class-boundary mixing width measured in pixels at the comparison rung
- [ ] Near-ground luminance variance off the floor
- [ ] Wetness as a material state — darkening, specular rise, puddles in depressions
- [ ] Snow cover as a material state with a slope and aspect mask
- [ ] Frost, ice, mud, ruts, trampled paths
- [ ] Tracks and desire lines where things walk repeatedly
- [ ] White limestone and rock patches reading as snow at 36 N in August — a tonal defect in the existing table
- [ ] Deferred decals for local dressing — REFUSED in the reference's form (authored textures); the procedural substitute is a material row plus a noise function

### II.4 Water

- [x] Water polygons with a level per ring (`world/WaterField`)
- [x] Water surface tessellated at level + 0.15 m over a declared 24 B layout
- [x] Water lines with a monotone downstream gradient
- [x] Water depth at a point as a type that cannot be negative (`core/WaterDepth.h`)
- [x] Depth derived analytically from water level minus ground height — no blended fragment, no separate pass
- [ ] Level model that does not put nine of nine outlines under their own ground — the fifth percentile of a ring under 22 points *is* the minimum
- [ ] Body colour by depth with a declared extinction per wavelength
- [ ] Surface normal perturbation from a wind-driven wave function
- [ ] Fresnel reflection of the sky LUT
- [ ] Reflection of the shore — the reference uses a screen-space term; UNSURE which
- [ ] Refraction of the bed at shallow depth
- [ ] Caustics — the reference ships water-volume caustics from an authored texture; NO SUBSTITUTE is false here, a function reaches it, but nothing is built
- [ ] Foam at a shore line, driven by depth and slope
- [ ] Foam and turbulence at a weir or a rapid
- [ ] Flow direction and speed on a watercourse, visible in the surface
- [ ] Waterfall — the reference's river tool cannot make one either, and says so
- [ ] Shoreline wetting band, darker than the dry bank
- [ ] Floating debris, leaves, ice
- [ ] Ocean with a swell spectrum — out of scope for the acceptance place, named so it is not an oversight
- [ ] Boats displacing water and leaving a wake (band V depends on this)
- [ ] Rain rings on a still surface
- [ ] Underwater view: extinction, god rays, surface from below

### II.5 Atmosphere and sky

- [x] Bruneton transmittance LUT (`TransmittanceStage`)
- [x] Multiple-scattering LUT (`MultiScatterStage`)
- [x] Sky-view LUT (`SkyViewStage`)
- [x] Sky draw from the LUTs (`SkyStage`)
- [x] Irradiance readback that is the scale for everything lit (`IrradianceStage`)
- [x] Aerial perspective / haze along the view ray, Koschmieder-derived (`AtmoHaze.h`)
- [x] Sun disc with limb (`SunStage`)
- [x] Moon as a lit sphere with a phase, over the NASA LROC albedo — measured data that is a raster by nature, principle 2 admissible
- [x] Stars at true altitude and azimuth from the HYG catalogue, magnitude-sorted, with B−V colour
- [ ] Star magnitudes that do not clip at the display white — `maxY ≈ 1.0` on every night frame
- [ ] Airglow and zodiacal light
- [ ] Milky Way band — needs a source that is measured raster rather than authored; UNSURE whether HYG suffices
- [ ] Moon glow and its halo around the disc
- [ ] Horizon lift at night
- [ ] Mesopic response, so a night is not a dark day
- [ ] Ozone absorption band separated in the model — UNSURE whether the current Bruneton parameterisation carries it
- [ ] Rainbow, halo, sun dog — the reference has none of these either
- [ ] Crepuscular rays through a cloud break
- [ ] Volumetric fog with shadowing (`e_VolumetricFog` + `r_FogShadows` is the reference's; ours must fit inside a stage that already reads the HDR target or it does not get built)
- [ ] Ground fog in a valley at dawn, driven by the terrain's own hollows
- [ ] Fog volumes as declared local shapes — the reference's boxes and ellipsoids; ours would be a function of place instead

### II.6 Clouds

- [x] Cloud density as one function with two evaluators, C++ and a WGSL transliteration whose constants are emitted from the same place
- [x] Per-deck separable model: wind-advected 2-D coverage FBM × an analytic vertical profile − 3-D erosion
- [ ] Anything that draws a cloud — `Renderer::CreateClouds()` is an empty function and there is no cloud stage
- [ ] Cloud shadow on the ground
- [ ] Cloud lighting: forward scattering, silver lining, powder term
- [ ] Cloud base from the weather ceiling rather than a constant
- [ ] Three decks — low, mid, high — driven by the four GFS cover diagnostics that the provider already carries
- [ ] Cirrus fibres sheared along the wind (the constants exist; nothing draws them)
- [ ] Contrails
- [ ] Storm cell with anvil
- [ ] Cloud advection consistent with the declared wind, so a shadow moves at the right speed

### II.7 Weather

- [x] Weather provider as an injected seam, with a data-local default and a live implementation
- [x] Wind as the air mass's own NED velocity at altitude, interpolated over pressure levels
- [x] Cloud cover per deck plus a ceiling that can legitimately be absent
- [x] Visibility, with an "unlimited" value outside the format's own window
- [ ] Precipitation: rain intensity, snow, sleet, hail
- [ ] Rain as particles or as a screen-space function — the reference uses particles; ours is undecided
- [ ] Wet-surface response coupled to precipitation history rather than to the current rate
- [ ] Puddles filling and drying
- [ ] Wind gusts as a time series rather than a constant
- [ ] Temperature and humidity fields, because snow line and fog need them
- [ ] Lightning as a light source
- [ ] Weather state blending over a declared interval, reproducibly
- [ ] Weather preset picked every four hours — REFUSED in that form: keying the sky's radiance would make us less physical than we already are. Only the tone shoulder, the fog lobe and the transition length are keyable

### II.8 Light, shadow, occlusion

- [x] Sun as the directional source, its radiance from the atmosphere model
- [x] Sky as an area source through the irradiance LUT
- [x] Four cascaded shadow maps
- [x] Screen-space ambient occlusion at a 0.9 m radius, half resolution
- [x] One lighting model spliced into every lit surface (`SurfaceLight.h`), so a second one cannot appear
- [x] Auto exposure from measured irradiance, with gain and white point read by the tone chain
- [x] ACES-Narkowicz tone curve with no free parameter
- [x] Temporal antialiasing with a Halton(2,3) jitter
- [ ] Nothing in the frame occludes between 1 m and 20 m — the whole of a tree. Cascade 3 is 1.2 m per texel and SSAO reaches 1 m
- [ ] Coarse world-space sky visibility over the cluster DAG's own bounds, per vertex — the cheap candidate, no new pass
- [ ] Voxel cone tracing in the AO pass's existing slot, only if the cheap candidate demonstrably cannot produce the term
- [ ] Sky visibility at 1.5 m under a closed canopy inside the band an LAI of 4.5–5.1 implies
- [ ] Ambient specular in an enclosed place — NO SUBSTITUTE: under a canopy, in a gorge, indoors, the reference hand-places a baked probe, which is measured appearance of an authored scene and principle 2 forbids it. In the open the sky LUT is the correct substitute and is better founded
- [ ] Baked environment probes — REFUSED, principle 2
- [ ] Baked lightmaps — REFUSED, same
- [ ] **The two micro-relief bounce terms move out of the lighting model and into the material row.** `render/stages/SurfaceLight.h:33 kGroundBounce = 0.12` and `:40 kSelfShelter = 0.35` are *correctly* documented as the mean reflectance of Central European land cover and as the sky fraction a clod or a sward hides from a point between them — both are statements about **a surface**, and both are currently engine constants spliced into every lit surface, water and glass included (`doc/bugs.md`). They are two scalars, they switch no pipeline state, and the material row is defined as exactly that (`CLAUDE.md`, *the core dictates the pipeline*). Ground keeps 0.12/0.35 and a manufactured surface declares 0 — at which point a Lambertian configuration becomes **spellable**, which is what rung 3 of § I.26 needs and what nothing in this tree can express today
- [ ] Point and spot lights as a list the core lights from — **and § I.26 scene 8 is what first requires it**: Blender's factory key light is a 1000 W point at 7.244 m, so matching the literal default lighting is this line, not new scope invented for a test
- [ ] Emissive surfaces contributing to that list — `Material` has the field and `SurfaceState::Emits()` derives from it; nothing emits
- [ ] Shadow from a point light
- [ ] Contact hardening on a shadow
- [ ] Shadow proxy: a cheap single-material representation per caster — free, because the LOD ladder already produces one
- [ ] CPU coverage-buffer occlusion culling with authored occluder meshes — REFUSED: authored *and* CPU-bound, the wrong direction on wasm32
- [ ] GPU occlusion culling against the depth of the previous frame
- [ ] Vegetation tinted toward the ground class colour with range — the single mechanism that makes a distant foliage field read as one mass; the reference ships it at 50…80 m

### II.9 Night

- [ ] It is not a night today: ground lit by a constant display crutch in `SurfaceLight.h`, `skyRGB = 0,0,0`, trunks bright grey, road legible, sky pure black
- [ ] Moon as a light source with a phase-dependent illuminance
- [ ] Moon shadow
- [ ] Night sky radiance that is not a constant
- [ ] Street lamp emission on placed geometry — no new pass needed
- [ ] Window light with a plausible duty cycle per building
- [ ] Vehicle lights (band V)
- [ ] Skyglow on a cloud base over a settlement
- [ ] Aviation warning lights on masts and turbines

### II.10 Season, and what changes with it

- [ ] Day-of-year reaching anything at all — no `season` in the tree
- [ ] Leaf-on / leaf-off state per species with its own phenology
- [ ] Autumn colour per species, with the sequence right (ash early and dull, beech copper, larch late gold)
- [ ] Leaf fall and a litter layer that thickens
- [ ] Bare-crown silhouette with branch structure legible — the crown geometry already exists, so this is cheap
- [ ] Spring flush with a lighter, yellower leaf
- [ ] Snow lying on branches, roofs and the ground with a slope mask
- [ ] Crop calendar: sown, green, eared, ripe, harvested, stubble, ploughed
- [ ] Meadow cut state: standing, mown, windrowed, baled, regrown
- [ ] Grass senescence — the dry fraction exists per template and is not driven by a season
- [ ] Water level and flow varying by season
- [ ] Ice on a pond

---

## Band III — Vegetation

*The reference is a vegetation picture: canopy plus undergrowth plus grass, superposed from one
declared preset, plus mushrooms and herbs. Ours is one stem class and a stands-per-m².*

**Form before species, and the split is the band's whole argument.** A **growth form** is a shape the
generator must be able to make at all; a **species** is a declaration carried by a form. A species line
is cheap once its form exists and impossible before it, so forms stand first and every species section
below names the form it rides. Where a species needs a form nothing else uses, the line says so — that
is the expensive kind.

**Measured state:** 31 species files exist across seven growth forms — single-stem tree, multi-stem
shrub, bush, hedge, snag, stump and fallen log — and the grower takes the form as an input
(`generators/GrowthForm.h`). What no form yet has is a **cut response**, a severed shoot answering with
several, which is why the hedge reads as a row of saplings and why coppice stool, pollard and
stump-with-resprouts are all still blocked on one mechanism.

### III.1 Placement machinery

- [x] Stands per square metre per ground class (`trees.perM2`, twelve templates)
- [x] Species mix per class as declared weights (seven species in mixed broadleaf, four in conifer)
- [x] One cell of the region's own lattice proposes one stand, so a border is exact and no stand can double
- [x] Every refusal has a name, so the counts partition the region and a missing tree is attributable
- [x] Height drawn per stand with the species' own sigma, triangular
- [x] Yaw per stand, uniform over the circle
- [x] Alpine limit refusing a stand above the rock band
- [ ] Placement per **stratum** rather than one scatter — canopy, understorey, field layer, ground layer
- [ ] Strata declared per class with no global default, so an unclassified place grows nothing
- [ ] Slope and aspect biasing the mix — a north-facing scree does not carry a beech stand
- [ ] Soil moisture proxy from distance to water, so alder and willow sit where they belong
- [ ] Clumping rather than a Poisson scatter — a natural stand is patchy and a uniform scatter reads as a plantation
- [ ] Gap dynamics: a clearing, a windthrow patch, a young cohort
- [ ] Age structure within a stand rather than one draw per stand
- [ ] Forest edge: a shrub mantle and a herb seam, denser and lower than the interior
- [ ] Hedgerow placement along a field boundary or a way
- [ ] Avenue placement along a street centreline
- [ ] Riparian gallery along a watercourse — the `riverbank` template exists and no OSM rule selects it
- [ ] The `conifer_forest` template exists and no OSM rule selects it — the served schema has one `forest` kind and cannot tell the two apart. TILE, or an inference from elevation and region
- [ ] Vegetation cleared under a power line right-of-way
- [ ] Vegetation refused on a road, a rail bed and a building footprint — held by the class grid, unverified against the drawn geometry
- [ ] Mown state inside a park, a garden and a cemetery
- [ ] Ground-cover stratum at all — the near-field ground is a shader and nothing else
- [ ] Clutter density per class is declared (`clutter.perM2`, 0.01…1.2) and **nothing reads it**

### III.2 Growth forms — what the generator must be able to shape

*One line per form. `[x]` means the grower can produce that shape today, not that a species using it is
declared.*

- [x] Single-stem tree — one leader, a clear bole, a crown above it
- [ ] `habit` in a species file is a **prose sentence for a human** and nothing reads it — the grower works from numbers, so a form written there cannot reach the geometry. Either it becomes parameters or it goes
- [ ] Crown shape as a declared envelope — conical, columnar, ovoid, domed, vase, weeping, umbrella, flat-topped. The prose already distinguishes eight and the numbers distinguish none
- [ ] **A shoot stops at the ground.** Every form's geometry is bounded below by the base plane to within one small declared dip — a branch tip lies *in* the sward, not under it — and the tip that would hang further is clamped there, the way it is already bent back at the crown envelope. `TreeGrower.cpp:598-606` puts y = 0 at the **trunk foot** deliberately and correctly, and then constrains nothing below it: measured 2026-08-12 over all 31 declarations, bark only, as a fraction of the tree's own height, willow **−0.933** (16.8 m of crown under the planting point at `height_m: 18`, 22.5 % of its bark vertices), dog_rose −0.542, blackthorn −0.229, hedge_hornbeam −0.117, hawthorn −0.112, hedge_privet −0.082, guelder_rose −0.064. The dip is a sourced number and not a free consequence of shoot length: *Salix* × *sepulcralis*'s branchlets tumble **to touch** the ground and *Rosa canina*'s arching stems climb **up** through their neighbours to 1–5 m (RHS), and neither grows downward. What it costs today is in `doc/bugs.md` — geometry shaded every frame that cannot be seen on the flat, and a crown hanging out of the bank face where willows are actually placed
- [ ] Multi-stem tree — several leaders from one base, common in ash, lime and maple on an edge
- [ ] Multi-stem shrub — no bole, stems from the ground, crown to the ground
- [ ] Bush — a low rounded multi-stem form under about 2 m
- [ ] Dwarf shrub — woody, under 0.5 m, the heath and bilberry form
- [ ] Thicket — a colony spreading by suckers, with no individual outline (blackthorn, bramble)
- [ ] Hedge — a managed linear form with a cut section, not a row of shrubs
- [ ] Coppice stool — many stems of one age from a cut base
- [ ] Pollard — a bolling with a rod crown at head height
- [ ] Trained orchard form — central leader, spindle, bush, on a post and wire
- [ ] Espalier and cordon — a plane trained against a wall or a wire
- [ ] Vine on a trellis — a stock, a cordon and annual canes
- [ ] Climber and liana — a form that needs a **host** to grow on, which nothing in the contract supplies
- [ ] Creeping and mat-forming — a form with no vertical axis at all
- [ ] Rosette — a basal leaf whorl with a bare flowering stem, and it is the commonest meadow herb shape
- [ ] Erect leafy forb — a stem with leaves along it and a terminal inflorescence
- [ ] Umbel — the tall flat-topped form of hogweed and cow parsley, and it is a silhouette on its own
- [ ] Tussock — a dense basal clump with arching leaves, the grass form that reads at distance
- [ ] Turf-forming graminoid — rhizomatous, no clump, the closed sward
- [ ] Cane and reed — an unbranched vertical stem in a dense stand
- [ ] Bulb and geophyte — a short-lived spring form, which forces a phenology the engine has no clock for
- [ ] Fern crown — a shuttlecock of pinnate fronds, a form nothing else uses
- [ ] Frond mat — bracken, a continuous stand rather than individuals
- [ ] Cushion — alpine, a compact hemispherical mass
- [ ] Moss carpet and turf — a surface form, closer to a material than to a plant
- [ ] Lichen crust and foliose thallus — a surface form on bark, stone and roof
- [ ] Floating-leaf aquatic — leaves on the water plane, a form nothing else uses and one that needs the water surface as its datum
- [ ] Submerged aquatic — a form that streams with a current
- [ ] Emergent aquatic — rooted in the bed, standing above the surface
- [ ] Fungal fruiting body — cap and stipe, and the bracket form beside it
- [ ] Sapling and juvenile stage of every woody form above — a young beech is not a small beech
- [ ] Standing dead trunk — a form with no foliage and a decaying outline
- [ ] Snag — a broken top
- [ ] Stump — cut or broken, with and without resprouts
- [ ] Fallen log — a horizontal woody body
- [ ] Root plate — a windthrow's vertical disc of roots and soil
- [ ] Crop row form — a field of one form with a row rhythm, not a scatter of individuals

### III.3 Representation and level of detail

- [x] Procedural growth: trunk, taper, minimum radius, twig radius, branch chance, branch angle and its variance, order length and radius, wander, leader bias, branch up-bias, whorl count and spacing, terminal fork, shade prune — `conical bias`, `bare steps` and `crown base` were three declarations of one thing and are replaced by the crown envelope
- [x] Trunk sides as a declared polygon count
- [x] Bark colour, darkening, frequency, ridge and style
- [x] Leaf kinds: broad, needle, palmate, pinnate, palmate compound
- [x] Leaf blade: segments, length, width, widest point, base fill, base skew, tip, lobes, lobe depth, serration, fold, curve, leaflets, palmate lobes and spread
- [x] Needles: width, length, forward rake, droop
- [x] Leaf cards per point and a card budget per prototype
- [x] Leaf angle distribution as a declared population
- [x] Four mesh LOD levels plus an impostor rung, one ladder, model-space error as a fraction of height
- [x] Instanced sheets standing for sixteen quad elements each
- [x] Octahedral impostor atlas baked at runtime from our own grown prototype — the cache of a computable function, principle 2 admissible
- [x] Every declared species measured by a bench (`make treebench`, `test/clients/TreeBench.cpp` enumerates the directory rather than listing names, so a form nobody grew cannot look green)
- [ ] A grower that takes a **form** as an input rather than assuming one
- [ ] Impostor cells that are never sampled without a bake — counted, not assumed
- [ ] A far rank that is one plane per stand, merged per fixed spatial cell into a single draw, corners expanded in the vertex shader so each element faces the camera individually — the reference's UBERLOD, minus its offline bake and minus its single view, because we have a bird's eye
- [ ] Crowns that are not bow-ties: the cross must never survive to the range where its own geometry is legible
- [ ] Stands that do not vanish seen from directly above — 15 995 of them do
- [ ] Crown self-shadowing, so a crown reads as one mass with a lit top and a shadowed underside
- [ ] Leaf albedo at the top comparison rung — NO SUBSTITUTE for now: theirs is an authored alpha and colour; ours is geometry plus a colour, which is enough at 320×180 and unsolved where venation and translucency variation speak
- [ ] Two-sided transmission through a leaf, driven by the material declaration rather than a per-leaf shader
- [ ] Bark normal detail at the near rung, as a function
- [ ] Root flare, so a trunk meets the ground instead of intersecting it
- [ ] Buttress roots on a mature beech
- [ ] Lean and sweep, so a stand is not a set of verticals
- [ ] Damage forms: broken leader, forked stem, lightning scar, browsing line
- [ ] Epiphytes on a host: ivy on the trunk, moss on the north side, lichen on the bark
- [ ] Merged-mesh treatment for a dense low stratum, batched into fixed cells that LOD by removing items — the reference's answer for grass fields
- [ ] Flower and fruit as declared elements — rape's yellow, a cherry in blossom and a rowan's berries are all colour at distance

### III.4 Wind and interaction

- [x] Declared wind field: log profile to canopy top, honami wave at the stand's eigenfrequency, phase speed at canopy-top wind, local speed at a place and time
- [x] Per-species wind amplitude and frequency
- [ ] Response as the closed solution of the plant's own bending equation, driven rather than animated — the field publishes the Cauchy number and nothing consumes it for a tree
- [ ] Detail bending on foliage, distinct from trunk sway
- [ ] Touch bending: a body walking through a bush displaces it — the reference has this for bushes, ferns and trees
- [ ] Breeze generation: local gust sources rather than one global vector
- [ ] Gust visible as a wave crossing a field, not as a phase everywhere at once

### III.5 Species on the single-stem tree form — broadleaf

*The form the grower started with. The declared botanical names are what the files actually say, and
three of them are not what a reader would assume.*

- [x] Fagus sylvatica — common beech
- [x] Quercus robur — pedunculate oak
- [x] Carpinus betulus — hornbeam
- [x] Fraxinus excelsior — ash
- [x] Acer pseudoplatanus — sycamore maple
- [x] Tilia cordata — small-leaved lime
- [x] Betula pendula — silver birch
- [x] Ulmus minor — field elm, and not the wych elm a "elm" in a beech forest would be
- [x] Populus nigra 'Italica' — Lombardy poplar, a **columnar cultivar**, not a floodplain black poplar
- [x] Salix × sepulcralis — weeping willow, a **garden hybrid**; the `riverbank` template's declared 50 % willow is therefore an ornamental where a floodplain wants *Salix alba*
- [x] Sorbus aucuparia — rowan
- [x] Aesculus hippocastanum — horse chestnut
- [ ] Quercus petraea — sessile oak
- [ ] Acer platanoides — Norway maple
- [ ] Acer campestre — field maple
- [ ] Tilia platyphyllos — large-leaved lime
- [ ] Betula pubescens — downy birch
- [ ] Alnus glutinosa — black alder, the floodplain's own tree and absent
- [ ] Alnus incana — grey alder
- [ ] Populus tremula — aspen, whose leaf tremor is its recognisable property
- [ ] Populus nigra — black poplar, the species rather than the cultivar
- [ ] Populus alba — white poplar, with its two-tone leaf
- [ ] Salix alba — white willow
- [ ] Salix fragilis — crack willow
- [ ] Ulmus glabra — wych elm
- [ ] Ulmus laevis — European white elm
- [ ] Prunus avium — wild cherry
- [ ] Prunus padus — bird cherry
- [ ] Malus sylvestris — crab apple
- [ ] Pyrus pyraster — wild pear
- [ ] Sorbus aria — whitebeam
- [ ] Sorbus torminalis — wild service tree
- [ ] Sorbus domestica — service tree
- [ ] Juglans regia — walnut
- [ ] Castanea sativa — sweet chestnut
- [ ] Robinia pseudoacacia — black locust, naturalised and common on poor ground
- [ ] Platanus × hispanica — London plane, the urban street tree with its flaking bark
- [ ] Ailanthus altissima — tree of heaven, the urban invader
- [ ] Quercus rubra — red oak, planted
- [ ] Corylus colurna — Turkish hazel, a modern street tree
- [ ] Gleditsia triacanthos — honey locust, a modern street tree
- [ ] Fraxinus ornus — manna ash; southern, named as out of the acceptance region

### III.6 Species on the single-stem tree form — conifers

- [x] Picea abies — Norway spruce
- [x] Abies alba — silver fir
- [x] Pinus sylvestris — Scots pine
- [x] Taxus baccata — yew, and it is declared here although its natural habit is multi-stemmed and often shrubby
- [ ] Larix decidua — European larch, the only deciduous conifer here, and it needs the seasonal state band II.10 owes
- [ ] Pinus nigra — black pine
- [ ] Pseudotsuga menziesii — Douglas fir, planted and now common
- [ ] Picea pungens — blue spruce, garden
- [ ] Pinus cembra — Swiss stone pine; montane
- [ ] Pinus mugo — dwarf mountain pine — needs the **krummholz** form, which is not the single-stem tree
- [ ] Juniperus communis — juniper; the tree form here, the shrub form in III.8
- [ ] Thuja / Chamaecyparis — garden conifers, whose real use is the hedge form
- [ ] Plantation stand: even-aged, even-spaced, no understorey — a placement form rather than a species, and OSM does not distinguish it

### III.7 Species needing the multi-stem shrub, bush and thicket forms

*The forms exist; what these lines wait on is the **cut response** — a severed shoot answering with several — which no form has yet.*

- [ ] Corylus avellana — hazel, multi-stem
- [ ] Crataegus monogyna — hawthorn
- [ ] Crataegus laevigata — midland hawthorn
- [ ] Prunus spinosa — blackthorn — needs the **thicket** form; an individual outline is wrong for it
- [ ] Sambucus nigra — elder
- [ ] Sambucus racemosa — red elder
- [ ] Viburnum opulus — guelder rose
- [ ] Viburnum lantana — wayfaring tree
- [ ] Cornus sanguinea — dogwood, with red winter stems
- [ ] Cornus mas — cornelian cherry
- [ ] Euonymus europaeus — spindle
- [ ] Ligustrum vulgare — wild privet
- [ ] Rhamnus cathartica — buckthorn
- [ ] Frangula alnus — alder buckthorn
- [ ] Rosa canina — dog rose — an arching cane form between shrub and climber
- [ ] Rubus fruticosus agg. — bramble — needs the **thicket** form and it is the commonest thing on a forest edge
- [ ] Rubus idaeus — raspberry, a cane thicket
- [ ] Ribes rubrum / uva-crispa — currant, gooseberry
- [ ] Lonicera xylosteum — fly honeysuckle
- [ ] Salix cinerea — grey willow, wet
- [ ] Salix viminalis — osier, and its real form is the pollard
- [ ] Cytisus scoparius — broom, a broom-like stem bundle with almost no leaf
- [ ] Ulex europaeus — gorse; western and atlantic, named as marginal here
- [ ] Genista tinctoria — dyer's greenweed
- [ ] Hippophae rhamnoides — sea buckthorn, coastal and gravel
- [ ] Buxus sempervirens — box, garden and churchyard
- [ ] Berberis — barberry, garden
- [ ] Forsythia, Syringa, Philadelphus, Hydrangea — the suburban garden set, and their flowering mass is the point
- [ ] Rhododendron — garden and, as *R. ferrugineum*, alpine

### III.8 Species needing the dwarf shrub form

- [ ] Calluna vulgaris — heather, the heath's defining form and a continuous mass rather than individuals
- [ ] Erica tetralix — cross-leaved heath, bog
- [ ] Vaccinium myrtillus — bilberry, the acid forest floor
- [ ] Vaccinium vitis-idaea — cowberry
- [ ] Juniperus communis in its prostrate montane form
- [ ] Empetrum nigrum — crowberry
- [ ] Thymus and Helianthemum — the dry-slope mats, which double as the creeping form

### III.9 Species needing the hedge, coppice and pollard forms

- [ ] Managed field hedge with a flat-topped section — the **hedge** form, not a row of shrubs
- [ ] Species-rich hedgerow with standards left to grow through it
- [ ] Garden hedge: privet, beech, hornbeam, conifer
- [ ] Windbreak row
- [ ] Coppice stool with multiple stems of one age — hazel, hornbeam, sweet chestnut
- [ ] Pollard with a bolling and a rod crown — the willow along a Weser ditch, and it is the region's signature form
- [ ] Laid and staked hedge
- [ ] Clipped topiary and a hedge cut to a rectangle — the same form with a declared cut section
- [ ] Woodland mantle: the graded height profile from the field to the canopy
- [ ] Herb seam at the mantle's foot
- [ ] Field margin strip, uncultivated
- [ ] Ruderal strip along a road or a rail line

### III.10 Species needing the climber form

*A climber needs a **host**, and the generator contract has no way to say "this grows on that". That is
a contract change, not a species.*

- [ ] Hedera helix — ivy, climbing on a trunk and a wall, and creeping as a ground carpet
- [ ] Clematis vitalba — old man's beard, and it drapes a whole hedge
- [ ] Humulus lupulus — hop, wild on a riverbank and trained in a hop garden
- [ ] Lonicera periclymenum — honeysuckle
- [ ] Vitis vinifera — grape on a trellis, the **vine** form
- [ ] Parthenocissus — Virginia creeper on a wall, and its autumn red is a façade's colour
- [ ] Wisteria, Rosa (climbing) — garden
- [ ] Convolvulus / Calystegia — bindweed on a fence

### III.11 Species needing the rosette, erect forb and umbel forms — forest floor

- [ ] Anemone nemorosa — wood anemone, a spring carpet, which means phenology as well as a form
- [ ] Galium odoratum — sweet woodruff, the association's name-bearer
- [ ] Mercurialis perennis — dog's mercury, in dense masses
- [ ] Allium ursinum — ramsons, a carpet and a **geophyte**
- [ ] Oxalis acetosella — wood sorrel, creeping
- [ ] Hepatica nobilis — liverleaf
- [ ] Corydalis cava — hollowroot, geophyte
- [ ] Ficaria verna — lesser celandine, geophyte
- [ ] Arum maculatum — lords-and-ladies
- [ ] Lamium galeobdolon — yellow archangel
- [ ] Asarum europaeum — asarabacca, creeping
- [ ] Paris quadrifolia — herb Paris, a whorl form nothing else uses
- [ ] Convallaria majalis — lily of the valley
- [ ] Polygonatum multiflorum — Solomon's seal, an arching stem form
- [ ] Maianthemum bifolium — may lily
- [ ] Circaea lutetiana — enchanter's nightshade
- [ ] Stachys sylvatica — hedge woundwort
- [ ] Impatiens noli-tangere and I. parviflora — balsams
- [ ] Urtica dioica — nettle, on nutrient-rich ground and in dense stands
- [ ] Aegopodium podagraria — ground elder, a carpet
- [ ] Geum urbanum — wood avens
- [ ] Vinca minor — periwinkle, naturalised near settlement

### III.12 Species needing the rosette, erect forb and umbel forms — meadow, pasture, ruderal

- [ ] Leucanthemum vulgare — oxeye daisy
- [ ] Achillea millefolium — yarrow
- [ ] Trifolium pratense — red clover
- [ ] Trifolium repens — white clover, creeping
- [ ] Lotus corniculatus — bird's-foot trefoil
- [ ] Ranunculus acris — meadow buttercup
- [ ] Taraxacum officinale agg. — dandelion, in flower and in seed head, a rosette
- [ ] Plantago lanceolata — ribwort plantain, a rosette
- [ ] Plantago major — greater plantain, on a trodden edge
- [ ] Rumex acetosa — common sorrel, and its red flowering haze is a meadow's colour in June
- [ ] Rumex obtusifolius — broad-leaved dock
- [ ] Knautia arvensis — field scabious
- [ ] Centaurea jacea — brown knapweed
- [ ] Campanula patula — spreading bellflower
- [ ] Campanula rotundifolia — harebell
- [ ] Salvia pratensis — meadow clary
- [ ] Geranium pratense — meadow crane's-bill
- [ ] Heracleum sphondylium — hogweed, the **umbel** form
- [ ] Anthriscus sylvestris — cow parsley, the May roadside, umbel
- [ ] Daucus carota — wild carrot, umbel
- [ ] Pastinaca sativa — wild parsnip, umbel
- [ ] Hypericum perforatum — St John's wort
- [ ] Vicia cracca — tufted vetch, a scrambler
- [ ] Medicago lupulina — black medick
- [ ] Primula veris — cowslip
- [ ] Bellis perennis — daisy, on a lawn
- [ ] Veronica chamaedrys — germander speedwell
- [ ] Ajuga reptans — bugle, creeping
- [ ] Colchicum autumnale — autumn crocus, geophyte, and it is an autumn meadow's only flower
- [ ] Narcissus pseudonarcissus — wild daffodil, geophyte
- [ ] Cardamine pratensis — cuckoo flower, damp meadow
- [ ] Silene flos-cuculi — ragged robin, damp meadow
- [ ] Caltha palustris — marsh marigold
- [ ] Filipendula ulmaria — meadowsweet
- [ ] Lythrum salicaria — purple loosestrife, bank
- [ ] Cirsium arvense — creeping thistle
- [ ] Cirsium vulgare — spear thistle
- [ ] Jacobaea vulgaris — ragwort
- [ ] Solidago canadensis — Canadian goldenrod, the invader on every fallow strip
- [ ] Verbascum — mullein, a tall spike on rubble
- [ ] Echium vulgare — viper's bugloss, on gravel
- [ ] Origanum vulgare — marjoram, on a dry slope
- [ ] Thymus pulegioides — thyme, a mat
- [ ] Papaver rhoeas — corn poppy, arable weed
- [ ] Centaurea cyanus — cornflower, arable weed
- [ ] Matricaria chamomilla — scented mayweed, arable weed
- [ ] Chenopodium album, Amaranthus — the stubble weeds
- [ ] Artemisia vulgaris — mugwort, ruderal
- [ ] Tanacetum vulgare — tansy, ruderal
- [ ] Reynoutria japonica — Japanese knotweed, riparian invader, a cane thicket

### III.13 Species needing the tussock and turf graminoid forms

- [x] One aggregate blade class (`graminoid`), green and dry, as a ground-shader term with **no geometry**
- [x] Blades per square metre per class, 0 to 1165
- [x] Sward height, height jitter, blade width, dry fraction
- [x] Leaf area index derived from blades/m² × width × the population mean of the tangent's vertical component
- [x] Canopy top as a three-scale ladder — stand, patch, tussock
- [x] Senescence from the tip down, with a whole-dry fraction
- [ ] A tussock as **geometry** at close range — the aggregate is correct beyond the fade and there is nothing behind it
- [ ] Arrhenatherum elatius — false oat-grass, the hay meadow's dominant
- [ ] Dactylis glomerata — cocksfoot, and its tussock is a distinct silhouette
- [ ] Festuca pratensis — meadow fescue
- [ ] Festuca rubra — red fescue
- [ ] Festuca ovina — sheep's fescue, dry
- [ ] Poa pratensis — smooth meadow-grass
- [ ] Poa trivialis — rough meadow-grass
- [ ] Poa annua — annual meadow-grass, trodden ground
- [ ] Lolium perenne — perennial ryegrass, the intensively managed sward and the lawn
- [ ] Trisetum flavescens — yellow oat-grass
- [ ] Anthoxanthum odoratum — sweet vernal grass
- [ ] Holcus lanatus — Yorkshire fog, with its grey-green haze
- [ ] Avenula pubescens — downy oat-grass
- [ ] Bromus hordeaceus — soft brome
- [ ] Bromus erectus — upright brome, calcareous grassland
- [ ] Phleum pratense — timothy, and its cylindrical head
- [ ] Alopecurus pratensis — meadow foxtail
- [ ] Agrostis capillaris / stolonifera — bents
- [ ] Briza media — quaking grass
- [ ] Nardus stricta — mat-grass; montane pasture
- [ ] Molinia caerulea — purple moor-grass; bog and damp heath, a strong tussock
- [ ] Deschampsia cespitosa — tufted hair-grass, a strong tussock
- [ ] Deschampsia flexuosa — wavy hair-grass, acid forest floor
- [ ] Calamagrostis epigejos — wood small-reed, disturbed ground
- [ ] Milium effusum — wood millet, forest
- [ ] Melica uniflora — wood melick, beech forest
- [ ] Brachypodium sylvaticum — false brome, forest edge
- [ ] Carex sylvatica — wood sedge
- [ ] Carex acutiformis / riparia — the bank sedges
- [ ] Carex elata — tussock sedge, and the tussock *is* the form
- [ ] Carex nigra — common sedge, bog
- [ ] Juncus effusus — soft rush, and it marks wet ground in a pasture
- [ ] Luzula luzuloides / pilosa — woodrushes
- [ ] Eriophorum angustifolium — cotton grass; bog, and the white heads are the whole picture there

### III.14 Species needing the cane, emergent and aquatic forms

- [ ] Phragmites australis — common reed, the **cane** form, and a reed bed is a landscape element on its own
- [ ] Typha latifolia / angustifolia — bulrush, emergent
- [ ] Glyceria maxima — reed sweet-grass
- [ ] Phalaris arundinacea — reed canary grass
- [ ] Schoenoplectus lacustris — club-rush
- [ ] Sparganium erectum — branched bur-reed
- [ ] Iris pseudacorus — yellow flag
- [ ] Butomus umbellatus — flowering rush
- [ ] Alisma plantago-aquatica — water plantain
- [ ] Nuphar lutea — yellow water-lily — the **floating-leaf** form, whose datum is the water surface
- [ ] Nymphaea alba — white water-lily
- [ ] Potamogeton natans — broad-leaved pondweed
- [ ] Ranunculus fluitans — river water-crowfoot, **submerged**, streaming with the current
- [ ] Myriophyllum / Ceratophyllum — submerged, visible only in clear shallow water
- [ ] Elodea canadensis — waterweed
- [ ] Lemna minor — duckweed as a surface film, which changes the water's colour entirely
- [ ] Nasturtium officinale — watercress at a spring
- [ ] Algal bloom as a surface state
- [ ] Bank zonation: open water, floating, emergent, reed, sedge, willow — the sequence, not the species

### III.15 Species needing the fern, moss, lichen and fungal forms

- [ ] Dryopteris filix-mas — male fern, the **fern crown**
- [ ] Dryopteris dilatata — broad buckler fern
- [ ] Athyrium filix-femina — lady fern
- [ ] Polystichum aculeatum — hard shield fern
- [ ] Pteridium aquilinum — bracken — the **frond mat**, a continuous stand rather than individuals
- [ ] Blechnum spicant — hard fern, acid
- [ ] Asplenium scolopendrium — hart's tongue, shaded rock, a strap-leaf form
- [ ] Asplenium trichomanes / ruta-muraria — the wall ferns, and they need a wall as a host
- [ ] Polypodium vulgare — polypody, on a bank or a branch
- [ ] Pleurozium schreberi, Hylocomium splendens, Dicranum scoparium — the conifer floor moss carpet
- [ ] Thuidium tamariscinum — on a broadleaf floor
- [ ] Polytrichum commune — haircap moss
- [ ] Sphagnum spp. — bog moss, and the hummock-hollow pattern is the form
- [ ] Brachythecium / Hypnum on stone, wall and roof tile
- [ ] Marchantia — liverwort on wet bare soil
- [ ] Cladonia rangiferina — reindeer lichen, heath and dune
- [ ] Xanthoria parietina — the yellow lichen on a roof tile and a wayside tree
- [ ] Parmelia / Hypogymnia — grey bark lichen
- [ ] Green algal film on the north side of a trunk, a post and a wall
- [ ] Amanita muscaria — fly agaric
- [ ] Boletus edulis — cep
- [ ] Cantharellus cibarius — chanterelle
- [ ] Russula, Lactarius, Amanita — the common floor set
- [ ] Macrolepiota procera — parasol
- [ ] Fomes fomentarius — hoof fungus on beech, a **bracket** on a standing trunk
- [ ] Ganoderma / Trametes versicolor — brackets on deadwood
- [ ] Mycena / Armillaria — the small ones on a stump
- [ ] Fairy ring in a pasture as a growth pattern rather than an object

### III.16 The deadwood forms and what rides them

- [ ] Standing dead trunk with bark falling off
- [ ] Snag with a broken top
- [ ] Windthrow with a raised root plate and a pit beside it
- [ ] Fallen log, decayed in stages
- [ ] Stump, cut flat, with saw marks
- [ ] Stump with resprouts — a coppice stool's first year
- [ ] Brash pile from thinning
- [ ] Log stack at a forest road, which is a strong human signal
- [ ] Branch litter on the floor
- [ ] Woodpecker holes and bark beetle galleries
- [ ] Charcoal and burnt ground after a fire
- [ ] Habitat pile, deliberately left

### III.17 The crop row form and the crops that ride it

*Ranked by German acreage (Destatis 2024): wheat 2.62 Mha, barley 1.66 Mha, oilseeds 1.15 Mha, rye
0.54 Mha, grain maize 0.50 Mha, sugar beet 0.44 Mha, potato 0.28 Mha, oats 0.16 Mha.*

- [ ] Crop row form: a field of one plant with a row rhythm, a row direction and a drill spacing
- [ ] Winter wheat — the dominant field, and its colour sequence from green to gold is the summer landscape
- [ ] Winter barley, with its awned, nodding head
- [ ] Spring barley
- [ ] Rye, taller than wheat, greyer
- [ ] Triticale
- [ ] Oats, a panicle rather than a spike
- [ ] Grain maize, over two metres, rows legible from the air
- [ ] Silage maize, cut earlier and shorter
- [ ] Oilseed rape in flower — the single most recognisable field colour in a German April
- [ ] Rape after flowering, grey-green and pod-heavy
- [ ] Sugar beet, a low dense canopy with a distinct blue-green
- [ ] Potato, in ridges with a visible furrow rhythm
- [ ] Field bean, pea, lupin
- [ ] Sunflower
- [ ] Soy
- [ ] Hemp, flax
- [ ] Lucerne / clover ley
- [ ] Mustard or phacelia as a cover crop, sown after harvest
- [ ] Flower strip on a margin, deliberately sown
- [ ] Set-aside and fallow
- [ ] Row direction per field, constant within it, varying between neighbours — this is what makes an agricultural landscape read
- [ ] Tramlines from the sprayer, at the machine's own working width
- [ ] Headland, worked across the rows
- [ ] Stubble after harvest
- [ ] Ploughed bare soil, with the furrow direction
- [ ] Harrowed and drilled seedbed
- [ ] Crop lodging patches after a storm
- [ ] Irrigation reel and its wet arc
- [ ] Round bales left on a field
- [ ] Square bales stacked
- [ ] Wrapped silage bales, white and shiny
- [ ] Hay windrows before baling
- [ ] Slurry application darkening a field
- [ ] Bird scarer, kite, gas gun
- [ ] Field boundary stone, marker post
- [ ] Game cover strip and a raised hide

### III.18 The trained forms and the plantings that ride them

- [ ] Streuobstwiese — standard fruit trees on tall stems over a grazed or cut meadow, and it is the form the Weser valley actually has
- [ ] Modern dwarf apple orchard: the **spindle on post and wire** form, with hail netting
- [ ] Cherry orchard
- [ ] Plum and pear orchard
- [ ] Walnut in a field corner
- [ ] Vineyard in rows on a slope, the **vine on trellis** form with posts and wires
- [ ] Terraced vineyard on drystone walls
- [ ] Individual-stake vine training, distinct from the trellis row
- [ ] Espalier fruit against a wall
- [ ] Hop garden with its high wire framework
- [ ] Asparagus ridges under film
- [ ] Strawberry rows under a tunnel
- [ ] Field vegetable rows: cabbage, onion, carrot, leek
- [ ] Nursery rows of young trees
- [ ] Christmas tree plantation
- [ ] Short-rotation poplar or willow coppice
- [ ] Allotment garden: plot grid, sheds, fruit trees, vegetable beds, hedges
- [ ] Domestic garden: lawn, border, ornamental shrub, hedge, terrace, tree
- [ ] Park: mown lawn, specimen tree, avenue, shrub block, bedding
- [ ] Cemetery: clipped hedges, grave plantings, yew and thuja, mown grass
- [ ] Green roof and façade planting — the setting is post-scarcity, so this is not decoration
- [ ] Street tree in a pit with a grate, and a young one with a stake and tie
- [ ] Planter and container planting
- [ ] Sports turf, marked

### III.19 Forms belonging to other biomes, named so they are not mistaken for oversights

- [ ] Krummholz — a wind-formed woody mass, and it is a **form** before it is *Pinus mugo*
- [ ] Alpine dwarf shrub heath — Rhododendron ferrugineum, Vaccinium
- [ ] Alpine mat — Carex curvula, Festuca, the turf form at 2 500 m
- [ ] Cushion — Silene acaulis, Saxifraga, a form nothing temperate uses
- [ ] Scree pioneers
- [ ] Snowbed community
- [ ] Timberline transition, as a density and height ramp rather than a line
- [ ] Mediterranean maquis and garrigue — holm oak, cistus, rosemary
- [ ] Olive, umbrella pine, cypress — three distinct crown envelopes
- [ ] Arid: creosote bush, saltbush, yucca, cactus — the **succulent** form, which nothing here has
- [ ] Arid: ephemeral bloom after rain
- [ ] Arid: desert pavement with no plants at all — the `badwater` scenario exists and there is still no arid template among the thirteen
- [ ] Boreal: spruce–birch taiga with a lichen ground layer
- [ ] Coastal: dune grass (Ammophila), salt marsh (Salicornia, Spartina)
- [ ] Palm — a form nothing else uses, and named for completeness
- [ ] Tropical forms — out of scope, named so they are not an oversight

## Band IV — Buildings, structures and infrastructure

*The reference for this half is GTA 5, for range and construction only. KCD's built world does not
transfer: a Bohemian village is not modern infrastructure. Infrastructure comes first inside the band
because the street network is what buildings, vehicles and lighting all hang off.*

### IV.1 Data prerequisites

- [x] Building footprints from the served vector tiles, kept as rings with an index rather than re-parsed
- [x] A `height` attribute where the provider carries one
- [x] The provider's 5.0 m fill detected and named rather than trusted (`kFillHeightM`, 1634 of the Hameln tile)
- [x] Base elevation per footprint from the ring's own lowest corner
- [ ] One base per building — `FeatureTop` takes the ring's lowest corner, `Buildings::At` derives it from the bbox centre, so a queried prism floats ≈1.5 m against the drawn one on a 10 % slope
- [ ] `building:levels` — TILE: the Shortbread buildings layer carries no attributes at all beyond the provider's height extension
- [ ] Building use / kind (house, church, industrial, retail) — TILE, same reason; the `pois` layer is the only place a use is spelled and it is not fetched
- [ ] `roof:shape`, `roof:levels`, `roof:material`, `building:material`, `building:colour` — TILE
- [ ] Address and house number — in the `pois`/`addresses` layers, not fetched
- [ ] Storey count inferred from height when no level count is served, with the inference stated
- [ ] Building age or period inferred, which the epoch dial needs

### IV.2 Mass and footprint

- [x] Footprint extruded to a prism
- [ ] A building's LOD ladder on the one cluster DAG, with the same model-space error the vegetation ladder uses — Band IV declares 342 features and not one rung, while buildings are the largest single memory consumer measured (70 894 KiB of heap over one block, 545 KiB per tile)
- [ ] A far rung that drops openings, trim and roof furniture and keeps mass and roof plane, because at 320×180 those are what a silhouette is made of
- [ ] A block of buildings merged into one draw at the far rungs, the way the reference merges vegetation per cell — the count of draws, not the count of triangles, is what a street costs
- [ ] An impostor rung for a distant block, its error anchored on the atlas cell texel like every other impostor
- [x] Wall vertices carrying a façade coordinate — `uv.x` = `256·style + bay`, `uv.y` = storeys, so one façade function serves the whole town with no per-building constant to pass — `src/core/FacadeUv.h`
- [ ] Multi-part mass: a main block plus a lower wing, rather than one prism per ring
- [ ] Courtyard buildings as several rings resolved as one structure
- [ ] Terrace: a row of prisms sharing walls, recognised as a row
- [ ] Setback on an upper storey
- [ ] Overhang and cantilever
- [ ] Building on a slope: a stepped base rather than a floating or buried plinth
- [x] Plinth and base course as a distinct band — `src/render/stages/BuildingDraw.cpp` — a shading band, not geometry
- [ ] Party wall exposed above a lower neighbour
- [ ] Attached garage, porch, conservatory, extension
- [ ] Building contact body for physics — deliberately none today, and a wrong body would be worse than none

### IV.3 Roofs

- [x] Flat roof with a parapet — `src/generators/draw/RoofSurface.cpp` + `src/generators/draw/BuildingMesh.cpp`
- [x] Monopitch / shed roof — `src/generators/draw/RoofSurface.cpp`
- [x] Gable roof — the default for the region, and the one that must exist first — `src/generators/draw/RoofSurface.cpp`
- [x] Hip roof — `src/generators/draw/RoofSurface.cpp`
- [ ] Half-hip (Krüppelwalm)
- [x] Mansard roof — `src/generators/draw/RoofSurface.cpp`
- [ ] Gambrel roof
- [x] Pyramidal roof — `src/generators/draw/RoofSurface.cpp` — emergent from Hip on a square box, not declared
- [ ] Conical roof on a round tower
- [ ] Dome
- [ ] Barrel vault
- [x] Sawtooth / north-light roof, the industrial hall — `src/generators/draw/RoofSurface.cpp`
- [ ] Butterfly roof
- [ ] Folded-plate and shell roofs
- [ ] Roof pitch as a function of the footprint's proportions and the declared epoch
- [x] Ridge running along the long axis by default, with the exceptions declared — `src/generators/draw/BuildingShape.cpp`
- [ ] Roof over an L-shaped or T-shaped footprint, with valleys resolved
- [x] Eaves overhang, fascia, soffit — `src/generators/draw/BuildingMesh.cpp`
- [ ] Verge and bargeboard
- [ ] Gutter, hopper, downpipe, and a downpipe that reaches the ground
- [ ] Ridge tiles and hip rolls
- [x] Chimney stack, with pots or a metal flue — `src/generators/draw/BuildingMesh.cpp` — stack only, a pot is sub-pixel at every rung
- [ ] Roof vent, extract cowl
- [ ] Dormer: gable, hip, shed, eyebrow
- [ ] Roof window flush in the plane
- [ ] Roof lantern and skylight strip
- [ ] Roof terrace with a railing
- [x] Plant room, lift overrun, stair head — `src/generators/draw/BuildingMesh.cpp`
- [ ] Rooftop HVAC units and ducting — the flat-roofed commercial building's whole silhouette
- [ ] Rooftop water tank
- [ ] Photovoltaic array, and in a post-scarcity setting it is the default rather than the exception
- [ ] Solar thermal panel
- [ ] Green roof, planted
- [ ] Aerial, satellite dish, lightning conductor
- [ ] Snow guard
- [ ] Roof covering: clay pantile, plain tile, concrete tile, slate, shingle, corrugated metal, standing seam, bitumen felt, gravel ballast, membrane
- [ ] Thatch — heritage, epoch 1 only
- [ ] Moss and lichen on the north pitch, and it is one of the strongest ageing signals on a roof
- [ ] Missing tiles, sagging ridge, collapsed section — decay dial

### IV.4 Façade and openings

- [x] Storey division derived from height, so a window grid has a rhythm — `src/generators/draw/BuildingShape.cpp`
- [x] Window rhythm: bay spacing, alignment between storeys, a wider or narrower ground floor — `src/render/stages/BuildingDraw.cpp`
- [x] Window as an opening with a reveal depth, not a decal — `src/render/stages/BuildingDraw.cpp` — a marched box recess
- [ ] Window frame, mullion, transom, glazing bars
- [ ] Sill and lintel
- [ ] Casement, sliding, fixed, tilt-and-turn
- [ ] French window and door to a balcony
- [ ] Bay window and oriel
- [ ] Arched, round and porthole openings
- [x] Shop window at ground floor, full height — `src/render/stages/BuildingDraw.cpp`
- [ ] Roller shutter, louvre shutter, folding shutter
- [x] Blind or curtain visible behind the glass — `src/render/stages/BuildingDraw.cpp`
- [x] Glass: reflectance and a dark interior at the comparison rung; a lit room only at night — `src/render/stages/BuildingDraw.cpp` — Schlick; the lit room is not built
- [ ] Entrance door, double door, revolving door
- [ ] Garage door: up-and-over, sectional, roller
- [ ] Loading dock door and a dock leveller
- [ ] Gateway passage through a perimeter block
- [ ] Ventilation grille, air brick, meter box
- [ ] Balcony: cantilevered slab, recessed loggia, French balconet
- [ ] Balcony railing: steel, glass, masonry, and planting on it
- [ ] External staircase and fire escape
- [ ] Ramp and handrail
- [ ] Porch and canopy over an entrance
- [ ] Awning, retractable
- [ ] Pergola and terrace
- [ ] Buttress and pilaster
- [ ] Cornice, string course, quoins, lesenes
- [ ] Gable ornament and finial
- [ ] Timber framing (Fachwerk) as a visible structural grid — Hameln's old town is the epoch 1 anchor and it is exactly this
- [ ] Surface finish: render, exposed brick, stone ashlar, rubble stone, board cladding, fibre cement, metal panel, precast concrete, curtain wall, EIFS
- [ ] Brick bond and course height as geometry rather than a texture
- [ ] Weathering: rain streaks below sills, splash zone at the base, efflorescence, algae on the shaded side
- [ ] Shop signage lettering — NO SUBSTITUTE: a typeface is authored appearance, and a shop that reads as a shop needs one. A procedural pseudo-glyph is legible as noise at the top rung
- [ ] Advertising imagery and posters — NO SUBSTITUTE, same reason, and REFUSED as an asset
- [ ] Graffiti — NO SUBSTITUTE: a mark made with intent. A statistical smear is not the same thing
- [ ] Stained glass — NO SUBSTITUTE, authored imagery
- [ ] Company logos and liveries — NO SUBSTITUTE, and legally distinct from the above

### IV.5 Interiors

- [ ] A dark room box behind the glass, so a window is not a hole into the world
- [ ] Interior wall plane at a declared depth, lit only by what comes through the window
- [ ] Lit interior at night with a per-room duty cycle
- [ ] Curtain or blind plane
- [ ] Enterable ground-floor shop
- [ ] Enterable dwelling: hall, room, stair
- [ ] Stair core and lift shaft as geometry
- [ ] Floor plan generated from the footprint and the storey count
- [ ] Furniture as declared bodies — the body format already has to carry furniture
- [ ] Interior lighting as a light list contribution
- [ ] Portal or occlusion boundary at a door, so an interior does not cost the exterior
- [ ] Basement and cellar
- [ ] Loft space under a pitched roof
- [ ] GTA 5's hand-modelled interiors — REFUSED as a method; the substitute is a generated plan, and where it does not reach, a closed door

### IV.6 Building types by use

- [ ] Detached house
- [ ] Semi-detached pair
- [ ] Terrace / row house
- [ ] Apartment block, four to six storeys
- [ ] Slab block and tower block
- [ ] Perimeter block with an inner courtyard
- [ ] Villa in a garden
- [ ] Farmhouse
- [ ] Barn
- [ ] Stable and livestock shed
- [ ] Silo, tower and clamp
- [ ] Greenhouse, glass and film
- [ ] Warehouse and distribution shed
- [ ] Factory hall
- [ ] Workshop and small industrial unit
- [ ] Office building
- [ ] Curtain-wall tower
- [ ] Shopping centre
- [ ] Supermarket with its car park
- [ ] Retail park shed
- [ ] Kiosk
- [ ] Restaurant, café, pub
- [ ] Hotel
- [ ] School
- [ ] University building
- [ ] Hospital
- [ ] Church, and a tower with a spire is a landmark at any distance
- [ ] Chapel
- [ ] Mosque, synagogue
- [ ] Town hall and civic building
- [ ] Museum, theatre, cinema, library
- [ ] Police station, fire station
- [ ] Prison
- [ ] Sports hall
- [ ] Stadium with a stand roof
- [ ] Swimming pool building
- [ ] Multi-storey car park
- [ ] Petrol / charging station with a canopy
- [ ] Car wash
- [ ] Bus station
- [ ] Railway station hall and platform canopy
- [ ] Airport terminal
- [ ] Aircraft hangar
- [ ] Power station block — Grohnde is a declared acceptance target and it is a building set of its own
- [ ] Cooling tower
- [ ] Substation building
- [ ] Waterworks and sewage plant buildings
- [ ] Waste transfer station
- [ ] Data centre
- [ ] Telecom exchange
- [ ] Cemetery chapel
- [ ] Allotment hut
- [ ] Garden shed
- [ ] Garage and carport
- [ ] Boathouse
- [ ] Lighthouse
- [ ] Windmill and watermill — heritage
- [ ] Castle, keep, town wall, gate tower — epoch 1, and Hameln has them
- [ ] Bunker
- [ ] Grain elevator
- [ ] Market hall
- [ ] Public toilet
- [ ] Container terminal and stacked containers
- [ ] Scaffolded building under construction, with a crane
- [ ] Ruin — decay dial: collapsed roof, standing gables, vegetation in the shell

### IV.7 Boundaries and small structures

- [ ] Masonry wall, with a coping
- [ ] Drystone wall
- [ ] Rendered garden wall
- [ ] Retaining wall
- [ ] Timber fence: close-board, picket, post-and-rail
- [ ] Wire fence, chain-link, welded mesh
- [ ] Palisade and security fence
- [ ] Electric fence for stock, on plastic posts
- [ ] Deer fence
- [ ] Crash barrier used as a boundary
- [ ] Noise barrier
- [ ] Gate: field gate, driveway gate, pedestrian gate, sliding gate
- [ ] Bollard: fixed, removable, illuminated
- [ ] Stile, kissing gate, cattle grid
- [ ] Hedge as a boundary — cross-references III.7
- [ ] Ditch and bank as a boundary
- [ ] Terrace steps, garden stair
- [ ] Pergola, arbour, gazebo
- [ ] Bin store and refuse bins
- [ ] Letterbox, house number plate, doorbell panel
- [ ] Washing line
- [ ] Playground equipment
- [ ] Bench, picnic table
- [ ] Monument, memorial, wayside cross, shrine
- [ ] Fountain and basin
- [ ] Flagpole
- [ ] Statue

### IV.8 Roads

- [x] Ways carried as centrelines with a declared half-width per kind (`world/StreetField`)
- [x] Seventeen street kinds classified: motorway, trunk, primary, secondary, tertiary, unclassified, residential, living street, pedestrian, service, track, path, footway, cycleway, steps, bridleway, busway
- [x] Rail kinds classified: rail, light rail, tram, narrow gauge, subway, monorail, funicular
- [x] Aeroway kinds classified: runway, taxiway, apron, helipad
- [x] Street polygons as areas rather than ribbons
- [x] Way surface as a class-grid colour under the terrain shader
- [x] Point query: what is made here, and how wide (`generators/Infrastructure`)
- [ ] A road drawn as its own geometry rather than as a colour on the terrain
- [ ] Carriageway with camber and superelevation on a curve
- [ ] Lane subdivision from the width, with the lane count stated
- [ ] Hard shoulder and verge
- [ ] Kerb with an upstand, dropped at a crossing, with a corner radius
- [ ] Gutter channel and drainage grate
- [ ] Manhole and inspection cover
- [ ] Junction geometry: the corner fillet, the flared mouth, the island
- [ ] Roundabout with its island and apron
- [ ] Motorway interchange ramps as geometry
- [ ] Level difference between carriageway, verge and field
- [ ] Cutting and embankment along a road
- [ ] Surface by class: asphalt, concrete, setts, gravel, unpaved, and the served `surface` attribute already carries it
- [ ] Tracktype as a surface gradient on a farm track
- [ ] Wheel-track polish bands and a darker centre strip
- [ ] Patches, joints, crack sealing
- [ ] Potholes and edge break-up — decay dial
- [ ] Road markings: centre line, lane line, edge line, stop line, give-way triangles, arrows, zebra, box junction, hatching, chevrons, cycle lane, bus lane, parking bay, painted speed limit
- [ ] Reflective studs
- [ ] Tactile paving at a crossing
- [ ] Wet road reflectance, and it doubles the apparent light at night
- [ ] The reference's road tool is a decal along a spline — REFUSED as a method: a decal needs an authored texture. Ours must be geometry plus a material row

### IV.9 Road furniture, signage and lighting

- [ ] Traffic sign: warning triangle, prohibition circle, mandatory blue, direction sign, gantry sign
- [ ] Sign face content — NO SUBSTITUTE for lettering and pictograms; the geometry and the colour are procedural, the glyphs are not
- [ ] Street name plate and house number
- [ ] Traffic light: mast, arm, pedestrian head, countdown, and its states
- [ ] Street lamp: column, bracket, lantern, and the modern LED versus the sodium heritage form
- [ ] Street lamp placement from the street centreline at a declared spacing — measured: the served vector data has no lamps, so placing them is the only route
- [ ] Lamp emission with a photometric cone, contributing to the light list
- [ ] Bollard, guard rail (steel W-beam, cable, concrete barrier)
- [ ] Crash cushion
- [ ] Delineator post
- [ ] Junction mirror
- [ ] Speed camera
- [ ] Bus stop pole, shelter, timetable case
- [ ] Bench, litter bin
- [ ] Cycle rack
- [ ] Charging post — post-scarcity default
- [ ] Parking meter and ticket machine
- [ ] Post box
- [ ] Advertising column and billboard frame
- [ ] Planter, tree pit with a grate
- [ ] Road works: cones, barriers, temporary lights, diversion signs, an open trench, a steel plate over it
- [ ] Height restriction bar
- [ ] Weather station and variable message sign

### IV.10 Rail

- [ ] Ballast bed with a shoulder
- [ ] Sleepers, concrete and timber
- [ ] Rails with a rail head shine
- [ ] Points and a crossing frog
- [ ] Buffer stop
- [ ] Platform with an edge line, a tactile strip, a canopy, seats and signage
- [ ] Overhead line: masts, cantilevers, catenary and contact wire, tensioning weights
- [ ] Third rail
- [ ] Signals: light signals, and semaphore for a heritage epoch
- [ ] Cable trough, kilometre post, lineside fencing
- [ ] Level crossing: barriers, lights, road surface panels
- [ ] Tram track set into a road surface
- [ ] Tram stop island
- [ ] Marshalling yard and siding
- [ ] Rail bridge and tunnel portal
- [ ] Funicular, narrow gauge, monorail beam
- [ ] Underground station entrance

### IV.11 Bridges and tunnels

- [ ] Beam and slab bridge
- [ ] Girder and box girder bridge
- [ ] Truss bridge
- [ ] Masonry arch bridge
- [ ] Concrete arch bridge
- [ ] Cable-stayed bridge
- [ ] Suspension bridge
- [ ] Bowstring arch
- [ ] Footbridge
- [ ] Pipe bridge and aqueduct
- [ ] Viaduct with repeated piers
- [ ] Abutment, bearing, expansion joint
- [ ] Parapet, railing, deck drainage
- [ ] Bridge lighting and under-deck shadow
- [ ] Culvert and headwall
- [ ] Tunnel portal
- [ ] Tunnel lining, lighting strip, ventilation fan, emergency bay, cross passage
- [ ] The served `bridge` and `tunnel` booleans consumed — they are in the streets and water_lines layers and nothing reads them

### IV.12 Water infrastructure

- [ ] Weir, and its foam line is visible from a distance
- [ ] Lock chamber and gates
- [ ] Sluice and penstock
- [ ] Dam and spillway
- [ ] Fish ladder
- [ ] Dyke and levee
- [ ] Revetment and riprap
- [ ] Groyne
- [ ] Quay wall, jetty, pontoon, slipway
- [ ] Mooring bollard, buoy, navigation marker
- [ ] Canal towpath
- [ ] Ford and stepping stones
- [ ] Storm drain outfall
- [ ] Water tower
- [ ] Pumping station
- [ ] Hydrant and standpipe
- [ ] Sewage works tanks
- [ ] Irrigation channel and ditch network

### IV.13 Power, energy and communications

- [ ] Lattice transmission tower, in its several types
- [ ] Conductor catenary sag, computed rather than drawn straight
- [ ] Insulator strings and earth wire
- [ ] Distribution pole, wood and concrete
- [ ] Pole-mounted transformer
- [ ] Ground transformer kiosk
- [ ] Substation: busbars, breakers, gantries, fence, gravel
- [ ] Cleared right-of-way through woodland under a line — a vegetation consequence of an infrastructure object
- [ ] Photovoltaic field with tracker rows and their shadow pattern
- [ ] Wind turbine: tower, nacelle, three blades, and the rotation is part of acceptance
- [ ] Aviation warning lights and the daytime red bands
- [ ] Battery storage containers
- [ ] Biogas plant with its digester domes
- [ ] District heating pipe bridge
- [ ] Industrial chimney with a plume
- [ ] Cooling tower with a plume
- [ ] Nuclear plant: reactor building, turbine hall, stack — the epoch 3 anchor at Grohnde
- [ ] Telecom mast with antenna panels and microwave dishes
- [ ] Guyed mast
- [ ] Street cabinet
- [ ] Satellite ground station

### IV.14 Aviation and ports

- [ ] Runway with threshold markings, centre line, touchdown zone
- [ ] Taxiway with its centre line and edge lights
- [ ] Apron and stands
- [ ] Approach lighting
- [ ] Windsock
- [ ] Control tower
- [ ] Helipad marking
- [ ] Quay crane
- [ ] Container stacks
- [ ] Ro-ro ramp
- [ ] Marina pontoons

---

## Band V — Vehicles

*GTA 5 names the construction: a vehicle is a hull on wheels with suspension, tyre grip and a torque
curve; a human is a capsule whose locomotion the animation leads. The field groups below follow RAGE's
own `handling.meta` division — mass and aero, drivetrain, brakes and steering, traction, suspension,
damage — because it is the published enumeration of what a driveable body needs. **Nothing in this band
exists.** It depends on I.12 in full.*

### V.1 Prerequisites

- [ ] Rigid-body dynamics (I.12) — every line below is blocked on it
- [ ] Declared body format carrying segments, joints, contacts, force sources and medium
- [ ] Vehicle as one declaration in that format, not a second format
- [ ] Vehicle prototype and instances, as vegetation already is
- [ ] Vehicle LOD ladder on the same one ladder
- [ ] Vehicle spawned by an actor spawner sharing the region key
- [ ] Vehicle occupancy claimed against the same sink

### V.2 Mass, hull and aerodynamics

- [ ] Mass, centre-of-mass offset, inertia multiplier
- [ ] Hull as a collision shape distinct from the drawn mesh
- [ ] Drag coefficient and frontal area
- [ ] Downforce
- [ ] Submersion depth at which the engine cuts
- [ ] Buoyancy volume and centre, so a car sinks and a boat does not
- [ ] Body panels as sub-bodies: doors, bonnet, boot, hatch, with hinges and limits
- [ ] Glass panes as breakable elements
- [ ] Number plate — NO SUBSTITUTE for its lettering; the plate is geometry, the glyphs are not
- [ ] Paint as a material row: base colour, clear coat, metallic flake, and it needs no texture
- [ ] Livery and decals — NO SUBSTITUTE, authored appearance
- [ ] Dirt accumulation as a function of use and weather
- [ ] Rust and wear — decay dial

### V.3 Wheels, suspension, tyres

- [ ] Wheel as a body with a hub, a rim and a tyre
- [ ] Wheel raycast or shape cast against the drawn terrain
- [ ] Suspension: spring force, compression damping, rebound damping, upper and lower travel limits, raise
- [ ] Anti-roll bar
- [ ] Roll centre heights, front and rear
- [ ] Suspension bias front to rear
- [ ] Tyre longitudinal and lateral force curves, maximum and minimum
- [ ] Traction spring delta and low-speed loss
- [ ] Camber stiffness
- [ ] Traction bias front to rear
- [ ] Surface grip multiplier per contact material — asphalt, gravel, mud, grass, wet, ice
- [ ] Tyre deformation and burst, with the rim then running on the road
- [ ] Wheel spin, lock-up and the marks they leave
- [ ] Steering geometry: lock angle, Ackermann, self-centring
- [ ] Ride height change under load

### V.4 Drivetrain, brakes, controls

- [ ] Drive bias: front, rear, all
- [ ] Gear count and ratios, final drive
- [ ] Drive force and drive inertia
- [ ] Clutch engage and shift rates
- [ ] Top speed limiter
- [ ] Reverse
- [ ] Electric drive with a single ratio and instant torque — the post-scarcity default, and it changes the sound and the acceleration curve
- [ ] Brake force, brake bias, handbrake
- [ ] Anti-lock behaviour
- [ ] Throttle, brake, steer as the only inputs a brain or a player reaches
- [ ] Cruise and speed limiter

### V.5 Damage

- [ ] Collision damage multiplier and body deformation
- [ ] Panel detachment
- [ ] Glass cracking and shattering
- [ ] Engine damage, smoke, fire
- [ ] Fuel or battery leak
- [ ] Light breakage
- [ ] Deformation reflected in the collision shape, not only in the mesh

### V.6 Road vehicle classes

*GTA 5's own 23, listed so the range is explicit; the classes are declarations over one construction.*

- [ ] Compact
- [ ] Sedan
- [ ] SUV
- [ ] Coupe
- [ ] Muscle — an era declaration rather than a construction
- [ ] Sports Classic — an era declaration
- [ ] Sport
- [ ] Super
- [ ] Off-Road
- [ ] Van
- [ ] Industrial — tipper, mixer, flatbed
- [ ] Utility — tractor, forklift, tow truck, crane
- [ ] Commercial — articulated tractor unit and semi-trailer
- [ ] Service — bus, coach, taxi, refuse truck, street sweeper
- [ ] Emergency — police, ambulance, fire appliance
- [ ] Military — setting-dependent; a post-scarcity world may have no place for it, and that is the owner's call rather than mine
- [ ] Open Wheel
- [ ] Motorcycle
- [ ] Cycle
- [ ] Boat
- [ ] Helicopter
- [ ] Plane
- [ ] Train

### V.7 Two-wheelers

- [ ] Rider lean as the steering input, not a yaw torque
- [ ] Counter-steer at speed
- [ ] Stand and parked pose
- [ ] Pedal drive with a cadence
- [ ] Gears on a bicycle
- [ ] Cargo bike and trailer
- [ ] E-bike and e-scooter — post-scarcity street furniture as much as vehicles

### V.8 Rail vehicles

- [ ] Constraint to the rail rather than free contact
- [ ] Bogies, and a long body articulating over them
- [ ] Coupling between units
- [ ] Pantograph contact with the catenary
- [ ] Doors, and a stop at a platform
- [ ] Freight wagon types: flat, hopper, tank, container
- [ ] Tram in a road surface, sharing it with traffic

### V.9 Watercraft

- [ ] Buoyancy from displaced volume against the core's water level
- [ ] Hydrodynamic drag and added mass
- [ ] Propeller or water-jet thrust
- [ ] Rudder
- [ ] Hull planing at speed
- [ ] Wake and bow wave, and they must move the water surface, not sit on it
- [ ] Sail: wind force on a sail plane, heel, tacking
- [ ] Rowing
- [ ] Mooring and anchoring
- [ ] Classes: dinghy, motorboat, cabin cruiser, yacht, canoe, barge, ferry, tug, workboat
- [ ] Inland barge on the Weser — the acceptance river carries them

### V.10 Aircraft

- [ ] Lift and drag as coefficients over angle of attack, not a table lookup
- [ ] Stall and its recovery
- [ ] Control surfaces: elevator, aileron, rudder, flaps, airbrake
- [ ] Propeller or turbofan thrust
- [ ] Landing gear with suspension and a ground handling model
- [ ] Ground effect
- [ ] Rotor thrust, cyclic and collective, and the torque a tail rotor answers
- [ ] Autorotation
- [ ] Classes: light aircraft, airliner, cargo, glider, helicopter, drone
- [ ] Drone as the everyday post-scarcity aircraft, and it is the one the camera can follow anywhere
- [ ] Wind field from the weather provider driving all of the above — the provider already answers wind at altitude

### V.11 Systems, equipment and the verbs

- [ ] Headlights: low, high, daytime running
- [ ] Tail, brake, reverse, indicator, fog, hazard
- [ ] Beacon and siren on an emergency vehicle
- [ ] Interior and instrument lighting
- [ ] Mirrors
- [ ] Wipers, and their effect on a wet windscreen
- [ ] Horn
- [ ] Doors, boot, bonnet opened
- [ ] Seats and passengers
- [ ] Cargo bed, tipper, crane, winch, plough, sweeper brush
- [ ] Trailer coupling and an articulated joint
- [ ] Roof rack, bike carrier
- [ ] Charging flap and a charging cable to a post
- [ ] Enter and exit
- [ ] Drive, brake, park
- [ ] Fly, land, taxi
- [ ] Sail, moor
- [ ] Tow and be towed
- [ ] Be repaired
- [ ] Be driven by a brain rather than by a player

### V.12 Traffic and the parked world

- [ ] Parked vehicles as dressing along a residential street — this carries more of the picture than driving does, and it costs almost nothing
- [ ] Parking bay occupancy from the street class and the time of day
- [ ] Traffic spawned on street centrelines at a density derived from the road class
- [ ] Lane following and junction rules
- [ ] Traffic light obedience
- [ ] Yield at a pedestrian crossing
- [ ] Headlights on at night, and it is the single most visible night-time element after street lighting
- [ ] Vehicles despawned outside the observer's reach without their *knowledge* becoming observer-dependent
- [ ] Agricultural machinery in a field in season — a tractor with a trailer is the Weser valley's traffic

---

## The count

*Recounted mechanically 2026-08-12 after the first three tests landed. The block this replaces said
1974 / 1504 / 233 and was wrong on the day it was written: the method it declared — one pass over the
`- [ ]` / `- [x]` lines — does not produce those numbers over any state this file has had. Two things
are corrected with it. The method now says what it excludes, because the block's own table rows and
the legend at the head of this file match the pattern and were being counted as features; and the
count is taken at the top of a bullet (`^- [`), so an indented sub-item cannot inflate it. **A count
nobody recomputes is a claim**, and this one had drifted twice.*

*Recounted again 2026-08-12 after § I.19's shader ruling and § I.26.1–I.26.6 landed — 96 new lines, all
unticked. **The previous block's total was right and its split was wrong**, which is the drift this
file keeps warning about: it read 227 built / 1405 not built over a correct total of 1632, where the
committed file carried **244 / 1388**. Seventeen ticked lines were counted as unticked. The total is
the number nobody checks against reality and the split is the number everybody quotes, so the drift
landed in the worse of the two. Method, stated so it can be re-run: count lines matching `^- [` at the
top of a bullet; the legend at the head carries none and this block's rows are table rows, so neither
needs excluding after all.*

| | |
|---|---|
| Lines in this file | **2937** |
| Feature lines — `^- [` at the top of a bullet | **1825** |
| `- [x]` built and checked | **244** |
| `- [ ]` not built | **1581** |
| Band 0 — residency | 78 |
| Band I — engine (§§ I.1–I.17 plus the library sections §§ I.18–I.26) | 645 |
| Band II — world | 156 |
| Band III — vegetation | 459 |
| Band IV — buildings and infrastructure | 346 |
| Band V — vehicles | 141 |
| `NO SUBSTITUTE` | 12 |
| `REFUSED` | 17 |
| `TILE` | 5 |
| `TOOL` | 18 |
| `UNSURE` | 3 |

*The 1 796 are not the suite's size, and there is no longer one suite to size. § I.26.9 splits the tests
into three by instrument — **render**, **scenario**, **unit** — and § I.26.11 designs the render matrix,
whose enumeration is mechanical and whose expected instantiation is **≈ 200 render cases for the renderer
stage alone**, deliberately not written out as 200 lines because a hand-typed list would have holes and
no way to prove it does not. A count of requirement lines and a count of tests are different numbers and
this file holds the first.*

**Read the 244 correctly.** 43 of them are declarations of one thing — sixteen tree species and twelve
land templates — and every one of the sixteen rides the single growth form the generator can shape. The
engine's own machinery accounts for most of the rest. Nothing in bands IV and V beyond the footprint
prism, the way widths and their point queries is ticked, and Band V is entirely unticked.

**Three of the 1 825 have a test today**, and they are the whole of the suite's coverage: the closed,
wound, unit-normal and in-range bark invariants over every declared species
(`test/generators/draw/GrownBarkIsAClosedMesh.cpp`), the planar geodesy's round trip and its priced
approximation (`test/core/PlanarGeodesyHoldsToItsScope.cpp`), and the harness's own red
(`test/harness/ExpectFail.cpp`). The twelve gates beside them are structural. § I.21 carries the
classification that says how many lines ever can be tested — **537 with what is in the tree, 707 more
once each band's reference table is written down, 124 behind a device, 62 behind motion, 6 behind a
sense we cannot instrument** — and that reading was taken over 1436 lines, which is the population it
must be retaken over when § I.21's own line is worked.
