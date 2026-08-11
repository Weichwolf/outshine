# Bugs

**What belongs here.** Something that exists and is wrong. `doc/requirements.md` says what must exist
and an unticked line there means *not built*; a line here means *built and broken*. If it has never
worked, it is a requirement. If it worked, or looks like it works, it is a bug.

**A line carries where it is and what decides it** — file and site, the measurement or the picture that
shows it, and what right would look like. A bug without a way to tell it is fixed is a rumour.

**A fixed bug is deleted, not struck through.** `git log` is the record.

---

## Silent success — a call that answers and nobody reads

The most expensive class in this tree, three instances found in one day, all with the same shape.

- **`RoofSurface::Cover` returns `void`, and `EarClip` bails silently** (`generators/draw/RoofSurface.cpp:32`, called at 209). When triangulation fails, `Covering` loops over an empty vector and draws nothing, then `BuildingMesh.cpp:383-387` draws `Gables`, `Eaves` and `Chimney` unconditionally — **a roof drawn as its own trim, floating in the sky with no covering**, visible in the shipped frame at (930,240)–(1190,370) of `after/street.png`. Right: `[[nodiscard]] bool Cover(...)`, and eaves, gable and chimney unspellable without a closed covering.
- **`emscripten_run_script_string` can return null and is not checked** (`clients/AppWasm.cpp:271`).
- **`emscripten_exit_pointerlock()` discards its result** (`clients/AppWasm.cpp:92`).

## Buildings

- **A dome on a rectilinear plan.** `ReadsAsRound` (`generators/draw/BuildingShape.cpp:258`) keys on corner count, fill and squareness — all of which a stepped rectilinear plan satisfies. Visible as a smooth 390 px arc over the town in `after/town.png` (620,255)–(1010,330). Right: add a turning test, every exterior angle ≤ 60°.
- **One ridge kinks over a bent plan.** `MassOf` applies Row → Wing → Setback once, top-down, to the whole. A bent bar fails `RowCut`'s `Fill ≥ 0.80`, is winged into two long masses, and neither piece is offered to `RowCut` again. Same defect on a T-plan. Right: a work list, ~15 lines.
- **The chimney reads as a 5 m post.** `BuildingMesh.cpp:338` runs the box from eaves to ridge + 0.85 m. The 0.85 above the ridge is correct practice; the box must terminate at the roof plane, not at the eaves.
- **The pavement's flag grid degenerates on inhabited ground.** `render/stages/BuildingDraw.cpp:339-340` builds its basis from `cross(upv, vec3f(0.577,0.577,0.577))`. That rotates with position on the globe, so flags never align with the kerb, and it is **exactly degenerate where up ∥ (1,1,1)/√3 — geocentric 35.264 N, 45.000 E**, near Kirkuk–Sulaymaniyah: `normalize` of ~0 → NaN. Root cause is the encoding: `uv.x < 0` spends the whole float on kind plus identity, leaving one metre coordinate, so any non-wall surface needing a 2-D pattern must invent a frame in the shader.
- **The footway ends mid-frontage** (`generators/draw/BuildingMesh.cpp:413-414`) — a per-edge binary accept/reject on a continuous stand-back. A consequence of the footway belonging to the building instead of to the street.
- **A wrong reason defends a right number.** `BuildingMesh.cpp` states "38–45 is the German pantile range (below 22 a pantile does not seal)". The ZVDH Regeldachneigung for a Hohlpfanne is H1 ≥ 35°, H2 ≥ 40°, and the absolute minimum for Ziegel is 10° with additional measures. 22° is neither. `kPitchOutbuildingDeg = 22` may be right; its stated reason is not. Likewise `MinAreaBox`'s comment "on L, T, U and notched bars every hull edge is a ring edge" is false — an L's hull has a chord.

## World and streaming

