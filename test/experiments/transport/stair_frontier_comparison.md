# Bounded original-source discovery and actual interior roots (#537)

The experiment discovers alternative source modules from the unchanged original
Wien provider input and actual cached interior profiles. It preserves the
verified 43-source parent from #535. The reusable discrete operation is
[`transport.frontiers`](../../../lib/frontiers.md); automatic admission and
independent replay are `stair_frontier_experiment.py` and
`stair_frontier_verify.py`. `stair_frontier_compare.py` compares indexed lookup
with complete node scanning on cached Zürich, Basel and Midtown Manhattan data.

Original sources and semantic reasons remain separate from proposals and actual
geometry acceptance. The current ordinary-ground adapter excludes indoor,
non-ground levels, bridge/tunnel recipes and unsupported profiles. A root is an
explicit original vertex in an actual saved parent road, with unchanged original
axis, actual three-point cross section and covered whole profile. Provider-node
coincidence alone is not an exact OSM topology claim.

New endpoint and through profiles share one complete root hull with the frozen
old cross section. Whole actual source surfaces, junctions, bodies, Double and
Float32 profile contacts, terrain boundaries/fields and all old pier contacts
remain mandatory. Optional per-source overhead checks use actual lower facets,
including conservative whole borrowed old-cap facets, and whole bridge facets:
foot/cycle require 2.5 m, the current minor-road profile requires 4.5 m. The old
strict bridge-overlap exclusion remains the default family.

The first prototype `177fe724a` generated a locally valid mesh but its independent
saved-product reader rejected partial root pads. It is not a passed acceptance.
`bind_root_pad` now includes frozen, through and branching profiles in the same
whole hull. A three-profile counterexample protects that correction. Proposals
that fail geometry retain the old valid world and an explicit reason; no
boundary epsilon, terrain/pier contact or solver budget was relaxed.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5 figures 7–8 and section 6.1, inspected visually. Separating
graph/seed topology from geometric construction is useful here. This is an
original bounded discrete provider-source traversal, not their tensor-field
generator. No SDK code or dependency is used.

Current scope is one automatically selected module of at most eight whole
sources, at most eight candidate roots, at most three actual geometry attempts.
Alternative proposals may overlap. Original input limits and all older body,
source-owner, parent-cache, solver and contact limits remain unchanged. Complete
worldwide source coverage, global module/cache graphs, reservations/LOD products,
all route/vehicle dynamics, complete road-generator equivalence and native
integration/device acceptance remain #434/#50/#48.

## Frozen products and controls

Accepted generator and independent reader: `2a74dbeab81877090ffba23379c38d6385086994`.
The first proposal passes in one whole-geometry attempt: eight original parts,
1573.028217 m, rooted at cached `138663000:8`, original vertex 5. The new parts
are `4420992360:4`, `138663000:3`, `97041640:2`, `80922970:208`,
`80922970:211`, `80922970:262`, `9988118722:0`, `39881120:20`, each prefixed
`openfreemap:14/8937/5678:transportation:`. All eight are foot/cycle sources;
this delivery does not add another minor-road body.

