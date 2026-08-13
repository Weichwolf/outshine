Type: bug
Area: world
Tags: oracle, perf, instrument

**World and streaming**

- **The tile pool's worker threads outlive the registry, the content store and the transport they
  borrow.** `clients/Sim.h:231` declares `World::World W_;` and `:271-274` declare `Wire_`, `Store_`,
  `Content_` and `Sources_` **after** it. Members are destroyed in reverse declaration order, so
  `~Sim` destroys the `SourceSet` and the `ContentStore` first and reaches `~W_` — which is the only
  thing that calls `fb_stream_close()` → `~TilePool` → `join()` on the workers — forty declarations
  later. Every worker inside `TilePool::FetchInto` holds `Data::SourceSet &Sources_` and
  `Data::Transport &Wire_`, and `SourceSet::Collect` reads `Store_` on the way in. *The two entry points that wrote the same inversion went with the clients on 2026-08-12; the
  member-order defect inside `Sim` is untouched by that and is what this entry is.* `C.13` names exactly this
  (*"if data member `B` uses another data member `A`, declare `A` before `B`"*), `R.3`/`R.4` say the
  reference is non-owning, and `Lifetime.1` is the failure. **Latent, not live, and the harmless
  explanation was looked for and holds for the runs that exist**: `demo frame` and `demo walk-500`
  under `ASAN_OPTIONS=detect_stack_use_after_return=1` are both silent (measured 2026-08-12,
  `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/sar/out.log`
  and `.../sar500/out.log`), because both reach `progress=1` with `fetchGaveUp=0` and drain the queue
  before exit. It fires the first time a run ends with a tile in flight — an early return between
  `Prepare` and the end of the traversal, a scene shorter than its stream, a refusal after `Open`.
  Right: the pool stops before the things it borrows, which is three line moves — `Sources_` and
  `Content_` declared before `W_`, the transport constructed before `app`. Unspellable rather than
  moved: `TilePool` is scoped inside whatever owns the registry, so the order cannot be written the
  other way round. **The gate that would catch it does not exist**: `verify-walk-asan` never sets
  `detect_stack_use_after_return`, which is off by default and is precisely this class.
- **A refusal carries no reason, which is the defect § I.22 was written to close, one level down.**
  `data/Fetched.h:14` `Meaning::Refused` is minted in `data/WebTileSource.cpp:52-54` from
  `Classify(status, size)` and **the status is dropped on the spot**; `data/Delivery.h:46`
  `Delivery::Wire()` carries nothing at all; `world/TilePool.cpp` logs `tile_refused` with only the
  request key. A 401, a 500 that outlived its four-try budget, a body over `CurlTransport::Config::
  MaxBodyBytes`, a `StarBands` file that would not open and a `TryTake` on a wire that did not answer
  are one value — four readings of one empty buffer, moved from the top of the type into its
  `Refused` arm. `data/` may not name `Log` by the section's own ruling (`-Isrc/core -Isrc/data`,
  `Makefile` `INC_DATA`), so the reason has to be a **value** and not a log line. Right: `Refused`
  carries the status, or a declared enumeration beside it, and `TilePool` writes what it was handed.
- **`Delivery::Undeclared` is retried for ever and the load never ends.** `world/TilePool.cpp`
  `FetchInto` maps `Delivery::State::Undeclared` to `Reply::Refused` under a comment that says *"it
  will not heal by asking again"*, and `world/World.cpp:447-449` then maps `Reply::Refused` to
  `Adm_.Waiting` — correct for a wire error, wrong for a declaration error. `World::CountTargets`
  counts a leaf settled on `Ready || Vacant` only, so a node no source covers never settles,
  `progress` never reaches 1, and every pass writes another `tile_undeclared` error line. Right:
  `Undeclared` is its own reply at the pool and it ends the run loudly (§ I.17) instead of sharing an
  arm with the refusal that does heal.
