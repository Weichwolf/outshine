# Selection before emission: verified first component

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), native cost/input
counterexample [#567](https://github.com/Weichwolf/outshine/issues/567).
The complete whole/cell/hierarchical comparison requested in #12 remains active.
This component provides shared source-envelope queries and an independently
checked enriched-source reader. It does not establish real-scene coverage gains,
primitive/batch reductions, scaling or native/device budgets yet.

## What the native input establishes

The independent Python schema-8 reader matches all 6,352 actual native source
records from the #800 experiment: source index/ID/kind/cell, nullable envelope
and nullable shell error. Original native decoder/input producer: 82e7a69d7,
diagnostic d8df700ef. The three original source sets and their native individual
Fine/Shell choices are unchanged when partitioned, while projection groups and
emitted triangle counts differ. The 16-source threshold alone is not an isolated
cause. The native receipts remain canonical in #567.

A read-only CRC/SHA-bound snapshot of all 147 local enriched indexes preserves
150,637 source records and their original coordinates, inner rings, heights,
frontages, optional attributes and selection summaries. There are 149,612 known
envelopes/errors (all observed errors 0.25 m), 148,943 projectable records and
1,025 unsupported/unknown records. Unknown values are retained, not replaced by
zero. No detail mesh is generated or used as a reduction input.

## Shared component contract

`outshine_ref.geometry.occlusion` and `occlusion_hierarchy` accept explicitly
known joint source envelopes and separately supplied opaque-patch certificates.
Source indices address the complete immutable caller record table. Cache-cell
IDs are deliberately absent from hierarchy construction and query decisions.

An occluder must be an explicitly certified opaque, convex, planar patch without
holes. Winding defines its front side; two-sided opacity needs a separate flag.
Neither projectability, a bounding box nor a surface-distance estimate proves
opacity. A patch must fit its known owner's envelope. Nonplanar/concave contours
are unsupported; callers must retain those sources and obtain an appropriate
certificate rather than silently flattening roofs or filling openings.

Each certificate owns an observer box and copied immutable coefficients. It
certifies all observer positions inside the box and every look direction, not
just one sampled camera. Leaving that region or changing the captured geometry/
opacity invalidates reuse. This component does not create the coarse fallback or
asynchronous handover needed by a complete moving-world consumer.

Unknown source bounds stay selected. Explicit protection flags retain sources
needed for shadows, reflections, interactions or spatial reservations; deriving
those demands remains the caller's responsibility. The geometric-query budget
returns a valid partial cull: unproved sources remain selected. Output flags keep
one slot for every original source. They can route subsequent emission to bounded
cache products; neither source IDs nor cache ownership is reassigned.

## Geometric and numerical reference

SIGGRAPH-first basis: Durand et al.,
[Conservative Visibility Preprocessing using Extended Projections, updated extended version](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/occluext.pdf).
Page 3 and its projection/occluder/depth diagrams were actually opened. This
implementation uses direct single-patch 3D halfspaces; it does not implement the
paper's projection maps or occluder fusion.

For a convex planar patch, a fixed-eye shadow cone consists of the behind-plane
halfspace and the planes through each polygon edge and the eye. For a fixed
target point, each edge predicate is affine in the eye position. Testing the
eight observer-box corners therefore covers the entire observer box. Each
resulting halfspace is affine in target position, so an entire target envelope
can be tested by its coordinate intervals. This requires the whole observer box
to lie on the patch's accepted side. A box crossing the patch plane supplies no
certificate. No single-view mask is extrapolated to movement.

Plane construction uses exact rational arithmetic on represented input values,
then brackets each coefficient with Float64 bounds. AABB queries use outward
`nextafter` interval operations and a nonnegative world-space safety margin.
The reference assumes ordinary IEEE Float64 element operations; a native/GPU
port needs its own arithmetic contract. No smaller floating-point bound or
pixel-error guarantee is inferred. Up to 16 contour vertices, 129 halfspaces,
512 boxes/query batch, 262,144 sources and 4,096 patches are explicit caps.

The median-split immutable hierarchy accepts entire hidden subtrees and rejects
outside ones. Query counters include node/source box and plane tests, flag reads/
writes and protection preparation. Flag handling still costs work; sublinear
geometric queries do not imply zero work per source or a native time gain.

## Current verification and reproduction

18 source tests and the same 18 tests from an isolated installed wheel pass.
Exact rational segment/triangle tests independently check translated and rotated
patches and moving boxes. Controls cover a single-view disocclusion failure,
backfaces, nonplanarity/concavity, ties, stale-region reuse, unknown opacity,
unknown bounds, protected/owner sources, cache-neighbor isolation, input shuffles,
range/coordinate limits and budget exhaustion. Actual native sources without an
opacity certificate remain selected: all 6,352 records preserved in the installed
consumer check. This is not a native visibility, image or budget acceptance.

A rendered wall/cuboid movement counterexample also passes at 1280×720. With a
fixed observer, selection emits 2 rather than 14 triangles and the image is exact.
Illegally reusing that point-only mask after movement loses 320 pixels. The
whole-observer-cell certificate retains the box, emits the original 14 triangles
and remains pixel exact. The actual moved before/wrong/correct images were opened.
This is a declared analytic fixture, not one of the required real native scenes.

```sh
PYTHONPATH=lib/src nice -n 10 .venv/bin/python -m unittest discover \
  -s lib/tests -p 'test_occlusion*.py'
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/verify_building_plan_inputs.py \
  --native "$native_receipts" --cache "$asset_db" --output "$fresh_temp_output"
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/viewcell_counterexample.py --output "$fresh_image_output"
```

Private inputs/receipts: `/tmp/outshine-research-selection-930392b72/`.
`source-inputs/` retains all 147 original enriched indexes with manifest hashes;
the native originals remain at
`/tmp/outshine-engine-building-selection-evidence-82e7a69d7/`.
`installed/`, `wheels-isolated/`, package source, exact metadata checks and the
optically read papers remain there for the ongoing experiment. No images are
published and no native source, cache format or GPU contract changes here.

## Real native roof plans: useful negative result

`building_roof_domains.py` consumes the original #801 roof bands and cached hip
facets, including deck height, hip origin/scale, original footprint holes and
disconnected clipped components. It never rebuilds a skeleton or silently repairs
an invalid footprint. `verify_building_roof_domains.py` audits all three complete
native exports: 6,352 sources, 8,200 shapes, 8,109 shapes with 18,824 planar domains,
56 self-intersecting original footprints and 35 forms without planar domains.
The latter two classes remain explicit, without planar occluder admission.

100,843 comparisons between covering planes and native height controls have a
maximum difference of 1.4211e-14 m; all 18,824 reconstructed plane normals agree
with the supplied normals within the declared floating-point transport check.
1,303 native-inside controls lack exact GEOS domain membership; their maximum
distance is 6.9656e-15 m. They stay counted as uncovered, without snapping or
declaring a universal geometric tolerance from these finite samples.

An important diagnostic failure is preserved: on GEOS 3.14.1, `union_all` loses
one complete 9.594168 m² input facet for parent `bbd9aab743`, source ordinal 415,
shape 0. Every individual clipped facet remains present and valid. The guard
rejects this union statistic because it excludes that input facet's interior
representative point. This is evidence against that computed union statistic,
not evidence of a native roof gap. Neither snapping nor fabricated infill is
used to obtain a passing result. Individual accepted triangles drive selection;
the diagnostic union is not an occluder. The guard is not an exact union proof.
The original footprint/facet/gap plots were opened locally.

`building_roof_selection.py` now exercises the real plans in a deliberately
limited roof-only Float64 software reference. It emits at most 32 nearby source
roof seeds, keeps their complete emitted geometry in every plan and admits at
most 256 front-facing triangle certificates for the entire observer box (±2 m
in parent coordinates). Unknown acceptance, invalid/degenerate contours and
patches outside the original source envelope supply no occluder. All remaining
roof emission, including the full control, happens **after** selection. This
is not native Fine geometry, a native material/parity claim or a full-world image.

The full roof control, cell-isolated selection and shared hierarchy all retain
every source in the three inputs:

| Parent prefix | Source records | Roof triangles in each plan | Occluder triangles |
| --- | ---: | ---: | ---: |
| bbd9aab743 | 457 | 5,256 | 256 |
| 6d41e63e73 | 1,754 | 15,869 | 256 |
| 035e2150a3 | 4,141 | 24,690 | 110 |

Eight poses per input (four quarter turns at the original and moved eye) each
produce equal 1280×720 depth images; 16 poses are nonempty and eight are empty.
Only nonempty controls contribute visual evidence. These are finite sampled views of the
analytically scoped observer box, not the continuous native one-second turn
gate. The actual roof panels were inspected. Equal images with **zero culling**
prove no saving: the current single-triangle/full-source-envelope strategy adds
query work and is not recommended as the native selection solution. Subtree,
source and plane counters are in each receipt; these are single Python runs,
not stable timing, allocation/peak or device-budget evidence.

This negative result changes the next comparison: test joint coverage across
adjacent accepted patches and partial source emission, rather than adding more
single-patch query tuning. Native whole/cell grouping, three geographic load
classes, shadows/interaction/neighbor transitions, complete cost/scaling proofs
and real runtime acceptance all remain required by #12. These three exports
share the Wien camera and do not stand in for all requested load classes.

Eight focused consumer tests cover holes, disconnected bands, piecewise planes,
hip origin/scale/deck, invalid originals, nonplanar unknowns, a lost-union negative
control and independent height/normal corruptions. Reproduction uses the local
`.venv` with NumPy, Shapely ≥2.1 and Pillow (measured here with GEOS 3.14.1):

```sh
.venv/bin/python -m unittest discover -s test/experiments/render \
  -p test_building_roof_domains.py
.venv/bin/python test/experiments/render/verify_building_roof_domains.py \
  --input "$native_surfaces/bbd9aab7431104d6b1565886f434e239db5fb511a754a5c529c52b98665dc2ad.json" \
  --input "$native_surfaces/6d41e63e73285d58dd7ab97ab7215e6a44510b3b7628ddef313947d6869904bd.json" \
  --input "$native_surfaces/035e2150a3c1d52a53b4f299007ec4557489a8b51c5780f58928a84a7c8b4500.json" \
  --output "$fresh_audit_output"
PYTHONPATH=lib/src .venv/bin/python \
  test/experiments/render/building_roof_selection.py \
  --input "$native_surface_file" --output "$fresh_comparison_output"
```

Native surface originals remain under
`/tmp/outshine-engine-building-surfaces-evidence-4be90897a/`; actual native source
transfer was independently proven Mesh/Project-free in #801. Audit and roof-only
comparison receipts, input hashes and inspected images remain under
`/tmp/outshine-research-selection-930392b72/roof-domain-audit/` and
`roof-selection-final/{bbd9,6d41,035e}/`. No source/asset cache or native product changes.