The unchanged PBF has SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`,
1657 line parts and 6014 original node occurrences. Actual profile widths admit
465 currently eligible ground sources inside the existing terrain bounds.
Six actual cached interior roots yield identical indexed/scan recipes. The
largest uses 39 node lookups, 66 indexed incidence visits versus 63024 naive
node comparisons. All six use 95 lookups, 148 visits versus 153520 comparisons;
these count different operations. Index construction holds 1432 nodes and 1613
unique source incidences. Actual cached-profile sampling, semantic checks and
index/validation setup are included in wrapper times 0.097244/0.194915 s.
This single loaded comparison is evidence of reduced work, not a device bound.

Actual new surface: 275 vertices/411 facets; closed body: 550 vertices/1108
facets. Ten new whole inner endpoint profiles, one newly bound old interior
profile, four new through-source cross sections and six outer profiles pass
Double/Float32. The preserved parent contributes 74 inner endpoints, its prior
old interior contact and twelve outer profiles: 51 sources, 84 inner endpoints,
two cached interior contacts and eighteen outer profiles. Through-source cross
sections are counted separately. The frozen old source's outer port is retained.
The intersecting new cycle source has whole overhead clearance 2.884346756 m
Double / 2.884350768 m Float32, above its unchanged 2.5 m requirement. Other
new sources have no positive-area overlap with the whole upper bridge surface.

Local terrain: 930 selected old facets, six protected parent facets, four
affected old piers, 1256 points/2310 facets, 200 exact boundary vertices and 985
fixed heights. Numerical welding moves no point: 1822 input/1269 unique
vertices, 553 candidate pairs; thirteen unused points removed, 73 split edges
and 453 conformance facets added. Whole terrain has 14064 points/27770 facets;
closed world body 28128 vertices/56252 facets. All 124 whole pier contacts pass,
maximum existing Float32 contact error 15.170 micrometres. Outside the local
patch 25460 old XYZ triangles are byte-exact. Old road display arrays are exact.
Existing 16384/32768 terrain limits, four parents, 64 owner bits, eight-source
modules, solver limits and Float32 rounding-derived guards remain unchanged.

107 source and 107 installed controls pass, including original-input/root-axis
binding, three-profile whole root hull, T profiles, headroom and all related
surface/height/weld/curve controls. The previous 1000+2000 ramp and whole swept
hull corpus remains the evidence in [#535's comparison](stair_road_curve_comparison.md);
its library code is byte-identical. Fresh wheel: 201 modules, every installed
Python file equal to its source; previous 200 unchanged. Wheel SHA256
`316da89afd9fc51f736b948f19f2c5cfba32002dfb309b00b5ef54f0a931549b`.

Whole generation blocks nine old planning entry points and calls the new
arrangement once with eight sources. Independent replay blocks sixteen
CDT/solver/body-generation/array-export entry points, replays original source
selection, checks exact recipes and all actual saved module/world products;
terminal exit 0. Automatic report SHA256
`e5069661c5c8d549d3c2d2f935fa3a882e4cda77fc27fbee77522432143fdad9`;
chosen module report
`e6c35f9561b057ee6cf9ced79ac08d354f0604e6a9ed04f03f5208f6133d5cb5`.
A fourth explicit cache-parent import restores all 51 sources/eighteen ports
without old planning. Four full original Terrarium files decode exactly to the
versioned crops; 15636 world/local/module/original-axis prior samples equal
full-raw bilinear sampling bit-for-bit. Input hashes and parent receipts remain
in the linked preceding comparison and private `raw-proof.json`.

## Further real sources and costs

Cached OpenFreeMap `20260927_080001_pt`, zoom 14; four longest eligible original
endpoint roots per case, same bounds/semantics and eight-source budget in both
methods. These roots are graph controls, not actual saved road profiles. There
is no DEM, physical mesh or 3D admission claim for these three cases.

| Input | Original parts/nodes | Indexed visits / naive comparisons | Complete indexed / scan seconds |
|---|---:|---:|---:|
| Zürich, 14/8579/5737 | 3222 / 12509 | 679 / 3350628 | 0.075691 / 1.388391 |
| Basel, 14/8538/5724 | 1407 / 5629 | 365 / 578619 | 0.025644 / 0.272990 |
| Midtown Manhattan, 14/4824/6157 | 2621 / 8510 | 422 / 971040 | 0.062411 / 0.295042 |

All selected IDs, parent links, lengths and stop reasons match. Raw PBF hashes:
Zürich `526fabb609cc07657589c47fdbd3bc2b2018c0eaa49a577f7c80c6d12661b2db`,
Basel `7de0e6cf9ca99739f716f967c7539c95dacee4d25d3d54deda8bef3fd23714c6`,
Manhattan `1d5b5d3dcd35b1e1c98ad2c1df93ea77e4a26afa6bb502cd7244f8c206ba05b7`.
Receipts retain original URLs/bytes; the comparison runs with `fetch=False`.

Current Linux VM exposes four logical CPUs, Intel i5-1135G7, Python 3.13.5,
NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0. BLAS/OMP one thread, background
processes nice 10; concurrent Buildings CPU/render work. Automatic generation
takes 23.318928 s / 184795136 B peak RSS, including source selection, cached
inputs, whole actual products and serialization. Its module generation alone
takes 22.753638 s; independent module reader 20.591597 s. No frame or native
device bound follows from these offline diagnostic timings.

Allocation-profiled same automatic build takes 72.614057 s / 208150528 B peak
RSS. Memray 1.20.0 traces Python allocators and native allocation events, with
native call-stack unwinding disabled. Whole `run()` scope after imports:
112471168 allocation events, 12872067016 B cumulative requests, 59608584 B
tracked peak heap. Requested bytes are churn, not live or unique memory; RSS
also includes imported/native resources. Renderer, independent reader and stats
subprocess allocations are excluded. All nine normal/profiled NPZ products
match in every array's shape, dtype and bytes. Explicit local raw XY 89904 B,
unique XY 20096 B, labels 44952 B; assembled XY/labels 342064 B, outside XYZ
comparison 3666240 B. Display arrays own 10787664 B. These counters do not
observe every internal native copy. Global physical diagnosis remains costly.

The first statistics process wrote complete JSON but exceeded its 120 s process
deadline during teardown (143); it is not a successful process acceptance.
The repeated bounded official CLI returns successfully; fresh JSON is fully
decoded and validated before explicit flushed exit 0, excluding interpreter
teardown. This does not alter allocation recording or reported measurements.

## Actual images, reproduction and remaining gaps

Four fresh Eevee images were fully decoded and actually opened: before/after
whole overview and moderately closer network view, each 1280×720. Source mesh,
scene and PNG hashes bind every receipt. Software backend llvmpipe LLVM 19.1.7;
render times 18.081586/13.729662/16.136262/15.561254 s. New paths extend the
central/left/northern network, retaining bridges and terrain. `pixels.py` finds
5921 changed overview pixels (0.6425%) and 7761 network pixels (0.8421%). This
is a small visible coverage improvement; the scene remains sparse and pale,
with angular outer nodes. No public gallery, native image or GPU-performance
claim is made. Research images remain private system-temp artifacts.

Authoritative private evidence root:
`/tmp/outshine-transport-frontier-ec987a8fd/`; accepted complete output
`/tmp/outshine-transport-frontier-auto-2a74dbeab/`, its child `outshine-module-0/`.
`automatic.py` preserves exact guarded generation/replay/profile commands;
`automatic-reader-guard.json`, `normal-profile-array-proof.json`,
`installed-proof.json`, `raw-proof.json`, `fourth-parent.json`,
`automatic-render-completeness.json` and terminal logs distinguish scopes.
`partial-root-counter-177fe724a/` retains the rejected pads/mesh; the root also
retains its immutable recipe and failed reader evidence. Superseded whole
outputs/images were removed.
`interface-countercase.json/npz` retains another locally rejected interface:
literal shared boundary is a point despite near-coincident canonical edge. It
is not repaired by increasing the wall/contact tolerance.

To reproduce, install `./lib`; keep the verified base/supports and three
parents listed in [#535](stair_road_curve_comparison.md). Pass them using
`--base`, `--supports`, three ordered `--cached-module OUTPUT FIXTURE` pairs,
and fresh explicit `--output /tmp/outshine-...` to
`stair_frontier_experiment.py`; `stair_frontier_verify.py` takes the same arguments.
The automatic recipe is produced from original nodes; no handpicked source-ID
fixture is required. `stair_frontier_compare.py --sources CACHE --output
/tmp/outshine-...` replays all three other cities offline. Ordinary source/DEMs,
needed parents and counterexamples are preserved; no private default output is
introduced under `build/experiments/`. Format/Ruff, changed functions ≤120 lines,
107 relevant controls and docs lint pass; native engine gates are not run for
this pure Python delivery.

The other proposals remain alternatives, not accepted parallel modules. Earlier
local probes expose infeasible terrain, frozen-contact height conflicts,
undeclared old-cap reuse and an empty literal interface. Full automatic world
growth, multi-module overlap/priority, source-class/level recipes, exact OSM
identity and all previously listed global/native acceptance remain open. The
source catalogue preserves all 1657 original parts/71 areas, 51 candidates,
1606 unused lines and every reason; native `admitted_source_ids=[]` remains.
