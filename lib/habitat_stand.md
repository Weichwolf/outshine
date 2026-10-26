# Source-bound stand parents

`outshine_ref.vegetation.StandHabitat`, `HabitatStand` and `StandCell` provide
the Python parent measure requested in [#382](https://github.com/Weichwolf/outshine/issues/382).
They preserve source support and integer stock before any individual tree is made.
Native region selection, finished forest products and engine integration belong to master.

```python
from outshine_ref import LocalFrame
from outshine_ref.vegetation import HabitatStand, StandHabitat
from shapely.geometry import box

habitat = StandHabitat.prepare(
    LocalFrame("example-east-north-up"), "provider:wood:123", "root:8m",
    box(0, 0, 8, 8), (0, 0), 8,
)
stand = HabitatStand(
    habitat, ("broadleaf", "conifer"), (120, 80),
    (18.0, 22.0), (6.0, 5.0), levels=3,
)
parent = stand.field()                 # one cell; zero sites, trees or organs
children = stand.children()            # four area-conditioned count records
near = stand.sites((3, 2, 5))          # only this canonical leaf, or None on capacity
saved = stand.recipe()                 # prepared cells; finished arrays are separate
restored = HabitatStand.from_recipe(saved)
```

## Spatial and identity contract

All geometry is finite, valid, two-dimensional Polygon/MultiPolygon in one explicit
shared `LocalFrame`, measured in metres. `prepare` retains the original source
geometry/area and normalizes/clips effective support once to a caller-chosen square
root. Optional `support=` is a subset already resolved against transport/buildings;
height-aware reservations, ground height and terrain adaptation remain caller-owned.
An asset read uses `from_recipe`, avoiding normalization and repeated source clipping.

Kinds are sorted unique identifiers, with explicit nonnegative integer root counts.
Counts, heights and crown spreads are authored descriptors, not inferred botanical
observations. Shared provider IDs remain distinguishable from original OSM element IDs.
The caller owns canonical root selection, tile ownership, overlap policy and source
deduplication. There is no automatic worldwide region selection in this class.

`layout_identity` binds source/domain, frame, region, counts, kinds, canonical depth,
seed, format and NumPy/GEOS versions. Full `identity` additionally binds physical size
descriptors. Size edits retain site XY/IDs; count/kind/layout changes create a new layout.
The complete site namespace is `(layout_identity, ids_row)`. Rows are five UInt64
values: level, x, y, kind ordinal, ordinal within that leaf/kind. Positions are Nx2
Float64 metres; ground Z is deliberately absent. Arrays have immutable backing bytes.

## Parent measure and bounded demand

For each split, clip the parent domain to its four child squares. Areas partition the
parent, including holes/disconnected components. Given fixed count N of one kind and
child areas A_i, sample `Multinomial(N, A_i / sum(A_i))`. This is the count marginal of
N uniform points partitioned by area. Recursion conserves exact per-kind integer
counts at every parent and does not enumerate those points. Empty children get zero.
Per-cell keyed randomness makes requests, saved replay and subset order consistent.
This is an own procedure, not a recovered SpeedTree population algorithm.

Only a requested canonical leaf calls constrained Delaunay triangulation, chooses
triangles by area and samples square-root barycentric coordinates. There is no
unbounded rejection loop. This is independent uniform sampling, with no minimum
tree spacing, competition or ecological succession guarantee. `GroundCoverPlan`
remains a separate fine-scale Poisson procedure.

`field(key)` returns cached immutable count/support descriptors, weighted mean and
maximum declared height, conservative XY cell bounds enlarged by half maximum spread,
and `cover_estimate = 1-exp(-sum(N*pi*(spread/2)^2)/area)`. That overlapping-crown
estimate is neither certified canopy coverage nor a radiance/shadow/LOD error bound.
There are no children below the requested field, sites, tree models or organs.

`max_cells` preflights all four siblings along the requested path. Geometry payload
capacity is checked before publishing a split or drawing counts; site capacities
precede sampling. On capacity, demand returns `None`; already prepared ancestors
may remain. Defaults: 4096 retained cells, 1,000,000 retained sites, 16 MiB geometry
WKB payload; `sites` also defaults to 100,000 requested sites and 32,768 triangles.
WKB/array payload bounds do not bound GEOS scratch, Python allocation or process RSS.
The lazy mutable caches require a single owner; the plan is not thread-safe.

## Persistence and evidence

`recipe()` stores prepared non-root cells with counts and clipped WKB, source/root
and versioned identities. Restore validates capacities, geometry, area, parent counts
and complete sibling sets. It neither splits cells nor draws randomness. This is
trusted versioned asset input, not a general hostile-file parser. Finished field,
mesh and site arrays are separate products: load those directly on a finished hit.
Unrequested descendants/near sites can subsequently be derived from the same plan.
JSON recipe restoration alone is not a complete native forest cache contract.

The local SIGGRAPH reference [Deussen et al. 1998](../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf),
PDF page 4, was optically inspected: distribution and reusable plant geometry are
separate stages. It motivates that separation, not the exact hierarchy above.

`test/experiments/vegetation/` contains the executable consumers:

- `stand_source_cases.py`: extract three pinned real local MVT cases to the fixed
  metric fixture `lib/tests/data/habitat-stand-cases.geojson`, preserving provenance.
- `habitat_stand_experiment.py`: root/level-3 fields without sites, then one near leaf;
  saves prepared recipes, ready numeric products and diagnostic support geometry.
- `stand_feldkirch.py`: master's shared 37-record input under
  `test/outshine/data/vegetation/feldkirch-habitats.json`; original provider IDs and
  five holes remain. Clip tile buffers to each owner before metric transformation;
  repeated IDs use distinct owner regions. All owned domains have no positive-area
  overlap. Its affine Mercator/camera-latitude scale is an explicit local metre
  approximation, separate from the three-case fixture's ENU projection.
- `habitat_stand_cost.py`: alternating parent fields versus enumerating all canonical
  sites for exactly the same support/recipe and per-cell counts; population scaling,
  traced live allocations/peak and restore with split/RNG/clip/normalize forbidden.
- `habitat_stand_replay.py`: fresh offline ready-array read, prepared hierarchy replay
  with those operations forbidden, then byte-exact one-leaf XY/ID regeneration.
- `render_habitat_stand.py`: Blender consumer of saved support geometry only; all
  output paths are explicit system-temp directories. These flat, constant-height
  support bodies verify source geometry, not natural forest appearance.

Focused source/fresh-wheel controls cover real polygons, synthetic holes, thin and
disconnected support, triangle area/centroid sampling, exact count conservation,
immutable IDs, changed size traits, request order, corrupted replay and early budgets.
Measurements, commands, frozen commits and optically opened 1920×1080 diagnostics
are linked in the delivery PR. No native gate, general LOD/wind admission, mature
species approval or device performance follows from these Python proofs.
