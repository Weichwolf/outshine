# Source-informed building-parent materials (#1167)

`build_mass_appearance.py` adds camera/light-independent wall/roof material
measures to the **frozen** geometry from `build_building_mass.py`. It reads the
same BSC3 source block and parent index, never native triangle/view products.
Only grouped sources receive new material work; unchanged original remainder
sources keep their existing fields. All source/function/near products survive.
Missing source measures reject a whole group through `supported=false`.

Each source contributes physical surface area, mean linear diffuse albedo and
a discrete roughness distribution to its group. Roof slopes affect source area;
roof/wall roles are separate. The existing owned region-building defaults,
source paint/style/variant, window/frame/pane fractions and roof tints/joints
inform these measures. Geometry and policy hashes bind the separate output to
the exact frozen product and producer policy. This is an experiment format,
not a new native binding or asset-cache contract.

The declared model erases repeat phase and finite boundaries. It omits coating
and masonry fluctuations, geometric recesses/overhangs/bases and perturbed
shading normals. Whole-repeat window and roof-joint areas are approximations;
they do not reproduce native pixel-footprint filtering or its roughness means.
Current source models and captured native material bindings are distinct.

For the controlled opaque isotropic dielectric model, F0=.04/F90=1 is shared.
At common geometric normal/light/view, diffuse is linear in albedo and specular
is independent of albedo. Therefore mean diffuse colour plus the **full finite
roughness measure** reproduces the independently shaded source-material mixture
under that common normal. It does not average roughness or normals, reconstruct
missing surface orientation, or preserve material/visibility correlation.
`building_mass_lighting.py` uses the existing checked direct GGX CPU reference
and pinned energy table; multiple roughness lobes cost multiple evaluations.

`compare_mass_appearance.py` preserves all original-source remainder and compares
original geometry/current-policy material states, a gray parent control and the
source-informed parent at the existing moving-023 camera. The fixed16×16 point
quadrature per pixel is **not an exact PBR integral**. Its alpha is separately
compared with continuous opaque projected coverage; no radiance-error bound is
inferred from that alpha check. Error reports separate premultiplied C and T
from compositing against a fixed gray background. The half transition blends
only volatile C/T endpoints at the same camera; it does not qualify depth,
casters, world streaming or LOD selection. The existing orthographic construction
camera exposes lost gables and filled spaces at1280×720, using the same material
model and geometric normals without shadows. Actually inspect both local images.

SIGGRAPH-first reference: Feibush/Levoy/Cook,
[Synthetic Texturing Using Digital Filters,1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf),
p.2/Fig.1 optically inspected: texture/object/image domains and edge filtering
must remain distinct. It does not justify this whole-source area approximation.
The [existing material-filter experiment](material_filter.md) supplies the
independently checked direct GGX slice and counterexamples to roughness averaging;
its old native probe does not qualify these building materials natively.

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/build_mass_appearance.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --geometry /tmp/outshine-mass-build/product.npz --cells 33 \
  --output /tmp/outshine-mass-materials --trace
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/compare_mass_appearance.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --geometry /tmp/outshine-mass-build/product.npz \
  --appearance /tmp/outshine-mass-materials/appearance.npz \
  --output /tmp/outshine-mass-material-view
```

Build, optional traced replay and comparison peaks are separate. Known array
bytes count their named accumulators, not cumulative allocations, GEOS heap or
physical copy traffic. Trace scope is the whole source-read/build replay; output
words must match. Python/NPZ times do not predict native/GPU/device performance.
Measured results and actual image findings live in the delivery PR. Unknown
transport reservations, source shading-normal/native PBR equivalence, shadows,
IBL/night/emission, transitions and complete-world/p99 admission remain open.
