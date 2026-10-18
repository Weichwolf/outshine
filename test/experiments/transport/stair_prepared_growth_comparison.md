# Complete prepared transport states feed the existing growth CLIs

Issue #749; remaining spatial handoff #770; reconstruction #758/#766.
This delivers verified loading and geometry growth. #749 remains open for the
complete 105-source Transport→Buildings→Vegetation sequence.

## Change and actual state

The existing independently admitted terrain codec can now feed
`stair_growth_experiment.py`, `stair_growth_verify.py` and
`stair_growth_compare.py` through the same explicit
`--prepared-reference`, `--reference-sha256` and
`--prepared-variant stored|compressed` arguments already used by the batch CLI.
The batch parser reuses that argument declaration. Without the explicit pair,
loading follows its existing strict path. Unrelated terrain stays strict.
No library/native API, solver/geometry limit or runtime cache manager changed.

Producer/reader/measurements: `3eaf78511b39a634b9e91a33f8d971ec631c7980`.
The original 102-source cohort was rebuilt from versioned originals, then
independently read and compared against complete source bodies/terrain after
every stage. The complete snapshot retains 19,288 old road-body facets, 32 root
profiles, 24 road assets/terrain patches, 124 piers and one logical borrowed
headroom declaration. Whole terrain has 34,456 exact XYZ facets/attributes.
Historical binary equality is not inferred: current pier heights differ from
one historical witness; #766 records that difference and keeps old pins strict.

Fresh general construction/composition independently admits both codec variants:
stored 1,255,592 bytes; compressed 484,645 bytes. Caller-held reference SHA256:
`aab1a0949b20c9a71a5f1a53cbe4d464a40a46fea2dba49b14e650a0cf880c9e`.
All twelve normal loads and four separately profiled loads match full state
`ba14f0a366e105a73c6d36e878eb03697c24a2c32fa7fe1fbc31962969691d1b`.
This includes road arrays/IDs/profile metadata, terrain patches/active masks,
support bodies/caps, frame, catalog hash and logical headroom, not only visible XY.

## Identical real continuation

Original chain and admitted snapshot produce identical roots, recipes, source
reasons and ordered original IDs. The prepared discovery CLI also checks
index/scan equivalence. The explicit bounded continuation window starts at
proposal offset 1; it is a controlled case, not a maximal network claim.
Three whole original footways `80922970:230`, `80922970:232`, `80922970:238`
produce 105 sources. Original chain, stored, compressed and installed-library
runs all pass actual generation and saved physical reading. Every array/shape/
dtype/byte in all four NPZ products matches, including the full updated state:
`d2b92023613cd66c4d306ab882adcbdc2df91cf73310d0f606e173f5c1933f65`.

Independent whole construction proves 34,632 exact XYZ facets/attributes and
bit-exact module/layout/field arrays. Whole before/after 1280×720 Workbench
images were rendered and opened; sparse coverage, low contrast and coarse
terrain remain. Nothing is published as an image or gallery.
A separate automatic 105-source batch has a different source list; its outputs
are separate evidence and do not replace the original-cohort comparison.

## Equal-state costs

Debian 13 Linux x86_64, Intel i5-1135G7, four cores/eight allowed logical CPUs,
32 GB host RAM. Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1,
Shapely 2.2.0/GEOS 3.14.1, Clarabel 0.11.1. BLAS/OMP one thread.
Normal measurements use normal priority, shared development load and three fresh
processes per method, interleaved in the order below. Filesystem cache was not
dropped. Imports, source recovery, independent admission and result hashing are
outside the loading timer. IO/hash/decode, trees and applicable replay are inside.

| Same 102-source state | Median load, s | Decoded arrays / bytes |
|---|---:|---:|
| Original chain | 1.278794 | 489 / 2,714,925 |
| Strict complete snapshot | 1.248828 | 233 / 2,238,098 |
| Independently admitted stored | 0.175847 | 258 / 2,285,736 |
| Independently admitted compressed | 0.178638 | 258 / 2,285,736 |

The strict snapshot constructs 24 strict patches and performs 23 replacements;
both admitted variants perform zero of each, restoring all 24 patches once.
Numeric metadata adds decoded arrays/bytes; these counters are neither all IO
bytes nor all internal copies. Generic polygons/STRtrees and binding checks remain.

Separate Memray 1.20.0 runs trace Python/native allocations with native stack
unwinding disabled. Positive-size non-free requests include realloc/repetition.