- **A `Data::SourceSet::Query` owns a live transfer and is not a resource handle.**
  `data/SourceSet.h:41-63` holds `Ticket_` with `Query(Query &&) = default` and **no destructor**;
  cancelling is `SourceSet::Abandon(query, transport)`, a free operation the caller must remember. A
  moved-from `Query` keeps the ticket value, so `Abandon` on either end cancels the other's transfer
  (`C.64`: a move must leave the source valid); a `Query` dropped without `Abandon` leaves a
  `CurlTransport::Transfer` — url, status and the whole body — in `Transfers_` for the life of the
  transport, because `test/host/CurlTransport.cpp:70-78,80-100` erase only on collect or cancel.
  Nothing leaks today: the one caller calls `Abandon` on the one path that needs it. The rule is
  written down and not carried — `R.1`, `C.30`, `C.31`. Right: the ticket is an owning handle that
  cancels in its own destructor, so a dropped query cannot leave a transfer running.
- **`TryTake` is documented as once-only on four types and is not.** `data/Transport.h:36` says
  *"hands the status and the body over exactly once"*; `Wire::TryTake`, `Fetched::TryTake`,
  `Delivery::TryTake` and `World::TerrainBytes::TryTake` all move the payload out and leave `Where_`
  unchanged, so a second call returns `true` with an empty body and a caller reads *delivered, zero
  bytes* — the exact reading the four states exist to make unspellable. Right: the take transitions
  the state, so the second call is `false`.
- **`Data::ContentStore`'s cap is enforced once, before the first write, and never during the run.**
  `data/ContentStore.cpp:46-77` sweeps in the constructor; `Keep` (`:106-132`) adds bytes and counts
  nothing. `ContentStore.h:37` says so in its own words. A long session grows the directory without
  bound — the same failure as the 7 GB store this replaces, moved from *no cap* to *a cap checked
  before anything is in it*; and the test that says *"the store is capped"*
  (`test/unit/data/TheStoreNamesBytesByTheirKey.cpp:94-115`) only ever reopens a full directory, so it
  cannot see the difference. Right: a running byte total, and the sweep amortised over `Keep`.
- **The DEM path answers for a place the caller did not ask about, because the projection clamps.**
  `world/tiles/TileMath.h` `GeoToTileClamped` clamps the latitude into the Mercator band and then projects, so a
  standpoint outside the band gets the ground of the nearest point inside it with no refusal and no
  note. Measured on `fb_world` at 85.052 N / 15 E before the band was refused at the declaration: the
  terrain requests were `/t/terrain/14/8874/0` — x correct for 15 E, y clamped to row 0, whose
  southern edge is 85.0466 N — and the run reported `groundAslM=-3448.27` for a standpoint **97.3 m**
  north of anything the scheme can address, exit 0. The second spelling of the band left with the
  server (2026-08-12) and `world/tiles/TileMath.h:19 kMercatorLatMaxDeg = 85.05112877980659` is now
  the only one — but a third value is still *printed*: `Scene::Read` hands the constant to `Need`,
  whose message runs it through `std::to_string`, so a refused declaration reads `lat out of range
  [-85.051129,85.051129]` while `lat: 85.051129` is itself refused (measured on `fb_world`: 85.05112877
  exits 0, 85.05112878 and 85.051129 exit 2). The message names an admissible value that is not one,
  by 1.3 cm. Right: the clamping form refuses like `TileIndex::TryXy` does — it is the same
  projection — and the bound is declared once, which it now is. Priced at three call sites in
  `world/TerrainLoader.cpp`.
- **The fabrication window was reported 21 % too wide, and the upper bound is wrong.** `board/active/`
  and the round's report give `85.05113 < lat ≤ 85.0534 — about 250 m`. Measured against the tree's
  own code (`Schedule(Ring{14,1}).Widest(lat, 15)` and `osmmesh_geo_to_tile`, bisected to 1e-12 deg):
  the window is **85.051128779807 < lat ≤ 85.053023927135**, width 0.0018951 deg = **211.7 m**
  (WGS84 meridional 111 694 m/deg at 85 N; 211.0 m spherical). The upper end is where
  `Region::Of(14, lat, ·).Y()` drops from −1 to −2 and the ring's whole `y+1` row leaves the grid —
  at the reported 85.0534 the Y is already −2 and the run refuses. Right: the bound is a property of
  `Schedule::Widest` at `RadiusRegions = 1` and moves with the radius, so the number is derived from
  the ring rather than quoted.

