# Cached-node grids and shared road/footway paving

Owners: [#627](https://github.com/Weichwolf/outshine/issues/627),
[#633](https://github.com/Weichwolf/outshine/issues/633),
[#600](https://github.com/Weichwolf/outshine/issues/600).
Construction/exports: `64d9451a3b50d19fe8a6d54f517ddfa4ea4e83ab`;
six-case comparison, costs and fresh installed reader: `aefc275802`.
Python research only; no library/native/runtime dependency changes.

The valid Vienna chain grows **76→77 whole original MVT line parts**, preserving
all76 old bodies, sources, attributes and their coordinate frame. The original
mixed six-source request after71 is now represented by bounded verified modules:
one earlier footway, four later footways, then the remaining67.642082403m minor
road. Joint construction remains an actual failing counterexample. This is no
worldwide road generator, routing/swept-body certificate or native admission.

## Actual source and precision model

New source `openfreemap:14/8937/5678:transportation:1605770952:0` retains its two
original MVT points `[1438,1915]`→`[1344,1773]`, original ENU axis and
`surface=paved,class=minor,oneway=1`. Its5.5m profile/two forward2.75m lanes are
procedural; the source supplies no lane count. Longitudinal/crossfall limits
remain0.1/0.03, whole-source clearance4.5m. Original MVT coordinates establish
this input graph, not raw OSM node identities or legal driving permissions.
PBF SHA256 `97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`;
DEM SHA256 `67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.

At the far node, old road sources1578623672:0 and92807082:0 actually use the
verified `source_grid_m=2^-11m` model. Their fixed point
`[-715.61962890625,579.1640625]` and a new ungridded pad intersection
`[-715.6196214081783,579.1640506700892]` are14.005994µm apart and round to the
same Float32 point. The saved small coordinate/owner fixture reproduces this.
Legacy, free-space-union and cavity triangulation do not repair fixed coincident
Float32 constraints. Tiny symmetric-difference area does not prove valid coverage.

Explicit `cached_box_grids_m` binds only declared old source IDs to their verified
cached rows, unchanged axes and actual original root/contact nodes. Only
`profile/shared_box` recipes qualify; at most five bindings. For a new box, use
`floor(min/g)*g`, `ceil(max/g)*g`, retaining the entire original box. Maximum
additional Hausdorff reach is `sqrt(2)*g=0.000690534m`, within |XY|<8190m.
Old bodies, frames, source axes, tolerances and solver/geometry budgets stay exact.
The saved reader independently rebinds these declarations. Ordinary unbound
`shared_box` and `level_box` behavior remains unchanged.

The selected fixture binds only the far157/928 node. Its original near root
1580280942:0 stays ungridded to match the accepted76-source paving. Gridding
every old node instead creates new thin facets and fails the actual new road's
Float32 profile limit. Outward boxes are an explicit alternative construction
model, not a general terrain repair or globally preferable grid.

## Shared paving and access

At the near original node, new minor160, old minor158 and old footway80922970:248
meet. The new road reuses29.270941930m² of that actual old footway cap without
rebuilding it. Opt-in `minor_footway_node` requires a straight unchanged new
minor axis, distinct actual minor root, the same single original node/frame,
verified immutable old footway owner and its full1.8m profile. Axis overrides,
repeated/fabricated nodes, unrelated owners and incompatible access are refused.
The common pad may include declared incident old caps only inside both their
actual reuse mask and the incident profile hull; unrelated assets are not freed.

All old caps used by the new road, including its root/other road caps, must
satisfy the **new road's longitudinal/crossfall ellipse** over every parent facet
with positive intersection in either Double or Float32. The straight source has
one constant frame. Four cap checks pass; maximum normalized foot-cap grade is
0.444393443/0.444409630 D/Q. A footway's isotropic slope check alone is insufficient.
The added owner-facet-use limit is32768; existing local geometry/solver limits stay.

A direct4.478698184m portal compares different actual old footway/new road
facets on opposite sides, over at least the whole old1.8m walking width in D/Q.
The whole new5.5m road profiles are checked separately. Access uses the existing
parser and explicit `procedural_walk_default`: absent walking tags are an
assumption, restricted/conditional/unknown walking access is refused. No motor
or cycle contact arc is introduced onto the footway axis. The new driving source
connects through its actual original road nodes. No indirect/via-root portal
recipe is delivered; [#629](https://github.com/Weichwolf/outshine/issues/629) is reserve.

The complete new source still needs4.5m headroom over borrowed paving. Old footway
clearance2.5m cannot substitute. Source-clearance checks use the whole new source
and its traffic profile. Shared Reservation/Habitat types are unchanged, but the
verified Buildings/Vegetation handoff in #568 still covers the65-source snapshot.
Those consumers must reserve the new car clearance over the borrowed footprint;
77-source downstream consumption is **not yet verified**.

## Same five-source cohort, six bounded alternatives

`stair_cached_grid_compare.py` preloads the same accepted72-source state, then
attempts exactly the same five whole original source parts in every case. Each
stage keeps normal construction budgets. Temporary physical products carry their
real NPZ hashes/asset-local owners; they are not fabricated verification receipts.
Times below sum construction only, excluding stage export/append, from one run.

| Variant | Completed new whole sources | Construction s | Remaining rejection |
| --- | ---: | ---: | --- |
| Footways first, undeclared old paving | 4/5 | 7.243719 | Undeclared cached asset reuse |
| Joint original boxes | 0/5 | 1.177225 | Invalid terrain coverage |
| Joint bound grid boxes | 0/5 | 1.452007 | Invalid terrain coverage |
| Road first, bound grid | 1/5 | 4.927685 | No complete opposite-side walking portal |
| Footways first, every old box gridded | 4/5 | 7.229616 | Actual new Q source grade exceeded |
| Footways first, selective grid/paving | 5/5 | 11.329391 | None in this bounded case |

The selected first-stage module is byte-identical to accepted76; its second
stage is byte-identical to the independently admitted77-source trial below.
Rejected rows do not offer the same completed service, so shorter times are not
speedups. No global optimum or broadly fastest solver is established. The CLI
sets CPU180s/address-space3GiB; this recorded comparison called `run` directly,
so those whole-process CLI caps were not imposed on that measurement. Per-stage
geometry and solver caps were imposed.

## Fresh verification and actual images

**103 source/103 freshly installed controls pass**; installed run uses `-W error`.
Fourteen additions cover actual Q collision/whole outward boxes/immutable source
bindings/default behavior, actual owner caps/frame/nodes, false root/axis/repeated
node, access, insufficient car crossfall/width/height and narrow or stepped portals.
The packed real cap fixture is2707B, SHA256
`4a4959a4a11c323e7dc7056c98d17caeb8d14f71717fc29bdb8af60041f52f50`;
its metadata binds original asset/owner/facet indices/XYZ. All205 installed
module files equal current library bytes; fresh wheel SHA256
`3a44cde75d48d5659146760fc5f8d38ca3f200dc5d64cb9959c00e80b047b5e5`.
The independent installed reader imports51 reference modules exclusively from
that target; source/module/physics/pier proofs equal the source reader. Fresh
source and installed76-source collateral readers also pass without overwriting
the protected original76/72 receipts.

Normal CLI generation, saved reader, independent whole-world reconstruction and
display export all terminated successfully with fresh, inspected artifacts.
New owned surface:18 vertices/16 faces; closed body36 vertices/68 faces and
103.742874027/103.743641934m³ D/Q,36 cyclic vertex links. Whole source contains
345.809580089m² newly owned surface and84.811598797m² unique reused union;
co-owned old-cap areas must not be summed. Two original shared nodes/four full
pad uses pass. Local terrain304→352 faces/216 points; complete world15570 points/
30782 faces and all attributes match independently,30430 outside XYZ facets exact.
All old bridge/body checks and124 whole pier contacts pass; maximum Q contact
error1.517014482e-5m. All original76 source/body products remain unchanged.
Catalog:572 features/1657 line parts,77 selected,1580 unadmitted lines and71
non-line features retain inputs/reasons. `admitted_source_ids=[]` is still native.

Both actual new EEVEE16-sample1280×720 whole views were opened alongside the
two accepted76-source views. Camera, lights, helpers and colour handling match;
only geometry/visible input vertex counts change. Old road display arrays remain
exact. Overview312 changed pixels, network439: a small gray connector closes the
upper-left gap. Whole coverage remains sparse and the pale materials remain weak.
The before PNGs are reused accepted renders; both after PNGs were freshly rendered.
bpy5.2.2/llvmpipe LLVM19.1.7, nice10: overview10.238087s/RSS1809858560B,
network19.624628s/RSS1821351936B, warmed shader cache. No GPU/device/frame claim.
Images stay private in `/tmp`; no gallery/server or `doc/` image publication.

## Work, copies and measured costs

Host WSL2/i5-1135G7 VM,4vCPU/16GiB, one BLAS/OMP thread. Other builds/Java/fuzz
work were running; observed host load was about18. These single wall-time samples
cannot establish a speedup or profiler overhead. Normal CLI generation/export
6.947933s/RSS133705728B, reader9.266338s/RSS154742784B, whole/export32.103793s/
RSS193503232B; these include more work than the preloaded construction below.

| Same77-source output | Local construction | Whole diagnostic control |
| --- | ---: | ---: |
| Normal construction s | 12.226406 | 56.806893 |
| Process peak RSS B | 129720320 | 193761280 |
| Profiled construction s | 9.637555 | 89.125206 |
| Profiled process peak RSS B | 145760256 | 216862720 |
| Positive allocation events | 18068256 | 141492565 |
| Cumulative requested B | 2424654896 | 16006205311 |
| Incremental tracked heap peak B | 2056159 | 52187895 |

All four runs yield the same result hash. Memray1.20 tracks Python/native
allocations after source/resident preload, excluding imports/decode/hash/export
and preexisting allocations; Python allocator tracing on, native stacks off.
Cumulative requests are neither copied bytes nor simultaneous resident storage.
Incremental tracked peak and process RSS cover different scopes. The profiled
local run being faster is consistent with contention; it proves no negative cost.

Concrete local work:13 resident road assets tested, three selected/14 old sources/
1380 body faces/60232B road arrays. Terrain weld409→219 points/191 candidate pairs,
maximum drift6.5231e-11m, then three unused points removed; nine split edges/26
added triangles. Raw XY15696B, unique3472B, inverse7848B; explicit indexed
copies6544B plus difference6544B/displacement3272B. Patch composition changes
27392B old masks and copies0B retained meshes. Internal GEOS/SciPy/NumPy copies
are not fully measured. Local allocation attribution is dominated by Shapely
union/intersection/difference. Even this small bounded case performs18 million
allocations: low final heap and correct geometry do not justify its total work.
The full-world diagnostic deliberately repeats more work. These are research
correctness/cost baselines, not accepted runtime load/frame budgets.

## Reproduction, artifacts and limits

Use Python3.13, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1, Clarabel0.11.1;
no newly required package. Set `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, one
BLAS/OMP thread and `PYTHONPATH=lib/src:test/experiments/transport` or the verified
installed target. Run `python -m unittest stair_cached_box_grid_checks
stair_shared_paving_checks stair_surface_contact_checks stair_metric_line_checks
stair_profile_mask_checks stair_endpoint_checks stair_growth_checks
stair_frontier_checks stair_seed_checks stair_local_checks stair_module_checks
stair_module_chain_checks stair_reference_checks stair_junction_precision_checks`.
These103 controls need no private world state.

For original inputs/prior chain, follow
[the76-source reproduction](stair_surface_contact_comparison.md). Append its
verified76 module to `--prior-module`, select
`fixtures/stair-cached-grid-paving-vienna.json`, then run `stair_local_generate`,
`stair_local_verify`, `stair_local_reference` against the76-source baseline and
`stair_local_display`, each with explicit fresh `/tmp/outshine-*` output paths.
The six-case comparison uses the same resident/prior CLI arguments **through72**,
without appending76, and explicit `--output`. `stair_local_cost` compares preloaded
`--mode local/whole`; add `--profile` only for separate allocation runs. Fixed
collision and real cap fixtures are versioned; original negative terrain inputs,
accepted chain, current exports, profile traces and before/after pins remain local.

| Artifact | SHA256 |
| --- | --- |
|77 generator report | `372dca5bc47bd2b2074d48793b329b4582266104643553190e1c50befa5ab6d5` |
|77 source receipt | `f39ee77d106a052765a48d11da6233956ac10ecb7ea5f489fed4962ab72d4d2a` |
|77 module NPZ | `750b1a546cd8e9747393360370dc7c381d68d9929631359bd99efedb0d655e4e` |
|77 whole report | `fa6718b80a3bb6952aaf293e90d41f53b092ac1a7bfc0d8bfdde2ef4c1fff8cb` |
|Fresh installed77 receipt | `1ab3e0b16cbb95a091671f5f6c2bcbc73903066d0cef2604173f4200eea0c011` |
|Six-case comparison | `0a9fd28a0f5904a5c4e4dc74293c5ab69294feefebbcbb9c4bb530dca45aeaea` |

SIGGRAPH starting points, visually reread locally: Deering1995
[Geometry Compression, p3/section4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf)
and Chen et al.2008
[Interactive Procedural Street Modeling, p2/Fig2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
They motivate explicit precision and graph/construction separation; neither
proves this implementation. No vendor code or SDK oracle was needed here.
Remaining work includes the actual77-source spatial consumers, broader source/
profile/level families, bounded global plans, whole vehicle/pedestrian swept
bodies, cheaper geometry reuse and native/device/Place integration. No tolerances,
quality requirements or runtime budgets were relaxed; the overall goal stays active.
