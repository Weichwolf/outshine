# Fine payload access after static group selection

Owner: outshine_research, #1870 / #1797. Python file experiment only.
`source_group_payloads.py` first queries the same static group metadata as #1869,
then reads selected exact triangle corners. No native implementation, AssetCache
format, runtime cache/queue policy or SourceID is introduced. Original attributes,
parts/materials, winding and native consumer remain unchanged. Original ordinals
are returned; corner duplication does not weld equal positions or alter attributes.

## Method and lifetime

Preparation validates and copies the immutable source, writes one original fallback
file (Float32 positions, UInt32 indices) and one corner file (nine Float32 values
per triangle), and releases its fine arrays. The layout retains bounds, original
ordinals/offsets, source witnesses and SHA-256 digests; paths address private
experiment files. Full vertex/index input is not retained by the layout. Build,
extra file storage and resident metadata are charged separately. This deliberately
simple artifact is not a proposed production package.

`group_demand` is shared with the resident reference; the fine selector's upload,
raster and work-limit validation is shared as well. Unsupported source/upload or
an actual vertex outside the supported clip interval keeps the whole original
without fine reads. Unknown root bounds read and validate the original fallback
before calling the existing selector. A corner subset exceeding the fine vertex
limit also uses the original. Cooperative fine-budget failure keeps every original
face, including unread groups. Missing, short or corrupt selected payloads keep the
whole original with `payload_unavailable`; no partial set replaces its parent.

Within a query, adjacent selected groups merge into exact ranges. Excluded group
bytes are never included to save a request. One descriptor is opened for the
selected corner ranges; every actual `pread`, requested/returned byte and logical
4096-byte file page is recorded. Selected blocks are hashed before use. The final
concatenate copy and duplicated projected vertex rows are counted separately.
These reads are synchronous research operations, unsuitable for a frame hot path.

`Reads.retain(layout)` optionally returns only this request's immutable selected
block bytes. The caller may supply them to the next request; unchanged groups need
no read. Blocks outside the next demand are dropped. Reuse requires the exact same
prepared layout object; equal ordinals in another preparation cannot alias. There
is no timeout, eviction scheduler or persistent cache. At most one previous demand
is input; previous-plus-next unique block payload is bounded by the corner file.
Transient read buffers, dictionaries, NumPy objects and concatenation add overhead
and are included in scoped allocation traces. A still-valid previous block remains
valid even if its file disappears; a new missing block keeps the whole original.

This reference binds a static immutable generation only. Native product identity,
publication, cancellation, IO budgets and retention remain with #10/#12. It makes
no live World, shadow/simulation or general hardware raster admission claim.

## Comparison

The complete original input has19 operands,1558 sources,78374 vertices and38476
triangles. Original fallback file1402200B; additional corner file1385136B. Metadata
includes four-triangle groups and per-group hashes; its binary payload is731236B.
Python object overhead is additional, measured in scoped peaks. The original file
remains necessary for exact unsupported-root fallback; do not omit its storage.

Three static layouts preserve the same group membership and final ordinals:
original group order, 10-bit Morton order of group centers and squared-box-diagonal
order. These change file placement, not cluster membership. Native Morton128 leaves
were already rejected for this input in #1869. Spatial placement alone does not make
this pixel-center demand local: groups are scattered across most file pages.

Linux i5-1135G7, NumPy2.5.4, Memray1.20.0, BLAS/OMP1. Concurrent work and
focused Python tests during the first part; load12.06/8.44/7.06. Absolute timings
are noisy; no isolated speedup claim. Allocations and bytes determine this decision.
All candidate IDs and Whole status/reasons match. Build original/Morton/size:
383.415/368.094/351.185ms, scoped peaks7069622/7683125/7066443B. Original-order
build allocates1222673 times and requests218927283B across its temporary work.
At the noisy guard0 near timings, charging the full383.415ms build needs about
16 repeated queries to amortize against whole-file selection. Guard1/256 side
is slower, so it has no time amortization in this run. Both60-step sequences
save more query time than the build, while turn does not. Against the already
resident hierarchy the packed bound queries are slower; no build amortization
can turn that into a query-time win. This is a conditional cost comparison, not
a production cache-hit or device threshold.

| Guard0 query | Whole file | Original groups | Morton groups | Size groups |
| --- | ---: | ---: | ---: | ---: |
| Near read B | 1402200 | 294192 | 294192 | 294192 |
| Near requests | 1 | 1070 | 1250 | 1184 |
| Near logical pages | 343 | 325 | 335 | 307 |
| Side read B | 1402200 | 500832 | 500832 | 500832 |
| Side requests | 1 | 1387 | 1666 | 1484 |
| Side logical pages | 343 | 336 | 339 | 335 |

Near whole/resident-hierarchy/original-group query114.422/53.874/88.939ms;
side116.837/80.161/114.849ms. Scoped peaks9660188/4644723/5109299B near,
9851892/5597471/6384770B side. The loaded hierarchy has fewer projected rows:
18932/31513 versus24516/41736 duplicated corner rows. Query allocation counts
10455/18206/229628 near and10455/19113/353031 side. Original-group requested
allocation bytes88784828/126529159B; known concatenate copies294192/500832B.
Guard1/256 reads301824/510768B,1083/1397 requests,326/336 pages; the side query
is slower than whole read124.677 versus91.076ms. Full rows/rounds in report.json.

