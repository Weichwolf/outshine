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
`ContentStore` before starting a transport request. `TilePool` also has a
per-process byte cache. `Loading` and client `STAT` rows now report persistent
store and provider counters plus completed preload duration. A fresh process
capturing Hockenheimring from its existing disk cache used 477 store hits,
25,186,276 delivered bytes and zero provider/remote starts: first Playable
preload 3.056 s, full Refined shot 8.692 s with 1332 measured frames. The
stage ledger reports 407 structure-bake advances. These are wall-time and
frame-count evidence, not a CPU critical-path breakdown. Repeated Feldkirch
attempts reached the 30 s preload limit, but their cache provenance was not
recorded; do not label them warm benchmarks.

The same Hockenheim shot with `--offline --cache-dir /tmp/outshine-drive-cache`
kept digest `16908acb`, 477 store hits, zero misses and zero provider starts.
An empty isolated offline directory yielded zero hits, 36 misses and 36
provider starts before its 0.2 s preload limit. These local probes establish
root isolation and provenance; a repository-owned complete fixture is still
required for a portable warmstart benchmark.

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
   cache. `run` and `shots` accept `--cache-dir <path>` and `--offline` before
   the scenario/place name; both configure the same public `Roots` contract.
   Empty paths are rejected before engine creation. Offline prohibits actual
   transport, but `provider_starts` still counts a source attempting `Begin`
   after a miss; zero is required for complete warm coverage. Run in a fresh
   process twice with the same source versions, then change one source
   revision. Warm run requires zero provider starts, all needed bytes from
   store, equal native outputs, and no unexplained 30 s wait. An empty cache
   is the negative control and must not silently fall back to a global cache.
3. Measure byte counts and sustained local read/decode/generate/upload rates
   on the exact fixture. Calculate a critical-path lower bound from the stage
   dependency graph and measured rates; report actual/lower-bound ratio at
   p50/p95. Set numeric time budgets only from that evidence, with separate
   first Playable and Refined targets. Optimize the slowest stage first:
   bounded parallel decode/ingest, reuse unchanged native products, batch GPU
   uploads, and keep IO/compute work ahead of the moving focus.
   First attribute each `Engine::preload` call's wall time to pump, candidate
   flush, and await, with call counts in `Loading`/client `STAT`. Sum must not
   exceed `preload_ms`; residual includes callback and loop overhead. Record
   five fresh-process warm Hockenheim runs before optimizing a phase. This
   accounting lives outside the frame path and allocates nothing per cycle.
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
