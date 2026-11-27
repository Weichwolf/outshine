# Vienna pedestrian references

Run from the repository root with Blender 5.3 on the development Mac.
Install the reference library with the Python shipped with Blender so binary wheels match.

```sh
/Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 -m venv --system-site-packages build/blender-python
build/blender-python/bin/python -m pip install './lib[places,transport]' astral==3.2
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- --place Adlerhof --repeat
```

The measured interval includes rendering and PNG output. Source acquisition, scene construction and
Blender startup are separate. Run with `--engine cycles --output /tmp/outshine-vienna-light` for
an independent lighting comparison. Inputs are genuine ZXY tiles pinned in sources.json.
The seed, camera, clear sky and UTC are in places.json. The final eye position follows a vertical
intersection with the rendered support mesh.

Omit `--place Adlerhof` to render the twelve pedestrian views in catalog order. Use
`--place Wien` for the historical reference. With `--repeat`, the
second interval measures the warm image, including PNG encoding and writing. Allow up to five
seconds at 1280×720 for this reference workflow.

## Near material comparisons

Adlerhof and Adlerhof-Near show the same facade and paving at eye height. Dechantlacke-Path
and Dechantlacke-Path-Near look down the mapped path past the same exposed roots. They use
the anchor coordinates, the offsets and bearings in places.json, and the same clock and
seed. The additional views are selected explicitly so the twelve-view run keeps its order.

```sh
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place Adlerhof --place Adlerhof-Near --place Dechantlacke-Path --place Dechantlacke-Path-Near \
  --repeat --output build/shots/1962/eevee
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place Adlerhof --place Adlerhof-Near --place Dechantlacke-Path --place Dechantlacke-Path-Near \
  --engine cycles --output build/shots/1962/cycles
build/blender-python/bin/python test/experiments/vienna/glsl_render.py \
  --place Adlerhof --place Adlerhof-Near --place Dechantlacke-Path --place Dechantlacke-Path-Near \
  --output build/shots/1962/glsl
```

Both paths take scene-linear colours from materials.py and call ground_detail.py to bake
the same seeded RGBA table. Red stores half the reflectance multiplier, green stores
perceptual roughness, and blue stores relief as `0.5 + height_m / 0.024`. Alpha carries the
macro variation. The detail covers 2.56 m at 5 mm per texel, and macro tint uses a 23 m
period. The nominal stone face spacing is 213 × 128 mm. Glazing uses a 1.52 IOR over a dark
opaque backing to stand in for unmodelled interiors.

The Adlerhof photo guides the plaster and rectangular paving. The Dechantlacke photo
guides the soil, bark and leaf palette. The renderer infers material for paths without
surface tags and generates the exposed root geometry. [Adlerhof, 21 August 2008](https://presse.wien.gv.at/bilder/2008/08/21/adlerhof-innenhof) · [Lobau near Dechantlacke, 9 August 2022](https://commons.wikimedia.org/wiki/File:Lobau_near_Dechantlacke_in_Wien-Aspern_2022-08-09_03.jpg)

## Reference geometry

The initial integration covers the local DEM, mapped building shells, near recessed facade cells
and ground-level road surfaces. A separate level mesh fills mapped water polygons cut from the
local terrain. Real DEM rings extend the terrain from the 250 m local square to 100 km along
each axis. The reference uses 240 rings with 128 intervals per side to retain Alpine
ridgelines in both renderers. Subdivision of the inner edges matches the original local
terrain. The far terrain currently has a uniform material.

The first twelve-image review shows missing steps at Strudlhofstiege, a missing passage at
Adlerhof and striping at Prater Hochstraße. The Schemerlbrücke camera looks from a mapped
bank path across the channel: the first integrated bank-to-bank recipe spans the water with a closed slab and edge guards.
Its steel truss, piers and graded approach tie-ins still need work.
Hauptallee and Dechantlacke need fuller crowns and bank vegetation. Franz-Karl-Fernsicht
looks southwest from an opening on the mapped footpath. The camera retains the 1.70 m
walkable-surface offset. Wienerblick still needs a review with the current city geometry.
Shore levels, road profiles, ground materials and light still need comparison with real scenes.

Two dated photographs anchor the structural review. The November 2023 stair photograph shows
paired flights, landings and retaining walls; compare geometry across seasons. The June 2017
bridge photograph shows horizontal clearance, the supporting piers and a continuous truss.

- [Strudlhofstiege, 23 November 2023](https://commons.wikimedia.org/wiki/File:Strudlhofstiege_2023-11-23_01.jpg)
- [Schemerlbrücke, 1 June 2017](https://commons.wikimedia.org/wiki/File:2017-06-01_AT_Wien_19_D%C3%B6bling_%26_Wien_20_Brigittenau,_Donaukanal,_Nussdorfer_Wehr,_Schemerlbr%C3%BCcke_(50699212833).jpg)

Mapped woodland uses the oak and beech stand recipes from the original biome study
(`9492ed2f7^`, `biome_recipes.py`). Shared mesh instances are rooted in canonical tile
coordinates. Road, water and building exclusion precedes placement. Stem density is an
art parameter (120/ha). The first comparison at Dechantlacke shows sparse crowns and
missing understory relative to the [August 2022 Lobau woodland](https://commons.wikimedia.org/wiki/File:Lobau_near_Dechantlacke_in_Wien-Aspern_2022-08-09_03.jpg).


The thirteenth reference, Wien, reads the elevated camera and clock directly from
`test/outshine/data/places/Wien.scenario`. The native comparison image is
[`doc/images/wien.png`](../../../doc/images/wien.png) (commit `ab335a928`).
The current reference series enables vegetation for every camera, including Wien.
Wienerblick remains an independent, eye-level site.

The regional reference samples the mapped building heights, water, woods, grass and deck
footprints around each camera. Its height field extends to the distance at which a 12 m
building occupies one pixel (6–15 km range). Closer tiles retain mapped building outlines, facade openings and shoreline positions.
Mapped water crossings use a raised slab and piers where both banks can be resolved.
Local and regional shorelines, roads and paths partition the same terrain triangles. The planar
arrangement cuts triangles only at material boundaries. Terrain heights are interpolated
within the original faces. Water takes precedence over ground routes. Mapped footpaths
retain their footprint where an inferred road width overlaps them. Tunnels and elevated
links are excluded from the ground network.
The outer height field still quantizes roofs and shorelines.

The native Wien capture is a historical diagnostic comparison. Visual acceptance uses the
twelve pedestrian cameras. Compare the historical view in
`build/shots/places/Wien.png` (Blender) and `build/shots/glsl/Wien.png` (standalone GPU).
Both consume the same camera, source products, material values, sun direction and tree
instances. GLSL uses Outshine's metal/rough BRDF and atmospheric lookup kernels. Aerial
perspective uses the ratio of transmittance at the eye and surface along each ray,
as in `src/render/shaders/aerialPerspective.frag`. Blender combines its own sky-only
layer and Principled BSDF render in scene-linear light. It uses Beer–Lambert attenuation
at the green-band Rayleigh coefficient and the camera-altitude density for clear-air
depth, independently of the GLSL tables.

```sh
build/blender-python/bin/python -m pip install moderngl
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --place Wien
build/blender-python/bin/python test/experiments/vienna/compare_wien.py
```

The comparison writes all three views at the same scale to `build/shots/comparison/Wien.png`.
Check the bridgehead and approaches, riverbanks, city silhouette and mountain ridge before
judging material and lighting differences. Vegetation stays enabled in both generated views.
