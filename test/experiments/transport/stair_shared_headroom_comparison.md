# Logical road headroom over unchanged walking facets

Owners: [#642](https://github.com/Weichwolf/outshine/issues/642),
[#568](https://github.com/Weichwolf/outshine/issues/568).
Producer and initial independent readers: `23372cb61f642871f1ffce41ddd47e717ee273de`;
actual images: `df563fa2c03c53edf3c78849c19e37b3f059ee87`.
Final guards, source/installed readers and repeated paired costs: `b2c3b5827a6e1a655fe70e9e26765cd71ba15b63`.
Python experiments only; shared library/native/runtime types stay unchanged.

## Failure and corrected spatial product

The [77-source world](stair_cached_grid_comparison.md) already verifies the new
minor1605770952:0's complete4.5m clearance, including borrowed old walking caps.
The old occupancy exporter instead inspected only physical facet owners. Over
80922970:248 it therefore exported2.5m walking clearance. Fifteen actual small
height probes in the reused29.270941930m² pad were falsely free. Source/road
geometry, original graph, IDs, attributes and coordinate frame were correct;
the downstream spatial product was incomplete.

`stair_shared_headroom.binding` binds the accepted module's declared
`minor_footway_node` contact to its earlier immutable asset, owner column,
complete actual facet indices and XYZ hash. Recompute the whole new source
corridor/pads and its intersection with the old owner's actual cap. A missing,
changed or incomplete mask/owner/clearance binding refuses the export. Existing
retained refusals in the input chain add no body and remain valid chain entries.
No source axis, physical body, terrain mesh or routing relation is rebuilt.

Physical reservations remain intact. For each positively intersecting old facet
whose existing owner clearance is insufficient, append only the missing upper
band: lower plane is the existing physical volume's **exact upper plane object**;
upper plane raises its intercept by the clearance difference with outward binary
rounding. Clip the domain to the actual new source. This leaves no seam gap and
avoids duplicating the already reserved body/lower headroom. The exported row
retains both logical new source and physical old source; the report additionally
binds the actual old facet/asset and both clearance values. No motor route on the
walking axis is introduced.

Actual77 product:5329 road facets plus1134 support facets, and15 supplemental
bands = **6478 volumes**. Only39 old footway owner facets need intersection tests.
Every whole parent facet's upper height is independently checked with exact
binary-rational arithmetic against original XYZ+4.5m, outward by at most1e-8m.
The lower plane must equal the already validated physical upper plane. An exact
original-triangle barycentric witness independently places a saved actual probe
strictly above the old2.5m limit and below the required4.5m limit.
Full duplicate bands and upper-only bands give identical positive conflict
geometry for231 queries:156 original footprints/bridge probes plus75 diagnostic
height bands around the15 actual failure locations. Height candidates are
390→352 in this mixture; this is no general query-time or memory bound.

This fixes the actual walking-pad/new-road family. The existing affine
binary64-XY/topology limits in [the precision report](stair_occupancy_precision.md)
remain. These spatial volumes bind Float64 XYZ; a conservative Float32 spatial
volume envelope is not established by earlier D/Q road-body checks.
Error/budget exhaustion does not authorize free space. There is no
general legal-routing, swept-body, native or public affine-library admission.

## Reuse before fitting

Optional `--prior-occupancy` loads a verified physical-only prefix rather than
fitting all old planes again. Root resident hash, frame, ordered original source
prefix, parent reports, physical facet IDs/order/source owners and support asset
must bind to the current immutable chain. The prior saved-volume hash and
successful receipt are checked. Prefixes already containing logical supplement
rows are explicitly outside this first reuse path. The final independent reader
still checks **every** current physical cap against actual XYZ, including reused
ones; this is not self-certification from the prefix receipt alone.

For the saved65-source prefix, reuse5986 volumes and fit only477 new ones instead
of6463: **92.62% fewer physical-volume fits**. Append the same15 new logical bands.
Fresh and reused complete `volumes.json` are byte-identical, including all6478
domains/planes/owners/source IDs. This is a bounded research handoff, not a new
cache framework or runtime codec. Prefix JSON decoding, validation and allocation
remain substantial real work.

## Controls, actual consumers and images

38 source/38 installed affected controls pass with `-W error`. Ten new controls
include the real versioned old-cap/height counterexample, whole mask/owner/XYZ
bindings, false motor claims/clearance, exact upper/lower seam, missing/shortened/
changed extra rows, same conflict service as full duplicate volumes, exhausted
facet budgets, changed prefix lineage, retained refusals and injected nonempty
zero-area clipping results. Producer/reader refuse such results; detecting GEOS
results that already became empty remains part of the general precision limit. An additional existing SITE-consumer control covers a free root/old walking
band with an overhanging whole plant envelope in the new upper car band: the
plant is refused before graph emission, while pure ceiling contact stays free.
This is an analytic consumer counterexample, not a newly observed real planting.
Original cap fixtures remain unchanged. The additional counterexample is
`fixtures/stair-shared-headroom-vienna.json`.

Normal fresh/reuse CLI exports terminate0. Initial and final independent source
and installed readers all pass; each final source/installed receipt is byte-equal,
checking6478 volumes/77 sources/124 whole supports. The installed reader imports51
reference modules exclusively from the existing verified target. Its205 Python
module files still equal current library sources; reused wheel SHA256
`3a44cde75d48d5659146760fc5f8d38ca3f200dc5d64cb9959c00e80b047b5e5`.
No unchanged wheel is rebuilt just for these experiment-only changes. Original
65/72/76/77 transport/consumer receipts and source inputs remain intact.

The existing Buildings producer and fresh source/installed whole-body readers
pass on77:100 valid polygon parts/10 invalid original features,64 prepared parts,
65 emitted bodies,46 deferred ledger entries with reasons. Two original provider
footprints still lose40.792421876m²; this particular building set does not exercise
the missing upper car band, which is established by the saved height probes.
The existing Pinus producer and fresh source/installed readers also pass:
394 original habitat parts,128 complete attempts, one accepted SITE. Prototype
289094 vertices/549948 triangles is byte-identical to the earlier four-plant
product: SHA256 `c4b6da222245ce52fca9967dcd79105ac69c247978ea3fa537aebb340661fefc`.

**Observed selection regression:** only one old site is selected again. Direct
replay proves all four old roots/whole conservative envelopes remain unchanged
and free; three are simply not attempted within the globally reprioritized128
trials. This is not necessary removal for new clearance or a plant-form defect.
[#646](https://github.com/Weichwolf/outshine/issues/646) owns preserving validated
SITE products before generating new proposals. No density or stable-world claim
is made from the one-site result; the bounded pilot is not a production planner.

Two fresh whole1280×720 EEVEE16-sample images were opened: the actual77-source
world plus65 building bodies, without/with its one fully emitted Pinus. Same
camera/lights/helpers; **seven pixels change**, three by more than1/255, worst5.
These images compare plant presence, not the spatial headroom fix or cost methods.
No significant visible gain is claimed. Coverage/materials remain pale and sparse.
bpy5.2.2/llvmpipe LLVM19.1.7, nice10:44.231016/19.562003s,
RSS2302771200/2240606208B. First uses a fresh private shader cache; later cache is
warm. CPU background builds/statistics were running. No GPU/device/frame evidence.
All images stay private under `/tmp`; no server/gallery or `doc/` image publication.

## Cost evidence and tradeoff

`stair_shared_headroom_cost` compares identical preloaded77-source/asset inputs,
fresh/reuse/reuse/fresh order, then separate Memray1.20 Python/native-allocation
traces with Python allocator tracking and native stacks disabled. Timer/trace
scope is preparation, including old JSON decode/validation; imports, source/asset
preload, output digest and export are outside. All six outputs have result hash
`c635611216f933f4ed25e7f9ee788d0e89d94e47bda41cfbd1faacb087f01f0d`.

| Same6478-volume output | Fresh fits | Verified prefix reuse |
| --- | ---: | ---: |
| Normal paired construction s | 8.407295 /8.578083 | 2.845833 /2.919407 |
| Profiled construction s | 28.035065 | 9.994728 |
| Positive allocation events | 45013243 | 17179132 |
| Cumulative requested B | 4292561044 | 1651057552 |
| Tracked incremental heap peak B | 8959755 | 13828890 |

Reuse reduces allocation events61.84% and cumulative requests61.54%, but tracked
temporary peak **increases4869135B**, as JSON/metadata and decoded volume objects
coexist. This tradeoff is explicit, not an overall memory win. Cumulative requests
are not copies or simultaneous storage; process RSS spans both methods/preload:
160579584B, not separate method RSS peaks. Fraction construction is a leading
allocation site. Seventeen million remaining events are still excessive for a
small research handoff; none of these results accepts runtime/device/world costs.

Host WSL2/i5-1135G7 VM,4vCPU/16GiB, one BLAS/OMP thread. Other Python,
clang-tidy and MIDI-fuzz tasks were present. Load≈14–16 was observed earlier
in the block; per-run final host load was not captured. One paired order is a local trend,
not a portable speedup guarantee. Producer/reuse including source load/export:
7.556120s/RSS156913664B; whole current source reader/consumers measured separately.
Consumer architecture generation7.112608s/RSS200409088B; full Pinus process
RSS232202240B, placement1.799578s. No native/Place acceptance follows.

Supplement construction uses existing affine plane/XYZ objects; its recorded0B
geometry-array copies describes that supplement stage only. Binding the39 old
facets separately materializes2808B XYZ for its immutable-cap hash plus index/
geometry-reference arrays. Clipped GEOS domains and JSON objects allocate; complete
internal GEOS/NumPy copied-byte totals remain unmeasured. The reused prefix reads
3353523B serialized JSON; final full-volume export is3625845B. No source/body/
terrain solving or retessellation is repeated. Current consumers still assemble
`whole(view)` for their terrain sampler; that assembly/copy is outside these
preparation timers/traces and has not been removed by the0B supplement figure.
Every remaining allocation still needs a purpose;
research correctness machinery is not a runtime implementation prescription.

## Reproduction and provenance

Follow [the77-source chain](stair_cached_grid_comparison.md) and the saved65-source
[occupancy prefix](stair_occupancy_comparison.md). With current resident/prior
arguments **through77**, original full DEM, explicit fresh `/tmp/outshine-*` paths:

```sh
python -m stair_occupancy_experiment --resident "$RESIDENT" --prior-module "$MODULE55" --prior-module "$MODULE57" --prior-module "$MODULE65" --prior-module "$MODULE69" --prior-module "$MODULE71" --prior-module "$MODULE72" --prior-module "$MODULE76" --prior-module "$MODULE77" --dem "$FULL_DEM" --prior-occupancy "$PREFIX65" --output "$VOLUMES77"
python -m stair_occupancy_verify "$VOLUMES77"
python -m stair_shared_headroom_cost --resident "$RESIDENT" --prior-module "$MODULE55" --prior-module "$MODULE57" --prior-module "$MODULE65" --prior-module "$MODULE69" --prior-module "$MODULE71" --prior-module "$MODULE72" --prior-module "$MODULE76" --prior-module "$MODULE77" --prior-occupancy "$PREFIX65" --output "$COST77" --profile
```

Omit `--prior-occupancy` for the full fresh reference. Producer/cost CLI caps:
CPU180s/address space4GiB; measured launches additionally wall240s. Set
`TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, one BLAS/OMP thread. Producer/cost:
Python3.13.5, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1. Actual consumer/control environment
uses NumPy2.5.4 and existing manifold3d; SciPy1.18.1/Clarabel0.11.1 unchanged.
No new package installed. Test/consumer `PYTHONPATH` includes `lib/src` (or verified
installed target), `lib/tests`, `test/experiments/{transport,buildings,vegetation,assets}`.
Run `python -W error -m unittest stair_shared_headroom_checks stair_occupancy_checks
stair_occupancy_precision_checks stair_consumer_checks stair_plant_checks`.
Existing consumer/display commands in their respective reports remain applicable.

Private accepted products/pins/traces: `/tmp/outshine-transport-handoff-a46b62408/`.
Main SHA256 values:

| Artifact | SHA256 |
| --- | --- |
|Initial77 volume producer | `b901ec5f4b70152d9c7525c5695eba21a30c84fe203cd4f600968d6e97efed82` |
|77 complete volumes | `bab72dc49df84c04a2fd32e007584632eba5ae8c48f395330e955f3b107be308` |
|Initial source/installed reader | `a1577901dd4bc11972cdc75f3d1fc6fa5168e1b941c1f90dffa8a692afe807ab` |
|Final guarded producer | `7f3437a6e697b3ba054d716420cf21fa81e244dadadbf7fee610d71efd78cb60` |
|Final source/installed reader | `04b6fa3ac804c590af091317a42640c1c6ebc79c9ed1f50a4e4c954683ef28c0` |
|Actual Buildings report | `981bc96878c64bbf8eb9e08d5a88dcc701027daf3642b8bc3346b17e2cabb457` |
|Actual Pinus report | `0e4021086c58bcb2957b175fba30f3f85e2c0d785de0194f816f17e57f0c388a` |
|Paired preparation costs | `71f185f88eeef007104996ff40a40b3910e384ed4e7a447f424472ecc48494bf` |

SIGGRAPH starting point reread visually: Chen et al.2008,
[Interactive Procedural Street Modeling, p2/Fig2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
Graph/construction distinction motivates keeping logical traffic requirements
separate from physical facet ownership; this paper does not establish this
headroom algorithm. No SDK reference, vendor code or runtime SDK dependency used.
General affine precision, stable updated SITE selection (#646), wider transport
coverage/recipes, runtime assets and native/device integration remain open.
