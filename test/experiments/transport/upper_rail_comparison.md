# Physical rails on selected upper Vienna decks (#352)

The existing Python viaduct had closed decks and piers but no upper rail hardware.
This experiment reuses the installed horizontal profile kernel in metric bed
planes, preserving the complete #340 lower network as the exact body prefix.
All new geometry is procedural: one standard-gauge track per explicitly chosen
`transit/subway` source axis. This is an interpretation, not surveyed track count,
steel/concrete specifications, source reconstruction or a complete train route.

## Method and primary sources

First consulted the local [Chen et al., SIGGRAPH2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf), p2/Fig2
(graph and generated geometry separated), then [Galin et al., Eurographics2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf), p8/Fig17–18
(profiles and bridge/terrain recipes). Both pages were optically examined.
Neither paper supplies this rail kernel or its contact selection algorithm.
Existing own rail section/dimensions: [library contract](../../../lib/rail_hardware.md).
No foreign code or SDK/runtime dependency is added.

`rail_bed_patches.py` builds adjacency from shared finite mesh edges, grows one
connected patch against a fixed seed plane and checks **every member vertex**
within 1e-8 m vertical error. Affinity makes that a whole-face error bound.
Comparison against the fixed seed prevents transitive plane drift. Holes and
separate islands remain; no noncoplanar crease is smoothed. Same-plane merging
before the 1.35 m inset avoids false construction gaps along triangulation edges.
`triangle` retains every internal edge with the identical inset/physical criteria.
Both methods retain switch/crossing/end gaps and original axes/properties/IDs.

Actual rigid rail/sleeper generation uses #340, unchanged dimensions and gauge
semantics. Upper sources are selected explicitly from the earlier bridge fixture;
they are not reclassified as lower rail graph edges. Absent gauge tags use the
procedural 1.435 m default. Shared public APIs/Plan/Reservation remain unchanged.
The new patch/upper adapter remains experiment code; library migration and actual
consumer binding are separate work.

## Actual inputs and provenance

Generation, both export replays, selection measurements and all three renders:
clean frozen **3d82488b8bb05cd97b69b21830f2e7eae05a734a**. Later audit adds a
whole-section width test, including a gauge/volume-preserving shear counterexample;
final replay commit/result is recorded below. No generated body was rewritten.
Reference inputs retain the original OpenFreeMap/DEM receipt chain of #128/#139;
ENU origin48.21/16.37. No network refresh or SDK load performed.

| Input | SHA256 |
|---|---|
| `fixtures/bridge-continuations-vienna.json` | `fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4` |
| original upper solid | `72876259e66dd11ee3a2abeaa691d7134d8ec7d1b0a0852c2e237209c3324fec` |
| original upper report | `1ccf3d72505c0bc5bf5a2d8200b3c7a3298861e64ff78011c2b756bdc858cb55` |
| #340 baseline report | `69fb6e3e7fcd8cdcac98642c570d6a2e3f9200d1c7a984a024e73e0918d2cf0b` |
| #340 complete baseline body | `79c8d4c23dde0717b26410dbd150368cde55922bb94c1805f8a9b80687044c59` |
| selected patch final body | `19c392460839a4a7aebaec570982190d2ea18fccc3bd5aedac792be690697a4b` |

Original upper source generator5fe93d407862a1ed086a009b3523da097fd8849b;
468 top facets/438 vertices. 19 original source axes total7,414.019 m XY;
existing conservative mutual-crossing selection leaves5,445.995 m in23 pieces.
Original complete3259-source usage ledger remains at #303, unchanged by this work;
new piece provenance refers to original upper source IDs. New rows do not imply
new source-use credit, exact OSM track multiplicity or whole-source route coverage.

## Equal physical requirements, different useful coverage

| Quantity | Single triangles | Connected affine patches |
|---|---:|---:|
| Contact domains | 468 | 51 |
| Seed-plane whole-face tests | 468 | 523 |
| Patch/axis candidates | 340 | 67 |
| Candidate axis pieces | 238 | 66 |
| Candidate XY length m | 1,521.147 | 5,315.275 |
| Actual admitted pieces | 224 | 56 |
| Actual 3D centreline m | 1,475.858 | 5,131.497 |
| Whole-baseline bound rejects | 14 | 10 |
| Sleepers | 2,491 | 8,561 |
| Added vertices/faces | 30,800/49,844 | 73,312/111,932 |
| Generation/admission s | 1.959 | 6.747 |
| Full generation/export s | 11.487 | 16.267 |
| Worker peak RSS MiB | 574.469 | 573.707 |

Patch method retained for greater useful coverage and fewer artificial caps.
It does more physical construction and is **not a whole-generator speedup**.
No global optimum or complete usage claimed. 14/19 source IDs contribute to the
selected hardware; finite source lengths/cuts/reasons remain in the saved report.

## Independent actual body checks

Both saved alternatives replay independently: read actual rail triangles and all
eight sleeper box corners, derive actual projected footprints, measure gauge,
section height/width and box dimensions/volumes, check original selected axis
pieces, entire finite bed coverage and mutual physical-component projections.
The actual selected patch product reaches gauge error5.378e-13 m,
section-height error1.838e-14 m, sleeper contact error3.187e-14 m,
whole-deck plane error1.137e-13 m and missing area1.036e-10 m².
All111,932 added faces survive Float32 without degeneracy/inversion; supplied
shader normals are exact. Both alternatives retain the old positions, indices,
materials and normals as exact prefixes. New total384,928 vertices/687,524 faces.

