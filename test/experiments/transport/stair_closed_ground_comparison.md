# Whole original closed ground axes (#699)

The finite Wiener Python specimen now includes two original closed axes: a
145.343730 m minor one-way road and a 180.578558 m paved footway around it.
The [previous 84-source state](stair_automatic_components_comparison.md) becomes
**86 sources**, adding 325.922288 m. Each complete source remains independent
of the old ground network; geometric proximity creates no new connection.
Unplanned entrance/exit sources remain in the original source ledger.
This is a closed-axis geometry feature, not a complete roundabout/routing solver.

Original input is `test/outshine/data/transport/wien-junction-14-8937-5678.pbf`,
SHA256 `97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
The original DEM and root 78 manifest remain bound by SHA256
`67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177` and
`4e0c339d98df06e77cf1aecbf3f5c0386819dba3c188cbd0d325419ba2a9a917`.
All source IDs below have prefix `openfreemap:14/8937/5678:transportation:`.
Fixtures [road](fixtures/stair-seed-roundabout-vienna.json) and
[footway](fixtures/stair-seed-roundabout-footway-vienna.json) preserve whole
original MVT-node occurrences, direction, properties and source geometry.

## Geometry, topology and numerical behavior

Previously every repeated original node was rejected. `stair_seed_axes` now
distinguishes one exact final closure of a simple ring from repeated interior
nodes, self-crossings, false metric closure and degenerate segments. There must
be at least three distinct nodes plus the exact closing occurrence. Ordinary
open-source behavior remains unchanged. The existing generalized MVT graph is
not proof of exact OSM nodes or legal access.

A closed source has no outer endpoint port. Both automatic outer-root discovery
and explicit cached outer-root binding reject that interpretation. A three-point
cyclic bisector profile uses the incoming and outgoing original segments; actual
positive-area facets on both sides must carry the same complete varying-height
trace in Double and Float32. This replaces two artificial outer-profile records.
Interior nodes and any real other-source junctions retain their existing checks.
General attachments at a cached cyclic seam remain a separate future recipe.

The source buffers keep an inner hole. The existing declared binary model
retains whole original width coverage, hole topology and an outward bound of
2.9296875 mm inside its ±8190 m local coordinate domain. Actual road/footway
Hausdorff changes are 1.383979/1.429129 mm. Output XY stays Float32-exact.
The 5.5 m road uses the existing procedural two-lane minor profile: supplied
`oneway=1` yields two forward lanes and no reverse lane. Width/lanes/speed are
procedural assumptions, not recovered survey data. Footway width is 1.8 m.

| New whole source | Surface V/F | Closed body F | Double / Float32 volume m³ |
|---|---:|---:|---:|
|1574593312:0 minor|78/101|312|239.904039 /239.903590|
|80922970:246 footway|95/126|380|97.622188 /97.622872|

Both bodies have cyclic connected vertex links, positive volumes, intact inner
walls and no artificial end closure. Seam traces check all breakpoints: road
12 intervals/168 piece tests, footway 10/120, per precision. Original width,
projected coverage, single heights, source/frame bindings, traffic clearance,
old-ground separation and all actual longitudinal/crossfall limits remain checked.

The first real road correctly failed Float32 crossfall: normalized Double
0.993359381, Float32 1.001408870. Its thin triangle and frame are a fixed
[counterexample](../../outshine/data/transport/closed-ground-rounding-face.json).
The original valid Double fit is preserved as a small numeric witness. Only a
single closed axis without pinned junction plans may test seven finite uniform
height contractions around one common representable level. Each candidate must
move every fitted height by at most **5 mm** and pass the unchanged actual D/Q
facet limits; otherwise the source is locally refused. No XY, width, profile
limit, topology, solver budget or admission tolerance is relaxed.

The road selects factor 0.99: maximum correction 1.145774 mm, final normalized
Double/Float32 0.983425787/0.991269048. Maximum actual crossfall is 0.029591,
below 0.03. The footway selects factor 1.0, changing no height bits. This is the
first valid member of a bounded set, not a globally optimal quantized-height
solver. A fixed negative example has a valid unsampled 0.89 contraction but no
valid tested candidate within 5 mm; its refusal explicitly says “no tested”.
The original cone-fit report describes the fit before this bounded postprocess;
the independent final frame/seam/body audits describe the delivered geometry.

Starting points: Chen et al. 2008, [page 2/figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
separating street graphs from geometry, and Deering 1995,
[page 3/section 4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
explicit local coordinate precision. Both pages were inspected visually again.
The finite contraction and its final numerical checks are independently defined;
neither paper supplies this solver. Standard GEOS buffering/coverage and the
previous seed/height/terrain procedures are reused. No SDK or foreign code is added.

## Whole control and consumers

Independent full construction exactly matches both new modules/layouts/fields
and all 33,328 final XYZ facets with their face attributes; 16,843 field points.
Road locally replaces 252→486 facets/268 points; footway 572→1034 facets/554 points.
They select 5/6 old assets, 16/17 old source columns and 1496/1808 old body facets;
logical copied road working arrays 64,736/77,880 B. Local contexts 946/1350 faces
remain below 8192. No old pier is spatially affected; all 124 whole piers are still
checked by the complete control, maximum Float32 contact error 1.517014482e-5 m.
Soil maxima remain 0.467646/0.156232, below 2/3; no constructed soil is added.

[#701](https://github.com/Weichwolf/outshine/issues/701) fixes only the finite
whole-control capacity contract. For a valid planar triangular coverage,
Euler plus edge incidence gives F≤2V; exterior/hole boundaries tighten it.
The already explicit 32768-point control therefore gets at most 65536 surface
facets, including its contact/bridge clearance calls. Normal 16384/32768 point/face
limits, 262144 body-facet control cap, 1,000,000 pair/20 s clearance limits, local
solver/source/asset budgets and all quality checks remain unchanged. A fixed
130×130 grid has 16900 points/33282 faces and closed volume 83205 m³: default refusal,
explicit full topology/body/contact verification, no silent clipping or raised
local generator limit. The two earlier complete-control refusals remain evidence.

The new handoff contains 7131 volumes: all 6904 old records retain every owner,
source, geometry and plane field; 227 new physical records are added. All 65 actual
building bodies and reservations remain byte-identical. Four complete plants
retain their real forms/SITEs and pass current transport/building conflict checks.
Only four current flattened terrain ordinals (each shifted by -74) and eight
query durations differ in the plant records; source/SITE/world pose/form/normal
fields remain exact. Flattened face ordinals are not persistent source IDs.
All 18 old patch meshes/attributes and 18 old road assets remain byte-identical;
32,294 original active face keys remain active without change. Old logical
headroom declarations remain exact.

The four whole 1280×720 perspective images were actually opened. Both views show
the new ring road and separate footway; existing roads, bridges and piers remain.
Sparse overall coverage, weak path contrast and coarse terrain remain visible.
Before PNGs reuse the previously opened 84-source outputs only after identical
camera/palette/profile/backend inputs were checked; both after images are fresh.
These are private Python diagnostic exports, not native integrated images or a
new gallery. Only master publishes presentation images.

Original failed meshes/frames, source inputs, raw traces, current valid products,
images and process records remain under `/tmp/outshine-transport-loops-2f6b561a0/`.
Geometry producer 075e5dc44, complete control/consumers 6e587558c; final selection
and verification code b44ce117c. Shared library bytes are unchanged from #698;
the separately installed 210-file wheel is reused. Full world planning, LOD,
broader source/level families, seam attachments, vehicle dynamics and device/
native integration remain #434/#568/#48. The overall goal remains active.

## Costs and final checks

After identical source/resident preloading, each pair constructs byte-identical
module/local-layout/local-field arrays. Result hashes are
`a4094a002b52ad9b53843bd50f9497236799447c2a17a8cdcbae8dcdc7b4bf71` (85) and
`a0d5174a2f0a6e61fae8b5ac4da05bc279281a3c4b37f5e7d16dd6bc53378c57` (86).
Imports, IO/preload, result hashing and export are outside these timers/traces.
The whole control additionally audits unchanged world content; this is not an
isolated comparison against an optimized runtime implementation.

| Construction | Profiled s | Positive allocations | Cumulative requested B | Tracked incremental heap peak B | Process peak RSS B |
|---|---:|---:|---:|---:|---:|
|85 local|12.192465|15,823,232|2,066,172,957|2,045,977|148,058,112|
|85 whole|103.674026|163,552,369|18,439,904,911|57,483,300|221,667,328|
|86 local|27.897725|27,378,826|3,642,001,215|3,704,578|156,479,488|
|86 whole|104.604308|167,272,770|18,911,574,760|58,577,237|227,926,016|

Requested bytes are not held RAM, copied bytes or memory bandwidth. The tracked
peak excludes previously loaded objects; process RSS includes imports/preload and
is a separate observation. All native/Python allocations are traced without
native call stacks. The local road working arrays above and whole outside-XYZ
comparison buffers (4,662,720/4,650,336 B) are explicit scoped copy counts, not
all traffic. The original closed-fit witnesses add 624/760 B to module arrays.
Even the local paths remain far too expensive to approve for runtime use; the
small new geometry does not justify millions of allocations. Revalidation of
unchanged content and temporary geometry work remain concrete optimization needs.
No CPU/GPU/frame/loading/device budget is increased or declared satisfied.

Four actual cost workers record terminal 0; all four fresh statistics JSONs were
fully written and their counts/metadata and equal results checked. The outer tool
session later reported 143, so a successful wrapper exit is not claimed. No cost
worker was restarted, and no observation timeout was treated as a new job.
Raw finalized traces and outputs remain retained with that limitation.

Cost environment: Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Clarabel 0.11.1,
Shapely 2.2.0/GEOS 3.14.1, Memray 1.20; WSL2/i5-1135G7 with four logical CPUs and
16,190,028 KiB RAM. Measurements run at normal priority; other checks at nice 10.
Host-load observations are in `cost-terminal.json`; these shared-host profiled
wall times are not isolated latency or A18/RTX evidence. Controls/actual plant
and building readers use the existing NumPy 2.5.4/manifold3d 3.5.4 environment.
Software-GL Workbench/bpy 5.2.2 renders both new whole 1280×720 views; no Cycles/GPU
performance claim follows.

153 source plus 153 installed controls pass. Both new stages reproduce all seven
original artifacts exactly in both import modes, followed by fresh strict cold
readers. Each generation/query closure has 52 matching reference imports;
controls and current actual consumers have 119 each. Every one of the 210 library
Python files matches the reused installed wheel, SHA256
`86c3e5709aa33ee06a8b9b9788c30eb536d2aff8bb0d9594d7c00694eadcafcb`.

The complete 86-source state SHA256 is
`3b907ad17a4b73f641de3d3641e9aaa66d9d2f6d8212a2b4348e074ac770844d`.
Both import modes preserve 424 points per Double/Float32 precision with identical
index/scan results: 11,412 indexed versus 22,908,850 scan face tests. Current actual
7131 volumes/124 supports/128 probes/65 building bodies/four full plants pass in both
modes. Prepared 78 terrain restoration is reused through #698's known-product
library path; the later five local replacements still run strictly.

Closed-root filtering is an explicit research selector option, not a new global
world scheduling policy. Indexed and complete-scanned discovery agree on roots,
recipes and all source dispositions on both 84 and 85 states. On 84, the single-road
recipe is the existing bounded continuation at offset 6, one attempt; larger
proposals are retained and are not declared impossible by this case. On 85 the
ordinary first window (up to three attempts) selects the whole footway in its
first attempt. Both automatic results have all seven artifacts byte-identical
to the canonical 85/86 products. Four fresh source/installed saved readers also
pass; accepted original receipts are preserved, with disposable reader copies.

Standard `test/scripts/pixels.py` confirms localized visible changes: overview
956/921600 pixels (0.1037%), opposite view 628/921600 (0.0681%), mean RGB errors
0.005773/0.004626 of 255, maxima 49/53. Neither before nor after has black or
near-black pixels under the script's criterion. Pixel counts are not a quality
score; actual image inspection establishes the new visible rings and remaining
deficits described above.

Reproduce the focused controls with `python -m unittest stair_closed_seed_checks
stair_seed_checks stair_growth_checks stair_frontier_checks stair_reference_checks`
using the reference/experiment PYTHONPATH. Use `stair_local_generate`,
`stair_local_verify`, `stair_local_reference`, then the real occupancy/building/
plant drivers with the retained parent chain and versioned fixtures. Automatic
comparison uses `stair_growth_compare --family seed --seed-root-form closed`;
actual admission/saved reading use `stair_growth_experiment`/`stair_growth_verify`
with that same mode and explicit attempt windows above. The unchanged default
continues to consider all supported seed forms. Commands, process terminals,
import hashes and complete product hashes are in the retained private drivers/
receipts; outputs must be fresh explicit /tmp paths. Ruff/format/function-size,
diff and documentation lint accompany delivery. Native gates follow integration.
