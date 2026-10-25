# Source area mattes with matched original remainder

Owner #1167. After #1330's negative direct coarse sampling result, retain small
source surfaces through analytic projected area instead of estimating coverage
from point density. Source roof/wall domains, actual normals and full roughness
measures remain coupled. No fine/native triangle input is used by the matte
producer; the paired control uses the existing coarse prism extrusion.

The finite plane-cover heuristic selects at most32 actual source normals and128
oriented planes. Candidate offsets are spaced by epsilon, root radius/32. A plane
accepts a patch only when all corners of its parameter-domain bounding rectangle
are within epsilon, including a conservative floating margin, and normal cosine
is at least.5. Projected area minus a10×near-miss penalty guides greedy choice.
This borrows SIGGRAPH2003 validity/area/tangency ideas, not its Hough-space adaptive
algorithm or guarantees. No optimal-cover claim. A partly uncovered source falls
back wholly to its original product; final corner distances are checked again.

The original prism producer is rerun on that represented subset. Any further
whole-source fallback also leaves the matte subset, so both paired paths retain
exactly the same sources as originals. This separates representation improvement
from the quality/cost of extra native fallback. Original-cut prisms remain a third
control. Source IDs, functional/near assets and the complete remainder are kept.

Projected source polygons are intersected with root-relative cells at max-axis
extent/16 spacing. Area of the union inside a cell defines alpha; holes survive
integration and surfaces smaller than a cell still contribute. Overlap is not
summed into alpha. Double union and Float32 reconstructed areas are reported.
Nearest cell reconstruction is an approximation; point-to-plane displacement
does not bound texture blur, image error, shadowing or visibility correlations.

Each cell retains up to four source normal/material profiles by projected area.
Their individual normals and discrete roughness mixtures are never averaged.
Discarded profile area is reported, not certified harmless. The optional offline
response merge combines only bit-identical Float32 normals after that selection.
For fixed F0=.04/F90=1 unit-mass dielectric GGX, diffuse is albedo-linear and
roughness-independent; specular is albedo-independent. Hence albedo/full roughness
measures can combine at the same normal before Float32 emission. Actual emitted
mass/RGB differences remain measured; this is not a general metallic/IBL rule.
Comparison checks the source lighting-program digests in the producer receipt.

Two-sided quads sample these fields, cull backfacing source profiles, sort complete
layer coverage with actual opaque original hits, and shade only nonzero weighted
records. A fully facing profile mixture has exactly unit coverage, avoiding false
roundoff transmission. Same1280×720/SS16 output box filter, four existing root
views. The dense native and small analytic references stay labelled. Reading
checks node/tree/frame/material identities and bounded array/mass contracts;
loading performs no source reconstruction or response merge.

The first paired images have lower RGB/coverage error in these four views, but
dense GGX work remains much higher than matched prisms. No default/native/device
recommendation follows. Additional fields and identical original remainders are
paid separately. Construction/control/field/merge CPU, raw/container/reader bytes,
known work and whole-process peak are reported. No full allocator/bandwidth,
global image, temporal/night/weather/caster/world or1280×720/60-p99 qualification.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_mattes.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --output /tmp/outshine-matte-products
python -B -W error test/experiments/render/compare_building_mattes.py \
  --input /tmp/outshine-transition-products --products /tmp/outshine-matte-products \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --native-cell 33 \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-matte-view
```

Input generation remains `building_transition.md`; outputs stay explicit Temp.
Methods/cost tables/evidence belong in the PR. SIGGRAPH-first:
[Décoret et al.,2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
pp.1–4, Fig.1 and PDFp.3/Fig.2/validity/area/penalty equations optically viewed.