For every whole hardware footprint, **all vertices of every intersecting original
body triangle** must be at/below the selected support plane within1e-7 m.
This conservative sufficient separation includes old vertical walls (projected
as lines), old ground rails, piers, roof and other bridges. Accepted hardware
is wholly at/above that plane. Some rejected candidates may be safe after tighter
triangle/footprint clipping: rejection is unproved separation, not proven collision.
7,084 baseline triangle candidates during generation,6,760 in saved accepted replay;
42,917 mutual footprint candidates, no mutual rejections. Exact deck contacts are
shared boundaries, not positive-volume overlaps. No collision/gameplay API added.

## Selection costs, allocations and copies

Identical decoded468-facet deck/23 safe axis pieces, same quality rules. Seven
normal repetitions, no render process during measurement; shared host, no isolated
device claim. One separate Memray1.20.0 call per method with Python/native tracing.

| Selection-only quantity | Triangles | Patches |
|---|---:|---:|
| Median normal ms | 73.123 | 57.924 |
| Allocation/reallocation events | 326,581 | 193,882 |
| Cumulative requested bytes | 31,383,077 | 29,567,886 |
| Tracked peak heap bytes | 624,631 | 536,687 |
| Explicit owned candidate XY bytes | 7,696 | 3,568 |
| Instrumented ms | 494.061 | 296.595 |

Phase excludes source decode, profile/instance generation, base index, admission,
packing, replay, export and renderer. It intentionally changes useful coverage;
no equal-output speed claim. Cumulative allocation requests are not live/unique
memory; GEOS/internal copy totals are unobserved. Full selected export payload
47,739,616 bytes; explicit indexed XYZ copy49,501,728 bytes and repeated corner
normal copy24,750,864 bytes are known partial copies, not complete memory traffic.
Packed hardware retains original sleeper transforms alongside baked inspection
boxes. Prototype instances are still the reusable physical representation.

## Actual regional 3D presentation

Shared #291 Blender5.2.2/EEVEE camera/mesh/light helpers,16 samples,2 threads,
nice10, Mesa llvmpipe/LLVM19.1.7 software. Actual source positions/topology and
shader normal attributes are checked at import. Render observations, no GPU/FPS claim:
720-before22.306 s/2069.133 MiB,720-after25.130 s/2147.766 MiB,
local1080-after41.254 s/2309.629 MiB. Actual Blender controls overlapped part of
the last render; do not interpret these as isolated renderer benchmarks.

Matched regional pair1280×720, same eye/target, unchanged1400 m span and palette.
Camera re-centres on the new upper hardware versus #340; it is not a same-camera
comparison against the old #340 PNG. Eye3534.1033/-1541.9082/790.3387 m,
target3954.1033/-141.9082/160.3387 m. No closeups, cutaway, extra ground,
height exaggeration or display replacement geometry. Both720 PNGs and matching
1920×1080 after PNG actually opened. 9,192/921,600 pixels differ (0.9974%),
7,788 by more than1/255; worst55/255. New dark rails/sleepers follow the elevated
curve; the regional gain remains fine. Overall materials and finite edges remain
schematic; these images do not meet a finished world-art target.

PNG hashes: before91a268b94b04b81134e971fe0cf2d9fd5dba215a4c8a6c2338dc4776e3223822;
after8af746c65e242ecbe501598cb1a618e7a9501b6a5f6fa37c08f6968d309bbcdd;
local1080-after7645e52cffa9a372fa36f838c44caafc6d6dcb0aa1707b54c88ac1763e154582.
Images remain local, no repository/gallery/server publication.

## Reproduction, bounded failures and remaining work

Commands in [README](README.md). Original upper inputs:
`/var/tmp/outshine-transport-tunnel-portals-f68a060a4/build/experiments/transport`;
baseline`/tmp/outshine-transport-tilted-340-final`.
Actual alternatives`/tmp/outshine-transport-upper-352-{triangle,patch}-3d82488b8`;
costs`/tmp/outshine-transport-upper-352-cost-3d82488b8`.
Each contains clean commit/pins, actual arrays and independent terminal replay.
Keep those original inputs and current proofs. Normal dependencies remain the
existing Python/NumPy/Shapely reference environment; optional Memray only for costs,
Blender/Pillow only for local rendering/inspection. No native source changed.

3 GiB address space/120 CPU seconds per generation/replay;40 s cooperative hardware
loop,512 admitted pieces,16,384 sleepers,262,144 hardware faces,2,048 axis pieces,
8,192 input bed faces,65,536 patch/axis candidates and1,048,576 mutual or cumulative
baseline candidates. One baseline query capped32,768, one contact query8,192.
Old #340 default piece limit128 remains. Existing per-profile limits remain.
Cooperative geometry calls can overrun the loop deadline; offline, not a frame gate.

42 source/library controls and13 actual Blender controls pass without skip.
Ruff, formatting, function lengths and docs checks recorded at final delivery.
We still need continuous crease/bank transitions, switch topology, smooth playable
routes, additional sources/places, functional rail accessories, LOD, spatial consumer
binding and native/device acceptance. Fine rail additions alone do not resolve the
washed-out regional road/material impression; prioritize a larger visible deficit
before another microscopic hardware increment.

Terminal final gate **ec210d649d89a36d45ab51b44889618c31d07a69**:42 source/library
controls +13 real Blender controls,0 skips; both saved alternatives independently
replayed as `verification-final.json`. The old #340 lower product independently
replays as `verification-upper352-width.json`, including the new section-width
check. All five child processes exit0. No geometry/render/helper input changed
since3d82488b8. Final report append is documentation only; Ruff/format/AST≤120 and
`make lint-docs` pass. No native/engine/device gate claimed.
