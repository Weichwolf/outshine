# Two automatic whole path components on the current network

Owners: [#691](https://github.com/Weichwolf/outshine/issues/691) and the separate
complete-control capacity correction [#693](https://github.com/Weichwolf/outshine/issues/693).
The existing bounded Python generator adds five whole original parts, totalling
824.504472 m, to the [complete 79-source state](stair_resident_snapshot_comparison.md).
Two connected components remain separate from the old ground network; proximity
does not invent a connection. This produces a verified finite **84-source**
specimen, not a complete tile/world generator or a legal routing graph.

## Source, selection and geometric result

The existing automatic discovery, seed geometry, whole-profile and local terrain
methods are reused. Their sources and assumptions remain in the
[growth](stair_growth_comparison.md) and [seed](stair_seed_comparison.md) studies.
SIGGRAPH starting points are Chen et al. 2008, [page 2 / figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
and Deering 1995, [page 3 / section 4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
both inspected visually again. Graph proposals, explicit position precision and
geometric admission are separate. No tensor-field implementation, vendor code,
SDK dependency or new runtime/library interface is introduced.

Original input: `test/outshine/data/transport/wien-junction-14-8937-5678.pbf`,
SHA256 `97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
DEM remains the retained source with SHA256
`67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.
The unchanged root manifest SHA256 is
`4e0c339d98df06e77cf1aecbf3f5c0386819dba3c188cbd0d325419ba2a9a917`.
All source IDs below have prefix `openfreemap:14/8937/5678:transportation:`.

| Whole original part | Length m | Profile width m | Component |
|---|---:|---:|---|
| 266436400:1 |316.801077|1.8|Three parts|
| 90731760:9 |57.191960|1.8|Three parts|
| 266436540:1 |17.438950|2.0|Three parts|
| 80922970:48 |352.889969|1.8|Two parts|
| 80922970:30 |80.182515|1.8|Two parts|

The three-part fixture is [versioned here](fixtures/stair-seed-three-paths-vienna.json),
the two-part fixture [here](fixtures/stair-seed-loop-paths-vienna.json).
The first two footways retain `access=no`, and the first retains
`bicycle=dismount`; those physical paths grant no new public route. The cycleway
retains its designated foot/bicycle attributes. Every original axis, property,
source ID and original generalized MVT-node occurrence remains available.
MVT-node connectivity is not proof of exact OSM nodes or legal permissions.

On the original 79-source state, indexed discovery and a complete source scan
produce identical roots, recipes and source reasons for both attached and seed
families. The attached family has 40 roots / 12 recipes; seed discovery considers
64 roots / 16 recipes. The two bounded attached windows fail. Seed windows at
offsets 0 and 3 each contain a valid component; other joint attempts fail height,
profile, interface, undeclared reuse or whole-gradient checks. A failed joint
recipe is not a proof that each constituent source is impossible.

The three-part component is first composed into a checked state of 82 sources. Fresh automatic selection on that updated state then chooses the two-part
component in its first three-attempt window, after two refusals. All seven
products match the independently generated final module with 84 sources exactly. Saved
automatic selection/admission is checked in fresh source and installed readers.
This is bounded continuation through existing operators, not a new persistent
world scheduler or a maximality claim.

The two-part component's six geometry/planning artifacts are also byte-identical
when built independently on 79 sources or after the three-part component.
Its genuine original corridor hole is retained. Each owned source and the union
retain polygon/hole topology, contain their entire original corridor and stay
within the existing outward bound of 6 binary grid cells = 2.9296875 mm.
Only new seed footprints use the existing 2^-11 m model within its ±8190 m local
coordinate domain; old geometry is not snapped.

The new modules contain 76/101 surface vertices, 110/146 top facets and 300/404
closed-body facets. Their Double volumes are 211.978484/233.275996 m³; Float32
volumes are 211.979207/233.277459 m³. Whole endpoint/through profiles and three
new level-node regions pass both precisions. The three-part module has three
internal endpoint profiles, three outer profiles and one through-profile check;
the two-part module has one internal, three outer and one through-profile check.
No new bridge, tunnel, old-ground junction or motor-vehicle arc is claimed.

Local terrain replacements are 148→434 facets / 243 vertices and 399→885 facets /
501 vertices. Their exact boundary counts are 50/115, with 197/396 finite pins.
Independent complete construction matches every new module/layout/field array
and all resulting XYZ facets/attributes. The final field has **16,495 vertices /
32,632 facets**; 31,313 facets of the old 79-source state outside the two changes remain exact.
No constructed-soil facets are added; maximum soil grades D/Q are
0.363710/0.363709 and 0.274528/0.274537, below the unchanged 2/3 limit.
All 124 old whole piers remain valid; maximum Float32 contact error is
15.170145 µm. Local overhead-bridge tests are explicitly inapplicable where
there is no positive-area overlap; old global bridges are checked by the full control.

Local and complete construction share the new corridor/height builders. Their
comparison checks whole-state assembly and localization. Separate saved readers
audit original sources, profiles, topology and physical constraints without
re-solving; matching builders alone are not an independent algorithmic oracle or
a general exact-topology certificate.

## Complete control is distinct from the local working window

The local generator for 84 sources and installed cold reader pass before the old complete
diagnostic encounters its own capacities. This exposed three separate limits:
16 assembled road assets, 16,384 field vertices and 65,536 closed-body facets.
The complete control needs 17 prior assets, 16,495 resulting field vertices and
65,976 terrain-body facets. No local geometry/solver tolerance is relaxed.

Existing defaults remain: 16 assets, 16,384 assembled vertices and 65,536 body
facets. The complete reference explicitly opts into at most 1,024 resident assets
and 32,768 field vertices. Total **road-body** facets and **field** facets each
remain capped at 32,768. Road geometry to be copied is additionally limited to
8 MiB before assembly; this is not the total Python/GEOS/process heap budget.

A 129×129 planar grid already needs 16,641 points for 32,768 field triangles.
Its closed slab has 33,282 vertices and 66,560 facets. The complete control first
requires every top use of an XY vertex to equal its one field height. Its slab
therefore has one top and one bottom per point, with quad side walls. The complete
audit explicitly allows at most 8×32,768 =
262,144 facets: two cap facets plus at most two wall triangles for each of at
most three edge references per field triangle. The body vertex cap remains
65,536. Every closed-volume, orientation, zero-area, cyclic-link and D/Q check
still runs on every face. The actual final body has 32,990 vertices / 65,976
facets, no zero-area faces and a positive volume in both precisions.

Tests retain the old default rejections, verify the planar-grid counterexample,
more than 64 complete assets/sources without a global bitmask, unchanged local
16-asset rejection, duplicate/invalid capacity and body-face limits. Oversized
logical geometry bytes are rejected before `vstack` copies. These are explicit
diagnostic requirements; native, device, frame, local-window and solver budgets
are unchanged. The complete reference remains an expensive offline control.

## Costs and actual consumers

Same-input local and whole methods return identical module/local-field hashes:
`abc70e0a92606403792c10f8adec3022ff29f593e81118c47aca86c3123cdc88`
for the three-part step and
`bc1282aab0fdadf2b68aa7a4cdc3a6c7c2b5ea7f7c8b088e9384b7183434ac7a`
for the following two-part step.

| Construction after preload | Profiled s | Positive allocations | Requested cumulative B | Tracked peak B | Process RSS B |
|---|---:|---:|---:|---:|---:|
| Three parts, local |5.985035|9484381|1211952761|1283354|141721600|
| Three parts, complete control |86.921060|156138731|17564832708|54771282|222474240|
| Two parts after 82, local |12.337067|20006729|2647674609|2745713|150327296|
| Two parts after 82, complete control |76.581196|161895465|18322453491|56104912|224358400|

Preload, imports, result hashing and export are excluded from these tracked phases;
complete controls additionally assemble/check the unchanged full field. Local
work preserves that outside geometry. The remaining local cumulative requests
of 1.21/2.65 GB are still too high for a general runtime solution; comparing with
an expensive diagnostic is not a device-budget approval or an optimality proof.
Requested allocation bytes are not copied bytes or resident RAM. Explicit array
work counters remain in each report; full memory-bandwidth traffic is unmeasured.

Three-part measurement commit: `0809a2c9acd007f3520cf5c62dc770df01b0dc31`.
Two-part local measurement: `a58a5bac96bb84766ed6cbbc4ecc8e9119537d92`;
its unchanged local construction is compared with the corrected complete-control
path at `e6dacab62fea8b2966e620445d7a27427c834ea3`. Diagnostic limits and audits
change, not the local generator or output geometry. All successful raw traces,
hashes and terminal statuses are retained; failed capacity attempts are separate.
Measurement processes run sequentially at normal priority; other own checks and
shared-host work may overlap. Retained measurement load averages span 11.7–17.0.

The actual shared handoff has **6,904 volumes**, including all 6,648 prior records
exactly by owner/source/geometry/planes, plus 256 new physical facets. The old
prefix-reuse adapter intentionally accepts physical-only products; the logical
headroom suffix in the input of 79 sources rejects that path. This handoff is therefore
rebuilt in full, with its time/work retained, not reported as a prepared hit.
All 124 supports and 128 probes pass. Actual Buildings produces 65 bodies; the
old before/after body buffers and reservations remain byte-identical. All four
complete plants are rechecked and retained without new placement proposals.
Source/SITE/pose/form/normal fields remain equal; current diagnostic global
terrain ordinals and measured query seconds change. Those ordinals are not
persistent source/facet identities.

At the geometry/control freeze, 106 source and 106 installed controls pass with
116 matching reference imports each. Both module generations reproduce all
seven artifacts through the installed package. Actual source/installed readers
verify the final full state, 408 queries per precision and index/scan parity;
the complete-state SHA256 is
`5606d12d9d315f31a1ea27f7a92c0ffa0c7295a6908c2b40cdcf089ad27becb8`.
Their real consumer closures contain 116 matching reference modules. Library
files are unchanged; the existing wheel SHA256 remains
`db931057f4d6073cb89d83a1211c69d011c2ac85232d79ae6d5dac40120c6c05`.
After newer Buildings/Oak/native integration, targeted actual Buildings and
whole-plant readers pass again in both modes at
`c99b57f8dda541dd9234f489d9d81c37b2b92ae0`; transport/library code remains unchanged.
Ruff format/check, function lengths, `git diff --check` and `make lint-docs` pass.
These Python/contract checks do not close native engine gates.

Python 3.13.5, NumPy 2.2.4, Shapely 2.2.0 / GEOS 3.14.1, SciPy 1.18.1,
Clarabel 0.11.1 and Memray 1.20.0 are used for geometry costs. Actual consumers
and broad controls use the existing NumPy 2.5.4 / manifold3d 3.5.4 environment.
No new package installation is required. WSL2/i5-1135G7 exposes four logical
CPUs and 16,190,028 KiB RAM; this is not A18/RTX, native frame or GPU evidence.

## Images, reproduction and remaining work

Exactly four current comparison PNGs stay private in
`/tmp/outshine-transport-growth-9fcc60f76/comparison84/`: before with 79 sources /
after with 84 sources, 1280×720, two whole-region angles. The second diagnostic camera now
mirrors the original overview around its target and uses 1.08× its span; the old
cropped camera could miss the added component completely. Every orthographic
input vertex must fit the image. Valid original before PNGs are reused only
after comparing every camera/palette/profile field; after images are fresh.
Blender 5.2.2 Workbench uses software GL/Xvfb, real hash-checked closed meshes,
depth, shading and shadows. These are 3D diagnostics, not native Place images.
The small new path lines are visible; sparse coverage, weak contrast and coarse
terrain remain. No public gallery/server or native visual-quality gain is claimed.

Use existing `stair_growth_experiment` / `stair_growth_verify` with the source
snapshot and preceding modules, seed family and at most three attempts. The
three-part component uses the explicit offset-3 window on 79; after adding it,
the ordinary first window on 82 selects the two-part fixture. Saved independent
readers use disposable clones without old receipts. `stair_local_generate`,
`stair_local_verify`, `stair_local_reference` and `stair_local_display` reproduce
the fixture checks; `stair_local_cost --mode local|whole --profile TRACE` measures
equal outputs. The existing occupancy, Buildings and Plants experiments provide
the real handoff. All outputs use fresh explicit `/tmp` paths.

Original source inputs, complete states, current images, raw successful traces,
negative recipes/reasons, import closures and compact evidence are retained under
`/tmp/outshine-transport-growth-9fcc60f76/`; predecessor inputs remain at the paths
in the linked snapshot study. All 1,657 original lines / 71 areas remain in the
catalogue; 1,573 lines and all areas are still unplanned. Worldwide selection,
regional graph/ports, more classes/levels, tunnels, LOD, spatial precision and
native integration remain open under #434/#50/#568/#48. The full goal stays active.
