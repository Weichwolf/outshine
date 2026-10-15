# Boundary-preserving fern products (#354)

The [fern study](fern.md) has1,052,256 instanced triangles on a4×4m tile.
This experiment reduces finite cached leaf surfaces while preserving roots,
organ identities, spatial leaf edges, ribs and procedural material inputs.
It is a Python research consumer, with no public/native API change.

## Alternatives and sources

Local [SIGGRAPH2003 Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
PDF-page4, was opened optically: plane coverage/selection and texture baking
motivate testing flattened groups. Existing Ghidra export of worker0x1f4d840
reads a100×100 RGB threshold into an occupancy bitset; its channel semantics
remain unknown. No new headless run, complete vendor algorithm or vendor
geometry/textures are claimed. GEOS constrained Delaunay supplies triangulation.

Same variant0 source,4×8 plane grid,0.003m contour simplification:

| Candidate | Leaf triangles/pinna | Selected pinnae/133 | Whole plant triangles |
|---|---:|---:|---:|
| original finite source | 322 | — | 48,006 |
| rectangular plane | 64 | 12 | 44,910 |
| expanded cutout contour | 290 | 12 | 47,622 |
| original3D boundary | 184 | 76 | 37,518 |

Plane selection additionally requires cosine≥0.4 for every face and the whole
camera/light direction cones. All candidates use0.75px displacement and the
same light projection gain. The plane candidates stop at this geometric
prefilter: no atlas shader, opacity/filter/shadow image or full material-cost
comparison is admitted. Their limited geometric gain does not justify building
that extra consumer here. Rectangle area0.605958m² versus source projection
0.266288m² suggests2.276× local plane coverage work before occlusion; this is
neither a measured GPU overdraw factor nor a billboard-cloud rejection generally.

## Retained spatial product and bound

`pinna_boundary` unions each individual leaf's projected triangles, retains all
polygon boundary vertices in their original3D positions and uses constrained
Delaunay for the interior. Smooth original corner normals/UVs follow retained
vertices. Source rib geometry remains exact. Hard attribute seams, degenerate
projections, holes or orientation reversals are refused. This is not an arbitrary
mesh decimator; projected leaves must each form a simple height sheet.

Canonical source/candidate XY triangles are intersected **per organ**, excluding
unrelated overlapping leaves. After the actual saved FrondGuide deformation,
both finite rendered surfaces are affine on each common overlay polygon. The
norm of their affine difference is convex, so its maximum lies at an overlay
vertex. Add1e−10m numerical padding. This bounds corresponding finite surface
displacement, not an analytic Blade surface, normal interpolation or radiance.
The exact original3D boundary segments also survive the guide deformation.

One bound is saved per(frond,pinna). Selection multiplies it by the largest root
scale of that prototype,1920/orthographic-width and the joint projection gain.
Infinite sun projection onto z=0 uses1/minimum-light-cosine. The whole finite
area emitter and receivers have enclosing spheres; its perspective Jacobian
adds`lamp_z/(lamp_z-receiver_z)`, bounded by9/(9−receiver-height).
Actual receiver sphere3.207836m, height upper0.822352m; maximum gain3.323133.
This explicitly does **not** certify shadows on arbitrary foliage receivers,
coverage area, BSDF response, wind or temporal transitions.

Tile selection retains76/90/67/64 coarse pinnae in variants0..3. Fern triangles
fall768,096→614,226; unchanged grass contributes284,160. Total898,386 is14.62%
less than1,052,256. At1.7m orthographic width variant0 selects only five pinnae,
47,316 triangles; detail is retained as the camera needs it. No new habitat/RNG
plan or plant is generated on renderer cache hits. Snapshots retain all ribs,
unselected leaves and full logical organ IDs; source recipes remain available.

## Costs and controls

Frozen source bb3d349d5, Python3.13/NumPy2.5.4/Shapely2.2.0/GEOS3.14.1:
complete four-variant comparison, source reads, bounds, selection and compression
5.215s wall/5.211CPU-s/114.29MiB peak RSS, terminal0; load1.64→1.67.
Variant0 canonical boundary/overlay preparation0.105s, assembly0.066s, bounds
0.454s,133-record lookup0.050ms. Preparation is offline work, not a frame cost.

Six alternating complete variant0 assemblies from prepared canonical products:
median97.69ms source versus71.17ms boundary candidate. Both evaluate seven
guides/133 placements; triangle work falls48,006→29,652. Array output falls
7,919,730→5,148,276 bytes; untimed tracemalloc peaks18,660,987→12,116,743 bytes.
Selected variant0 output6,336,042 bytes. These are Python arrays/traced peaks,
not all allocation counts/copied bytes or GPU resources. Timings were under
recorded shared load, without a native/device/frame acceptance claim.

Five new independent controls plus fern/Blade/FrondGuide controls:25 source and
25 installed-wheel tests, terminal0. Known pyramid displacement0.2m, original
edges/UVs/normals, owned arrays, cached RNG refusal, byte-exact partial replay,
all ribs/fallback, hard-seam and invalid-budget counterexamples are covered.
Independent finite differences of point-light ground projection check the
additional perspective factor. The installed wheel is unchanged from PR #329;
no new library build/dependency/API is claimed.

## Opened whole images

Actual Blender5.2.2 EEVEE/llvmpipe images,1920×1080, fixed original materials,
lights and populations: default/back/grazing tile views at four samples, and
the whole near-view field plant at16 samples. All original/selected pairs were
opened. Leaf-edge structure remains legible in these views, without gross crown
loss. Local brightness/coverage changes remain; low-sample noise, smooth repeated
pinnules, orderly crowns and authored hues are still visible limitations.

| View | Changed pixels % | Mean absolute RGB /255 | Maximum /255 |
|---|---:|---:|---:|
| default tile | 2.4753 | 0.030705 | 40 |
| back tile | 2.3618 | 0.025505 | 24 |
| grazing tile | 0.8820 | 0.012652 | 36 |
| field plant | 0.0346 | 0.000625 | 28 |

These whole-frame differences include geometry, lighting and shadows. Their
small means do not bound foreground silhouettes or establish a radiance gate.
Original default/field images are reused from the unchanged default renderer
in c379acc6c; new original back/grazing views and selected images use bb3d349d5.

| New process | Wall s | CPU-s | Peak RSS MiB | Setup s | Render s |
|---|---:|---:|---:|---:|---:|
| selected default | 40.859 | 43.798 | 2256.23 | 1.052 | 38.035 |
| source back | 41.488 | 44.967 | 2327.16 | 1.245 | 38.460 |
| selected back | 40.743 | 43.862 | 2340.41 | 1.101 | 37.727 |
| source grazing | 28.904 | 32.541 | 2360.52 | 1.277 | 25.836 |
| selected grazing | 28.165 | 31.694 | 2340.20 | 1.092 | 25.243 |
| selected field | 97.023 | 97.824 | 2032.27 | 0.431 | 94.749 |

Every measured render child terminates0, with receipts and load recorded.
These software-renderer observations do not establish GPU/device speedup.
The stricter1.7m profile build takes5.834s wall/5.822CPU-s/123.68MiB, terminal0.
No general LOD, botanical species, wind, full forest or native gate is closed.

## Reproduction

Use the existing `.venv`, pinned experiment dependencies and
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`;
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 LP_NUM_THREADS=1` for the render.
Generate the original inputs with`fern_experiment.py --output <temp-source>`.

```sh
.venv/bin/python -m unittest pinna_checks fern_checks test_fronds test_spatial_frond test_blades
.venv/bin/python test/experiments/vegetation/pinna_experiment.py \
  --inputs <temp-source> --output <temp-selected>
.venv/bin/python test/experiments/vegetation/render_fern.py \
  --inputs <temp-selected> --output <temp-shot> --case tile --view default
```

Output must be a fresh directory. Repeat views back/grazing against original
inputs; lights/populations stay fixed. For field use a separate product with
`--span-m 1.7`, then renderer`--case field`. Actual current products, original
input symlinks, images, process receipts and comparisons remain locally in
`/tmp/outshine-vegetation-pinna-bb3d349d5/`. Existing original pictures remain in
`/tmp/outshine-vegetation-fern-shots-c379acc6c/`. No research gallery/doc images.
