# Established asset-database and residency patterns

Reviewed 2026-11-19. Reference material; current decisions and implementation status live in [#1968](https://github.com/Weichwolf/outshine/issues/1968).

| Primary source | Named method / use | Limit |
|---|---|---|
| Epic [Derived Data Cache](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine) | Hierarchical derived-product lookup, regeneration on miss, asynchronous storage | Cooked UE games do not need an editor DDC. Outshine combines established lookup/build patterns with its own runtime generator, not a new database algorithm. |
| Epic [Zen Loader](https://dev.epicgames.com/documentation/unreal-engine/zen-loader-in-unreal-engine) | Chunk-addressed cooked data, prepared dependencies, reduced load-time object work | Do not import the full UE container/service. Keep independently needed native products separately readable. |
| Epic [World Partition HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine) | Spatial cells and prepared proxy products | The proxy is a visual product, not the functional transport/building plan. |
| SQLite [R*Tree](https://sqlite.org/rtree.html) | Bounding-box candidate queries before payload decode | Floating bounds round outward; contained queries need appropriate expansion. R*Tree candidates still need exact demand checks; avoid modifying an actively scanned index. |
| SQLite [WAL](https://sqlite.org/wal.html) | Consistent readers and atomic publication alongside a writer | One writer at a time; checkpoints and long-lived readers affect WAL growth. Use local storage and bounded transactions. |
| SQLite [VACUUM](https://sqlite.org/lang_vacuum.html), [incremental_vacuum](https://sqlite.org/pragma.html#pragma_incremental_vacuum) | Distinguish deleted payload from physical file reclamation | Full VACUUM may require substantial temporary space. Background bounded reclamation and WAL accounting are part of capacity planning. |

## Established implementation pattern

1. Spatial/type/LOD metadata selects demand; independently readable ready products satisfy hits.
2. Coalesce misses; generate requested products asynchronously and publish them atomically.
3. Preserve coarse coverage until replacement products are available.
4. Bound queued work, decoded memory, staging, GPU residency and physical SSD use separately.
5. Evict regenerable unused products using usage/cost-aware policy; protect active readers and last GPU use.
6. Store accepted recipes and permanent world decisions separately from disposable products.

Small prototypes can compare spatial lookup, packing, admission and eviction. SQLite and the current storage libraries are useful controls for correctness, latency and memory.

Integration limits: SSD is not free RAM; IO, decode, decompression and UMA traffic remain measurable work. The existing Outshine file budget/reclaimer is a candidate to retain, not proof that every source/cache/residency path is already bounded.

See [prepared-assets](unreal/prepared-assets.md) for the earlier source review and [visibility-driven-demand](visibility-driven-demand.md) for Cesium/MapLibre selection.
