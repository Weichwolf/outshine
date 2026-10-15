# Geographic candidate index for the species atlas

[#226](https://github.com/Weichwolf/outshine/issues/226) owns the compact atlas;
#25 owns actual botanical forms. This experiment compares exact membership in
one real botanical-country dataset and a geometry-free regional candidate
lookup, before binding ecological/species pools.
It does not identify individual plants or establish a worldwide species catalog.

The geographic cells follow existing `include/world/data/GeoCellId.h`: level L
partitions longitude −180..180 and latitude −90..90 into 2^L cells per axis.
Point queries canonicalize +180 to −180; the north pole uses the last row.
There is no Mercator latitude cutoff. All matching source memberships survive
overlap/shared-boundary queries in deterministic query/region order.

`DirectRegions` uses the established Shapely/GEOS STRtree and exact polygon
predicates. `CellRegions` adds a uint16 table (128 KiB at level8). Empty cells
return no region. A sole polygon covering an entire cell gives a direct region
ID. Every other intersecting cell refers to an interned candidate list; actual
point predicates still decide membership. Source geometry remains shared, and
its storage plus Python/GEOS overhead must be measured separately from arrays.
Candidate counts are checked before pair allocation. This is a comparison,
not a decision to ship a second spatial system or a new runtime asset ABI.

Primary research input: [TDWG WGSRPD](https://github.com/tdwg/wgsrpd),
`geojson/level3.geojson` at revision
`52da7828aba9d461dd133c27b3bd7a4407161f54` (369 records, 1,846,370 bytes).
The explicit source/receipt stays under
`/var/tmp/outshine-vegetation-atlas-inputs/`. Dataset redistribution terms remain
to be pinned before a public data delivery; no source or derived map is bundled.
Botanical countries do not provide local habitats, elevations or climate.

The real source contains 19 invalid MultiPolygons with nested sibling shells.
Default reading refuses them. The explicit `union_multipart=True` research
option unions only individually valid sibling polygons as sets, retains the
original source and reports every changed code/reason/degree-area sum. It does
not reinterpret nested siblings as holes or apply a generic silent repair.
Both alternatives use the same normalized source. Other invalid geometry still
refuses. The source has no duplicate region codes.

`MAG` also extends to longitude −180.000026°. Default reading refuses this;
`wrap_longitude=True` explicitly splits periodic copies at the world meridian,
retaining the small sliver on the opposite side instead of clipping it away.
The source remains unchanged; longitude limits and degree-area sums are recorded.
Together these produce 20 declared input normalizations. Point queries preserve
all in-domain coordinate bits and use ordered cell-edge comparisons: adding
180 before floor can otherwise erase a tiny offset at an internal cell edge.

Six independent controls cover source-coordinate precision, holes, overlapping regions, exact boundary
points, poles/dateline, random points, a direct interior path requiring no
polygon predicate, candidate refusal before allocation and nested siblings
remaining filled under union, plus preserved source slivers at the dateline.
The full experiment adds every normalized source vertex,
spherical-uniform random coordinates and nine named real geographic controls;
alternating query order avoids always favouring one warm order. Preparation,
table construction, query times and peak process RSS are distinct observations.
Known geographic controls remain a separate gate: the source does not cover
San Francisco 37.77/−122.42 (expected CAL) or Hobart −42.88/147.32 (expected TAS).
Both failed controls are retained; Sacramento is an additional interior control.
The benchmark records timings/equivalence even with these source gaps, then exits2.
Exact agreement with source polygons alone does not prove geographic coverage.

The second, separate quality contract returns all regions intersecting the
containing closed GeoCell as **regional candidates**, rather than claiming an
exact country at the point. `DirectRegions.query_cells` computes those same
candidates through the source STRtree. `species_pool_lookup.candidate_pairs`
uses only the compiled numeric tables, with NumPy and no geometry. Both outputs
must match exactly on the same corpus. This prior can retain a coastal region
candidate despite a coarse polygon's point gap; it still needs local habitat,
climate/height filtering, transition weights and actual species recipes.
Its cost is compared against the same cell-candidate task, separately from
the exact-point comparison. Both original failed geographic controls stay red.
The numeric experiment layout is not yet a public/untrusted asset decoder.

## Completed comparison

Frozen commit `976d78f5b9f8af39da48d32d36b1a23385885b07`, source SHA256
`c172bcf6aba20e19477adc60aebf0023068f0175dca8480f0760b090dcf64840`.
The 64,420-coordinate corpus contains all normalized source vertices and
20,000 spherical-uniform random points (seed17). Medians of six alternating
rounds on the same corpus and normalized geometry:

| Identical quality contract | Direct STRtree | Compiled cells |
|---|---:|---:|
| Exact point membership, boundary polygons retained | 254.260 ms | 121.122 ms |
| All regions intersecting containing closed GeoCell | 1,028.911 ms | 10.216 ms |

Both result-pair arrays agree exactly with their respective direct reference.
The second row uses no geometry in its compiled query; it does not claim exact
point membership. It finds the expected regional candidate in all nine named
controls, including both coarse-coast source gaps. At most five candidate
regions occur per query in this corpus; that is an observation, not a bound.

Level8 uses 65,536 cells: 40,102 empty, 15,378 direct interior and 10,056 boundary
cells. The table occupies 131,072 bytes and the 1,102 shared candidate lists
and offsets another 8,984 bytes: **140,056 bytes total**, excluding region names,
species metadata, ecological fields and recipes. Reading/normalization took
0.362 s; index construction 1.193 s. Whole-process peak RSS was 95.56 MiB,
including GEOS/source geometry and both comparison implementations; this is
not the runtime memory of the numeric index. Concurrent host load was not
recorded, so these observations do not establish isolated/device costs.

The complete runner ended with exit2 because the separate strict geographic
control gate remains red (7/9), while both equivalence gates and the regional
candidate controls passed. Reports and numeric research arrays are retained
at `/var/tmp/outshine-vegetation-atlas-976d78f5b/`; public delivery contains code
and this compact evidence, not the source or derived geographic map.

## Intended species selection

Compile geography and species-region links offline. At generation time, query
the geographic prior once per homogeneous stand/environment patch, merge its
small candidate pools, then weight species by altitude, long-term climate and
local land cover. Urban/cultivated pools may differ from native-range pools.
Prepare the resulting sampler once per patch and vegetation layer; select
with stable site identities independently of camera, render order and LOD.
Persist these choices in the existing complete stand/asset plan.

Cells accelerate candidate discovery; they must not define visible vegetation
borders. Ecological transition weights need spatially continuous support and
candidate unions across neighbouring patches before sampling. Abrupt declared
land-use changes may remain abrupt. Missing data and procedural assumptions
remain distinguishable. Neither transition quality nor species selection is
implemented or accepted by this geographic experiment.

Species metadata, recipe families and finite reusable form variants belong
in separate shared tables. A plant instance stores species/form IDs, seed,
transform and state rather than its own duplicate complete geometry/textures.
Far products and demand-generated near organs derive from the same plan;
instantaneous weather changes appearance, not the established species mix.

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  PYTHONPATH=test/experiments/vegetation \
  .venv/bin/python -W error -m unittest species_geo_checks
```

`species_geo_experiment.py --source <explicit-GeoJSON> --output <system-temp>`
runs the real comparison from frozen source with an explicit temporary output
directory. Choose concurrency from available resources and report actual load;
there is no global host lock. No image/LOD/native/device acceptance follows
from geography. Dataset rights, ecological weighting, actual species/recipes,
source/wheel library delivery and integration remain open under #226/#25.
The retained architecture reference is Deussen et al., SIGGRAPH1998 §2/Fig.1
(`doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf`, pages3/4 also
opened optically): distribution and shared procedural forms are separate.
