# Adaptive building cells before initial publication

Owner: [#567](https://github.com/Weichwolf/outshine/issues/567).

Commit `2221f1a5c` separates spatial selection from detail policy: a requested
cell may retain the existing automatic detail choice. The former validation
required an explicit LOD. No schema, cache key version or GPU binding changes;
previous fixed-detail and whole-tile paths keep their behavior.

Commit `24a174172` adds atomic publication of adaptive cell sets through the
existing StageCell/ActivateCells protocol. Nineteen actual depth-rendering checks
cover missing/unkeyed children, preservation of the old revision, initial
publication without a whole mesh, view readiness and switching to fixed details
and back. Three existing publication cases remain green; the implementation,
new test and 21 dependent units pass focused tidy. This is the publication
contract, not the remaining initial-build integration.

The new 57-check regression covers complete enriched source footprints, exact
near-wall positions/UVs/normals, distant coarse selection, disjoint cell coverage
and artifact roundtrip. Three existing selection cases pass unchanged. The
baseline rejects both adaptive cells. Production and new test pass focused tidy.

Cell subdivision alone is not a size guarantee. The 20-source model produces
1,880 triangles either way, yet its largest child is 131,398 bytes versus
110,034 bytes for the whole product. The original smaller-child assertion was
therefore false and replaced by the actual coverage/valid-payload requirements;
the measured counterexample remains in the test output. Global projection can
share work across cells, and partitioning must retain a measured cost comparison.

## Real cached inputs

Five existing Tokyo-area prepared indexes were read through the native codec.
No network requests or mutation of the shared cache. A private SQLite subset
retains the original asset records and package blobs.

| Sources | Occupied cells | Largest source count per cell |
|---:|---:|---:|
| 10,289 | 64 | 344 |
| 7,912 | 64 | 325 |
| 12,013 | 64 | 522 |
| 16,618 | 64 | 459 |
| 7,982 | 63 | 365 |

The 7,912-source index, key
`d2dcba37407c658bc64bc58d7545bca9b77a2296d2ca8a7e4b232af665c93bca`,
was generated as 64 adaptive cell products. All 7,912 source footprints survive
in the metadata; 15 meshes report unsupported geometry. Complete source metadata
does not prove that every building is drawn. The products contain 411,081 triangles. Largest encoded child: 23,536,791 bytes; total: 37,326,420 bytes.
All 64 products were stored and replayed byte for byte; zero shape misses.

A fresh process then loads those 64 cached products: 0.38 s real / 0.10 s user /
0.01 s system, 72,565,360-byte peak footprint. Geometry hits 64, writes 0;
37,326,420 geometry bytes read, source-shape bytes 0. Apple A18 Pro, normal
priority with a concurrent low-priority frozen API lint. This includes CPU/cache
loading, not GPU uploads, scene publication, a frame or complete-world readiness.

This experiment uses the existing Tokyo camera longitude/latitude, explicit
140 m ASL and focal length `1080 / (2 * tan(55*pi/360))`, one-pixel allowance.
It is a different cached input/view from the failed 8,068-source Linux product;
it does not repair or accept that Place. Private sources, commands, original
records and logs: system-temp `outshine-decoded-mesh-ce3d84a8c`.

## Engine integration

Commit `71f92310d` connects initial native source admission to the cell producer.
The queue retains the current child mesh, gathers source/contact metadata and
accepts it once after staging the complete source. Initial publication uses the
same atomic cell activation as refinement. Restarting a source discards hidden
children while retaining the old visible revision. Empty/fallback/non-native
paths keep their existing behavior. Unknown surface errors stay unknown.

The new producer case has 35 checks for cold child writes, fresh child hits,
complete metadata, cancellation and reuse of an existing bounded whole product.
The real queue replay has 184 checks including two-cell cold/warm admission;
publication has 20 actual depth checks. The existing revision and worker-lifetime
cases remain green. Five changed implementation units and 25 dependent units
pass focused tidy; the new producer case is clean under focused analysis.
These results are not a complete Place or native-backend acceptance.

The first wiring selected children before probing existing whole cache products.
That incorrectly replaced valid hits with new work. Commit `d6dc5517d` restores
the direct whole-product hit. Only a miss selects children before emission; no
oversized whole product is generated first and the 64-MiB limit stays unchanged.

## Public runtime scope and remaining costs

The initial Tokyo 1280×720/30 attempt exhausted its 10-second preload while new
children were being generated. A subsequent default-profile `prepare Tokyo 180`
became ready in 151.31 seconds, with 5,191,307,368-byte peak process footprint.
It includes complete-world preparation and a one-second sampling probe, not
isolated geometry generation. A fresh child-only attempt then still exhausted
10 seconds at 45/46 accepted source tiles, zero meshing ranges. Child loading,
upload and orchestration remain a runtime performance problem.

After restoring existing whole-product hits, fresh offline Tokyo and Wien
1280×720/30 processes load in 1,913.3115 and 1,567.3445 ms. CPU p99 is 3.7517 and
3.7510 ms; both PNGs are byte-identical to the previous successful captures.
GPU time remains unavailable on the main SDL_GPU path. These are single runs,
not a robust speedup or a child-only loading-budget proof. The failed 8,068-source
Linux 1920×1080/60 case and full native port acceptance remain open in #567/#328.

