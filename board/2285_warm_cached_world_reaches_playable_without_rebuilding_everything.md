Type: performance
State: active
Architecture: ready
Parent: 2105
Depends:
Priority: P0
Area: world, data, generators, engine, client
Tags: startup, streaming, cache, measurement

# A warm cached world reaches Playable without rebuilding everything

## Evidence and target

The client uses `/tmp/outshine-drive-cache`; `SourceSet` can return bytes from
`ContentStore` before starting a transport request and counts `FromStore`.
`TilePool` also has a per-process byte cache. Neither cache-hit count nor the
critical path from decoded source to first Playable frame is reported by the
client. Repeated Feldkirch `run` attempts reached the 30 s preload limit with
vegetation and ground-candidate work pending. Their actual disk-cache hit
ratio is unknown; do not label them warm benchmarks. A Koerbersee shot with
vegetation disabled completed, but it is not a full-world warmstart proof.

The product contract is low-latency playable contact from a complete warm
cache, followed by bounded visual refinement while the world is already
interactive. A source-cache hit must never wait for network. A native-product
cache may skip deterministic decode/generation only when its key includes
source revisions, generator/schema version, projection and material settings;
never publish mixed revisions or stale geometry.

## Executable architecture

1. Expose `SourceSet::Counters` and `ContentStore::Counters` through internal
   `GroundStack` diagnostics, then `Engine::loading()` value snapshots. Count
   provider starts at the `Source::Begin` call, including retries; distinguish
   these from tile-pool hits and disk-store hits. `outshine-client run --stats`
   and `shots --stats` emit stable, compact `STAT` TSV rows on success **and
   timeout**: cache hits/misses, provider starts/retries, delivered bytes,
   readiness, elapsed preparation, outstanding tiles and terminal reason.
   `--help` documents names and units. Existing `--measures` remains the full
   stage-level diagnostic channel; add missing stage timings there as measured.
2. Build a deterministic offline fixture with a complete persistent raw-tile
   cache. Run in a fresh process twice with the same source versions, then
   change one source revision. Warm run requires zero transport starts, all
   needed bytes from store, equal native outputs, and no unexplained 30 s
   wait. A disabled/empty cache is the negative control.
3. Measure byte counts and sustained local read/decode/generate/upload rates
   on the exact fixture. Calculate a critical-path lower bound from the stage
   dependency graph and measured rates; report actual/lower-bound ratio at
   p50/p95. Set numeric time budgets only from that evidence, with separate
   first Playable and Refined targets. Optimize the slowest stage first:
   bounded parallel decode/ingest, reuse unchanged native products, batch GPU
   uploads, and keep IO/compute work ahead of the moving focus.
4. Persist immutable native products only where profiling proves regeneration
   dominates read cost. Atomic write/rename, key validation, bounded eviction
   and corruption fallback are required. Repeated source-cache and
   native-cache runs must produce the same geometry/material/readiness.

## Acceptance

- Fresh-process warm-cache startup and moving-camera stream report zero
  network starts for covered tiles; forced source miss starts the provider.
- First Playable time, Refined time, p50/p95/p99 frame time, peak CPU/GPU
  memory, bytes and stage throughput are reported with cache provenance.
- `--stats` also prints after a failed preload; `--help` describes each field
  without requiring a successful world startup.
- Warm native-product reuse is visibly faster than raw-cache regeneration
  on the same fixture; no stale output after source/schema change.
- Focused cache, streaming, and scene tests; `make format`; `make lint`.
