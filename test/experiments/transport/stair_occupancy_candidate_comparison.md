# Bounded occupancy demand before affine volume construction

Python experiment for #770, parent #749. The real 105-source continuation has
8,308 physical facets and 15 logical headroom records. The unchanged flat
8,192-record handoff rejects it. This experiment selects numeric facet references
before constructing affine planes or GEOS volumes. It changes no shared
`Plan`/`Reservation` contract and is not native admission. Whole Plants consumption
and an explicitly agreed common contract remain open.

## Procedure and independent comparison

The source state, original inputs and reconstruction are the same as in
[the prepared-growth comparison](stair_prepared_growth_comparison.md): original
102 sources, then the whole three-source `80922970:230/:232/:238` continuation.
No smaller replacement group, raised old limit, simplified body, or historical
plant placement is substituted.

`CandidateVolumes.query(frame, whole_footprint, *, budget)` takes the same named
local frame, the whole polygon and an explicit height band. It returns complete
unchanged volumes and per-volume source bindings, or an incomplete result with a
reason. `CandidateQuery.conflict` raises `OccupancyError` on incomplete selection;
it never exposes unresolved space as free. Defaults: 256 products, 32,768 numeric
facet tests and 8,192 raw candidates per query. Product catalog and single-product
metadata have additional explicit caps. All three selection budgets are checked
before invoking any materializer. Failure after selection still returns incomplete.

Physical metadata uses original XYZ triangle bounds plus existing clearance and
outward padding consistent with the old 1e-8 m cap-error bound. Materialized
physical volumes are checked against that metadata. Logical clips are always
considered in this pilot: this intentionally allows extra candidates rather than
assuming their unmaterialized GEOS bounds. Actual selected records use the old
volume/supplement builders, original owner IDs, purposes and source bindings.

Compared alternatives: all old planes built eagerly; numeric facet scan; product
AABB then facet AABB; the same product selection with caller-owned
`functools.lru_cache(maxsize=8192)` reuse. Reuse lasts only for that query adapter,
has a fixed retained-facet cap, and does not bypass selection or its budgets.
There is no global cache manager or new solver framework.

SIGGRAPH-first basis: Gottschalk, Lin and Manocha, *OBBTree: A Hierarchical
Structure for Rapid Interference Detection* (1996),
[local paper](../../../doc/references/buildings/siggraph/1996-obbtree-hierarchical-interference-detection.pdf),
SHA256 `9f40b943dc906325f4fe552954363838f0cc6b71c58c1c1287d184b2df111281`.
Introduction and Figure 1/page 7 were read and optically inspected. The experiment
uses only conservative grouping and progressive rejection; it does not implement
OBBTree, oriented boxes or its SAT procedure, and copies no foreign code.

At `85d130192`, the shipped comparison CLI independently checked every one of the
8,323 final records, not just records returned by example queries. The baseline
reader preserves its old 8,113 limit; new 210 road records use the unchanged
builder in a separate bounded partition. Old road arrays, support digest and
logical bindings must remain unchanged. Reference conflict geometry is the union
of complete existing queries over those two partitions; no global limit is raised.

On both 102 and 105, all 156 whole requests (actual original building footprints
and 128 low-cover probes) agree geometrically for numeric scan and product bounds.
Every returned record and source binding is exactly equal to the independent
reference. At 105, maximum raw candidates are 5,916. A world-sized 105 query fails
with `candidate_budget_exhausted` before any materializer runs; the old flat query
also still rejects all 8,323 records. Metadata preparation constructs zero
`AffineOccupancy` objects. 105 metadata occupies 400,656 array bytes plus 7,200
product-envelope bytes, excluding Python objects and retained volumes. The
explicit two gathered XYZ triangle arrays require 1,201,968 bytes cumulatively;
this is a known indexed-copy payload, not a total allocation/copy claim.

## Actual Buildings and image check

At `68ec3580d`, `stair_occupancy_candidate_consumers` invokes the unchanged actual
Buildings producer on the complete current 105 terrain. It accounts for 100 valid
source polygon parts and 10 invalid original features, creates 64 original and
65 adjusted bodies, and retains source attributes and current root contacts.
An independent fresh Installed reader rechecks saved body arrays, all source
bindings, current roots, requested heights, whole cut parts and complete final
body conflicts using the reference partitions, not the candidate selector.
The extracted existing reader also passes the old actual 102 consumer products.

Source/Installed producers have exactly equal before/after NPZ arrays and body
reservations. Full context/ledger data are equal except measured
`context.preparation_ms` and query `seconds`. Two actual CLI countercases update
the artifact hash after tampering: changed source properties and a 0.01 m body
height shift both fail semantic reading without writing a receipt.

Whole 1280×720 before/after views were rendered with Blender 5.2.2 LTS Workbench
and both opened. The adjusted scene contains 54,063 vertices/96,771 triangles.
It remains a sparse, visibly incomplete transport network over diagnostic terrain;
this work proves the larger handoff, not a substantial image-quality improvement.
Images stay private under `/tmp`, without publication or a showcase server.

## Costs

Debian 13/Linux x86_64, i5-1135G7 (4 cores/8 allowed CPUs), 32 GB RAM;
Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0/GEOS 3.14.1, Memray 1.20.0.
Three fresh unprofiled processes per retained/eager case, normal priority,
BLAS/OMP one thread, shared host load (including separate nice-10 correctness
work). Orders alternate after the exploratory run. Imports, source/state decode,
DEM and request preparation are excluded; timing includes metadata, plane/GEOS
construction, all queries and constructor-counter bookkeeping. RSS includes
input preparation. No isolated device, native, A18, GPU or frame-budget claim.

