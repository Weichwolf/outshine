# Native source-membership counterexample

The NPZ contains exact native binary64 WKB: 48 selected source polygons, their
GEOS union and one densified constrained-Delaunay triangle. `source_indices`
maps collection order to the original native surface indices. No input is
repaired, rounded or relabelled. This is a diagnostic, not an accepted topology.

On GEOS 3.14.1 every individual source/triangle intersection has zero area.
The same triangle intersects the captured union by about 7.83084e-7 m².
Its shortest edge is about 1.32212e-7 m. Thus rejecting the whole source group
loses valid independent water surfaces; silently dropping the triangle also
cannot claim that the computed union intersection was empty.

The gzip retains the complete original WATER4 payload and source metadata.
It also contains historical prepared individual facets, which are **not** an
oracle for the new coupled model. The JSON records hashes, the package address
and the native reproduction commit. The source arrays were acquired previously
from OpenFreeMap; reproducing this case requires no network or camera rendering.

```sh
python test/experiments/water/inspect_source_membership.py --output /tmp/outshine-water-membership/report.json
```

NumPy and Shapely are the existing experiment dependencies. The report records
the active GEOS version because an alternative version may resolve the numerical
inconsistency differently. A robust replacement must preserve original sources,
islands and contacts, bound any geometric change and qualify more than this case.
The owner is #970, parent #760. Canonical domain/contact selection is a separate contract in #963.