- **Nothing streams during play.** From t=31 s to t=77 s of `sim/logs/demo-walk-wasm-20260811T150518Z.csv`: `poolHttpGets` 310 flat, `poolFetchedMB` 28.36 flat, `tilesTotal` 130 flat, `tilesBuilt` 0, `tilesEvicted` and `poolEvictions` 0. `TargetTot` is recomputed from the eye every pass (`world/World.cpp:411-419`) and did not move by ±1 in 46 s, while the direction-sensitive `tilesInView` did move 45→47 — so orientation reached `Refine` and position did not. **The record cannot decide it**, because the telemetry carries no camera.
- **`heapKB` is `sbrk(0)`, a break that never falls** (`core/io/HeapProbe.cpp:25`). Proof inside one run: `heapKB` frozen at 237 096 from t=13 to t=77 while `byteCacheKB` grows 14 727 → 28 943. Every conclusion from that column's flatness is unsupported, and eviction — once built — will be invisible in the ledger.
- **`poolSumKB` omits `byteCacheKB`**, a published column, which is 29 % of the apparent accounting gap. The genuinely unattributed residual is 71 216 KiB, and it is non-monotone (42 886 at t=1, 88 438 at t=11, 71 216 at t=31), so it is not static data.
- **Nothing evicts.** `BuildingField`, `WaterField` and `StreetField` grow monotonically and their unit of removal does not exist. At 545 KiB of building heap per tile and ~29 MiB of real headroom, that is on the order of fifty tiles before exhaustion.
- **The in-cone priority boost is multiplicative at 20×** (`world/World.cpp:181`, 1.0 against 0.05). The reference adds a capped 0.5 to a 10-point scale and documents 1.0 as the setting that produces thrash.
- **`kGrace = 180` is counted in passes** (`world/World.cpp:32`) — 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds.
- **The byte cache finds its LRU victim by linear scan under a held lock** (`world/TilePool.cpp:236-240`), n ≈ 600 at 64 MiB of z14 tiles.
- **`World::Refine` builds no intermediate level** (`world/World.cpp:241`, and the traversal's own comment at 246) — correct for a cold start, wrong for travel, because there is then no ancestor rung to hold coverage while a fine rung streams.
- **`Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.**
- **A crossing costs +1.77 ms at p50** against its neighbourhood, 1.03 of it the ring's own snapshot — in no column, because `Populate` runs after `Refine` inside one function.

## Picture

- **The demo road reads as a dirt track** since the unmapped substrate landed: the ground fragment uses the default row as the **runner-up** class where the structure has no second hit.
- **Crowns are too transparent at 30–80 m.** A stand reads as a wall of white trunks with a green fringe; no canopy closure. Opacity, not form.
- **The bow-tie crown persists**, reduced but not eliminated — two crowns in `horizon-after.png` still show a straight diagonal seam.
- **A near trunk reads as a straight grey slab**, not as a beech.
- **The hornbeam hedge reads as a young plantation** — ten stems in a row with a bare lower third. The grower has no cut response, which is also what blocks coppice stool and pollard.
- **Leaf lamina is wrong on small dense plants** — box comes out ~8 cm against a real 2 cm, because `CardLeafM` solves LAI ÷ crown projection ÷ card count and few cards means huge leaves.

## Declaration and build

- **`core/ClusterDag.h:72` reads `FB_TAU` from the environment** — the picture depends on an undeclared variable.
- **The winding is hard-coded at seven sites**; it belongs in the draw product beside the cluster list.
- **A stage that reports the same number every frame is reporting that it is not being measured.** In `after/town-spin.csv` (a rotation about a fixed point) `worldMs`, `meshMs`, `uploadMs`, `buildingMs`, `classMs`, `populateMs`, `nodes`, `drawnLeaves`, `draws`, `built` and `evicted` are all constant across 240 rows, and `distM` is 0.000 in every one — that run is not motion acceptance and was reported as if it were. Note the general form of the claim is false: of 89 columns in a live walk, 35 are constant and most legitimately so (identity, declared limits, stack ceilings). What is suspect is a *cost* column that does not move.
- **The log's timestamp is dead** — every `walk key` line carries `t=0.0` — and key repeat events are logged individually, so a held key floods the buffer.
- **`FacadeUv.h` has no `static_assert` anywhere**: 11 enumerators against a stride of 16, `kStyleCount 8` against 7 enumerators. A 17th `Facade` silently aliases identity 1.
- **`TreeGrower::GrowOnce` is ~130 lines** (`F.2`/`F.3`), and `TreeSpecies::Parse` is a 90-line flat key list (`F.3`).