- **The vector fields never re-anchor, and the shader pays for it in cancellation, not in
  resolution.** `world/World.cpp:87-94` gives `BuildingField` and `WaterField` one ECEF origin — the
  standpoint, at `Open` — and nothing moves it again, while both keep ingesting for as long as the
  camera travels. The vertex shader computes `p + b.anc.xyz` in **f32**
  (`render/stages/BuildingDraw.cpp:106`; `anc` is written at `516-518` as `(float)(Anchor - Eye)`), so
  a near wall's camera-relative position is the difference of two quantities whose magnitude is the
  distance travelled from the standpoint, and the subtraction cancels down to a few metres while both
  terms keep the absolute error of that magnitude. The half of it that matters is **temporal**: `anc`
  is rewritten every frame from a moving eye, so the near field steps by one ulp of the travelled
  distance per frame — visible as jitter, where the static quantisation of `p` would not be.
  Priced (derived, IEEE-754 binary32; 720p at 60° gives a pixel focal length of 623.5 px,
  `core/PixelFocalLength.h`): the step reaches **1 px on a wall 5 m away at 65 km** from the
  standpoint (ulp 7.8 mm) and 1 px at 20 m away at 262 km. By verb: 65 km on foot is 13 h and never
  arrives; at 30 m/s it is 36 min; **at 250 m/s it is 4.3 min**, and flying is a declared verb. It is
  also a deviation from this tree's own convention with no reason beside it — the terrain anchors per
  tile on the z10 ancestor (`World::SurfaceAnchor`), which is ≤ 24 km at this latitude and bounded by
  the block rather than by the traversal. Right: one anchor per DAG block, which bounds `|p|` by the
  block and `|anc − eye|` by the view radius, because only near blocks are drawn.
- **A refusal is logged once per pass, and a stalled load spins at kHz.** `world/TilePool.cpp`
  `RunMesh` emits `tile_mesh_refused` on every attempt and `FetchInto` emits `tile_refused` on every
  GET, both of which repeat for as long as the leaf is in the target cut. Measured against a
  synthesised decode failure on one z14 target leaf (`demo/frame`, native oracle, one tile truncated
  to half its bytes by a proxy): **340 359 `tile_mesh_refused` lines in 60.8 s** (≈5.6 kHz, one per
  pass) and, for a synthesised 403 on the same leaf, **48 802 `tile_refused` lines plus 16 365 GETs**
  against the tile server. The refusal itself is correct and must stay loud — an absence and a
  refusal are different facts, and the quiet version of this is the defect the round removed — but
  the record it writes is unbounded and the GET rate is a load the server did not ask for. Right: the
  count carries the repetition (`Ledger::MeshRefused`, `Ledger::FetchRefused` already do) and the
  line is written when the reason for a given tile CHANGES, not when it recurs.

- **An imposed arrival order and the content store cannot be exercised in the same run.** The order
  instrument is a decorator over the host transport (`test/host/DelayedTransport.h`), and the store is
  consulted inside `Data::SourceSet::Collect` **before** the source is begun — so a store hit answers
  without a ticket ever existing and no delay can apply to it. `verify-still` therefore declares
  `OUTSHINE_NO_CONTENT_STORE=1` and pays four cold scene loads over the real upstreams (110 s measured
  2026-08-12, against 103 s for the proxy version in front of a warm container). Two things are wrong
  with that: the gate never exercises the store at all, and the one path a shipping run takes — a warm
  store — is the one path no gate imposes an order on. Right: the arrival order is a property of the
  *delivery* rather than of the wire, so the instrument belongs where both a store hit and a wire
  answer pass; that place is inside `SourceSet`, which is library, so it needs to be a declared
  library facility rather than a host decorator.

- **`Delivery::At` is exercised only by a test, never by a run.** A request above a source's last
  native zoom is answered from the ancestor and says so (`data/WebTileSource.cpp` `Serves`,
  `test/unit/data/TheAnswerNamesItsAddress.cpp`), and the whole path is carried through the byte cache
  (`world/TilePool.h` `Landing`) because a crop computed from the requested address over an ancestor's
  pixels is a wrong picture drawn silently. **No run reaches it**: `world/World.cpp` `kMaxZ = 14`, the
  stitch asks at the same zoom, and the only elevation upstream declares `MaxZoom = 15`, so the
  difference is always zero. The mechanism is right and untested outside a unit test — which is the
  honest statement, not "it works". Right: the first scenario that asks above z15, or a declared run
  that narrows the registry to a source with a lower bound so the fill fires.

