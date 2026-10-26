# Explicit pedestrian surface contacts

Owners: [#602](https://github.com/Weichwolf/outshine/issues/602),
[#600](https://github.com/Weichwolf/outshine/issues/600),
[#434](https://github.com/Weichwolf/outshine/issues/434).
Frozen implementation: `e1ac99cf22b5c7354a3576893e038133b422f2d1`;
initial construction commit `418242d0d`. Python research only.

The valid Vienna chain grows from72 to76 whole original MVT line parts.
The four-source recipe is now constructed and independently read without private
monkeypatches. Its module, local layout/field, selection and complete world
layout/field/body arrays are byte-identical to the previously rendered physical
hypothesis. That hypothesis alone did not admit contacts. This delivery adds the
explicit recipe, immutable owner binding, pedestrian assumptions and D/Q portals.
The mixed six-source family, worldwide coverage and native integration remain open.

## Construction and provenance

`fixtures/stair-four-surface-contact-vienna.json` selects whole sources
`1950369630:9`, `80922970:248`, `80922970:247`, `80922970:244` under prefix
`openfreemap:14/8937/5678:transportation:`. Root is the actual old endpoint of
`1580280942:0`; new1950369630:9 also meets cached80922970:62 at their actual
original node. Original PBF/DEM, IDs, attributes, axes, MVT node graph, old bodies
and coordinate frame remain unchanged. MVT nodes are not raw OSM node identities.
The fixed PBF SHA256 is
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`;
DEM SHA256 `67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.

Opt-in `shared_box` retains incident profile hulls and adds explicit squares at
original shared nodes. New-node reach is half the largest new width plus0.75m;
cached-node reach also includes each entire actual old profile. Original shared
nodes/membership, mandatory boxes and exact full support masks are rederived by
the saved reader. Analytic cuts stop at whole original cross-sections. Default
`level_box`, seed precision, solver limits and existing tolerances remain unchanged.
The angle between a new profile and an old profile can require new common-pad
surface. The new recipe explicitly permits that complete constructed surface as
reference; it does not claim that the old cap alone covers the new profile.

Three explicit `surface_contacts` borrow only the actual named old owner cap
inside its intersection with the complete new corridor:

| New → old source suffix | Interpretation | Reused area m² | Full portal m |
| --- | --- | ---: | ---: |
| 1950369630:9 →1950369630:8 | footway overlap | 1.915864798 | 1.800000000 |
| 80922970:247 →1580280942:0 | pedestrian minor-road crossing | 9.903916642 | 1.800035356 |
| 80922970:244 →12998091542:0 | pedestrian minor-road crossing | 9.904164397 | 1.800146460 |

Bindings record verified old asset SHA256, asset-local owner column, actual top
facet indices and XYZ SHA256. Whole selected caps must be ground, exactly0.3m
deep and satisfy the new footway gradient in Double and Float32. Masks are
recomputed, not supplied authorizations for a whole unrelated asset. Interfaces,
source coverage, full profiles, body walls/topology, terrain and headroom retain
their existing independent checks.

The minor sources supply `oneway=1`, not a lane count. Their existing5.5m/two-lane
profiles are procedural. Contacts explicitly select `walk` and
`procedural_walk_default`; this is an assumed pedestrian interpretation of absent
tags, not an observed connection or legal permission. Existing access/foot/direction
tags are compiled by `transport_travel.source_travel`; restricted, unknown,
conditional and one-way pedestrian access is refused. Specific `foot=yes` may
override general `access=no` under that existing parser. Motor/cycle source
directions are retained; no motor/cycle contact arc is introduced. These are
validated contact relations, not a worldwide route graph or traffic simulation.
No cycle-contact recipe, signals, turn restrictions or pedestrian swept-body
certificate is claimed. Shared Reservation/Habitat and native contracts are unchanged.

Portal proposals come from old boundary segments intersecting the new owner's
actual surface. Simplification/buffering only propose lines. Full piecewise-plane
traces must agree over the whole width using **different old/new facets**, with
positive area on opposite sides in both D/Q. Original coordinates remain available
to pay actual Float32 rounding in the existing trace bounds. Reusing the same old
cap in both groups cannot establish this connection. Three actual portals use
174/116/158 facet intersections; their six height comparisons use776 piece tests.
Maximum D/Q height disagreement is3.73e-9/7.55e-6m, within existing derived guards.

Limits: four unique declared pairs,32768 total selected old facets; each portal
has at most4096 boundary segments,128 candidates, eight candidate attempts with
two side orientations,65536 D/Q facet intersections, and existing4096 trace pieces/
65536 interval tests per comparison. Normal local limits16 assets/64 temporary
old-source bits/32768 old body facets, eight new sources and8s/100 solver iterations
remain. Exhaustion refuses this bounded trial and preserves the valid prior.

## Actual verification and images

89 source and89 installed controls pass, including11 new controls: access
restriction/override/direction, conditional access, wrong immutable owner geometry,
duplicate/excess declarations, wrong family, bridge/steep/depth incompatibility,
disjoint masks, incomplete/height-separated/narrow/same-side portals, a positive
Double side that collapses in Float32, missing/unbound boxes and altered masks.
The unchanged205-module wheel is byte-compared against current lib sources:
SHA256 `404b355b17389cf8dd92ae94e028e9de7866ceea1ee2c1864d43a1a8a3736934`.
The installed reader loads51 reference modules exclusively from that target.
Its source-contract/module/physics/pier proofs equal the source reader exactly.

Fresh complete exports and reader at the frozen commit:109 new surface vertices,
123 faces,424 closed body faces,105.585782/105.584962m³ D/Q volume; normalized
maximum grade0.687593/0.686903. Five original shared nodes,16 validated pad uses,
six internal end profiles, two old profiles and two new interior profiles pass.
Local terrain replaces477→833 faces;29901 outside faces remain exact. Independent
whole control matches all30734 XYZ facets and attributes, plus module/local arrays.
All old bridge/body checks and124 whole pier contacts pass. Source catalog keeps
1657 original line parts/71 areas:76 generated candidates,1581 unused lines and
all areas retain original inputs and reasons; native `admitted_source_ids=[]`.

The four existing whole1280×720 EEVEE images were reopened. Their exact current
display inputs, scene settings, helper hashes, image hashes and render receipts
were rechecked; the images are **reused renders**, not a newly measured render run.
Fresh display hashes are before
`7b6496492f063b7d056947e28b589ef7b89bd9a2ee3abdf5c8068c0068f9da31`,
after `0c2aa704c459812964d463724d722f45db1201a5a114e1b28acd1500e1b1c9f4`;
all old road display arrays remain exact. Changed pixels:655 overview/869 network.
The small upper-left extension is visible; overall coverage remains sparse and
the pale terrain/transport materials need improvement. llvmpipe/LLVM19.1.7 is
software rendering, not RTX3060/A18 evidence. No images, gallery or server published.

## Costs and reproduction

Equal whole-source inputs and identical result hash
`31a60e9c61687d5b9f262f14e69c4d6ea255505c70adb643f7ae613f25490687`:
local construction6.868838s/RSS130551808B, whole control24.840844s/RSS193335296B
after loading original sources/resident assets. First normal local run overlapped
a nice10 installed-reader process; whole ran afterward. Host: WSL2/i5-1135G7,
4vCPU/16GiB, one BLAS/OMP thread. Generator+export15.015448s/RSS134053888B,
source reader19.232741s/RSS154415104B, whole+export49.923060s/RSS193220608B;
those gate runs overlapped the affected tests. No native/frame/load-time claim.
Memray1.20.0 tracks Python/native allocations after input preload, excluding
imports/decode/hash/export and baseline allocations; native stacks disabled.

| Same76-source result | Local | Whole control |
| --- | ---: | ---: |
| Profiled construction s | 14.455460 | 87.803714 |
| Positive allocation events | 24860505 | 141567285 |
| Cumulative requested bytes | 3384631056 | 16041440902 |
| Tracked incremental heap peak B | 3097170 | 52272951 |

These are allocation requests, not simultaneous storage or copied-byte totals.
Process RSS and this incremental tracked peak measure different scopes. Explicit
work includes three old assets/eleven sources/1004 body faces,44952B assembled
road arrays;722→453 welded points/269 candidate pairs and48 split edges/125 added
terrain triangles. Recorded terrain indexed copies11552B plus difference
buffers11552B;27207B changed old masks and0B retained mesh copies in patch
composition. Internal GEOS/SciPy/NumPy copy totals remain unmeasured. The diagnostic
full-world path repeats more work; this is no runtime/native optimality proof.

Use Python3.13, NumPy2.2.4, Shapely2.2.0/GEOS3.14.1, SciPy1.18.1 and Clarabel0.11.1;
no new package, SDK runtime dependency or copied vendor code. Set `TMPDIR=/tmp`,
`PYTHONDONTWRITEBYTECODE=1`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/transport` (or the verified installed target).
Run `python -m unittest stair_surface_contact_checks` plus the78 controls listed in
[the metric comparison](stair_metric_line_comparison.md). The new controls need no
private world. For the actual chain, use
[the72-source reproduction](stair_narrow_profile_comparison.md), append its verified
72nd module to `--prior-module`, choose the new fixture, then run
`stair_local_generate`, `stair_local_verify`, `stair_local_reference` and
`stair_local_display`. Reference baseline is the independent72-source world.
Use fresh output directories; installed readers run in disposable hardlinked
clones excluding verification.json/display, preserving all original receipts.

Private evidence only: `/tmp/outshine-transport-contacts-5a58ef526/`.
Local report SHA256 `24b6d7474aecc9ac14b953ed6af3a2cbcdbf1e151744ee650a654210cf3bd108`;
source receipt `333086a8d46e77b56eacb2938850f6a020c7d49423eccb1469a41b91ff264b1c`;
whole report `a05f368945e03dc397935788257183f477d94a333acba430cd74e1592a97ce2a`;
module NPZ `1f01d7e36a7b5a745f8805e164584c045e66b860853f2c35f9d93644290dce5c`.
Protected72 receipt remains
`47da9a798d41b3616d4814c656bdcd7a608809e6708af7ce8029f16d50544e98`.
`evidence.json` binds all seven arrays, installed proof and four image receipts.
Existing rejected global-frame and precision alternatives remain linked in
[the preceding comparison](stair_metric_line_comparison.md); global rebase is
not a remedy for unchanged old bridge geometry. This adds one explicit recipe,
not a general numerical-error proof or a complete OSM road generator.

SIGGRAPH starting point: Deering1995,
[page3/section4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
visually reread. Keep old quantization/frame stable and verify actual converted
geometry. Existing full metric queries/plane traces from PR #621 are reused;
boundary proposals alone provide no connection proof. No SDK oracle or new
decompilation was needed for this contact policy.
