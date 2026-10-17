# Exact local terrain patches across the five-module Wiener history (#542)

[#539](stair_prefix_comparison.md) leaves a verified 53-source network whose
fifth module cannot be loaded through the intentionally bounded four-parent
diagnostic importer. This experiment represents the already verified root
surface and all five exact local field changes independently. The legacy
importer, source/owner/solver limits and existing generated bodies stay unchanged.

Reusable API and exact finite geometry contract:
[`geometry.surface_patches`](../../../lib/surface_patches.md).
`stair_patch_experiment.py` converts explicit verified source products to
prepared resident patches; `stair_patch_verify.py` reads them without provider,
CDT, solver or global field generation. `stair_patch_cost.py` compares indexed
lookup and complete scans, and a flat equal-surface control. This is an
experiment format, not a second AssetCache or native runtime codec.

Stable patch/local-face IDs replace global facet indices only in this new
geometry view. Every removed face must be active, every whole XYZ boundary edge
must match, all face attributes/frame/kind remain explicit. Old meshes are shared
unchanged; no numerical welding occurs at a patch seam. Failed changes preserve
the old valid view. Source/level/access/reservation semantics remain the caller's
contract, storage/index/residency #10 and world hierarchy/LOD #12.

Initial exact replay: all five updates reproduce the same final 27962 XYZ
facets and ground/constructed tags as the saved 53-source reference, with
106/55/131/200/48 exact boundary edges. This establishes finite representation
equivalence, not new source coverage, network generation or native acceptance.
The next generator adapter still needs to consume local view products.

SIGGRAPH starting point: Lindstrom et al. (1996), page 4 figure 3 inspected
visually. Its local height-field/delta representation motivates inspecting
explicit edges; this operation is independently defined and implements none of
its LOD/morphing/simplification method. No SDK or external code is incorporated.

## Frozen products and independent acceptance

Preparation/kernel/cost code `840975e907ce1bd6d2508c445d520fff71eb1041`;
reader/source-frame binding `d3e926dc58055e23d8e188eccd3f37626d52a5d6`.
The latter changes only independent reference binding; kernel/load/query code
is unchanged. All five actual verified module exports are read directly,
without `load_cached` or relaxing its four-parent guard. Preparation takes
2.760301 s / 139137024 B peak RSS, including legacy integrity checks, conversion
and whole-field diagnostic comparisons. This is conversion of existing verified
products, not generating the network from raw sources.

The six prepared mesh payloads (root plus five patches) contain 15731 vertices,
30554 stored faces and 805300 B of vertex/index/attribute buffers, compressed
303060 B. Active result: the original 14160 vertices/27962 faces, 53 sources.
The equal flat control owns 731308 B, compressed 272219 B. Patch history retains
2592 inactive facets and its indices: there is additional resident data, no
general memory-reduction claim. Source identities/report lineage stay separate
from local face ownership; no global source-bit mask is created.

Each update preserves exact complete XYZ boundary edges (106/55/131/200/48),
all unaffected geometry and the exact ground/constructed tags. Changed old
activity masks total 129991 B; new masks 6166 B. The explicit temporary-copy and
immutable-byte-backing passes copy 266148 B of masks across the five updates.
The report's `retained_mesh_copy_bytes=0` counts retained whole Mesh buffers
duplicated into the new snapshot. Validation still gathers local geometry:
8758 old/new selected triangles require 735672 B of explicit index/XYZ gathers,
plus 20736 B of selected polygon references, derived from the executed array
shapes. Python/GEOS/index temporaries remain additional work. Queries deliberately
construct bounded local dense meshes. None is a total memory-traffic measurement.
The frozen `840975e90` evidence names this field `old_geometry_copy_bytes`;
the final diagnostic-only rename clarifies its scope without changing geometry
or the operations measured below.

Fresh independent reader terminates 0 with old import/planning, provider
decode/acquisition, CDT, solver, old global field assembly, closed-body generation
and array export blocked. It binds the manifest to the verified source report,
origin, source IDs and actual whole reference hashes. All 27962 XYZ facets and
both face attributes match the independent saved global field exactly. 222
points per precision, including seams and absence, reproduce whole-reference
positions/normals/reasons exactly in Double and Float32. Indexed queries and
scans select identical complete local faces: 4300 indexed incidence visits
versus 11440562 scanned face predicates over both precisions. Those count
different work, not equal-cost instructions or internal GEOS tree tests.

All 124 actual old whole pier contacts pass against local patch queries; largest
local reference has 41 faces, maximum Float32 contact error remains
0.000015170144820331188 m. The independent whole layout SHA256 is
`cbd53af0bef0a7b0bd24608ac46bd43a2ffdec4dd1426002c793128b82a01ee7`,
field `eb39ac460294ccfbbaaddb36f87a7d1b3f3d31e0ef28d6546a8065d3f9ec5314`,
prepared manifest
`d2cbeedfcf3137568a79a04a057921eb82b7adb3110b75e513e5a31e8a6a532b`.
Prior physical/source provenance is [#539](stair_prefix_comparison.md): same
original PBF/DEM inputs, 53 candidates/1604 unused line parts and 71 areas,
originals/tags/reasons unchanged; native `admitted_source_ids=[]` remains.

Wrong actual frame (even with coherently renamed frame metadata) is rejected
before geometry loading. A corrupted prepared root is rejected by its payload
hash; the previously loaded immutable view still answers unchanged. Original
bytes were restored exactly after both private counterexamples. Unit controls
also reject wrong kind/schema, stale/duplicate face IDs, holes filled, overlap,
boundary-height change, unsupported orientation and exhausted budgets. A
synthetic 131-patch/130-source-label history uses stable local identities without
global 64-bit ownership; it does not claim 130 real transport sources.

121 source / 121 installed controls pass. Fresh wheel has 205 Python modules,
all source-equal; previous 201 modules are byte-identical. Wheel SHA256
`32788c3f719bf88bd952cb2f75b45cb8d0c720611e070f710ec662ab1383a304`.
Format/Ruff, changed Python functions ≤120 lines, diff/docs lint pass. Native
engine/device gates are not run for this pure Python reference delivery.

## Equal-surface costs and actual images

Linux VM, four logical CPUs, Intel i5-1135G7, Python 3.13.5, NumPy 2.2.4,
Shapely 2.2.0; BLAS/OMP one thread, nice 10, concurrent host/Buildings/render
work. Fixed 222 real queries come from root vertices, actual patch boundaries
and an outside case. Every method emits the same positions/normals/reasons,
hash `a77067d4890815dc2f83a093d216f1c9387b8b290560c93fbfa1eab52d9bb565`.

| Resident representation/method | Load/index/validation s | Total s / peak RSS B | Face tests / patch-bound tests |
|---|---:|---:|---:|
| patches/index | 1.441560 | 1.663411 / 109690880 | 2150 / 1332 |
| patches/scan | 1.396768 | 1.827108 / 109936640 | 5720281 / 1332 |
| flat/index | 0.895276 | 1.107418 / 113086464 | 1734 / 222 |
| flat/scan | 0.936432 | 1.356440 / 112689152 | 6179602 / 222 |

The fully resident flat representation is faster for this small whole window.
The additive patch representation supplies independent local change identities
and shared old geometry; no CPU/peak-memory winner or native budget is claimed.
The flat control remains useful. Storage selection, eviction and demand-driven
loading must be resolved with #10 before calling this scalable streaming.

Allocation-profiled indexed variants, Memray 1.20.0 with Python/native allocation
events and no native stack unwinding:

| Representation | Total s / peak RSS B | Allocation events / requested B / tracked peak heap B |
|---|---:|---:|
| patches | 4.919000 / 124141568 | 11337325 / 1695401585 / 18818050 |
| flat | 3.456968 / 124850176 | 8637119 / 1338482502 / 21452972 |

Scopes start after imports and fixed query-case loading: prepared payload reads,
index/invariant replay and all local extraction/sampling. Raw providers,
CDT/solver/global body generation, independent verification, stats and rendering
are excluded. Requested bytes measure churn, not live/unique memory; RSS includes
imported/native resources. Both profiled result hashes equal all normal variants.
Official stats return successfully, fresh complete JSON is decoded, then output
is flushed before explicit exit 0, excluding interpreter teardown.

Each variant emits 1734 complete local faces overall: one XYZ gather/concatenation
buffer scale 124848 B, inverse labels 41616 B, owned dense Mesh buffers 64968 B.
Gather and concatenation each materialise local coordinates; the first counter
is one buffer size, not all copies. Internal uniqueness/GEOS/attribute copies
are not fully counted. The resident view avoids global geometry rebuilding on
replacement; per-query dense extraction still has measurable allocation costs.

For image/closed-body acceptance only, `stair_patch_display.py` materialises the
full finite diagnostic surface and closes it with the existing body helper.
That expensive diagnostic path is separate from loading/querying. Closed-body
positions/topology and all five display arrays (dtype/shape/bytes) equal the
previous actual 53-source export. Four fresh 1280×720 Eevee images were fully
decoded and actually opened: old/new whole overview and moderate network view.
Both pairs have zero changed RGB pixels under `test/scripts/pixels.py`.
Software llvmpipe LLVM 19.1.7; render times
13.310042/12.055505/14.275109/14.714008 s. This is visual preservation, with the
same sparse/pale scene, not new visible road progress. No public gallery/image
or runtime/GPU-performance claim is made.

## Reproduction and remaining work

Private evidence root `/tmp/outshine-transport-patches-a5611a9f0/`, complete
prepared payloads `/tmp/outshine-transport-patches-full-840975e90/`.
`run-frozen.py` records exact guarded conversion/replay/diagnostic commands;
`prepare-guard.json`, `verify-guard.json`, `counter-products.json`,
`installed-proof.json`, `cost-normal.json`, `cost-profile.json`, official
allocation JSON/original traces, `cost-and-display-proof.json`,
`render-completeness.json` and terminal logs distinguish their scopes.
Source originals/parents and required counterexamples remain; only disposable
wheel copies/intermediates are removed.

Install `./lib`. Use #539's verified base/supports and five ordered actual
`--cached-module OUTPUT RECIPE` pairs with `stair_patch_experiment.py` and a
fresh explicit system-temp `--output`. No `load_cached` of five parents is used.
Run `stair_patch_verify.py --prepared OUTPUT --reference LAST_MODULE --supports
SUPPORTS` in a fresh process. `stair_patch_cost.py --prepared OUTPUT
--representation patches|flat --method index|scan --output /tmp/outshine-...`
replays identical fixed queries; optional `--profile TRACE` records allocations.
`stair_patch_display.py` is a separate whole-body diagnostic with the same base,
supports and ordered `--module OUTPUT` arguments. All research outputs stay in
explicit system-temp directories.

This addresses fifth-module finite surface replay, preserving exact geometry.
It does not increase source coverage or connect the existing generator to this
view. New local generator adapters, road-asset ownership/ports, spatial loading,
compaction/residency, multiple world cells/frames, complete source/level/profile/
route coverage, LOD/reservations/statics and native integration remain
#434/#50/#48 (storage #10, hierarchy #12). The overall goal stays active.
