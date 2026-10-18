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
