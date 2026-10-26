# Source-first grouped building mass

This bounded geometry experiment groups actual BSC3 source forms before creating
individual building surfaces. It does not generate a new source population or a
qualified runtime asset. Source ordinals, functional/near products and the whole
original remainder remain separate from the coarse grouped mesh.

The one camera-independent profile uses3m proximity,3m total roof/bottom height
ranges and12m maximum building height. These are experiment parameters, not a
universal LOD policy or a0.5px surface-distance certificate. Raised sources and
sources with an unsupported part remain atomic original needs. Roof heights are
evaluated from the supplied planar recipes in the existing common tangent frame.
The source-floor footprint uses a common-up extrusion approximation; its lateral
roof/wall shift is reported, not claimed exact. Curved/invalid recipes are not fit.

Neighbor edges are ordered by distance and source ordinal. A union is allowed
only if the entire resulting component still satisfies all height restrictions;
pairwise similarity cannot accumulate into an unbounded height chain. Query
batches and candidate/geometry limits bound temporary work. No native view or
triangle package may be read by the generator. Existing parent archive digest
validation still reads the complete compressed archive; no avoided archive IO
or native selective-cache behavior follows.

Each multi-source component receives vector dilation/erosion at1.5m with mitre
joins/limit2 and zero-tolerance topological simplification. Actual source holes
are subtracted afterward; occupied islands inside a hole are not removed.
Added and lost plan area are measured. The offsets are not a certified circular
Minkowski closing, an extensive envelope or a surface-Hausdorff bound. New
inter-building space is not certified empty of roads/functions. This is a key
unresolved image/semantic cost, even if the coarse silhouette looks plausible.

The group is extruded between its actual minimum bottom and maximum roof height.
Constrained cap triangles must cover the footprint. Positive triangle area,
indexed oriented edge incidence, expected Euler characteristic and positive
volume are checked before/after Float32 packing. Distinct emitted nodes may not
collapse and face orientation may not flip. Only a duplicate-node packing failure
may retry: cast the original polygon rings to Float32, remove **adjacent identical
coordinate words**, then triangulate both caps and walls from those same rings.
Nonadjacent collisions, invalid rings/polygons, lost holes/components and collapsed
heights remain failures. No distance tolerance, feature-size cutoff or invalid-
polygon repair is introduced. All original strict checks run on the retried prism.
Previously accepted groups stay word-equal, including their indices. A failed group
retains every original source. Local closed prisms do not prove global nonintersection,
physical validity or source surface correspondence.

The retry reports ring casts, retained arrays, removed words, plan-area change and
the failed first attempt's known arrays/work. Its maximum vertex-rounding value
includes the initial XY cast as well as height packing. The retry's Double-volume
field describes the rounded-XY prism; `input_footprint_volume_m3` separately retains
the pre-rounding footprint/height volume. These are scoped numeric diagnostics,
not an error certificate against original buildings. More groups can increase
image error as they replace original geometry; smaller payload is not a free
quality improvement. The complete remainder/material comparison must be retained.

Initial group counts/accepted source totals are not portable material oracles:
even unchanged NumPy/GEOS versions can select different strict outcomes under
different BLAS kernels. Regression checks compare old/new groups in the same
environment; measured counts belong to their receipt. This retry does not promise
bit-identical triangulations across platforms. The separate material test checks
the supplied group/remainder partition and actual source-measure calls.

The complete comparison pays the original remainder. Both candidates and the
fixed original geometry control use shared Float32 XYZ/UInt16 indices in the same
reference frame; common-frame rounding is reported separately from the original
native post-vertex reference. All candidate metadata is counted. Group shading
normals, material/UV/metres/program fields are absent and unqualified, so these
geometry/metadata counts are not equal-quality full-asset/GPU savings. The native
field/normal/opacity/depth/caster/contact/transition contract is not changed.

The existing moving-023/1280×720 comparison uses the pixel box-filter reference.
A second fixed oblique construction camera displays actual meshes with geometric
normal shading at1280×720 each. It exposes flat roofs and filled gaps; it is not
a new runtime pose qualification, PBR view or native performance measurement.
No global p99/device/world admission is implied by these finite diagnostics.

SIGGRAPH-first review: [Simplification Envelopes,1996](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf),
p.3/Figs.2–4 optically inspected. Its topology-preserving envelopes are not the
method or guarantee of this deliberately topology-changing aggregation.
Primary aggregation precedent: Kada,[Aggregation of 3D Buildings with a Hybrid
Data Model,2010](https://www.isprs.org/proceedings/xxxviii/part4/files/Kada.pdf),
p.4/Figs.5–8 optically inspected. It uses morphological operations to connect
building masses; this experiment uses vector offsets rather than its raster/cell
decomposition. [Shapely's primary buffer documentation](https://shapely.readthedocs.io/en/stable/reference/shapely.buffer.html)
specifies the join/mitre controls. No raster/layer/phase/normal/parameter sweep.

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/build_building_mass.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 --output /tmp/outshine-mass-build --trace
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/compare_building_mass.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --product /tmp/outshine-mass-build/product.npz \
  --output /tmp/outshine-mass-view --render
```

Build and read/compare peaks are separate. The optional traced replay checks every
output word. Tracemalloc peak/live blocks exclude GEOS heap and are not cumulative
allocations or physical copy traffic. Explicit array sizes and counts are scoped
to their named operations; failed-group/internal-library work is not free.
Measurements, viewed diagnostics, detailed refusals and admission gaps live in PR.