- **A remembered absence has no expiry on the client, and no weight in the cache that holds it.**
  The deleted server's disk cache gave a negative entry a lifetime (30 d) because an upstream 404 can
  be transient; nothing in the tree does now. `TilePool::Remember(key, nullptr, 0, at, true)`
  (`world/TilePool.cpp`) carries no timestamp, so `Lookup` answers `Reply::Absent` from it for the life of
  the process. Worse, it is stored with `len == 0`, so it adds nothing to `CacheBytes_` and the LRU
  loop — `while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_)` — is never entered on its
  account. Over ground that is all 204 (open water, imagery gaps) `Cache_` grows in entry count while
  `CacheBytes_` stays at 0, and every byte request pays a linear scan of it under `CacheMutex_`.
  Decidable from the code, not measured. Right: the entry count is a budget of its own, and an
  absence carries the age at which it is re-asked.

- **`TilePool` can be told to forget a mesh and cannot be told to forget a DAG.** `Poll` now keeps an
  `Absent` in both `Posted_` and `Done_` on purpose, and `ForgetMesh` is the only eraser — it keys
  `MeshKey`, so a `DagKey` entry has no release. `World::Refine` sets `BuildingDagId = 0` after a
  refused block and never asks that id again (`BuildingDagId = ++BuildingDagSeq` is monotonic, so it
  cannot be re-derived either): the entry is unreachable and immortal. One map node plus one set node
  per refused building block, monotonic over a run. `R.1` — an owner with no matching release. Right:
  one `Forget(uint64_t key)` with `ForgetMesh` as its caller, and `World` calling it beside the
  `building_block_refused` line.

- **The 64-bit push guarantee stops at an `(int)` cast.** After this round `TelemetryRow::Push` and
  `LogField` carry no `long` overload, so an implicit `long` is ambiguous on both targets — verified.
  Six `long` accumulators survive behind explicit casts: `StreetField::Unwidthed_`, `Tunnels_`,
  `WaterField::NoGround_`, `Outliers_`, `OsmField::Missing_`, `Bad_`, `ClassField::UnknownFeats_`,
  `Submits_`, published as `(int)` at `world/World.cpp:518-519` and `clients/SceneRunner.cpp:261,263`.
  All are error counts at low rates, so no wrap is expected — the finding is that the new guard does
  not reach them and the cast is what makes them spellable. `World::MeshVram` is `long` too but is
  zeroed every `Refine`, so it is a gauge and not exposed.

- **The east stitch stops at the antimeridian instead of wrapping.** The new guard in
  the deleted stitcher (`if (x + 1 < n)`) correctly stopped a request the tile server
  cannot route, but longitude wraps and latitude does not: at `x == 2^z - 1` the true east neighbour
  is `x == 0`, so the fix leaves a one-column height seam at ±180° — the same seam the pre-existing
  `x > 0` guard leaves on the west side. The N/S guards are right as written. Cost of the correct
  form: a modulo instead of a bound, on two of four sides.

- **`stitch_edge` asked for tiles that cannot exist.** The predecessor of `world/tiles/TerrainTiles.cpp`
  guarded the west and north neighbours against `x == 0` and `y == 0` and had no
  guard at the other end, so a tile on the map's east or south edge issued a GET for `x = 2^z` or
  `y = 2^z` — an address no source covers (`data/WebTileSource::Covers` refuses it now, which is where
  the guard belongs). It was invisible because it only fires at the edge of the world and
  the answer was read as "this neighbour has no data". Fixed in the same round it was found, because
  the caller now reads a 404 as a defect rather than as a hole; recorded here because the class —
  a guard written on one side of a symmetric pair — is not searched for anywhere.

