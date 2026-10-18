# River profile experiment (#760)

Compare flat fragment levels, nearest-course projection and normalized compact blends
of local linear course profiles. This is an experimental height field, not a production
river solver. NumPy and Shapely/GEOS perform the array and spatial-index work. Surface
experiments use the shared `outshine_ref` mesh contract from `lib/`; use its Python>=3.11
dependencies (NumPy, SciPy, Shapely>=2.1, Pillow; Clarabel for the shared fitting reference).

```sh
PYTHONPATH=lib/src python3 -m unittest discover -s test/experiments/water
python3 test/experiments/water/river_profiles.py \
  --input test/experiments/water/fixtures/feldkirch-ill-profile.npz \
  --output /tmp/outshine-river-results --plot
```

Input NPZ arrays, with no pickled objects:

| Key | Shape | Meaning |
|---|---|---|
| `segment_nodes` | N×2×2 | Shared course endpoints, local east/north metres |
| `segment_levels` | N×2 | Endpoint ASL metres, after shared-node reconciliation |
| `boundary` | M×2 | Queries along the example body's boundaries |
| `old` | M | Existing constant fragment heights at those queries |

The fixed Ill example has 120 shared segments and 5667 boundary queries. Its adjacent JSON
records source hashes, selected native inputs, attribution and the approximate planar frame.
It supports reproducible field comparison; it does not establish source-class correctness.
Schema 2 adds the original `ring_points`, `rings`, `surfaces` and `surface_source_indices`;
all earlier arrays are unchanged. `surface_mesh` unions the selected outlines, preserves
holes and uses GEOS constrained Delaunay triangulation, as the native polygon mesher does.
Boundary refinement has a predicted vertex/face budget before triangulation; no interior
Steiner points are added. Every shared coordinate owns one height vertex. Coverage and
overlap are checked separately; no quantization or discarded thin faces repair the result.

The caller owns body selection, source identities/classes and datum. Geometric connection
alone does not establish a flowing-water body. The current native `WaterAsset` loses these
classes; that remains a required native integration change in #760.

`nearest` has a demonstrated discontinuity where two branches exchange nearest ownership.
`blend-clamped` is continuous there but does not reproduce uniform longitudinal slope.
`blend-affine` preserves that slope. It uses each segment's linear extension and weights
`(1-t)^3(1+3t+6t²)`, where t is distance divided by a positive local support radius.
The radius is the declared collar plus twice the nearest-course distance. It is not a
terrain raster size. Within valid support this normalized partition is continuous across
nearest-owner changes; it does not establish monotonic flow or a global gradient/error bound.

The fixed real-input counterexample rejects blending as a complete flow solver: sampled
heights can rise along a descending source segment. `nearest` avoids that particular
counterexample but fails the independent branch-switch continuity check. Neither candidate
is ready for native admission. `flow` in the report samples 17 positions on each source
segment dropping more than 1 cm; passing that sampling is not a monotonicity proof.
The next step is a shared conforming surface with directional constraints, rather than
porting either failed approximation directly into water and terrain consumers.

Cost: at most 8192 input segments; queries in batches of 64. The spatial query enumerates
all local candidates before checking the per-point reference budget. Report that work;
do not claim the query traversal itself is capped by that budget. Overloaded points return
NaN explicitly; independent points continue. A native recipe must select valid local pieces.

Missing coverage, inconsistent branch heights, lake/sea separation, source bridges/tunnels,
profile-conforming water triangulation, terrain contacts and spatial error bounds remain open.
The experiment does not change PNG pins, native caches or engine gates. See #760 for real
inputs, comparisons and the integration decision; OMT source semantics are in
`doc/references/data/osm/openmaptiles-water{,way}-schema.yaml`.

## Directional surface reference

```sh
PYTHONPATH=lib/src python3 test/experiments/water/directional_river_fit.py \
  --input test/experiments/water/fixtures/feldkirch-ill-profile.npz \
  --output /tmp/outshine-river-surface --spacing-m 20 \
  --down-grade 0.1 --cross-grade 0.005
```

The independent HiGHS dual-simplex reference minimizes maximum vertex-prior error.
Every affine face obeys `-down_grade <= gradient·flow <= 0` and
`abs(gradient·cross) <= cross_grade`. Optional fixed vertex heights are equalities;
incompatible contacts return no mesh. These numerical example limits and spacing are
explicit experiment choices, not source facts or native LOD/error budgets.

One inferred direction per face is insufficient at bends: another source course can
cross that face in a different direction. Every positive-length source/face overlap adds
its own directed row `[face, dx, dy, minimum, maximum]`. Multiple rows may bind one face.
Outside-domain source lengths and their fixture indices remain explicit; they are not
silently treated as modelled water. The previous one-direction fit remains a negative
control in the real-input test. Spatial enumeration uses bounded batches before the row cap.

One shared vertex owns each height. Whole-face checks independently reconstruct gradients
from triangle edge equations; the LP certificate checks dual stationarity, signs and gap.
Optimality refers only to this declared finite LP. The shared weighted Clarabel fit remains
the researcher's integration candidate; this reference supplies an independent comparison.
The solver time cap excludes preparation and can overrun within a solver step; total pipeline
time and sparse row/nonzero counts are separate. Failure is explicit, with no relaxed pins.
Flow directions are inferred from descending source segments, not asserted OSM directions.
Source semantics, native float/frame conversion, cross-region contacts, other geographic
cases and complete native water/terrain/cache integration remain unproved.

Optional diagnostic (Matplotlib) uses identical axes, colours and declared vertical
exaggeration for all three models. It preserves source holes and shows the whole body:

```sh
PYTHONPATH=lib/src python3 test/experiments/water/render_river_surface.py \
  --input test/experiments/water/fixtures/feldkirch-ill-profile.npz \
  --surface /tmp/outshine-river-surface/surface.npz \
  --output /tmp/outshine-river-surface/comparison.png
```
