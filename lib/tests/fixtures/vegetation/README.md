# Vegetation corpus inputs

`pilot.json` and `development.json` record the development sites, cameras,
photographic references and habitat checks. Use the Python image driver with
`--vegetation-site` to render a site's camera set. A reference photo's
`sha256` refers to the original image linked from its source page. Its
`camera` records the photographer's pose where available.

`holdouts.json` reserves ten independent site anchors per continent. The
coordinates are the World Heritage property reference points published by
UNESCO. Each anchor still needs walkable poses, input digests and dated
visual evidence before it can be evaluated. Use the reserved sites for final
evaluation. If a reserved site informs recipe tuning, keep its verdict and
qualify a comparable replacement from the same continent. The selected
coordinates and labels were adapted from the World Heritage List under
CC BY-SA 4.0. The manifest lists the dataset and source record identifiers.
[UNESCO World Heritage List](https://data.unesco.org/explore/assets/whc001/)
· [Data license](https://creativecommons.org/licenses/by-sa/4.0/)

## Build the atlas

Install the optional atlas tools into the Python environment used for the
reference library. Run these commands at the repository root.

```sh
nice -n 10 build/blender-python/bin/python -m pip install -e './lib[render,atlas]'
nice -n 10 build/blender-python/bin/python -m lib.tests.fixtures.vegetation.build_atlas \
  --fetch --check-pins lib/tests/fixtures/vegetation/atlas-pilot-pins.json
```

The builder retains inputs under `build/vegetation-atlas/sources/`. Run the
second command without `--fetch` to rebuild from the retained inputs. Use
`--output` to write a separate atlas and `--at LAT LON` to build around a
different location. Repeated `--at` values build all selected windows in one
manifest. Use `--site` to select individual captures by ID. Use
`--development` to build all captures from `development.json`.

```sh
nice -n 10 build/blender-python/bin/python -m lib.tests.fixtures.vegetation.build_atlas \
  --development \
  --check-pins lib/tests/fixtures/vegetation/atlas-development-pins.json
```

Add `--fetch` when populating the source cache. Use
`--vegetation-atlas build/vegetation-atlas/development` to render these sites
with the selected atlas.

`outshine.vegetation.atlas.VegetationAtlas` opens the output under
`build/vegetation-atlas/pilot/`. Use `sample(latitude, longitude)` for the
source layers or `sample_local(scene.source, xy)` to include mapped cover in a
Python reference scene. Each tile stores the input and output digests in
`atlas.json`.

Pass `--vegetation-atlas build/vegetation-atlas/pilot` to the image driver to
enable atlas-driven open-habitat vegetation. The Python `render_png` entry
point takes the same directory as `vegetation_atlas`.

The WorldCover grid uses geographic cells at 1/12,000 degree spacing.
SoilGrids fields retain their 250 m source resolution and provide a coarse
substrate prior. Source gaps return empty fields. Botanical regions provide
candidate species from the packaged WCVP catalogue. Plant abundance and
actual presence require site-level evidence.

In Chile the builder also joins the 2017 vegetation belts of Luebert and
Pliscoff. `potential_community` reports that regional source separately from
mapped or satellite-observed cover. It is empty outside the supplied layer.
[Geoportal de Chile](https://geoportal.cl/geoportal/catalog/32581/Pisos%20vegetacionales%20de%20Luebert%20y%20Pliscoff%202017)

The Hidden Valley windows also include the park's observed vegetation
associations and geological units. `observed_vegetation` reports the mapped
cover and height classes. `geology` identifies the mapped parent material.
Both sources describe areas and leave individual plant and boulder positions
to the procedural recipes.
[NPS geology](https://irma.nps.gov/DataStore/Reference/Profile/2209598) ·
[Park vegetation inventory](https://www.nps.gov/im/vmi-jotr.htm)

`shore-substrates.json` associates a mapped water body with documented
mineral shore material. The reference uses the solved shoreline contact for
moisture, deposits and the adjacent plant exclusion.