- **RETRACTED — "nothing streams during play" was tile-size confounding, not a defect.** The founding
  reading (t=31…77 s of a deleted run log: `poolHttpGets` 310 flat,
  `tilesBuilt` 0 over 46 s) covers 46 s × 1.4 m/s = **64 m of walking, 4.3 % of a z14 tile edge**
  (`kMaxZ = 14`, `world/World.cpp:27`; pitch `40 075 016.686 · cos 52.106° / 2^14` = 1502 m).
  `board/` line 154 already carried the method; the derivation for *this* cut, per rung,
  is what decides it. A rung's ring radius is where `WantSplit` stops — `R(z) = span(z) · f / kEdgeTau`
  with `f = 0.5 · 720 / tan 30°` = 623.5 px and `kEdgeTau` = 384 px — so the z14 ring reaches only
  **2 439 m**, not the scene's 60 km view. New tiles per second is `2πR(z)·v / span(z)²`, which
  halves per rung: 0.0095 + 0.0048 + 0.0024 + 0.0012 + 0.0006 = **0.0184 tiles/s at 1.4 m/s**. That is
  **0.85 tiles expected in those 46 s** and **6.6 per 500 m**. Flat is the correct answer, and
  `tilesInView` 45→47 with `tilesTotal` flat is a disc translating by 4 % of a tile — not
  "orientation reached `Refine` and position did not".
  Confirmed by the eye columns on two declared runs: `demo/walk-500` walks 501.4 m and gets
  `poolPosts` 161→172 (+11) and `meshAdmitted` 130→135 (+5) against 6.6 predicted;
  `demo/crossing` covers 2 252 m and gets `poolPosts` 217→238 (+21) and `meshAdmitted` 130→159 (+29)
  against 29.7 predicted. Same order both times, and the second is on the number. **Delete this bullet once `board/active/` items 2 and 3
  are restated** — it is kept only so that the walk gate is not built to catch a defect that is not
  there. What is genuinely unmeasured is *latency*: how far the eye travels between a tile entering
  the target cut and its mesh being drawable. No column carries it.