| Method | Allocation requests | Cumulative requested B | Tracked peak heap B |
|---|---:|---:|---:|
| Original chain | 5,062,873 | 785,641,538 | 25,458,598 |
| Strict snapshot | 4,624,394 | 642,775,962 | 25,701,161 |
| Stored | 821,602 | 119,496,954 | 25,399,195 |
| Compressed | 827,588 | 140,100,443 | 25,399,148 |

Stored loading removes about 82% of the strict snapshot's requests; peak heap
barely changes. Normal process RSS before result hashing is approximately
129–130 MB for original/strict and 127 MB for admitted runs, not a device-memory
budget proof. Heap/RSS are not added; cumulative requested bytes are not live
memory or measured copies. Complete internal copy totals remain unknown.
No global optimum, runtime/frame, A18 or RTX budget is inferred.

Whole-process preparation/reading on the same shared host, nice 10:
snapshot preparation 2.468 s, saved snapshot reader 3.321 s, codec packing 1.917 s,
independent general-codec admission 13.688 s. Those include imports and differ
from the hit timer. Instrumented load timings are separate from normal timings.

## Reproduction and checks

Install `./lib` with transport dependencies. Use explicit private outputs,
`TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, `OPENBLAS_NUM_THREADS=1`,
`OMP_NUM_THREADS=1`, plus `lib/src`, `lib/tests`, `test/experiments/transport`
on `PYTHONPATH`. Actual consumers also use buildings/vegetation/assets experiment
paths, NumPy 2.5.4 and manifold3d 3.5.4. No SDK is imported.

`fixtures/stair-reference-cohort-vienna.json` preserves the five ordered legacy
recipes, eighteen local recipes and actual continuation. Referenced files are
hash-bound; inline recipes serialize with `json.dumps(recipe, indent=2) + "\n"`
to recover the exact captured inputs. Catalog source counts/order and every
captured recipe hash were checked against the actual complete 102/105 products.
The fixed original DEM payload and receipt are under
`test/outshine/data/transport/wien-height-14-8937-5678.{terrarium,json}`.
It is original encoded elevation data, not a product texture.

Reconstruct the root with `stair_cycle_experiment.py --reconstruct` and its
reader, then supports. Generate/read legacy modules in catalog order using the
existing `stair_module_*` CLIs and prior `--cached-module` pairs. Prepare the
53-source terrain/resident using `stair_patch_experiment.py`, its reader and
`stair_local_prepare.py`. Later recipes use `stair_local_generate.py`, its reader,
the independent `stair_local_reference.py` and `stair_local_display.py`, with
ordered `--prior-module` arguments. For 79, use the current-pier input preparer
from #766 instead of overwriting the historical template.

Recover/check root profiles, then use `stair_resident_snapshot.py` and its
independent reader; `stair_prepared_surface_prepare.py` and
`stair_prepared_surface_verify.py` create/check both candidate formats before a
caller pins the external reference. `stair_prepared_surface_load.py` measures
strict/stored/compressed loading; `stair_resident_snapshot_cost.py --mode chain`
measures the equal original state. Growth CLIs take the explicit reference pair;
current expected hashes must come from that reproduction's independent reader.

Twelve affected controls pass source and installed. Nine actual CLI missing-pair/
wrong-pin cases reject without publishing or changing products. Snapshot readers
pass source/installed; all four real continuation readers pass. Ruff format/check,
diff check and `make lint-docs` pass. Native gates are outside this Python change.

## Explicit remaining consumer gap

Fresh 102-source handoff has 8,113 physical/logical records and 65 real building
bodies, independently read. The default fresh plant search exhausts its declared
128-attempt budget with zero accepted plants; its reader verifies those recorded
outcomes. Historical four-plant outputs were lost and are not freshly requalified.

The new three-source module contributes 210 road facets: 8,308 physical facets
before logical additions exceed the existing 8,192 flat-ledger limit. Mixed
physical/logical prefixes also cannot use the physical-only reuse path. Both
refusals are preserved; no limit is raised and no easier source group substitutes
for the case. #770 owns spatially bounded work and the complete nonempty actual
consumer sequence. Therefore #749 and the full research goal remain active.

Current original inputs, complete snapshots/codec bytes, raw profiles, all query/
module/whole checks, failed cases, terminal logs and opened images remain in
`/tmp/outshine-transport-resume-1f10d55eb/`. Old published reports remain historical.
