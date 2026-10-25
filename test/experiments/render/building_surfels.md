# Direct coarse source surface samples

Owner #1167, following #1328's source/group image-loss diagnosis. This is a
completed negative comparison of two deliberately coarse source sampling methods,
not a replacement for the current parent or a native recommendation. It tests
whether coupled position/normal/material samples avoid the filled-parent losses.

The producer reads the existing qualified root's BSC3 roof/wall domains before
triangulation. It uses a fixed16×16 grid on each of three axes of a root-local
cube. The first candidate keeps the closest/farthest intersection per axis ray;
the all-depth control keeps every intersection at exactly the same grid spacing.
No camera, current light, Place or fitted image threshold enters production.
Only sample points receive current-policy UV/category/paint/material evaluation.
Position, individual normal, albedo and the full discrete roughness mixture are
stored. Exact Float32 attribute duplicates are removed; normals/roughness are
never averaged. Patch/source IDs are not required per rendering sample.

Intermediate all-depth records are capped at65536; emitted products at1536.
Overflow fails this diagnostic instead of publishing a partial product. These
limits are not a world-quality guarantee or native fallback/queue policy.
Sources with no sample are explicitly listed; they are not proven hidden.
Original logical/function/near sources remain intact. Both image candidates pay
the identical complete native/analytic original remainder from the existing root.
Reading binds node digest, material policy, palette and frame without generation;
decoded geometry and material bounds are validated. Whole node hash reads still
include its lineage: this diagnostic sidecar is not a native far-IO contract.

Rendering projects tangent Gaussian kernels through the perspective Jacobian.
It uses source grid spacing as the tangent scale, c=1 cutoff and the paper's
empirical coverage threshold tau=.4. A normal-L1 density correction compensates
the three axial sampling grids; no partition-of-unity or sparse-coverage bound
is claimed. Contributions farther than one spacing behind the closest tangent
depth are excluded. This grouping is an approximation, not occlusion proof.

Individual samples are lit before kernel combination, preserving their normals
and roughness measures. View direction is evaluated at each point, another finite
approximation. Native remainder depth is honored before accumulation and opaque
native foreground suppresses covered samples. An empty accumulation remains valid.
All models use the same regular SS16 box quadrature. We do not add the paper's
analytic Gaussian pixel lowpass or its deferred averaged-normal shading. Thus
this is not a full implementation or general refutation of Surface Splatting.

Same four root views at1280×720; both dense native and small analytic references
remain labelled. Both coarse point candidates have fewer GGX evaluations but
worse total RGB/coverage error than the previous parent in these views. All-depth
sampling partly recovers dense coverage, still misses many small source surfaces
and does not yield a general image improvement. More resolution, fitted alpha
thresholds or density sweeps are not justified here. Explicit subcell source-area
preservation is required before reconsidering this direct coarse sampling idea.
Exact costs/counts/images and the two-method comparison belong in the linked PR.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_surfels.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --output /tmp/outshine-source-hull
python -B -W error test/experiments/render/build_building_surfels.py \
  --input /tmp/outshine-transition-products --cells 33 44 --all-depths \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --output /tmp/outshine-source-ldc
python -B -W error test/experiments/render/compare_building_surfels.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --surfels /tmp/outshine-source-hull \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-source-hull-view
```

Repeat the last call with the declared LDC product and a different Temp output.
Source inputs/strict fallback remain `building_transition.md`. Diagnostic images
stay in Temp; no native/shadow/Ready/world/1280×720/60-p99 admission follows.

SIGGRAPH-first primary review:
[Pfister et al.,2000](https://merl.com/publications/docs/TR2000-10.pdf),
paper p.337/Figs.3–4 optically inspected (university mirror PDF p.3), and
[Zwicker et al.,2001](https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/zwicker01.pdf),
pp.374–377, Fig.5/Jacobian, normalization, deferred shading and coverage read;
PDF p.4/Fig.5 optically inspected. The original adequate-sampling assumptions do
not hold for all small/disconnected sources at this direct coarse resolution.
