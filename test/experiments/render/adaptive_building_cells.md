# Adaptive building cells before initial publication

Owner: [#567](https://github.com/Weichwolf/outshine/issues/567).

Commit `2221f1a5c` separates spatial selection from detail policy: a requested
cell may retain the existing automatic detail choice. The former validation
required an explicit LOD. No schema, cache key version or GPU binding changes;
previous fixed-detail and whole-tile paths keep their behavior.

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
was generated as 64 adaptive cell products. All sources survive; 411,081
triangles. Largest encoded child: 23,536,791 bytes; total: 37,326,420 bytes.
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

The remaining integration loads source/contact metadata before geometry, stages
bounded children and atomically publishes the complete cell set. Warm hits must
repeat neither source enrichment nor meshing. The current engine still requires
a whole initial mesh. Unknown surface errors stay unknown, and old valid world
state must survive cancellation, retry or failed child publication.
