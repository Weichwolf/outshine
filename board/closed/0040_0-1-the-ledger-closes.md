Type: feature
Area: clients
Tags: perf, instrument

**0.1 The ledger closes**

- [ ] Heap, per-pool and device bytes on one telemetry row once a second (`clients/MemoryTelemetry.cpp`) — **unbuilt**: the client that carried this was deleted with the browser-era clients; the line is scope again
- [x] Stream ledger: fetch, decode, mesh, DAG, residency and evictions, cumulative and never behind a switch (`clients/StreamTelemetry.cpp`, `world/TilePool.h::Ledger`)
- [x] `heapKB` as live allocated bytes (`core/io/HeapProbe.cpp` — dlmalloc `mallinfo().uordblks`, guarded at static init by the identity `uordblks + fordblks == arena + hblkhd`; the column is EMPTY, never zero, if that fails). 14 336 000 B allocated reports 14 337 120 B, the excess being 140 chunk headers × 8 B; in a live run `heapKB` falls 35 times in 138 rows where the break falls never
- [ ] The wasm break published beside live bytes as its own column, since the two answer different questions (`clients/MemoryTelemetry.cpp`, `heapBreakKB`) — **unbuilt**: the client that carried this was deleted with the browser-era clients; the line is scope again
- [ ] A residual column, published rather than subtracted by the reader (`clients/MemoryTelemetry.cpp`, `heapResidualKB` = `heapKB − poolSumKB` from one heap walk per row) — 51 972 / 66 252 / 22 956 KiB at t=1/11/31 s, peak 90 607, non-monotone in the record — DECIDABLE — **unbuilt**: the client that carried this was deleted with the browser-era clients; the line is scope again
- [ ] The residual under a declared ceiling, with every allocation above 1 MiB inside a named pool — DECIDABLE
- [ ] The byte cache inside `poolSumKB` (`world/World.h`, `Pools::ByteCache`, and `Pools::Sum` adds all nine fields). It is *not* unspellable: a tenth field omitted from `Sum()` compiles, and `poolRegionsKB` is a published pool that is not a `Pools` field at all and is folded in by hand in `clients/MemoryTelemetry.cpp`. `C.41` does not bear on this — see the registry line below — **unbuilt**: the client that carried this was deleted with the browser-era clients; the line is scope again
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


---

**Closed as stale (2026-08-22).** The browser memory ledger is deleted; allocator counting landed and the steady-state scenario case carries the acceptance.
