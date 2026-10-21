# Native analytic surfaces for coverage research

Refs #12/#567. Extends the native selection export from #800 with original
untriangulated surfaces and actual ray/height/normal controls. No product
selection, meshing, cache, material, binding or device policy changes.

```sh
"$native_selection_exporter" "$input_manifest" "$fresh_temp_result" --surfaces
python3 test/experiments/render/check_building_surface_inputs.py \
  --input "$fresh_temp_result" --baseline "$original_selection_result"
```

Input preparation and complete compilation flags are described in
`native_building_selection.md`. The original source identities, envelope and
shell-error fields are retained. This explicit mode skips every whole/cell bake;
its result list is empty. It reads the original index/form/view snapshot and
never constructs a Fine mesh or a missing hip plan. Missing cached hip plans
remain unavailable; their height/ray controls are omitted explicitly.

## Coordinates and geometry

Each source surface contains its origin and east/north/up axes. All shape rings
and holes are original ENU metres. A point becomes ECEF through
`anchor_ecef + surface.origin + east*x + north*y + up*z`. Native rays and their
points/normals use ECEF-offset coordinates relative to the same parent anchor.
The actual source minimum height and native `BuildingBottomM` preserve raised
bottoms and underpasses. Courtyard holes remain separate rings.

`faces` retain every native FaceAt index, part, side and EstimatedTriangles value.
Side0 is the bottom, side1 is the entire roof, and subsequent sides follow outer
then hole edges. Side1 is **not necessarily one plane**. Bottom/wall geometric
normals are provided in ENU. Wall normals retain ring/hole orientation; zero
length edges remain null. These are geometric, not curved shading normals.

Roof kinds follow the native enum:0 Flat,1 Gable,2 Hip,3 Shed,4 Mansard,5 Sawtooth,
6 Dome. Original parameters remain in every shape. For Flat/Shed/Gable/Mansard
and zero-rise roofs, `roof_bands` describes planar pieces without triangulation.
Compute `v=-(x-centre.x)*axis_u.y+(y-centre.y)*axis_u.x`; each piece is the original
exterior minus holes intersected with its `v_interval`. Null bounds mean
unbounded. Closed/open endpoints are explicit. Its plane is
`z=height_at_v0_m+slope_v*v`, with its ENU normal. This representation deliberately
leaves standard polygon clipping to the consumer instead of generating a fine
mesh. It preserves disconnected pieces and holes when clipped correctly.

Hip roofs retain their cached native facet contours, gradients, intercepts and
source-ring/edge ownership. Facet coordinates are relative to `hip.origin` in
shape ENU. The height is `roof_deck_m + rise_m/maximum_distance_m *
(dot(point-hip.origin, gradient)+intercept_m)`. Facet normals include that scale;
native plan precision is retained. No skeleton is rebuilt. Curved or sawtooth
roofs with nonzero rise have no invented planar bands and remain nonprojectable
according to the native source contract.

Native `HeightAt` samples include original ring/hole corners, centre and profile
break/control positions. `inside` is a separate native Contains result; a height
function's extension outside its footprint is not roof coverage. Native Trace
controls include six directions through the source bounds and the actual view
direction, with hits and misses retained. Side/index/point/normal provenance is
explicit. These finite samples support transfer checks, not a universal geometry
or visibility proof.

## Visibility and acceptance limits

`opaque_for_coverage` and `interior_topology` remain null. Fine openings are not
represented. Projectable, Hausdorff distance and ShellError do not prove an opaque
interior or safe occlusion. Current engine-generated roof/wall materials are
registered opaque, but the applicable material state and accepted opaque subset
need the explicit consumer contract discussed in #12. This exporter does not
admit them globally as occluders. Shadow and interaction demand remain separate.

The checker verifies original identities, face topology, plane equations against
native height samples and geometric normals against native Trace. Hip ray hits
are checked against a matching native facet plane/normal; that alone does not
verify the entire polygon union. Normals at band creases are counted separately
because the geometric normal is not unique there. Nonplanar roof hits remain
explicit controls. Floating-point comparison allowances are FP64 transport
checks, not changed image/LOD error thresholds. Research still owns independent
coverage, footprint clipping, swept-view validity and actual image comparisons.

Build, full-flag focused analysis, source/input hashes, original JSONs, comparison
receipts and no-meshing tripwire controls are retained under
`/tmp/outshine-engine-building-surfaces-evidence-4be90897a/`. Inputs remain at the
canonical #800 evidence root. Source/asset caches and original native inputs are
unchanged. No device timing, memory budget, runtime image improvement or complete
world acceptance follows from these diagnostic exports.
