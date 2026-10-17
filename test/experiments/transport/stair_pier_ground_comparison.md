# Whole cycleway around an immutable pier (#668)

The original951.788267m Wiener cycleway39881120:32 now has a complete physical
candidate on the verified53-source resident with recovered root profiles (#660).
Producer38adbd3c4, recipe `fixtures/stair-pier-detour-vienna.json`. This is a
separate54-source cohort. The existing78-source chain remains unchanged;
coherent reuse of its bodies with the recovered profile metadata and a79-source
result are still required. Neither #660 nor #668 is closed by this delivery.

## Bounded interpretation and terrain choice

The actual old root32651280:3 at original MVT vertex8 supplies the whole existing
1.900073m contact. The raw cycleway intersects actual immutable pier123. A bounded
four-knot lateral interpretation between original knots19/20 clears it while
preserving all31original knots, all attributes and IDs. Four-metre straight runs
retain the original endpoint tangents. Offset0.673428m; extra length0.054092m;
the whole planned route is951.842359m. The declared model caps offset at3m,
extra length at1m and binds the actual whole pier's XYZ hash and footprint.

The new corridor replaces the colliding raw band:28.987703m²of the old band is
removed and29.095886m²added. The original provider axis stays archived. Ordinary
curve recipes retain their existing original-band union semantics. This explicit
walking/cycling interpretation introduces no new motor/access/routing semantics.
Original shared MVT coordinates are source provenance, not original OSM node or
legal routing evidence. Earlier two-knot/visibility comparisons are in #668;
this physical version adds straight guards rather than claiming those initial
planar proposals were already valid3D products.

The first whole3D attempt preserved every coarse terrain triangle touched by a
pier. One71.244188m²parent intersected the new road over10.374727m². Private larger
route hypotheses did not produce a valid whole road; they prove no impossibility.
The simpler chosen alternative refines terrain around the actual fixed interface.
Explicit `pier_ground_policy: whole_foot` retains original terrain planes under
each complete Double and Float32 foot, plus a1cm geometric guard. This guard is
additional protected ground, not a larger permissible contact error.

For three affected feet, oriented rectangles contain both actual footprints.
Only their intersections with selected nonroad parent facets are triangulated:
five intersecting parents,25polygon coordinates, conservative25-triangle bound,
ten actual pieces,16shared reference vertices,1.156257m²protected area. Original
plane interpolation supplies heights. Inputs/triangulation are bounded; no global
field assembly occurs in the local generator. Default `whole_parent` remains
unchanged and retains its original arrays/pins.

The private first prototype flattened triangles into disjoint vertex indices.
The existing sampler correctly called two actual shared-vertex queries ambiguous;
missing fixed heights subsequently moved foot121 by0.080271697m. Indexed shared
XY, checked consistent heights and an explicit missing-protected-pin rejection
fix this construction error. No sampler ambiguity rule or contact bound changes.
The independent saved-field reader samples original full parent planes under the
declared guards; it does not rebuild the producer's clipped reference triangles.
Its new reference-height comparison uses the existing1e-7m physical metric;
legacy pin comparisons remain exact. Final actual whole-foot checks remain strict.

SIGGRAPH-first robustness background: Cherchi et al., *Interactive and Robust
Mesh Booleans*, SIGGRAPH Asia2022, [local paper](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf).
The earlier local inspection includes page2/Figures2–3. This experiment uses
existing GEOS constrained triangulation and own plane/index assembly, not the
paper's implementation or an exact-predicate/whole-world proof. No foreign code,
SDK/runtime dependency or shared lib/native contract is introduced.

## Geometry, independent readers and actual consumers

122source and122installed controls pass, including five new real-foot/reference
checks and six bounded-detour checks. Actual saved reference inputs are versioned
in `fixtures/stair-pier-ground-vienna.{npz,json}` (1957Bpayload) and
`stair-pier-detour-obstacle-vienna.{npz,json}`. They retain original frame/body
provenance. A fresh real-product negative removes one protected pin and rebinds
the layout artifact digest: the independent reader still rejects it before any
successful receipt, rather than relying solely on an old hash mismatch.

Fresh source and installed readers accept the whole saved54-source result.
Independent whole reconstruction proves28,764exact XYZ facets, exact face
attributes and bit-exact module/local-layout/local-field arrays. The local patch
replaces913old facets with1715faces/946points, retains175exact outer boundary
vertices and leaves27,049outside facets unchanged. All53old road bodies remain
unchanged. The terrain solve takes25iterations; ordinary soil grades D/Q are
0.664667/0.664667, maximum constructed grades8.722098/8.721757 on18faces, below
the unchanged2/3and10limits within the existing5m construction zone.

All124whole old pier contacts pass,7286pair checks, maximum Float32 contact
error0.0000151702m within each independently derived envelope. Actual whole
cycleway clearance under existing bridges is5.040364m Double/5.040360m Float32,
above its2.5m requirement. Complete bodies/profiles/longitudinal/transverse frames,
old road beds, local coverage and exact outside gluing remain mandatory.

Actual spatial export:4564road/headroom facets plus1134support-cap facets,
5698total volumes, all124supports, no supplemental higher-clearance band needed.
The fresh reader checks every actual cap; maximum affine bound error is
2.873e-14m. Existing Buildings generates65actual bodies/reservations from64
prepared architecture parts, with two adjusted source parts. Fresh source and
installed readers check original attributes, terrain anchors and whole conflicts.
Four previous complete plants remain valid against these actual54-source
volumes and buildings: four retention checks, zero new proposals. Both plant
readers replay the full289,094-vertex/549,948-triangle prototype and actual SITE
products; this is no general crown/terrain or nonempty-GEOS certificate.

After merging master becafb9a7, frozen8a6a521aa repeats the122source controls,
actual installed module reader and source plant reader. The installed reader's
51actually imported reference modules come exclusively from the verified target
and match current source bytes. Both source and installed plant readers pass on
8a6a521aa. No broad native gate is inferred from these Python checks.

All four actual1280×720images were opened; camera projection/view matrices match
within each pair and render receipts bind the actual PNG bytes. Whole overview:
4587changed pixels, worst55/255; moderate network view:6119pixels, worst53/255.
The added curved right-hand route is visible. Thin low-contrast materials,
incomplete network coverage and coarse distant terrain remain poor. The network
view is diagnostic; the whole overview is the progress view. No public gallery,
server, native frame/device gate or four complete Places is claimed.

## Equal-result costs and remaining work

Preloaded local and whole construction yield the same result SHA256:
`95f29df1bacb13e0ce84c44a5cf04c8b54646ccbd88426ac182b0bb0b0d68f1d`.
Memray1.20.0 records Python allocator events and intercepted native allocations,
without native stacks; inputs/imports/hash/export are excluded from construction.

| Method | Profiled s | Positive allocations | Cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| Local |13.327450|22,632,529|2,990,901,151|5,028,073|148,676,608|
| Whole control |49.506566|115,960,926|12,962,674,168|47,832,512|207,515,648|

Measured on the shared WSL2i5-1135G7 host, concurrent native/agent work; measurement
processes normal priority, other own heavy work nice10. Render backend llvmpipe
LLVM19.1.7, not RTX/A18. These instrumented observations establish work/heap
costs, not uninstrumented speedups or device budgets. Positive allocations are
not copied bytes. RSS includes loaded inputs; tracked peaks omit unobserved
OS/driver/GPU costs. Explicit terrain raw XY68400B, unique XY15152B and
raw-to-unique indices34200B are additional partial copy/work counters.

The local path is still far too expensive: Shapely accounts for18,336,599events/
2,469,143,141B; union alone6,006,430events/792,154,498B. This identifies a next
attribution target, not permission to remove correctness checks or increase
budgets. Finer interface protection enables one whole road; it does not solve
worldwide routing, all profile/level families, statics or bounded asset loading.

## Reproduction and retained evidence

Use Python3.13, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1, Clarabel0.11.1;
no new required package. Set `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, one
BLAS/OMP thread and the reference/experiment PYTHONPATH. Prepare the same53-source
resident as [#660's profile comparison](stair_resident_profiles_comparison.md).
Run `stair_local_generate` then `stair_local_verify` with that `--resident`,
the detour `--fixture` and a fresh explicit `/tmp/outshine-* --output`.
`stair_local_reference` additionally receives the verified whole53-source
`--baseline`, accepted `--local` and fresh `--output`; `stair_local_display`
receives the same resident/local plus `--reference` to that whole control.
Render its four scene JSONs with `render_transport_research`; compare with
`test/scripts/pixels.py`. Use `stair_local_cost --mode local|whole --profile FILE`
with the same resident/baseline/fixture and fresh explicit outputs.

The122-control cohort is the103controls in the [cached-grid comparison](stair_cached_grid_comparison.md)
plus `stair_resident_profile_checks`, `stair_axis_detour_checks` and
`stair_pier_ground_checks`. Installed runs replace `lib/src` with the verified
installed target. Actual spatial export/readers use `stair_occupancy_experiment`
and `stair_occupancy_verify`, followed by the existing Buildings/Plants consumers.
Private inputs, current exports, raw allocation traces/statistics, original
negative cases, image/camera receipts and compact evidence hashes remain under
`/tmp/outshine-transport-pier-ground-38adbd3c4/`; the earlier failed adapters and
old-foot counterexamples remain under `/tmp/outshine-transport-detour-9f4379059/`.
No private outputs belong in the repository or build/experiments.
