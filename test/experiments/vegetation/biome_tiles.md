# Four flat Python vegetation research tiles (#208)

Laubwald, Mischwald, Steppe and Heide show physical plants produced by the
Python experiments. No DEM, OSM, native Place cameras, photographs or vendor
plant assets enter these four pictures. They are current research snapshots,
not accepted complete biomes or a native runtime comparison.

`biome_models.py` reuses the published `PlantPlan`, analytical axis/leaf surfaces
and `graminoid_tuft_mesh`. The broadleaf studies derive from the existing oak and
beech leaf/branch recipes; the mixed stand includes the existing cached young
*Pinus sylvestris* product. Those authored dimensions and enlarged demonstration
forms are not accepted mature allometry. The flowering dwarf shrub is a new
provisional heather form, without a validated species recipe. Steppe reuses the
generic graminoid family with dry colours and taller blades; seed heads and
species-specific steppe morphology remain missing.

These physical products retain wood, leaf/needle surfaces, UVs and unit corner
normals. Finite plant meshes are built once and instanced. There are no canopy
envelopes hiding missing foliage. The existing source-tile work and geographic
species sampling remain independent experiments; neither is needed to show
these plants. The population is reproducible, flat and spatially irregular,
using SciPy's Poisson disk sampler for forest roots. Metallic-roughness and
thin-leaf transmission use procedural colours/normal variation, no photo maps.

The first opened images exposed sparse, branch-dominated crowns, inadequate
continuous grass cover and an unfinished heather silhouette. The terminal-shoot
comparison [#295](broadleaf_crowns.md) improves the two broadleaf forms at matched
leaf area and is now used by `biome_models.py`. Dense grass, botanical crown
architecture and the heather silhouette remain generator deficits under #22/#1;
a successful render does not accept their art quality. Species and geographic work continue under #25/#226. Cached plant
subdivision is not yet selected from a measured screen-error bound; no frame,
LOD, botanical, memory-budget or device acceptance is claimed.

## Reproduce local comparisons

Use `.venv` with the existing vegetation requirements and installed `lib/`.
All private products, masters, receipts and logs use an explicit system-temp
path. `--conifer` points to the complete own experimental cached conifer product
with `whole.json`, mesh and placement files from `europe_conifer_products.py`.

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/biome_models.py --output /tmp/outshine-vegetation-biome-models \
  --conifer /tmp/outshine-vegetation-europe-conifers/pinus-sylvestris
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/render_biome_tiles.py --tile Laubwald \
  --products /tmp/outshine-vegetation-biome-models --output /tmp/outshine-vegetation-biome-shots
```

Repeat the render for the other three named tiles. Open all four local
1920×1080/four-sample masters. Completed process costs are recorded alongside
the image receipts as `<tile>-cost.json`; they are measurements, not a new
runtime budget. The publisher requires four successful process receipts and
reduces the masters to exactly1280×720 with Pillow's Lanczos filter:
`publish_biome_tiles.py --source /tmp/outshine-vegetation-biome-shots --output /tmp/outshine-vegetation-biome-comparison`.
All images remain local; only master publishes integrated Places. There is no server or auto reload.

Controls: `python -m unittest biome_tile_checks` with the same `PYTHONPATH`;
run the existing `test_graminoid.py` and `test_blades.py` library controls too.
They check complete reproducible flat populations, separate mixed/heath content,
positive scales, rigid poses, valid physical triangles and normal/UV layout.
Each local comparison image additionally needs optical inspection.

The procedural plant and instance separation follows the already studied local
SIGGRAPH1998 Deussen ecosystem paper. Local SpeedTree analysis is documented
under #24; these own products use no decompiled implementation or vendor assets.

## First completed snapshots, before #295

Frozen generator/renderer `533e71e571f00f66e28972eb06505e3491c75d64`:
all four processes exit0 on Blender5.2.2/EEVEE, llvmpipe software OpenGL,
1920×1080/four samples. Other agents were active (load roughly6–8); these are
measurements under that load, not an isolated device benchmark.

| Tile | Cached composition s | Rendering s | Process CPU-s | Peak RSS MiB | Instanced plant triangles |
|---|---:|---:|---:|---:|---:|
| Laubwald | 2.103 | 53.850 | 60.303 | 2645.48 | 2,778,840 |
| Mischwald | 6.693 | 63.365 | 82.067 | 2692.06 | 6,240,872 |
| Steppe | 0.148 | 33.294 | 40.068 | 2262.70 | 164,280 |
| Heide | 0.343 | 44.318 | 51.816 | 2327.20 | 2,948,800 |

All four initial masters and their then720p publications were opened. The final framing
retains complete crowns/tile bounds and a readable exposure. Those corrections
do not repair plant morphology or missing canopy/grass coverage. Masters,
receipts and per-process costs remain in
`/tmp/outshine-vegetation-biome-shots-bd84dcf4a/`; the own finite plant products
remain in `/tmp/outshine-vegetation-biome-models/`.

The current broadleaf products use terminal shoots at55m² authored leaf area
and eight blade segments. Full-model geometry and cache costs are intentionally
visible in [the matched-area comparison](broadleaf_crowns.md); these products
are offline references, not the selected forest render LOD. Current whole
Laubwald/Mischwald images and costs are linked there. Steppe/Heide are unchanged.
