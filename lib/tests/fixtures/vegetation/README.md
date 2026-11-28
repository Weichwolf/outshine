# Vegetation pilot inputs

`pilot.json` records the development sites, cameras, photographic references
and habitat checks. Use the Python image driver with `--vegetation-site` to
render a site's camera set.

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
manifest.

`outshine.vegetation.atlas.VegetationAtlas` opens the output under
`build/vegetation-atlas/pilot/`. Use `sample(latitude, longitude)` for the
source layers or `sample_local(scene.source, xy)` to include mapped cover in a
Python reference scene. Each tile stores the input and output digests in
`atlas.json`.

The WorldCover grid uses geographic cells at 1/12,000 degree spacing.
SoilGrids fields retain their 250 m source resolution and provide a coarse
substrate prior. Source gaps return empty fields. Botanical regions provide
candidate species from the packaged WCVP catalogue. Plant abundance and
actual presence require site-level evidence.
