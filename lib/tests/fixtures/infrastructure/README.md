# Worldwide infrastructure corpus

[`survey.csv`](survey.csv) fixes 150 survey windows across six inhabited
continents. Two cases per window give each continent 40 development cases and 10
holdouts. Holdout windows are reserved for final acceptance. Catalog preparation
uses source observations to confirm the required feature families and pin the
tiles. Development renders start with the Freudenau crossing.

The manifest covers 15 infrastructure families, each represented in both
splits. It pins 474 vector tiles and 159 elevation tiles. The nearest
development and holdout cases are approximately 83 km apart.

[`cases.json`](cases.json) records the selected OSM evidence, crop bounds, expected
route legs and check families. The source receipts pin the vector survey and
the local elevation tiles. Each view has coordinates, heading, eye height,
pitch and a fixed local-daylight time for the case. A location's preferred
family in the survey and its selected family can differ. The evidence describes
the delivered geometry and identifies source gaps for later checks.

A route leg lists its source family, start and end as latitude/longitude,
followed by the delivered one-way, access and layer values. Endpoints for
compound junctions are on the source approaches. An at-grade rail crossing
requires a shared source vertex with two railside legs and two roadside legs.
Separate modes keep separate connectivity. Links are acceptance requirements
for the generator, with restrictions carried by the source tags.

Regenerate and compare the pinned manifest from the repository root.

```sh
outshine_corpus_tmp=$(mktemp -d "${TMPDIR:-/tmp}/outshine-corpus-verify.XXXXXXXX")
PYTHONPATH=lib build/blender-python/bin/python -m lib.tests.fixtures.infrastructure.catalog \
  --output "$outshine_corpus_tmp/cases.json"
cmp lib/tests/fixtures/infrastructure/cases.json "$outshine_corpus_tmp/cases.json"
PYTHONPATH=lib build/blender-python/bin/python -m lib.tests.fixtures.infrastructure.inventory \
  lib/tests/fixtures/infrastructure/cases.json --output "$outshine_corpus_tmp/layers.json"
cmp lib/tests/fixtures/infrastructure/layers.json "$outshine_corpus_tmp/layers.json"
```

[`layers.json`](layers.json) counts delivered class values and keys in the
pinned vector tiles. Counts include buffered tile duplicates. The owning
issues define which values affect geometry, layout, access, materials or labels.
The corpus and checked output are tracked under [#2215](https://github.com/Weichwolf/outshine/issues/2215)
and [#2208](https://github.com/Weichwolf/outshine/issues/2208).

OSM data © OpenStreetMap contributors, distributed by OpenFreeMap. Elevation
tiles are from Mapterhorn.
