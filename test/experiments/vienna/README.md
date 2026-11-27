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

Omit `--place Adlerhof` to render the thirteen views in catalog order. With `--repeat`, the
second interval measures the warm image, including PNG encoding and writing. Allow up to five
seconds at 1280×720 for this reference workflow.

The initial integration covers the local DEM, mapped building shells, near recessed facade cells
and ground-level road surfaces. A separate level mesh fills mapped water polygons cut from the
local terrain. Real DEM rings extend the terrain from the 250 m local square to 100 km along
each axis. The far terrain currently has a uniform material.

The first twelve-image review shows missing steps at Strudlhofstiege, a missing passage at
Adlerhof and striping at Prater Hochstraße. The Schemerlbrücke camera looks from a mapped
bank path across the channel: the first integrated bank-to-bank recipe spans the water with a closed slab and edge guards.
Its steel truss, piers and graded approach tie-ins still need work.
Hauptallee and Dechantlacke need trees and bank vegetation, Wienerblick needs its city
skyline, and Franz-Karl-Fernsicht needs a recognizable Alpine silhouette. The shore levels,
road profiles, material variety, sky and light still need comparison with real scenes.

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
The outer height field still quantizes roofs and shorelines.

The native Wien capture is the first reproduction target. Compare the same view in
`build/shots/places/Wien.png` (Blender) and `build/shots/glsl/Wien.png` (standalone GPU).
Both consume the same camera, source products, material values, sun direction and tree
instances. GLSL uses Outshine's metal/rough BRDF and atmospheric transmittance, multiple
scattering and sky-view lookup kernels. Blender provides its own BSDF and sky model.

```sh
build/blender-python/bin/python -m pip install moderngl
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --place Wien
```