| Same complete requests | Total median s | Affine constructions | Numeric facet tests | Maximum process RSS B |
|---|---:|---:|---:|---:|
| 102 eager | 7.473832 | 8,113 | 0 | 159,068,160 |
| 102 product bounds + retained facets | 6.932190 | 6,252 | 545,295 | 158,822,400 |
| 105 product bounds + retained facets | 6.971074 | 6,462 | 545,925 | 159,268,864 |

The 102 retained case avoids 1,861/8,113 constructions (22.9%) and takes 7.25%
less time in this measurement. It retains 6,237 raw facet results and obtains
16,838 reuse hits; the analogous 105 counts are 6,447/17,126. The 105 world exceeds
the old flat limit, but the requested local work stays within unchanged caps.
At 105, coarse bounds reduce numeric tests from 1,302,132 to 545,925 (58.1%).
Without reuse the 102 coarse path constructs 25,415 affine objects and takes
25.983972 s in its exploratory run: it is rejected as the default consumer path.
All timed modes return exactly the same concatenated conflict-WKB digest
`55d568e35373f533229213938e4ddf6b6ed7e7f842606c468245bdb513e2fe8f`.

| Same workload, profiled separately | Allocation events | Requested B | Tracked heap peak B |
|---|---:|---:|---:|
| 102 eager | 55,858,657 | 5,377,838,039 | 11,106,810 |
| 102 retained | 47,400,876 | 4,645,923,235 | 10,694,344 |
| 105 retained | 48,847,881 | 4,790,115,629 | 11,036,870 |

Memray profiles are separate from ordinary timing and include Python allocator
requests, with native stack traces disabled. Requested bytes include repeated
allocations/reallocations and are neither live bytes nor copied bytes. Tracked
heap peaks exclude already decoded inputs; process RSS is a different scope.
No significant peak-memory reduction is established. Internal library copy totals
remain unobserved. The work reduction is stronger evidence than this small timing
difference; neither guarantees performance on another device or distribution.

## Reproduction and receipt pins

Use the fixed inputs and exact full-cohort reconstruction from the linked growth
report. Set `BASE` to fresh verified 102 occupancy products, `RESIDENT` to the
complete 102 snapshot, `MODULE` to the verified actual three-source continuation,
`DEM` to the pinned original complete tile, and all outputs explicitly below `/tmp`.
Use the consumer environment with Buildings/Vegetation dependencies installed.

```sh
export TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport:test/experiments/buildings:test/experiments/vegetation:test/experiments/assets
"$PY" -W error -m unittest stair_occupancy_candidate_checks stair_occupancy_checks stair_occupancy_precision_checks stair_consumer_checks test_reservation_query test_reference
"$PY" -m stair_occupancy_candidate_experiment "$RESIDENT" --baseline "$BASE" --prior-module "$MODULE" --dem "$DEM" --output "$QUERY_PROOF"
"$PY" -m stair_occupancy_candidate_consumers "$RESIDENT" --baseline "$BASE" --prior-module "$MODULE" --output "$BODIES"
"$PY" -m stair_occupancy_candidate_consumers --products "$BODIES" --output "$BODY_RECEIPT"
"$PY" -m stair_occupancy_candidate_cost "$RESIDENT" --dem "$DEM" --method retained --output "$COST"
```

Omit `--prior-module` for the original 102 query case. For costs compare `eager`,
`flat`, `coarse`, `retained`; eager keeps its old limit and is only admitted for
102. Add the same module for retained 105. Use fresh output directories/processes;
run allocation captures separately with `--profile`. Both CLIs bound CPU to 600 s
and virtual address space to 4 GiB. Run Installed with `lib/src` replaced by the
actual wheel installation directory; keep source test paths for the controls.
Use existing `stair_consumer_display` and `render_transport_solid` on the verified
body product and matching whole 105 transport scene, without changing the camera
to a detail view.

48 affected source and 48 affected Installed tests pass, plus the whole original
102/105 query probes and whole 105 CLI comparison, fresh actual body reader,
Source/Installed producer equivalence and the two semantic CLI negatives.
The unchanged wheel SHA256 is
`46a35a571e81c27e98e852466ff5e666068d675089684ac55ebee3b840d8b9b7`.
Private current proof root: `/tmp/outshine-transport-resume-1f10d55eb/`.

| Current private artifact | SHA256 |
|---|---|
| `candidate-proof105/comparison.json` | `3b09966a2f761c18445de9ace31e5aaaf5be0a7af201be179585d46eb61b9c76` |
| `candidate-buildings105/comparison.json` | `730c2c28d77a6506f9a8d42764923c732d740f4c8cd467a35e8c2e4289788bfc` |
| `candidate-buildings105/buildings-after.npz` | `540b8a1e27dc93f5ebfab598541e11a2940c30ffbe6cee7d9562ce5c11b80766` |
| `candidate-buildings105/building-reservations.json` | `752e6103642170c192aaafea5129d8a83e954d5230e4bf4f4e14df732a4fe369` |
| `candidate-buildings105-display/after.png` | `90fc2295078a1fd3f658445ac3173f5a2d72885a2e329bed0c2efce689634d58` |

The common contract proposal and explicit consumer replies live in #770; this
experiment does not presume approval. Whole Plants, common library migration,
continued real source families and native integration remain separate unfinished
requirements. #770/#749/#434 and the broader goal remain active.
