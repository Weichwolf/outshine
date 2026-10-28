# Buffered building polygon reference

This Python reference investigates the source defect in
[#1578](https://github.com/Weichwolf/outshine/issues/1578). A vector feature can
contain hundreds of separate building exteriors. Its provider feature ID is
container provenance, not an individual building ID. Original OSM object identity
is unavailable in these inputs; the reference does not invent it.

The fixed inputs preserve all 2853 exterior observations, their holes, source
namespaces, attribute hashes and input digests from two native vector products.
`inputs/manifest.json` records the native library commit, private exporter hash, asset keys, tile
addresses and checked export hashes. These are preserved historical inputs,
not downloads from an assumed current provider revision. Geometry is from
[OpenFreeMap](https://openfreemap.org/), with
[OpenStreetMap contributor attribution](https://www.openstreetmap.org/copyright).

The two original MVT bodies are also fixed under `inputs/*.pbf`. Their SHA256
values equal the historical JSON exports' `source_input_digest`; the manifest
records byte counts and the cached provider declaration. That declaration does
not independently attest the historical upstream revision or generator binary.
The inputs and existing attribution travel with the repository; no private
cache or network download is needed.

A standalone native counter passes both original bodies together through the
public `OsmField::Accept` and `PrepareBuildingInputs` APIs. Use a normal native
build configured with `BUILD_TESTING=ON`, then run from the repository root:

```sh
cmake --build build/review --target test_harness_building_source_counter --parallel
build/review/test_harness_building_source_counter \
  test/experiments/buildings/source_polygons/inputs/tile-14-8744-5704.pbf \
  test/experiments/buildings/source_polygons/inputs/tile-14-8744-5705.pbf
```

The JSON result currently contains two prepared structures: one whole footprint
and its buffered cut fragment, with two original observations and two original
controls. Exit **1** reports the unresolved #1578 requirement: one whole footprint
and no cut duplicate. Exit **0** is reserved for that requirement with unchanged
input controls, heights and unknown original OSM IDs; this counter invents no
source identity. It is not registered with CTest, has no expected-failure flag
and does not turn the product defect into a passing assertion. It does not
qualify provider calibration, canonical dependency/cache binding or atomic
World publication. Other exits are 2 for usage, 3 for a missing input, 4 for a
decode refusal, 5 for wrong original bytes and 6 for changed fixture controls.

Use Python with the existing buildings dependency `shapely==2.2.0` installed.
From the repository root:

```sh
python test/experiments/buildings/source_polygons/check_reference.py
python test/experiments/buildings/source_polygons/benchmark_reference.py --output /tmp/outshine-source-polygons.json
```

The checks optionally accept `--output /tmp/outshine-source-polygons-checks.json`.
Neither command updates fixed inputs or expected results. The benchmark requires
an explicit result path and uses every CPU available through process affinity.
It compares complete outputs, including unresolved cases, across serial ABBA
runs and a process pool. Input loading, component construction and resolution
are reported separately. Worker CPU time and elapsed time are separate; traced
Python heap excludes GEOS and is not physical peak memory. Timing is a local
procedure comparison, not an engine or target-device budget result.

## Procedure and boundaries

Coordinates are inverted onto the known source grid with an explicit floating
point ULP check. The observed profile has extent 4096 and buffer 64; this does not
establish a universal provider declaration. Source namespaces include provider,
dataset, revision, layer and zoom. Upstream admission must reject mixed snapshots.

Positive spatial overlap across source tiles forms candidate components.
Different co-observed shapes are not directly joined just because they share a
container or overlap. Transitive components can still contain multiple
co-observed shapes and must pass the same geometry checks; grouping alone never
certifies identity. Attribute conflicts and incompatible whole polygons remain
unresolved. Container IDs and ring ordinals are retained as provenance.

One procedure first uses an available complete polygon. The other first tries
to reconstruct the outline from original corners with exact original-edge
support, then falls back to the same complete witness when necessary. They are
two execution orders of the same qualification, not independent global proofs.
Neither unions incomplete fragments into a newly invented building. Holes stay
part of the checked polygon. Every fragment must stay within the candidate's
coordinate envelope, including points on clipping boundaries. Clipping a common
source geometry on the same grid cannot enlarge its coordinate range. This is
an exact necessary bound, not a complete certificate for quantized cut edges.
Canonical winding, cyclic ring starts and hole
ordering determine a geometry digest; source tile/input digests independently
determine its dependency digest.

The controls cover the actual clipped/full Rosenheim pair, distinct houses
sharing a container, different container IDs, revision changes, ring ordering,
conflicts, crossing footprints, courtyards, missing neighbours and incompatible
cut points outside a complete footprint's envelope. In the saved
whole batch the current procedure yields 2663 candidates in 2759 components and
leaves 96 unresolved. This is not worldwide coverage or a dateline test. Missing
original corners, quantized diagonals and ambiguous overlaps still require
qualified inputs or another reconstruction procedure.

## Closed observation counts

`python3 check_closed_observations.py` compares proposed geometry multiplicity
against the complete manifest-checked observation catalogs. For the original
Rosenheim pair, one shared rational whole predicts the two recorded rings. A
second, separate cut geometry predicts four observations: the first tile would
need two copies of its cut ring instead of one, and the second would need a cut
ring absent from its complete catalog. The controls also retain two actual
distinct buildings with equal attributes and preserve repeated observations.

This is a conditional counterexample, not a general reconstruction certificate.
The declared model counts convex, simple, hole-free geometry **after** provider
merging, then clips and rounds without further simplification, dropout, attribute
changes or repair. Its applicability to the historical provider is unproven.
Unknown neighboring inputs remain unclosed; holed and concave candidates are
outside this model. A supported proposal fits the supplied observation counts;
it does not prove uniqueness, original OSM identity or World-ready publication.

## Native integration remains open

These results are geometry candidates, not World-ready products. Existing
needed coverage must survive until a complete qualified replacement is ready.
Source/cache request aliases, content keys and current job validation must all
include the canonical meaning and dependencies. Filtering only
`PrepareBuildingInputs` cannot invalidate warm products: `SelectNativeBasis`
can return a cached basis before that function runs. Terrain/transport contacts
and publication must consume the same selected geometry. Canonical ring order
also changes the current first-vertex building seed and requires an explicit
recipe version and fresh native image qualification. No native fix, movement,
frame-budget or whole-issue acceptance follows from this experiment.

## Conditional whole-polygon witnesses

`clip_round_witness.py` checks a stronger, explicitly conditional model: one
simple rational polygon, with shared corners, is clipped to every observation's
window and then rounded with `floor(x + 1/2)`. Its entire resulting rings must
match the observations up to cyclic rotation and winding reversal. Exact fractions and
segment predicates certify each positive result; separate edge witnesses alone do
not establish consistent shared corners. Each half-plane intersection and its
rounded result must retain a simple boundary. Concave outlines are supported;
holes, disconnected intersections and topology repair remain outside this model.
An intersection that splits is unresolved, never joined by an invented bridge.
A failed bounded search means unresolved, not
proven infeasible. Neither canonical identity nor the existing `resolve` changes.

```sh
python test/experiments/buildings/source_polygons/check_clip_round_witness.py --output /tmp/outshine-clip-checks.json
python test/experiments/buildings/source_polygons/benchmark_clip_round_witness.py --output /tmp/outshine-clip-witnesses.json
```

The original integer whole outline does not reproduce its rounded clipped
neighbour. Moving the two right-side original corners by exactly one quarter
grid unit in x gives a single rational polygon that reproduces both saved rings.
The checker verifies this complete witness, a declared incompatible cut mutation
inside the complete envelope, half-open midpoint rules and invalid star topology.
Additional controls cover concave clipping, disconnected and tangent results,
backtracking, repeated corners and segment contacts. Four hundred seeded rational
rings compare exact simplicity against GEOS validity on exactly representable
input coordinates; their 18 valid rings and 382 invalid rings agree.
Independent GEOS clipping agrees on the rounded output; floating intersections
are not claimed equal to arbitrary exact rational intersections.

The batch audit includes all 32 existing candidates with clipped observations:
all 32 have conditional global witnesses, including the nine concave cases
previously outside the convex model. Complete outputs must agree across two serial runs and a process pool
using every CPU available through affinity. It does not replace the full-batch
benchmark above or authorize dropping the unsupported source geometry.

Primary implementation context is versioned, not a claim about the historical
deployment: the [OpenFreeMap installer of 2026-09-13](https://github.com/hyperknot/openfreemap/blob/ac7bbbde608c5db67d3aa37e1ea07cb72067dc77/tilegen/deploy_tilegen/install_planetiler.py)
pins Planetiler `546486f63`. Its [FeatureRenderer](https://github.com/onthegomap/planetiler/blob/546486f63c00d56b9f697b5c4659aa40e30e6e5c/planetiler-core/src/main/java/com/onthegomap/planetiler/render/FeatureRenderer.java)
simplifies before clipping and invokes [GeoUtils.snapAndFixPolygon](https://github.com/onthegomap/planetiler/blob/546486f63c00d56b9f697b5c4659aa40e30e6e5c/planetiler-core/src/main/java/com/onthegomap/planetiler/geo/GeoUtils.java)
after clipping. That implementation may enter topology-repair branches; this
experiment models pointwise rounding with simple connected boundaries. The pinned submodule's
[Building layer](https://github.com/openmaptiles/planetiler-openmaptiles/blob/373030bce912550e426683eda60812e35048f461/src/main/java/org/openmaptiles/layers/Building.java)
merges polygons into feature containers at zoom 14. Neither that source history
nor a compatible rational witness proves which binary generated the saved
tiles. Provider calibration, general polygon admission, neighbour dependencies
and native publication remain open in #1578.
