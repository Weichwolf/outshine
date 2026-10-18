# River profile experiment (#760)

Compare flat fragment levels, nearest-course projection and normalized compact blends
of local linear course profiles. This is an experimental height field, not a production
river solver. NumPy and Shapely/GEOS perform the array and spatial-index work.

```sh
python3 -m unittest discover -s test/experiments/water
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

Cost: at most 8192 input segments; queries in batches of 64. The spatial query enumerates
all local candidates before checking the per-point reference budget. Report that work;
do not claim the query traversal itself is capped by that budget. Overloaded points return
NaN explicitly; independent points continue. A native recipe must select valid local pieces.

Missing coverage, inconsistent branch heights, lake/sea separation, source bridges/tunnels,
profile-conforming water triangulation, terrain contacts and spatial error bounds remain open.
The experiment does not change PNG pins, native caches or engine gates. See #760 for real
inputs, comparisons and the integration decision; OMT source semantics are in
`doc/references/data/osm/openmaptiles-water{,way}-schema.yaml`.