- **22 950 KiB of the heap has no owner at rest.** `heapResidualKB` is now measured, not inferred: 52 650–73 564 KiB at t=1, up to 94 132 KiB mid-load, settling at 22 950 KiB in three of five runs of `dfdd8e3a82efeefc` (a deleted run log `*.csv`). The eight pthread stacks account for ~2 MiB of it. `prototypeKB` 6 527 and `standsKB` 1 676, logged at `outshine stands_collected`, are CPU-side and in no ledger column — the first place to look, and a decidable one: give them a column and the residual must fall by their sum or the overlap is somewhere else.
- **Nothing evicts.** `BuildingField`, `WaterField` and `StreetField` grow monotonically and their unit of removal does not exist. At 545 KiB of building heap per tile and ~29 MiB of real headroom, that is on the order of fifty tiles before exhaustion.
- **The in-cone priority boost is multiplicative at 20×** (`world/World.cpp:181`, 1.0 against 0.05). The reference adds a capped 0.5 to a 10-point scale and documents 1.0 as the setting that produces thrash.
- **`kGrace = 180` is counted in passes** (`world/World.cpp:32`) — 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds.
- **The byte cache finds its LRU victim by linear scan under a held lock** (`world/TilePool.cpp:236-240`), n ≈ 600 at 64 MiB of z14 tiles.
- **`World::Refine` builds no intermediate level** (`world/World.cpp:241`, and the traversal's own comment at 246) — correct for a cold start, wrong for travel, because there is then no ancestor rung to hold coverage while a fine rung streams.
- **`Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.**
- **A crossing costs +1.77 ms at p50** against its neighbourhood, 1.03 of it the ring's own snapshot — in no column, because `Populate` runs after `Refine` inside one function.
- **A failed allocation is reported as a refused tile instead of ending the run.** `world/TilePool.cpp` `RunMesh` reaches `Miss::Refused` when `ChunkBuildEcef` returns 0, and that function returns 0 from **three `malloc` failures** (`world/ChunkMesh.h:52,95,153` — see *An exhausted heap is reported as malformed terrain*, whose consequence this line states). An exhausted heap is not a statement about a tile and must not be reported as one. **The terminal-hole half of this is closed** (2026-08-12): the global `Classify` that turned every 4xx into `Absent` is gone — status-to-meaning is per source and declared now (`data/TerrariumDem.cpp`) — the thread-local `tMiss` is gone with it, and `RunMesh` maps only `Miss::Hole` to `Reply::Absent` while a refusal is `Reply::Refused`, which `World::AdmitMesh` retries rather than retracting the split. What is left is the allocation: it should reach `Heap::Exhausted` like the OOM path beside it, not the mesh verdict.
- **`poolPosts` and `poolRepeats` still wrap on wasm32.** The round that widened every counter to `long long` left `long Posts_ = 0, Repeats_ = 0` (`world/TilePool.h:177`) — the accumulators — and widened only the `Ledger` fields they are copied into, so the column is 64-bit-typed and 32-bit-valued. These are the two fastest counters in the tree: `poolRepeats` reaches **2 201 113 194 = 1.025 × 2³¹** in 2 868 rows of a deleted run log. That run shows no negative value because `walk` is the native oracle, where `long` is 64-bit — **the measurement used to clear the defect is the one build that cannot see it**. Signed overflow is UB (`ES.103`), not a wrapped column. Right: `long long`, and every 32-bit accumulator behind a 64-bit column found the same way — by type, not by looking at native output.
- **A completed mesh nobody asks for again is retained for the life of the pool.** `TilePool::Poll` erases a `Done_` entry only when a caller polls for it; a node that stops asking — which is exactly what the new retraction makes happen to a sibling whose build was in flight — leaves a full `TileBuild` (verts + indices + clusters, ≈ 4 MB at `kGrid = 128`) in the map for ever. Decidable from the same run: `tilepool_closed` reports `meshTiles=131 meshAbsent=1`, i.e. 130 completed builds, against `built=129` uploads (`world fbworld`, `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/hole/afterF.log`). The round cleared `Build` for the `Absent` arm and not for this one. Right: the pool drops a `Ready` result whose key left the caller's cut, which needs the cut to be a thing the pool can be told about — see the cut-once shape below.
- **The drawn cut and the counted cut are two implementations that must agree, and nothing makes them.** `World::Descend` decides coverage with `Ready`, `World::CountTargets` decides progress with `Settled` (`Ready || Vacant`), and both re-derive the same tree from `Splits`. They agree today only because `Splits` removes a vacant child from *both* walks; delete that one call from `CountTargets` and progress reaches 1.0 over a square the draw pass leaves empty — the silent-hole failure, one edit away, with no test and no identity that catches it. `CanCover` makes it worse structurally: `Descend` calls it per child and it re-walks the whole subtree, so one pass is O(N·depth) node visits with a hash lookup each (`Splits` calls `Find` although `Descend` already holds the index), ~800 visits per root ring at ~1 800 passes/s during load. Right: **the cut is computed once per pass** into `(idx, role)` — target leaf · holder · drawn — and `Descend`, `CountTargets` and the request walk read it. Then two walks disagreeing is unspellable, the retraction is one rule in one place, and the duplicated `anyV`/`Wants`/`Ready`/`Emit` block the retraction added to `Descend` disappears with it.
- **A DEM hole deletes the built world standing on it, although the ground under it is drawn.** Measured 2026-08-11 over a synthesised hole (one z14 terrain tile, 8620/5404, answered 204 by a proxy in front of `fb-tiles`): the terrain cut retracts to the z13 parent and the picture is continuous, but `buildingVerts` falls 405 504 → 170 601 and one whole region grows nothing (`sim` `region_without_ground`, `clients/Sim.cpp` `Ask`). The cause is one answer used for two questions: `world/BuildingField.cpp:235` drops a footprint whose corner heights do not resolve (`NoGround_++`), and a region whose ground block is Missing is refused. But a height for that place **does** exist — it is the one the picture draws, the coarse ancestor's. Right: a place with no tile at the finest rung reads the finest rung that HAS one, so the footprint and the stand stand on the surface the eye sees; today the oracle answers only at `gSurface.Z` (`world/TerrainLoader.cpp:301`, *"no other zoom of this surface exists"*).
- **An absent tile is remembered for the life of the pool and nothing removes it.** `world/TilePool.cpp` `Poll` keeps the `Absent` result in `Done_` and the key in `Posted_` — that is what makes the answer final and stops a thread being spun on it — but nothing evicts either table, so a flight over a large hole grows both without bound. Bounded today by the number of distinct absent tiles a run asks about, which is 1 in every measured run. Right: the same unit of removal eviction needs everywhere else (see *Nothing evicts*), not a second mechanism.
- **The per-pass build budget bounds installs, not asks.** `world/World.cpp:390-417` decrements `budget` only in the `Ready` arm, so a pass the pool cannot answer asks **every** candidate and spends nothing: the cost of a stalled pass is O(wanted), not O(2). Measured over `demo/crossing` (900 frames plus load, a deleted run log): `meshCapped` 217 against `meshWanted` 2 029 402, i.e. 0.011 % of wants. Two separate things are wrong: (a) the ask is unbounded, which is what `board/` § 0.2 calls the missing second cap — *how many may start* per update; (b) even as an install cap, 2 is not the binding constraint and neither is the in-flight cap. The binding constraint is **CPU inside the mesh build**: `world tilepool_closed` for the same run reports `meshCpuMsPerTile = 237.29` over 4 threads = 16.9 tiles/s, against a measured drain of 12–13/s (`poolQueued` 116→0 while `meshAdmitted` 11→130 in 9 s) and 118/s admissible at 59 fps. Do not conclude that the cap is useless — it is the only bound on a warm-cache teleport; conclude that it is measured against the wrong thing, and that a queue that empties is not a pool that is fast.
- **The load loop polls the pool ~190 000 times a second** (`poolRepeats` 2 069 319 against `poolPosts` 196, same run). Attribution matters and the earlier phrasing had it wrong: **99.99 % of the repeats happen before residency, not during play.** In `demo/walk-500` `poolRepeats` is 1 953 923 at t=10 s (residency) and 1 954 287 at t=183 s — 364 repeats in 173 s of walking against 1.95 M in 10 s of loading. The load spins `Refine` at ~1 800 passes/s × ~119 unready leaves; every one of those takes `QueueMutex_` and attempts a `std::set<uint64_t>` insert on the thread that draws, against the four workers that need the same mutex to pop (`world tilepool threads=4 inFlightCap=4`). The host was loaded during the measurement, which makes 190 kHz a **lower** bound. Right: a pending ask answerable without the queue's lock — a per-node "already posted" flag in the node, or an atomic set — and a load loop that is not a spin.
- **`TileEnuMap` answers with a plausible wrong place instead of refusing.** Every other type in
  `world/tiles/TileGeodesy.h` hides its payload behind a state — `TileIndex::TryXy`, `EnuFrame::
  TryFromGeo`, `TerrainGrid::TryField`, `TerrainMesh::TryPositionsEnuM`. `TileEnuMap` (`:153-175`) has
  no state, a private all-zero default body, and `Apply` is a plain total function; its own comment
  argues for it — *"A map over an unusable frame carries zero scale, so it places every vertex at the
  origin rather than somewhere plausible."* Placing every vertex of a tile at the frame origin **is**
  the plausible-looking wrong answer, and it is unreachable to a caller: nothing distinguishes a
  degenerate map from a good one. The guarantee the other four types establish leaks at the one seam
  that converts per-vertex positions. Right: `EnuFrame` exists only in its usable state — `At` returns
  the state-carrying answer and the frame is obtained through it — after which `TileEnuMap::Over` is
  total, `TryFromGeo`/`TryToGeo` lose their `bool`, and the degenerate map has no spelling (`C.41`,
  `C.42`).
- **The producer knows the grid's width and the consumer guesses it back with a 0.5 m threshold.**
  `world/tiles/TerrainGrid.cpp:61-62` computes `rowsOut`/`colsOut` and `TerrainMesh` keeps neither;
  `world/ChunkMesh.h:31-38` then recovers the column count by scanning for the first easting that
  decreases by more than `0.5f` metres and refuses the tile as *soup* when it finds none. The constant
  is a magic number (`ES.45`) standing in for an extent that was exact one call earlier, and it fails
  silently wherever the posting spacing drops under 0.5 m — at z14 the spacing is ~4.8 m, so this is a
  latent bound on stride and zoom rather than a live wrong picture. Right: `TerrainMesh` carries `Rows()`
  and `Cols()` beside `VertexCount()` (`F.21`), and `ChunkBuildEcef` takes them.
