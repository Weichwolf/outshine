# Independent ordinary-ground source seeds

Owner: [#561](https://github.com/Weichwolf/outshine/issues/561), parent
[#434](https://github.com/Weichwolf/outshine/issues/434).
The [resident attached generator](stair_growth_comparison.md) previously needed
an actual old interior/end profile for every addition. This separate seed family
admits whole original components outside that core, without an invented old
profile, old surface borrowing or a connection inferred from proximity.

## Actual 57→65-source world

Frozen successful producer, fresh independent reader and full control:
`81a8f7dd831c962e40c0cc7516f184753ed84415`. The second automatic proposal
succeeds after one joint eight-source recipe fails its unchanged height limits.
Eight whole original minor-road parts add **1343.133863 m**:

| Original part suffix | Whole length, m |
|---|---:|
| 103612620:18 | 320.632968 |
| 1578623772:0 | 178.390860 |
| 305284142:0 | 121.432179 |
| 1578623672:0 | 175.315427 |
| 92807082:0 | 145.860829 |
| 12998091542:0 | 142.822487 |
| 1580280942:0 | 141.414802 |
| 103612620:2 | 117.264311 |

All IDs have prefix `openfreemap:14/8937/5678:transportation:`. The same Vienna
PBF/DEM/origin as #547/#555 is used; source PBF SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
Original axes, properties, priors, one-way values and all 1657 line parts / 71
areas remain; 1592 line parts are unplanned. These eight roads use the existing
explicit procedural 5.5 m carriageway profile. No exact OSM lane reconstruction,
new G2 vehicle trajectories, complete statics or native admission is claimed.
Native admitted-source IDs remain empty.

Six actual original shared nodes / ten whole pad uses are checked. Surface:
215 vertices / 333 faces; closed body 430 vertices / 856 faces. Double/Float32
volume is 2210.147015 / 2210.139870 m³, maximum normalized profile grade
0.489668 / 0.489700. Whole internal/through profiles and complete original outer
cross-section coverage pass both precisions, with cyclic connected vertex links.

997 old terrain facets become 2171 facets / 1164 vertices, with 161 exact
boundary vertices/edges and 791 pins. The combined field has 14937 vertices /
29516 facets. All module/layout/field dtypes, shapes and bytes and all resulting
world XYZ facets/ground attributes match an independent full construction
exactly; 27345 outside old triangles are byte-identical. Diagnostic closed
terrain matches that independent complete body.

All 333 whole new body-bottom facets pass source-bed D/Q checks (28692 pair
tests, maximum Float32 contact error 14.362219 µm). No old ground-body facet or
bridge underside overlaps this changed region; those local checks are explicitly
inapplicable. Old geometry outside the exact patch boundary remains unchanged;
the full control retains the previous complete bridges/roads, and all 124 whole
piers pass (7150 pairs, maximum 15.170145 µm). This adds no new underpass.
Soil grade D/Q is 0.317547 / 0.317544; no constructed-soil facets are needed.

## Source model, finite precision and actual topology

SIGGRAPH starting points: Chen et al. 2008,
[page 2 / figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
inspected visually, separates graph proposals from geometric generation.
Deering 1995, [page 3 / section 4](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf)
inspected visually, motivates explicit bounded local position precision. No
tensor-field generation, compression implementation or vendor code is used.

The initial full-precision seed proposals correctly fail Float32 vertex merges
or footprint preservation. The chosen new-seed model declares a **2^-11 m**
binary XY grid with `|XY| < 8190 m`. Outward construction plus noding uses
[Shapely 2.2.0 / GEOS valid-output precision reduction](https://shapely.readthedocs.io/en/stable/reference/shapely.set_precision.html).
Collapsed geometry and disconnected/changed source topology remain failures.
The grid is a different procedural footprint model, not an increased old-test
epsilon. Existing attached profiles keep their previous exact-footprint contract.

For each actual owned source region, the independent reader requires:

- Every point of the original complete source corridor/pads is covered.
- Every generated point lies inside its original region buffered by at most
  **6 grid cells = 2.9296875 mm**; an inscribed circular buffer makes this a
  conservative set-inclusion check. Discrete Hausdorff diagnostics alone are
  not the bound proof (observed maxima here 1.348…1.675 mm).
- Polygon component/hole topology is unchanged. The actual whole union also
  passes these conditions, and unrelated band overlaps outside declared original
  junction regions are refused.
- All actual new XY vertices are exactly representable in Float32. Whole original
  cross profiles, actual D/Q body topology/grade/frame/volume and original source
  membership still pass separately. Old assets/boundaries are never snapped.

The same outward grid-aligned rectangle defines each new junction's fitted
level region and saved-reader audit. Using a raw box for one and a rounded
boundary for the other was rejected by the actual whole-region height check.
The chosen policy is the default for `--family seed`; `--seed-grid-m 0` keeps
the strict full-precision alternative reproducible. A microscopic buffer on a
fixed-precision geometry can collapse; private failed prototypes were rejected,
and fixed-precision metadata is removed before ordinary metric operations.

## Selection, bounded work and counterexamples

The current explicit ordinary-ground profile/level classifier is unchanged.
Seeds additionally require unique original nodes and separation from all old
ground bodies; near old ground requires an attached/contact recipe. Conservative
prefilter queries use existing per-asset trees with the actual XY rounding guard;
65536 queries / one million candidate facets bound this work. There is no global
road geometry assembly or global source mask in normal selection/generation.
Actual selection performs 3074 old-asset queries / 615 candidate facet visits.
At most eight proposed roots, eight new sources per module and three physical
attempts remain the limits;
64 candidate source seeds are examined through the existing frontier operator.
Untried/rejected original sources remain catalogued with explicit reasons.

The accepted local context contains 1647 faces; two of eight old road assets,
ten temporary old-source bits / 1300 body faces / 46416 B working road arrays.
Empty local road windows are explicitly legal only for this family; missing
actual ground bodies cannot be hidden. Height/terrain/source/profile/clearance/
pier budgets and rejection semantics remain. Independent replay really loads
65 global sources with asset-local owners; subsequent one-face budget failure
preserves all 65 and geometry hash
`a6d8fbcd13b7d7bd56840953b4d4b3193e90e1967d7679a20f8cfab0af0ff6cf`.
This is not a global module scheduler, cache, paging or LOD implementation.

154 source / 154 installed-library tests pass. The unchanged 205-module wheel
from #547 remains byte-equal to source. Controls reject erosion, excessive
outward growth, source-hole loss, nonexact Float32 grids, fake old roots,
original-node/geometry disagreement, old-ground overlap and insufficient actual
overhead headroom. Real frame/payload/stale-face/old-receipt and retained-65
counterexamples pass. Fresh protected-input clones reverify the old 57-source
and new 65-source products on `1032eeae0c579b10f0df31155b7da3cc6410c610`;
original accepted receipts and referenced inputs remain unchanged.

## Equal-input selection costs and actual images

Same four-logical-CPU i5-1135G7 VM, Python 3.13.5, NumPy 2.2.4 and Shapely 2.2.0
as the preceding comparisons; one BLAS/OMP thread. Generation: 7.787033 s /
134344704 B process peak; automatic trial plus refusal/success readers:
22.893537 s / 154923008 B. Fresh automatic reader: 13.770533 s / 152453120 B.
Full independent control: 27.447525 s / 183197696 B, overlapping that reader.
These are different operations, not equal-cost performance estimates. Automatic
orchestration caps are 300 CPU seconds / 3 GiB address space; full control and
the selection benchmark use 120 / 3 GiB. No native/frame/device proof.

Frozen versioned selection benchmark:
`1032eeae0c579b10f0df31155b7da3cc6410c610`. Inputs/verification are preloaded,
outside core timers/trackers; no own render/test worker overlaps its core runs.
Both methods and both profiled runs have identical roots/recipes/reasons and
unchanged resident geometry. Recipe digest:
`91d16f54dfa4a6f5777867d521b6322a944874ffb0ae2205fbf6ef21610fe7fe`.

| Selection | Index | Complete scan |
|---|---:|---:|
| Normal seconds | 0.165002 | 0.205600 |
| Profiled seconds | 0.482664 | 0.591501 |
| Node lookups | 177 | 177 |
| Incidence/node comparisons | 254 | 199125 |
| Allocation events | 847135 | 1164474 |
| Requested allocation bytes | 78193398 | 91452176 |
| Peak tracked heap, B | 1432498 | 1298418 |

Memray 1.20.0 tracks native/Python allocation events with Python allocators on
and native call stacks off. Official statistics return zero with fresh complete
JSON/end metadata. Requested allocation traffic is not copied bytes/RSS;
tracked heap excludes preload. Process peak including preload: 130961408 B.
Counters describe different work, not equivalent instructions; the scan has the
smaller traced peak. Context visits 1327 / 1735 candidate faces initially/with
auxiliary ground. Terrain work reports 534 weld pairs, 42 conformed edges / 415
added triangles, 84288 raw-XY / 18624 unique-XY / 42144 remap bytes, plus explicit
indexed/difference copies and sparse-solver data. Other GEOS/solver/Python copied
bytes are unmeasured; no complete app/GPU budget is inferred.

Four fresh complete **1280×720** 3D PNGs were hash-bound to actual meshes/scenes,
fully decoded and opened. The new street block is visibly present in both whole
tile views: 6863 / 8933 changed RGB pixels, versus 198 / 241 in #555. Roads now
extend beyond the old core; the tile remains sparse and pale and is far from
complete coverage. Software backend llvmpipe LLVM 19.1.7; render times
12.672746 / 11.847971 / 13.685765 / 15.729027 s, partly overlapping nice-priority
tests/readers. No public images/gallery/server or native Place quality claim.

## Reproduction and retained products

Use the same prepared 53-source resident input, then ordered verified 55- and
57-source local parents from #547/#555. Run `stair_growth_experiment.py` and
`stair_growth_verify.py` with matching explicit `--resident`, two
`--prior-module`, `--family seed --seed-grid-m 0.00048828125` and fresh
`--output /tmp/outshine-*` paths. The accepted child is `outshine-module-1/`.
The existing local replay is unchanged. `stair_local_reference.py` uses the
complete 57-source diagnostic baseline and that child's recipe/local result;
its old global 64-source temporary-mask / 16384/32768 limits remain diagnostic
only. This comparison starts with 57 old sources and adds eight separately;
it does not prove a whole-reference importer above 64 old sources.

`stair_growth_compare.py --family seed --seed-grid-m 0.00048828125 --profile`
with the same resident/parents and a fresh explicit output reproduces the
selection comparison (optional Memray installation). `stair_local_display.py`
uses the full reference plus explicit preceding 57-source display; its four
scene JSONs use the existing research renderer and `test/scripts/pixels.py`.

Accepted products/images: `/tmp/outshine-transport-seed-junction-e2479caba/`.
Full control: `/tmp/outshine-transport-seed-reference-81a8f7dd8/`. Current original
allocation traces/statistics: `/tmp/outshine-transport-seed-cost-frozen/`.
Commands/logs/controls/hash/image receipts: `/tmp/outshine-transport-seed-e2479caba/`.
Needed original source/DEM/parent inputs are preserved; duplicate failed outputs
are compacted. Module report SHA256:
`b773ea3f614a3294c8b202faf1b5d807b44a690271737af39f815ca4dafe3f89`;
full control:
`78a736cd46b00e6245f1d025744ae1142f49297d38d7220debff5e4f69ab84d4`.

No lib/Plan/Reservation/Provider/native ABI changes or SDK/foreign runtime code.
Worldwide source/level recipes, connected regional routes/ports/frames,
persistent growth, reservations/LOD/streaming, vehicle dynamics/statics and native
integration/device acceptance remain #434/#50/#10/#12/#48.
