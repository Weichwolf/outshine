# Procedural stair-to-path T proposals (#1003)

The frozen native counterexample in `mvt_stair_t_junction.json` has a stair end
inside a path ribbon, but no upper prepared connection. This experiment adds a
small **2D construction proposal**, before profile, navigation, mesh or terrain
admission. It does not repair the recorded 1.63 m vertical disagreement.

Use the existing research `.venv` (Python, Shapely, Matplotlib):

```sh
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -m unittest test_mvt_t_connections -v
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python test/experiments/transport/compare_mvt_t_connections.py --output /tmp/outshine-stair-t-proposals
```

The report and inspected plot stay in the explicitly chosen temporary directory.
No product API, native implementation or global graph mutation is introduced.

## Construction and limits

Keep original axes and metadata. For explicitly connectable ground stair ends,
query nearby eligible ground foot-path **segments**, then intersect the outward
endpoint tangent with each candidate. The recipe caps forward extension at0.5m
and the target half-width; supplied positional error bounds additionally cap it
at their sum. Require crossing sine≥0.5 and one branch-half-width of through-path
on both sides of the target station. These are construction choices, not provider
facts or a universal network-recovery algorithm. Existing source frontiers remain
exact and unchanged (`lib/.../transport/frontiers.py`).

Explicit terminal/fragment/unknown endpoint roles, incompatible/unknown ground
access, levels, bridges, tunnels, parallel/behind-target geometry and ambiguous
targets do not produce extensions. Coincident original coordinates take priority
over speculative extensions, without claiming shared source-node identity.
Adjacent target segments at one identical vertex count once. A derived node
records both source parts, original endpoint, target segment/fraction/station and
recipe basis. Its coordinate is inserted into both constructed axes; every
original knot remains in order. Per-target/per-branch maps avoid an all-pairs
construction pass after spatial selection.

The portable fixture has no layer extent or provider generalization error bound,
and no endpoint-role provenance. Its adapter labels connectable ends, tunnel
absence and foot access as assumptions. `position_error_bound_m=None` remains
visible in the plan; do not infer a generalization guarantee from tile size or
quantization. Sensitivity reaches0/0.1/0.28m retain the coincident lower contact;
0.3/0.5/0.75m also propose the upper0.2824639606m extension. Native levels/bridge
states are supplied derived facts, not original OSM elevation evidence.

True untagged dead ends cannot in general be distinguished from missing junctions
by these lines alone. Endpoint roles must be qualified upstream; the assumption
is intentionally not promoted to source fact. The next step must jointly solve
both landings, whole profiles and cuts, preserving all three source lines and
their common consumer ports. Old height priors are soft, not mandatory targets.

## Verification and cost interpretation

Targeted tests independently check constructed-axis coincidence with Shapely,
both contacts, retained original knots, no input mutation, rotations/translations,
all listed countercases and refusal of incomplete dense candidate sets. Scalar
STRtree queries return at most the bounded number of input target segments.
Input/output and candidate caps bound work; the caps are cooperative, not hard
deadlines inside GEOS. Dense spatial overlap can still return many candidates;
exhaustion publishes no partially searched endpoint. Complete reference/index
searches agree; exhausted searches need not have identical throughput.

The comparison includes the portable three-part case, saved Vienna MVT paths and
32/128 translated copies of the three originals. Copies test locality/scaling,
not geographic coverage. The narrow Vienna adapter preserves supplied tags but
assumes missing width/level/access/roles. Its zero new proposals are not a proof
of a complete Vienna graph, and other transport classes are outside this recipe.

Fresh workers separately measure uninstrumented Python time, Python-traced peak
allocation and whole-worker peak RSS (including imports, both runs and GEOS).
Numeric Float64 coordinate payload and JSON sizes describe output/retention
cost, not measured native copies or IO. For this tiny case an index may cost more
than a scan. Device timings, total app RAM, native rendering and world-load gains
remain unmeasured; the PR owns actual numbers and frozen commit evidence.

## Method sources

Chen et al., SIGGRAPH2008, [local paper](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
p.2/Fig.2 (visually inspected) separates street graph from generated geometry.
Existing stair junction/landing experiments already consume explicit contacts;
they do not discover this missing generalized-MVT endpoint connection.
This tangent/width admission recipe is our limited construction, not the paper's
tensor-field street algorithm.

[Shapely2.1.2 STRtree](https://shapely.readthedocs.io/en/2.1.2/strtree.html)
documents immutable2D bounding-box queries and returned input indices. All
candidates, rather than one potentially ambiguous nearest result, are checked.
The [MVT specification](https://mapbox.github.io/vector-tile-spec/) defines tile
geometry/attributes, and explicitly leaves clipping and simplification outside
its scope; it supplies no missing endpoint intent or error guarantee for us.
