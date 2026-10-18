# Validated surface composition with coverage union (#678)

The normal `SurfacePatches.replace` now uses coverage union for its two already
validated XY regions. Complete exact XYZ-boundary, active-mask, source/frame,
schema and capacity checks remain; the symmetric-difference limit stays1e-7m².
Public signatures and generated geometry are unchanged. This follows the
[constructor optimization](stair_coverage_boundary_comparison.md), and remains
a partial delivery for [#678](https://github.com/Weichwolf/outshine/issues/678).

## Assumptions, alternatives and sources

SIGGRAPH starting point remains Lipman2012, section5/theorem5.1, visually checked
with its figures in the preceding report: oriented boundaries constrain planar
coverage; positive triangles alone are insufficient. An own directed-edge
comparison is tested as an alternative, not treated as a full 3D certificate.
The selected established operation is [Shapely coverage_union_all](https://shapely.readthedocs.io/en/stable/reference/shapely.coverage_union_all.html).
The actual GEOS3.14.1 [C entry point](https://github.com/libgeos/geos/blob/3.14.1/capi/geos_ts_c.cpp#L1597)
calls [CoverageUnion](https://github.com/libgeos/geos/blob/3.14.1/src/coverage/CoverageUnion.cpp).
Its [input conditions](https://github.com/libgeos/geos/blob/3.14.1/include/geos/operation/overlayng/CoverageUnion.h)
require non-overlapping polygons with matched/noded edges. No new vertices or
precision model are needed for a valid coverage. No external code is bundled.

These conditions follow from the existing surface contract: validated root,
selected subsets, independently validated replacement and complete exact XYZ
boundary/XY-domain preservation. Thus every accepted update retains a valid
active coverage. Hole boundaries, disconnected regions and point contacts do not
invalidate that reasoning. It is not permission to feed arbitrary overlapping
polygons to the faster operation. The constructor still uses general GEOS when
its sufficient simple-boundary proof is unavailable. No stored validity flag,
new API, source update, cache manager or SDK dependency is introduced.

All14actual replacements/19844selected old+new triangles are collected using
the general-union reference path. Each input region independently passes general
GEOS coverage validation. All three methods return the same predicate; every
one of28coverage-union output regions is topologically equal to its general-union
reference. Input-pair SHA256:
`6d9d70ec5fe8c733db7f192348ff57d23e10caa8fb99238f89cfeb2fedfd9e22`.

Only these equality operations are measured below, after input loading/validation;
the source is `4c13b6e0c57f45df504fe42a987af9d2f3532f6d`.

| Method | Profiled s | Positive allocations | Requested cumulative B | Incremental tracked peak B |
|---|---:|---:|---:|---:|
| General union |1.425771|3215226|435101154|1044280|
| Coverage union |0.083457|227513|19916909|1324576|
| Own directed-edge proof |0.100190|89407|25407333|1282973|

Coverage union requests95.4225%fewer bytes than the former operation. The directed
proof succeeds14times and needs fewer allocation events, but requests5490424B
more than coverage union and adds coordinate reconstruction/sorting machinery.
Choose the smaller standard implementation for this Python path. Its phase peak
increases280296B. This is a local comparison, not a global optimality claim or
evidence of an uninstrumented/device speedup.

## Whole-state and real consumer checks

Runtime/test/wheel commit `585b547715820f640805cdb0ecd5328598939038`.
Use the original [complete78-source snapshot and79th-source recipe](stair_resident_snapshot_comparison.md).
Manifest SHA256 `4e0c339d98df06e77cf1aecbf3f5c0386819dba3c188cbd0d325419ba2a9a917`.
Both complete loading methods produce state SHA256
`09343d36a760a2cb8e6647287cf99e0ab0cd8ee9b2048ac3b4af2c796981891d`:
same31058active XYZ facets/attributes, patch IDs/masks, road/profile metadata,
whole supports/caps and logical headroom.152arrays/1894047input bytes are decoded
in each. IO, hashing, decode, copies, trees and all patch replacement checks count;
imports, preparation, profile recovery and result hashing do not.

| Whole prepared load | Profiled s | Positive allocations | Requested cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| Previous general composition |3.048550|6634593|933892554|21673018|127856640|
| Normal coverage composition |1.631557|3646883|518708005|21672911|126873600|

45.0323%fewer allocation events and44.4574%fewer requested bytes. Peak differs by
only107B; this and RSS variability prove no RAM saving. There are still28unions
and14symmetric-difference operations;28general overlay unions are replaced by
28coverage unions. Input decode and existing XYZ-boundary work are unchanged.
Allocation totals are not copied bytes; full copy/memory-bandwidth traffic is
unmeasured. Tracked peaks omit OS/driver/GPU costs. No budget is increased.

113source and113installed controls pass, with115current matching reference
imports per mode. This includes holes, separate/point-touching components, narrow
domains, altered hole boundaries, stale/duplicate masks, wrong frame/schema and
the existing conservative skinny-contact controls. One newly proposed control
initially assumed identical contact classifications across different tessellations
of a2^-40m-thin flat region. The old general path reproduces exactly the same
`ambiguous_projected_contact` on that refined mesh. The final check compares old
and new operations on identical meshes, preserving that conservative decision;
the actual negative mesh/reasons remain in `thin-contact-counterexample.json`.
No sampler acceptance rule or established gate is weakened.

Both source and wheel re-create the actual whole79th Radweg: all seven road,
terrain, planning and catalogue artifacts are byte-identical to the original.
The independent cold reader forces **both** general constructor validation and
the previous general composition.361points per precision preserve Double/Float32
index/scan geometry, heights and normals. Actual fresh consumers also agree:
6648volumes,124whole supports,128probes,65buildings and4complete plants. Full
consumer closures contain116current matching reference modules; the installed
generation/reader imports52. Wheel SHA256:
`db931057f4d6073cb89d83a1211c69d011c2ac85232d79ae6d5dac40120c6c05`.
All processes terminated successfully; original receipts/products are preserved.
After merging the newer vegetation experiments, the actual source/installed
Pinus reader also passes again at `a4b7c4889`:116reference/249experiment imports,
including the changed `colonization` module. Library runtime code remains585b54771.
The original full1280×720perspective image was opened again. Identical artifacts
preserve its geometry; no new render, public image or visible improvement is claimed.
Sparse source coverage, weak contrast and coarse terrain remain.

## Reproduction and boundary

Costs: Python3.13.5, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1 and Memray1.20.0.
Actual Buildings/Plants controls use the existing NumPy2.5.4environment with the
same Python/Shapely/GEOS versions; no new runtime package is required. The
wheel build uses isolated setuptools84.0.0. WSL2/i5-1135G7 with four exposed logical
CPUs/16190028KiB system RAM. Shared-host work and own nice10controls/consumers may
overlap measurements at normal priority. This is not A18/RTX or frame/GPU evidence.

From a clean repository root, set `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`,
one BLAS/OMP thread and the established reference/experiment PYTHONPATH.
Run `python -m stair_composition_study --products SNAPSHOT --method METHOD
--output REPORT [--profile TRACE]` for general/coverage/oriented operation comparisons.
Use `stair_composition_load` with the same arguments for complete-load comparisons.
Run `python -m stair_coverage_boundary_verify --resident SNAPSHOT --reference ORIGINAL79
--fixture test/experiments/transport/fixtures/stair-pier-detour-vienna.json --output FRESH_DIRECTORY`
for real generation plus independent old-path verification. Run the surface patch,
composition, sampling and affected transport/consumer tests in both import modes.
Original input paths are retained by the predecessor report. Current raw traces,
wheel, import closures, original counterexample and hashed `evidence.json` are
under `/tmp/outshine-transport-composition-dc2da24ae/`.

Repeated constructors, boundary extraction, sorting, masks and geometry decoding
still occur on prepared hits. #678 remains active for actual immutable prepared
reuse; the full original requirement is not redefined around this partial win.
Native cache/engine integration remains with master/#48. Worldwide source use,
LOD, spatial precision, device budgets and visual-quality requirements remain open.
