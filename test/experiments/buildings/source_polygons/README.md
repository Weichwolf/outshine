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
