# Local generation from resident terrain and independent road bodies

Owner: [#547](https://github.com/Weichwolf/outshine/issues/547), parent
[#434](https://github.com/Weichwolf/outshine/issues/434). This delivery connects
the [exact surface patches](stair_patch_comparison.md) and the
[original endpoint correction](stair_endpoint_comparison.md) to new generation.

Prepared input keeps each old closed road body with its own source-owner
columns. The root has 32 source columns, later assets at most eight. A bounded
temporary working set has at most 16 assets, 64 local old-source bits and 32768
body faces. These are local capacities, not a global source mask; distant
assets do not consume local bits. The four-parent legacy importer is unchanged.
Road selection includes the actual asset Float32 XY rounding error and a
conservative field rounding guard, including possible rounded boundary contact.

The local field initially contains whole active triangles within 40 m of the
new original axes. The generated original corridors must fit inside an exact
conservative 16 m axis buffer, leaving at least the existing 24 m soil margin.
Whole triangles intersecting any affected pier footprint complete the context;
five-metre auxiliary ground around that context preserves the constructive
soil eligibility. Explicit global prepared distance queries independently
confirm the final 24 m selection and whole-pier dependency closure. Local
terrain capacity is 8192 context / 2048 selected old faces; no partial query
silently substitutes missing neighbours. Local exhaustion retains the previous
valid world and original source catalogue with a joint trial reason.

New body construction and contact/height/clearance checks reuse the existing
profile/level_box family at cached original module interiors. Exact patch
edges/coverage preserve the old field and the prepared minimum-height datum.
Whole convex source-facet references check local ground contacts in both
precisions without assuming a global Lipschitz bound on a nonconvex local
region. Whole original frozen road undersides are checked directly as well;
old terrain samples do not replace those original body references.
All old whole piers remain checked. A saved hit replays independently
verified local deltas and road assets, with ordered report hashes, without
source decode, planning, CDT, solving or whole-field/body construction.
At most 32 explicitly ordered local research deltas are loaded per run; this
is not runtime storage, paging, eviction, LOD or a general world graph.

SIGGRAPH starting point: [Lindstrom et al. (1996)](../../../doc/references/transport/siggraph/1996-continuous-height-field-lod.pdf), page 4 / figure 3 inspected
visually again. Existing exact local geometry is retained; their hierarchy,
projected errors and simplification are not implemented. No SDK or foreign
code/runtime dependency is introduced. Source/frame/topology/semantics remain
explicit, shared Plan/Reservation/Provider/native APIs remain unchanged.

## Frozen products and physical acceptance

Preparation/library: `389df250482f9083fa1662d4d4a82e2efdcf3068`.
Generator and independent saved-product reader:
`e63b40cec46a750beb2b7b97cfd57a9b40844cad`.
Whole-field control, adverse cases and diagnostic 3D export:
`e1163b151fff704ad94d7b6df08141f8a9874576`.
Strict dtype/shape/byte comparison, costs and final source/installed controls:
`e8959d1defcf4c02a5ceba7829e5b44ca9b3a863`, including the independently merged
Buildings PR #548; no library or transport source changed in that integration.

The actual 53-source input grows to one combined **55-source** world. Two
whole original footways, `80922970:280` / 74.735021 m and `138663000:6` /
159.476881 m, add 234.211902 m. This combines the earlier prefix and endpoint
specimens; the endpoint comparison alone had only an alternative 53-source
world. Source PBF SHA256 remains
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`, the same
versioned Vienna tile/DEM inputs and origin as the linked comparisons.
All 1657 original line parts / 71 area features remain represented by their
original payload, IDs, attributes and disposition; 1602 line parts remain
unplanned. Native `admitted_source_ids` remains empty.

Initial distance selection returns 1041 active faces; whole-pier closure
touches four groups, auxiliary ground yields 1402 context faces. Only two of
six resident road assets enter the working set: 40 local old sources and
10216 old body faces. The module has 49 vertices / 66 surface facets and a
closed 98-vertex / 192-face body. Whole old root/end profiles, new through
profiles and four opposite wall segments pass Double/Float32. The two new
sources have no positive-area overhead-bridge overlap, so no new underpass
is claimed.

612 old terrain faces become 920 faces / 526 vertices, with 130 exact outer
edges/vertices, 466 finite pins, three protected parent facets and two affected
old piers. Whole control and local results have exactly the same module,
local-plan and local-field dtypes, shapes and bytes, including NaNs. All
28270 resulting XYZ facets and ground/constructed attributes match exactly;
27350 old outside triangles remain byte-identical. Diagnostic terrain closure
has 28628 vertices / 57252 faces and matches the independent complete body.

327 whole source-bed facets pass both precisions (31512 contact-pair tests,
maximum Float32 error 13.297422 µm). A further 176 whole original frozen body
underside facets pass directly (16190 pair tests, maximum 16.221998 µm).
All 124 whole piers pass (7150 pair tests, maximum 15.170145 µm).
Soil/constructive grades, five-metre structural reach, old bridge clearance,
road profiles, actual source hulls and all earlier limits remain enforced.
Local soil solve: 22 iterations, 245 reduced variables / 2033 rows / 1555 A
nonzeros; earlier sparse formulation and eight-second/100-iteration limits
remain. These are physical local products, not a complete statics or route
dynamics certificate.

138 source / 138 installed-library tests pass. All 205 installed library
modules match source; 203 previous modules are byte-identical, only the two
surface-patch query files change. Wheel SHA256:
`d6f3817a6e8b91b7fc869d650493aec1ecd9f3a2b33d1bc03e07f3dff327172b`.
The previous bounding-box API also replays the old 53-source prepared inputs:
222 sample positions/normals per precision match the flat reference exactly,
4300 indexed hits versus 11440562 scan face tests, plus all 124 whole piers.
This is different work, not equal-cost instructions.

## Saved hits and counterexamples

Fresh generator execution blocks the old global import/road merge/terrain
assembly/body export functions. Fresh saved replay blocks provider decoding,
planning, CDT/height solving and whole-field export: seven road assets / seven
patches / 55 source IDs load in 2.192518 s / 111456256 B peak RSS while the
separate diagnostic exporter starts alongside it. This is a finite research
load, not an engine/world-cache or device acceptance.

Real adverse controls reject wrong frame, damaged old road payload, a stale
face even with a fresh payload checksum, a changed report under an old reader
receipt, an omitted required neighbour and an original frozen underside
shifted by 1 mm. The held old snapshot remains unchanged. A new original
source trial after replaying the accepted 55-source product exhausts a
one-face context budget, records its original recipe/reason, passes the
retained-product reader and preserves the complete 55-source geometry hash.
There is no new partial mesh or blocking world replacement. Synthetic 70-asset
controls exercise global source counts above 64, local mask overflow and actual
Float32-only boundary contacts.

## Equal-result costs

Four-logical-CPU i5-1135G7 VM, Python 3.13.5, NumPy 2.2.4 and Shapely 2.2.0;
one BLAS/OMP thread. Generation, verification and full control use 120 CPU
seconds / 3 GiB address-space process caps. Source/installed tests briefly
overlap generation startup; full control overlaps adverse-reader work. The
separate construction comparisons run sequentially at normal priority with
no own render/build running. Inputs and prepared validation precede their
construction timer and allocation tracker; process RSS includes that preload.

| Run | Local | Whole diagnostic control |
|---|---:|---:|
| Source/resident load, s | 1.772881 | 1.830228 |
| Construction, s | 6.782943 | 19.869190 |
| Process peak RSS, B | 128086016 | 180035584 |
| Profiled construction, s | 15.216417 | 48.550857 |
| Profiled process peak RSS, B | 150966272 | 203104256 |
| Allocation events | 29543693 | 113664518 |
| Requested allocation bytes | 4059364781 | 12838116945 |
| Peak traced live heap, B | 5163958 | 47272256 |

All four runs have the same module/local-field result digest:
`bb79a3a9b82e5752eef88955be4fb792b8a6723c80f8e207a45d403a4561273c`.
Memray 1.20.0 records native/Python allocation events with Python allocators
enabled and native call stacks disabled. Official statistics CLI calls return
terminal zero and fresh fully decoded JSON with completed trace metadata.
Requested bytes are cumulative allocation traffic, not copied bytes, resident
memory or an app budget. Profiles are not timing estimates. The whole control
rechecks known old outside geometry and constructs its complete body; local
boundary/whole-contact proofs preserve that previously valid outside instead.
This measures that work removal for this case, not a universal speedup or a
globally optimal algorithm.

End-to-end generation is 8.798821 s / 132431872 B peak RSS; independent reader
9.351722 s / 149299200 B. The complete diagnostic control takes 24.255171 s /
180158464 B. The saved-hit and normal generation remain distinct operations.

Concrete array work: context setup visits 1293 indexed faces initially and
1729 for auxiliary ground; canonical extraction emits 1402 faces. The local
road working arrays own 454656 B; local module arrays 10653 B, terrain layout
40462 B, field 4675 B. Changed old masks contain 26698 B; copy plus immutable
byte-backing makes two passes, with two 920-B passes for the new mask. Old
retained `Mesh` copying is zero only for snapshot update, not loading, queries
or validation. Context raw triangle XYZ is 100944 B, owned extracted mesh
35112 B; terrain raw XY 38208 B / unique XY 8416 B / remap 19104 B. Terrain
welding examines 361 pairs / 898 inputs, conformance splits 14 edges and adds
128 triangles. The unchanged soil solver records 82484 B retained sparse data,
23028 B additional reduced sparse data, 17064 B explicit shift/delta/linear
arrays, 9896 B height/datum copies and 31560 B validation XYZ conversions.
The full control additionally reports 346512 B global labels and 3938400 B
outside-XYZ comparison arrays. These are scoped executed counters/array sizes,
not every GEOS/solver/Python copy; other copied bytes remain unmeasured.

## Actual images and retained products

Four fresh private 1280×720 3D PNGs were fully decoded, bound to actual
mesh/scene hashes and opened. Before/after overview changes 1373 RGB pixels;
network view 1932. The added links are faint; the tile remains sparse and pale,
with coverage, profiles and presentation still requiring substantial work.
No public images/gallery/server or four complete Places are delivered.
Actual backend: llvmpipe LLVM 19.1.7. Render times are 11.305121 / 11.344892 /
13.235925 / 13.458361 s; no RTX/A18, frame-rate or native integration gate.

Prepared road assets: `/tmp/outshine-transport-localgen-prepared-389df2504/`.
Accepted local products: `/tmp/outshine-transport-localgen-full-e63b40cec/`;
actual images are in its `display/`. Independent complete products:
`/tmp/outshine-transport-localgen-reference-e63b40cec/`. Commands, raw terminal
logs, wheel, official allocation statistics/traces, image completeness, pixel
comparisons and compact receipts: `/tmp/outshine-transport-localgen-64e9669a7/`.
Superseded unfrozen products and duplicate exports are removed; needed parent
inputs and earlier reference evidence remain.

- Resident manifest: `c897308d7276fa4e0412355bbeca187f99f670e683747d89a517aa504fa2467a`.
- Local comparison: `f76196daf44b98522120441d4f39995a192a550784947af4b9e316a3a902632e`.
- Independent reader: `34468d1fbe82f4918bee7ced81928514c95493869191ef99f0790b1224e4ec36`.
- Whole control: `2b6669695b6786a1066a37ea3254f10b9be2f5d814d01b3a2eed4cca86918fd9`.
- Adverse controls: `2e9063b7d025f4fc5b232aa0004e4b9057e5a26be804fc426cb95f0ae16fe30b`.

## Reproduction and limits

Use `stair_local_prepare.py` with the existing 53-source prepared terrain from
#542 and the same five ordered verified source modules. The first four parents
are in [the frontier comparison](stair_frontier_comparison.md), the fifth in
[the prefix comparison](stair_prefix_comparison.md). Pass explicit `--base`,
`--supports`, `--terrain`, five `--cached-module DIRECTORY RECIPE` pairs and a
fresh system-temp `outshine-* --output` directory. Preparation alone reads old
full products to bind original root/frame/source histories.

Then run these with identical explicit `--resident PREPARED --fixture
fixtures/stair-secondary-endpoint-vienna.json --output LOCAL` arguments:

```sh
python test/experiments/transport/stair_local_generate.py ...
python test/experiments/transport/stair_local_verify.py ...
python test/experiments/transport/stair_local_controls.py ...
```

`--prior-module VERIFIED_DIRECTORY` adds an ordered saved local result before
new generation. Replay uses `stair_local_replay.replay`; verify each generated
product first. `stair_local_reference.py` takes explicit `--baseline` (whole
53-source reference), `--resident`, `--fixture`, `--local`, `--output`. It is
deliberately independent diagnostic assembly, with its existing global 64-bit
and 16384/32768 capacities; those do not constrain the local generator's
global ledger. `stair_local_display.py` exports the verified local product
against that complete reference; use the existing research renderer on its
four scene JSONs and `test/scripts/pixels.py` on matching views.

`stair_local_cost.py --mode local|whole` takes the same source/resident/baseline
inputs and a fresh system-temp JSON `--output`; optional `--profile FILE`
requires the existing optional Memray installation. No new runtime dependency.
Run `stair_local_checks` plus the linked endpoint/patch controls, replacing
`lib/src` with the installed wheel for installed-library acceptance.

The current source adapter remains the saved Vienna ground family, at original
cached module interiors; other sources/levels/places need their recipes and
proofs. Complete automatic seed/module growth, global ports/routes, global
frames/domain expansion, paging/compaction, LOD/reservations, statics and
native/device integration remain #434/#50/#10/#12/#48. This delivery removes
the demonstrated four-parent construction blocker without redefining those
larger requirements as complete.
