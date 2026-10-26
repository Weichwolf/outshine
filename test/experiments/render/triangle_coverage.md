# Pixel-integrated opaque group coverage

Four point samples per pixel do not bound the error of a pixel box filter.
A strip can lie between every sample, and a narrow opening can be missed by all
four. This reference measures those effects before using sampled coverage to
judge a building aggregation candidate. It constructs no new parent asset.

`triangle_coverage_reference.integrate` projects the supplied finite triangles,
unions their two-dimensional opaque support and intersects that silhouette with
each unit pixel rectangle. The area of each intersection is its box-filtered
coverage. Overlapping triangles count once; openings stay open. The near plane
must precede every input vertex. Unsupported or oversized input is declined
whole, never silently removed. The existing direct rectangle-area clipper is
reused. No grid, pose, layer, phase or sample-count search is introduced.

This is an analytic polygon-area calculation on projected Double coordinates,
evaluated by GEOS. It is not exact-predicate arithmetic or a certified bound on
real geometry. The generic union is two-sided. The explicit original-group
comparison first selects front-facing faces by their geometric normal, matching
the existing CPU consumer's face orientation. Shading normals are irrelevant to
this mask. A fixed four-point mask is checked against the existing ray consumer;
disagreement declines the comparison. This finite check is not a general proof
of equivalence near grazing/degenerate faces or the consumer's epsilon cutoff.

The retained original576-source group, original local needs and existing
`moving-023` pose remain unchanged. The camera and every source vertex lie above
the reference floor, so it cannot hide the group. No other world occluders are
introduced. The signal is isolated opaque coverage: no varying opacity, material
or radiance integration, depth ordering, receiver shadows, PBR, native MSAA/TAA,
motion or device admission follows. Known original material/normal limitations
and the source/control distinction remain. In particular, sampled equality does
not establish equality under a box filter.

SIGGRAPH-first basis: Feibush, Levoy and Cook,
[Synthetic Texturing Using Digital Filters, 1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf),
p.2/Fig.1 inspected optically. The paper motivates filtering polygon edges in
consistent coordinate spaces; this reference declares a pixel box filter.

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B \
  test/experiments/render/compare_group_coverage.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --product /path/to/existing-1199-draw-products \
  --output /tmp/outshine-group-coverage --render
```

The driver preserves both product hashes, input receipt, coverage arrays,
same-phase ray agreement, work/explicit ndarray byte counts and process peak.
The union/clipper's internal GEOS operations, allocations and copied bytes are
unmeasured. The process holds all alternatives and full original inputs; these
reference costs are neither reduced runtime residency nor a renderer benchmark.
Raw measured results and actual local image inspection belong to the PR.
