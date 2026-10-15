# Geographic candidate index for the species atlas

[#226](https://github.com/Weichwolf/outshine/issues/226) owns the compact atlas;
#25 owns actual botanical forms. This experiment compares exact membership in
one real botanical-country dataset, before binding ecological/species pools.
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

Five independent controls cover source-coordinate precision, holes, overlapping regions, exact boundary
points, poles/dateline, random points, a direct interior path requiring no
polygon predicate, candidate refusal before allocation and nested siblings
remaining filled under union, plus preserved source slivers at the dateline.
The full experiment adds every normalized source vertex,
spherical-uniform random coordinates and nine named real geographic controls;
alternating query order avoids always favouring one warm order. Preparation,
table construction, query times and peak process RSS are distinct observations.
Known geographic controls remain a separate gate: the source does not cover
the San Francisco point 37.77/−122.42, although its expected region is CAL.
That failed control is retained; Sacramento is an additional interior control.
The benchmark records timings/equivalence even with this source gap, then exits2.
Exact agreement with source polygons alone does not prove geographic coverage.

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  PYTHONPATH=test/experiments/vegetation \
  .venv/bin/python -W error -m unittest species_geo_checks
```

`species_geo_experiment.py --source <explicit-GeoJSON> --output <system-temp>`
runs the real comparison only in an idle protected host window from frozen
source. Until its terminal result is inspected, speed and memory advantages
remain unproven. No image/LOD/native/device acceptance follows from geography.
The retained architecture reference is Deussen et al., SIGGRAPH1998 §2/Fig.1
(`doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf`, pages3/4 also
opened optically): distribution and shared procedural forms are separate.
