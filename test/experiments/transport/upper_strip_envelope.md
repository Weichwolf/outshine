# Transverse ribbon envelopes

Owner: #915 / #1955. Python reference and native producer; no World LOD admission.
Inputs: [fixed native road parts](native_road_parts.md), unchanged archive SHA
`5aafb7b17b2820135b45ed26ca57a7e48d575e84ee6262ead81fc438713810fb`.

## Construction

An ordinary source span has two ordered four-point transverse rings and six upper
triangles. A convex projected outer quad has two possible diagonals. Choose its
upper roof; use the reflected lower roof for a closed shell's underside. Sides,
caps, junctions and unknown source templates remain unchanged. This does not merge
different roads, change longitudinal sampling or flatten every source into one plane.

Preserve all four exact source points at both ends of each contiguous coarse run.
Split the adjacent roof triangle into three triangles at that boundary. Each roof
therefore saves `4 * (run_spans - 1)` triangles; an isolated span saves nothing and
retains its original. Surface and closed-shell runs save four/eight per interior
span respectively. Contacts are counted in the study, not omitted as overhead.

## Finite-source geometric estimate

Project each inner source-ring point onto its ordered outer segment. Let the largest
residual be delta. The idealized source triangulation lies inside the four-corner
tetrahedron; its XZ projection covers the convex outer quad. The maximum vertical
distance between the upper/lower roofs occurs at their diagonal crossing: gap.
Thus symmetric geometric distance to the unstitched roof is at most `gap + delta`.
Restoring actual source boundary vertices perturbs that roof by at most delta;
use `gap + 2 * delta` for stitched runs. Strictly folded/degenerate/unordered inputs
keep their original. `stitch_quad` retains exact boundary coordinates and edges.

These are exact-arithmetic arguments evaluated by a Float64 **probe**. Directed
arithmetic, native transforms/rasterization, normals/materials, continuous functional
curves and runtime error selection are not certified. Dense independent point/triangle
sample checks supplement the argument; samples do not prove a maximum.

## Fixed-corpus result

The corpus has 6,510 edge and 1,848 junction emissions, 321,472 total triangles.
Recognized templates: 5,579 surface ribbons and 567 closed shells, 25,674 spans.
Other geometry remains in every cost total. After exact run-end stitching:

| Diagnostic geometric estimate | Complete-corpus candidate triangles |
|---|---:|
| No imposed estimate limit |239,184|
| At most 0.05 m |242,572|

The 0.05 m comparison saves `(321472 - 242572) / 321472 = 24.54%`; it is **not**
a runtime setting. A consumer must derive its world-space budget from projection,
distance, permitted error and the bound reference domain. Some shell undersides
have large transverse residuals; they are not assumed coplanar.
The measured Python loop includes payload checks, topology recognition, geometric
queries and cost comparisons; it is not a generator/frame/device benchmark.

## Native implementation boundary

Emit coarse vertices/indices directly from the source sampling plan; the stored
fine meshes here are an offline opponent only. Bind the canonical finite visual
reference and sampling version explicitly. A mesh-proxy error is not an error bound
to the continuous CAD curve. Keep functional products separate. Qualified source
bounds must choose products before fine payload work. Original remainders, exact
run contacts and existing full-body silhouettes remain mandatory. Native image,
terrain-contact, material/normal and total-cost comparisons precede adoption.

The native `RibbonEnvelope` kernel uses outward interval arithmetic on final Float32
source rings. `check_native_ribbon_envelope.py` independently checks actual operands,
roof choice and bounds with exact rationals. `RoadSurfaceEnvelope` evaluates canonical
stations before mesh allocation, classifies adjacent spans and directly emits only
needed vertices/triangles, including complete run ends. Closed/step bodies and
junctions retain their original emission. Unqualified/isolated spans retain originals.
Exactly coincident stored contact profiles need no extra corner triangles; an empty
fan alone does not imply missing coverage. Continuous-curve error remains separate.

`RoadPageAddress.PositionErrorM` takes its reference from `DisplayPolicy`.
`StoredSurfaceEnvelope` has independent asset-v7/manifest-v6 identities; older products
keep their bytes/keys. Misses use the native producer; hits need no functional plan
or sampling budget, also in a fresh process. World selection before fine IO, projection,
terrain/material/raster comparisons and native device gains remain required.

Native World generation also persists a separate `road-page-recipe` package: source
stations, complete port planes/profiles, junctions, colours and shared native height
solids. Solver workspace/reference arrays are not part of this input. Its key excludes
render LOD/layout/error; material, source plan, parts and anchor remain bound.
`StoredRoadPageMeshProducer` admits source IO/decode ownership before loading this
package. A fresh process can generate a display miss after deleting the fine page;
a later display hit works after evicting the recipe. Busy retains both prepared
packages without regeneration, and cancellation releases their publication ownership.
Ground recipe 103 rebuilds older regions once so their new inputs are populated;
source acquisition itself is unchanged. Automatic World selection remains separate.

```sh
python -m unittest discover -s test/experiments/transport -p test_upper_strip_envelope.py
python test/experiments/transport/compare_upper_strip_envelope.py --output /tmp/outshine-upper-strip.json
```

Python checks require existing NumPy. Native mesher/cache tests cover production;
they do not establish World image quality or frame-budget compliance.