| Sequence / method | Selection read B | Requests | Sum query pages | Wall ms | Scoped peak B |
| --- | ---: | ---: | ---: | ---: | ---: |
| Turn / whole | 16826400 | 12 | 4116 | 441.823 | 11067389 |
| Turn / selected | 3877344 | 6027 | 1681 | 585.257 | 13714707 |
| Turn / previous demand | 2432016 | 5235 | 1539 | 716.625 | 13003996 |
| Coarse translation / whole | 16826400 | 12 | 4116 | 1455.268 | 9754088 |
| Coarse translation / selected | 3833928 | 13276 | 3915 | 1185.809 | 5950603 |
| Coarse translation / previous | 2521800 | 9429 | 3643 | 1057.205 | 5747484 |
| 60 small steps / whole | 84132000 | 60 | 20580 | 7661.979 | 9807436 |
| 60 small steps / selected | 17890848 | 63479 | 19397 | 5069.087 | 5865450 |
| 60 small steps / previous | 8408232 | 34689 | 16421 | 3504.346 | 5604148 |

Turn retains Whole7/12 for every method; both translations0. Small step is(5,0,-2)m,
coarse step(100,25,-200)m; turn0…330°/30°. Small-step selected/previous allocation
events13814945/8927869, requested5254341390/4899098435B. Peak next-demand block
payload309456B in small steps and1109880B in turn; metadata is additional. Repeating
an identical supported demand reads zero corner bytes, verified separately.

**Decision:** do not adopt this four-triangle file layout as a runtime default.
731236B resident metadata and1385136B additional disk payload buy little page
locality, thousands of small requests and substantial Python allocation work.
The immutable previous-demand diff works, but returned-byte savings are much larger
than page savings; no general total-cost winner. Existing resident hierarchy remains
the work winner in these bound cases. A future independently readable product must
justify its granularity against representation quality and IO/decode/request cost;
current monolithic Zstandard AssetCache packages do not provide these range reads.
This negative layout result and the exact diff boundary are the research delivery;
no new cache/format migration follows from it.

The UInt32 original-file control avoids charging the joined Python Int64 topology
as a native storage requirement. A low returned-byte count is not a proportional
SSD-read reduction: small ranges still touch nearly all corner pages. Whole without selection reads still requests the entire original parent for
rendering; its geometry/attributes/residency are outside this selection experiment.
Zero selection reads therefore never mean a drawable whole scene with zero IO.
The file
cache is warm because artifacts were just written; no cache eviction or cold-SSD
experiment occurs. `known_retained_block_copy_bytes` in the frozen report is an
upper bound for block slicing: a whole-buffer byte slice can reuse that object.
The delivered counter is named `retained_block_slice_bytes_upper_bound`. The
retained-byte snapshot is named `caller_readset_block_bytes`; a caller may drop
its read set immediately. Only these telemetry keys changed after measurement. It is not added to returned bytes
as an independent physical traffic measurement.
Concatenation always produces a new fine-position array.

## Reproduction and acceptance

Measured source: `8130fb0cd2915fef03d6881ae736b85b00c5c022`. Use the existing project `.venv` (NumPy/Memray),
`PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`.
Run `python test/experiments/render/compare_source_payloads.py --output /tmp/outshine-source-payloads-<commit>`
from clean measured sources. Seven timing rounds and one separate allocation trace
per method. Build writes a fresh file set each round; superseded sets are removed.

Whole-file control, resident hierarchy and all three layouts are compared at both
bound native poses and guards0/1/256. Synthetic full-turn samples, coarse translation
and60 five-meter frame steps compare full reads, selected reads and previous-demand
reuse. Every final ordinal and complete-model/Whole reason/status is checked.
Synthetic words are not native camera uploads; sixty requests are not a frame gate.
Existing native input/image qualification remains applicable at identical bound IDs;
no renderer, guard, error tolerance or material data changes.

51 distinct focused cases passed across the50-case suite and11 updated boundary
cases:12 payload cases plus existing hierarchy, selector and
source/material tests. New cases include exact excluded byte ranges, missing/corrupt
selected blocks, untouched corrupt excluded blocks, original fallback integrity,
cooperative budget, empty demand, no IO on invalid input, immutable bytes and reuse
isolation; whole input tests cover all three layouts, both guards, turns/translations
and repeated requests. Ruff/format/diff checks and docs lint accompany the delivery.

Own #1320 acceptance: this is an isolated comparative Python reference with explicit
static source and payload ownership, exact selected-read boundaries and safe Whole
fallback. It proves finite candidate equivalence and logical request costs, not a
production DB, compressed-package range access, physical UMA/GPU residency, async
World publication, native frame time or p99. The existing parent remains necessary.

Primary starting point: local SIGGRAPH2015 GPU-Driven Rendering Pipelines, page12
optically inspected: cluster topology/work is distinct from layout and indirect
submission. #1869 records existing ClusterCook/GeometryHierarchy inspection.
Prepared product/selection boundaries are owned by #10/#12; the local reviews in
`doc/references/engine/unreal/prepared-assets.md` and
`doc/references/engine/visibility-driven-demand.md` explain their separation. No generic cache manager or separate renderer is derived from the paper.
