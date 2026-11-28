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
an independent lighting comparison. Cycles uses adaptive sampling with GPU denoising and a
128-sample ceiling. Use `--cycles-samples` to change that ceiling for a paired comparison.
Use EEVEE for rapid scene iteration and Cycles for light transport that needs off-screen
occluders or multiple bounces. Inputs are genuine ZXY tiles pinned in sources.json.
The seed, camera, clear sky and UTC are in places.json. The final eye position follows a vertical
intersection with the rendered support mesh.

Omit `--place Adlerhof` to render the twelve pedestrian views in catalog order. Use
`--place Wien` for the historical reference. With `--repeat`, the
second interval measures the warm image, including PNG encoding and writing. Report timing
with the image and profile so workstation cost can be compared across reference runs.
When using Cycles with `--repeat`, the fixed camera also reuses the scene data and, with
`--sky-dir`, the matching sky-only image for aerial compositing.

Blender uses the same mean solar diameter as the shared atmosphere, about 0.533°.
EEVEE uses six temporal samples and jittered sun shadows. The render report records
these settings. [NASA Sun Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/sunfact.html)

The EEVEE directional shadow map uses a minimum 16 mm texel. The default 1 mm texel
produced false shadows on sunlit roads with the full terrain horizon. The 16 mm limit
keeps the terrain and directional shadows in the reference.

EEVEE bakes diffuse irradiance volumes around nearby source-derived covered walks.
The volumes follow the generated passages and join around neighbouring portals.
Shading inside them includes world visibility and diffuse bounce. The render report
records the bake separately from the warm PNG interval.

Use `--street-bounce` with EEVEE for the one-bounce street-light reference in
[#1965](https://github.com/Weichwolf/outshine/issues/1965). It bakes sky visibility,
traces sunlight reflected by the generated geometry, and interpolates it on nearby
opaque surfaces. Under clouds it uses the mean solar transmission for the reflected term.
Compare it at the same camera and time with `--engine cycles --sky-dir …`.
With the shared sky isolated at Adlerhof, the EEVEE volume delivers about 7% of Cycles'
direct-sky luminance on a shaded paving patch. Cosine-weighted visibility against the generated
geometry yields 92% on that patch and 99% on the adjoining façade. Use the Cycles image to judge
those shadows. The one-bounce EEVEE image remains a fast comparison for reflected sunlight.

Standalone GLSL uses 16 m and 96 m camera-centred shadow footprints with 2048 texels
per axis. They resolve close sills, leaf shadows and root contact. Visibility blends
into the coarser maps at their footprint edges. The 16 m footprint moves in whole
shadow texels. Both share a depth interval, and the
4.8 km regional map also retains sunward casters beyond it. The GLSL command prints
the combined GPU time for the depth passes.

## Footpath replay

Use the mapped Vienna Woods path or the steep meadow edge at Körbersee for movement
checks. `Wienerwald-Footpath` starts at the woodland camera and moves along the path.
`Koerbersee-Footpath` approaches its tree-line camera while looking uphill. Both keep the
anchor's view direction and scenario time. The path centreline snaps the anchor to the
mapped path. The eye stays 1.70 m above the solved path mesh. The planner tests coverage
over the entire segment and samples support height at intervals of at most 10 cm.

```sh
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --walk Wienerwald-Footpath
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --walk Koerbersee-Footpath
```

Each walk writes sequential 720p PNGs 0.45 m apart under `build/shots/walks/<walk>`.
`walk.json` records the frame order, eye positions in local east/north/up metres,
source snapshot, scenario time, renderer settings and support probes. The prepared
scene uses the highest tree detail needed anywhere on the route. The clear procedural
sky renders from each pose. These profiles use paths without visible planar reflections.
[Anninger Süd, 1 May 2014](https://commons.wikimedia.org/wiki/File:Wienerwald_Weg.jpg) ·
[Körbersee, 17 October 2017](https://commons.wikimedia.org/wiki/File:Schr%C3%B6cken_K%C3%B6rbersee_Schr%C3%A4gluftbild.jpg)

## Matched sky radiance

Export the GLSL atmosphere for a fixed view and pass the directory to both renderers.
Each archive stores a scene-linear 512 × 256 equirectangular image, solar direction,
rendered eye altitude and aerosol extinction. Direct sunlight comes from the
separate sun in each renderer.
Blender uses the image as its world environment. GLSL integrates the same image with a
Lambert cosine kernel and evaluates unoccluded diffuse light from nine spherical harmonics.
For opaque materials, GLSL also filters the shared sky into the native renderer's
bordered octahedral roughness atlas and integrates a GGX reflectance table.
This profile controls the incident environment for diffuse and reflected light.
The renderers retain their respective local transport.
Water in this profile mixes a lit body with the reflected radiance using the
air–water Fresnel response. The reflection uses the planar capture where covered
and the same archived sky through its gaps. Coverage is applied once at filtered
capture edges. Sunlight on the visible water affects its body term, and objects
in the reflection carry their own lighting.

```sh
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --export-sky \
  --place Dechantlacke-Path-Near-Afternoon --output build/shots/1965/sky
build/blender-python/bin/python test/experiments/vienna/glsl_render.py \
  --place Dechantlacke-Path-Near-Afternoon --sky-dir build/shots/1965/sky \
  --output build/shots/1965/sky-glsl
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place Dechantlacke-Path-Near-Afternoon --sky-dir build/shots/1965/sky \
  --repeat --output build/shots/1965/sky-eevee
```

## Matched display

Standalone GLSL can export the view as top-down scene-linear Rec. 709 RGB float32
in an NPY file. It averages the four supersamples in linear light. Use Blender to
apply its reference AgX transform at +1.5 EV to those pixels and write a 16-bit PNG.
This makes the display transform identical to the EEVEE reference. Omit `--sky-dir`
to use the GLSL renderer's own sky.

```sh
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --linear \
  --place Dechantlacke-Path-Near-Afternoon --sky-dir build/shots/1965/sky \
  --output build/shots/1965/linear
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --display-linear build/shots/1965/linear/Dechantlacke-Path-Near-Afternoon.npy \
  --output build/shots/1965/glsl-agx
```

## Clear and hazy distance

FranzKarlFernsicht and FranzKarlFernsicht-Haze retain the same mapped trail support,
heading, pitch, eye height, and 21 June 2026 10:00 UTC solar time. Clear air retains
Rayleigh and ozone. The haze adds grey aerosol extinction of 0.04 km⁻¹ at sea level,
with a 1.2 km exponential scale height and 0.9 scattering albedo. GLSL samples the
atmospheric lookup tables for the camera-to-surface segment. Blender integrates the
aerosol column along each camera ray using its Mist distance pass and a curved height
profile. Blender's compositor uses scalar transmission for the image mix. Both renderers
attenuate direct sunlight through the aerosol column. Use separate sky archives for the
two weather profiles. [Hillaire, Scalable Atmosphere Rendering](https://doi.org/10.1111/cgf.14050)

FranzKarlFernsicht-Adjacent follows the same mapped trail by four metres.
Use the elevated Wien scenario for historical horizon comparison.

```sh
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --export-sky \
  --place FranzKarlFernsicht --place FranzKarlFernsicht-Haze --output build/shots/2052/sky
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place FranzKarlFernsicht --place FranzKarlFernsicht-Haze \
  --sky-dir build/shots/2052/sky --output build/shots/2052/eevee
```

## Cloud reference

FranzKarlFernsicht and Wien each have `-BrokenCloud` and `-Overcast` variants.
Each triplet shares a camera, ground geometry, seed and solar time. The trail view
uses 21 June 2026 at 10:00 UTC. Wien keeps the elevated scenario and 7 September 2026
at 10:40 UTC. The broken layer spans 1.45–2.95 km altitude and the overcast layer
0.90–1.36 km. The shared seeded density field moves east at 7 m/s and north at
3.5 m/s from the catalog clock. GLSL integrates opacity and estimates directional
light through the layer. Both renderers use its 2048 × 1024 sky archive. The archive
contains direct solar transmission at the observer and a 64 × 64 km transmission map
from the same cloud volume. Standalone GLSL projects surfaces below the layer top
along the solar direction onto the map, including during reflection captures. The
map fades to the observer's value near its edge. EEVEE captures a scene-linear
ambient image at the fixed camera and uses the Position pass to attenuate the
direct Sun in the surface image. Water reflection captures use the same projection.
The render report measures the ambient capture separately and repeats reuse it at
the same pose. Older archives and the independent Cycles path use the observer's
value for the Sun. The archive pins the profile, wind-advected origin, seed, eye
altitude and solar direction.

The `-Backlit` trail and Wien variants use the Sun of 15 October 2026 at
12:24 UTC. Both show the broken layer with the Sun near the direction of view.
Revision 2 weights direct cloud scattering with forward and backward
Henyey–Greenstein lobes. The forward lobe brightens sun-facing edges and gaps,
and a weaker diffuse term lights the cloud interiors. Replay retains the stored
radiance of revision 1 archives with matching field, altitude and Sun.
[Schneider and Vos, Volumetric Cloudscapes](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) ·
[Clouds with the Sun behind, Tyrol, 4 August 2010](https://commons.wikimedia.org/wiki/File:Clouds_with_sun_behind.jpg)

Use the Wiener Blick afternoon panorama for spaced cumulus, sunlit tops and soft
undersides. The Neue Donau photo shows a closed deck, diffuse illumination and a
brighter horizon under the deck. Michaelerplatz shows diffuse light on an urban
façade below a textured overcast sky. [Wiener Blick, 29 August 2010](https://commons.wikimedia.org/wiki/File:Wiener_Blick_Panorama.jpg) ·
[Neue Donau, 5 May 2014](https://commons.wikimedia.org/wiki/File:Cloudy_day_on_Neue_Donau_-_panoramio.jpg) ·
[Michaelerplatz, 28 October 2017](https://commons.wikimedia.org/wiki/File:Vienna,_Austria_(38308253026).jpg)

```sh
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --export-sky \
  --place FranzKarlFernsicht --place FranzKarlFernsicht-BrokenCloud \
  --place FranzKarlFernsicht-Overcast --output build/shots/2086/sky
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place FranzKarlFernsicht --place FranzKarlFernsicht-BrokenCloud \
  --place FranzKarlFernsicht-Overcast --sky-dir build/shots/2086/sky
```

## Near material comparisons

Adlerhof faces the passage from the street. Adlerhof-Courtyard and Adlerhof-Reverse
show the adjoining arches in both directions. Adlerhof-Near keeps the oblique
paving and facade view. The Dechantlacke path views look past the same exposed
roots. Select these extra views explicitly to keep the twelve-view run in order.

KarlMarxHof faces the east façade from a mapped sidewalk. Its taller sections share
a street-storey band and shallow stone belt; KarlMarxHof-Street inspects a separate
entrance nearby from its paved connection. The solved terrain keeps recesses and
sills clear on sloped lots. The source's light
and rose finishes stay distinct on separately modelled building parts.
KarlMarxHof-Courtyard retains the longer view for window rows. Nevillebruecke
checks the same window grid as the streetscape recedes. Near openings have
recesses, frames and projecting sills. Smaller visible openings keep the same
spacing in a flat wall mesh. Where building parts overlap, only exposed walls
receive windows. Openings and sills fit entirely on those walls.

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
macro variation and tints the beech bark with its lichen coverage at detail scale.
The detail covers 2.56 m at 5 mm per texel. The leaf map is filtered to
10 mm per texel over the same span. Macro tint uses a 23 m period. The nominal stone face
spacing is 213 × 128 mm. Glazing uses a 1.52 IOR over a dark opaque backing to stand in
for unmodelled interiors.

The Adlerhof photo guides the plaster and rectangular paving. The Dechantlacke photo
guides the soil, bark and leaf palette. The renderer infers material for paths without
surface tags. [Adlerhof, 21 August 2008](https://presse.wien.gv.at/bilder/2008/08/21/adlerhof-innenhof) · [Lobau near Dechantlacke, 9 August 2022](https://commons.wikimedia.org/wiki/File:Lobau_near_Dechantlacke_in_Wien-Aspern_2022-08-09_03.jpg)

The compacted trail detail has patches of worn soil and exposed grit. Where an
unpaved path borders rendered woodland floor, its inner 42 cm blends into the
same leaf-litter reflectance, roughness and relief. A contour inside the path
resolves the blend in the existing terrain triangles. Paved crossings, water and
meadows keep their assigned surfaces. Compare the trail shoulders at
Wienerwald-Woodland and Dechantlacke-Path-Near-Afternoon.
[Wienerwald at Anninger Süd, 1 May 2014](https://commons.wikimedia.org/wiki/File:Wienerwald_Weg.jpg)

The Vienna stand varies clear bole length and scaffold height across its two forms each for
oak and beech. Close boles vary their girth, cross section and branch scars with their
tile-seeded placements. Separate low root ridges spread along the soil beside the bole,
with unequal directions and reach. The root surface follows the terrain and blends into
the original bole over six trunk radii. Root spread contracts at path and water boundaries.
A lower-resolution mesh retains the stem profile and branch attachments beyond 32 m.
Oak has fissures between elongated plates, beech has fine grain and lichen patches,
and spruce has smaller bark scales. The park rows use chestnut-inspired stems and
darker plated bark. Compare roots at Wienerwald-Woodland, branch unions from the neighbouring
camera at Dechantlacke-Stem-Oblique, and the steep spruce margin at Koerbersee-TreeLine.
[Weber and Penn, Creation and Rendering of Realistic Trees](https://doi.org/10.1145/218380.218427) ·
[Hauptallee, 9 August 2022](https://commons.wikimedia.org/wiki/File:Hauptallee_Prater_Wien_2022-08-09_15.jpg) ·
[Hochbruckenberg, 25 November 2012](https://commons.wikimedia.org/wiki/File:Hochbruckenberg_Wald_02.jpg) ·
[Anninger Süd, 1 May 2014](https://commons.wikimedia.org/wiki/File:Wienerwald_Weg.jpg)

The summer crown recipes include a spreading form and an upright form for both oak
and beech. On spreading trees, the lower limbs extend sideways, the upper limbs fill
above them, and staggered shoots spread foliage across fewer supporting twigs.
Seeded blade roll keeps the midrib attached to its shoot.
The lower foliage stays flatter, and oak blades have a wider range of inclinations
than beech blades. The same leaf orientations feed the near meshes and the grouped
distance meshes. Compare leaf area and blade inclination against measured broadleaf
crowns. [Fleck, mature beech and oak crowns](https://epub.uni-bayreuth.de/id/eprint/264/1/FleckDiss.pdf) ·
[Chianucci et al., leaf inclination data](https://doi.org/10.17632/4rmc7r8zvy.2)

Koerbersee-TreeLine and Koerbersee-TreeLine-Oblique extend the comparison to a
mapped alpine trail. Where the provider includes `leaf_type`, that value guides
stand composition. In the central European reference, the spruce share for
untyped forest rises with DEM elevation from 1,200 to 1,570 m. The two spruce
forms use whorled branches, tapered crowns and grouped needle sprays sampled
along curved twigs. At distance, the parent twigs represent finer shoots. The
solved soil supplies the root heights and the mapped margins guide planting.
[Körbersee webcam](https://www.foto-webcam.eu/webcam/koerbersee/) ·
[European Atlas of Forest Tree Species, Norway spruce](https://forest.jrc.ec.europa.eu/media/atlas/Picea_abies.pdf)

## Photographic targets at eye level

The street, water and woodland references use the pinned Vienna catalog at 1.70 m.
The photo dates and locations identify the construction, surface scale and light
relationships to compare. The photographs and generated views use different camera poses.
The first review used 1280×720 EEVEE on Iris Xe Vulkan at `f65afb793`.

| View | Target from the site photograph | Gap in the generated view |
|---|---|---|
| Adlerhof | Close-set stone blocks lead through a deep sequence of plaster arches. A short stair links the courtyard levels. The bright exit and shaded recess remain legible together. [Hans Strunz, 21 August 2008](https://presse.wien.gv.at/bilder/2008/08/21/adlerhof-innenhof) | Mapped footways connect through vaulted openings with projecting stone surrounds and grounded plinths. Courtyard steps and the reflected daylight inside the vaults remain open. [#2055](https://github.com/Weichwolf/outshine/issues/2055) |
| Schemerlbruecke-Downstream | Retaining faces and the bridge enclose the channel. The drop interrupts the reflectivity of the upper reach and aerated water brightens the lower pool. [C. Stadler/Bwag, 29 May 2018](https://commons.wikimedia.org/wiki/File:Wien_-_Nu%C3%9Fdorfer_Wehr_(1).JPG) | The mapped crest, lower pool and reflected structure read from the east bank. Angular earthworks and the smooth lower pool leave less separation between spill and calm water than in the photograph. [#1966](https://github.com/Weichwolf/outshine/issues/1966) |
| Dechantlacke-Bank | Leaves at several heights interrupt the gaps between oak-sized trunks. Small sunlit patches break up trunks and woodland floor. [Leonhard Lenz, 9 August 2022](https://commons.wikimedia.org/wiki/File:Lobau_near_Dechantlacke_in_Wien-Aspern_2022-08-09_03.jpg) | The shore shows rooted shrubs, distinct stems and a reflected second layer of crowns. The opposite bank and canopy gaps remain more open than the photographed interior stand. [#1961](https://github.com/Weichwolf/outshine/issues/1961) · [#2063](https://github.com/Weichwolf/outshine/issues/2063) |

The site data supplies Adlerhof's building and route footprints, the canal and mapped
weir crest, and the Lobau water, paths and woodland boundary. Building openings and
setts use repeat recipes. The weir head and sediment at the bank are model estimates.
The woodland recipe chooses species, age variation and undergrowth within the mapped
habitat. The photographs guide those choices at pedestrian scale.

The clear-day pairs use the same pin set, pose, geometry, material recipes and AgX
exposure. Their clocks are 10:00 and 16:30 UTC on 21 June 2026. Adlerhof turns from
sunlit plaster and setts to façade shade. Dechantlacke turns from backlit stems and
dappled path shade toward more directly lit bark and path. In shade, look for
readable surface texture, local contrast and recognizable material colours.

```sh
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- \
  --place Adlerhof-Near --place Adlerhof-Near-Afternoon --place Dechantlacke-Path-Near \
  --place Dechantlacke-Path-Near-Afternoon --output build/shots/2044/light/eevee
```

The 1280×720 Iris Xe Vulkan captures show the same joints, window recesses, root
collars and path edges at both times. A ray probe selected fixed pixels that cross
the geometric sun/shade boundary between frames. Median PNG luma on a 0–255 scale
changes from 163 to 43 on Adlerhof plaster and from 171 to 22 on the setts. At
Dechantlacke it changes from 23 to 104 on oak bark, 31 to 136 on beech bark and
17 to 86 on the unpaved path. The afternoon sun makes the bark contrast and stem
curvature clearer. In shade the paving and path lose much of their fine relief.
The diffuse-light comparison belongs to [#1965](https://github.com/Weichwolf/outshine/issues/1965).

The twelve-view review covers the catalog anchors. At Wienerblick, the dated
panorama shows a wooded middle distance, a distinct city band and scattered clouds.
The generated view resolves the foreground slope and urban massing. The tree belts,
distance contrast and sky need further comparison under the photographed conditions.
[Doris Antony, 29 August 2010](https://commons.wikimedia.org/wiki/File:Wiener_Blick_Panorama.jpg) ·
[Review and remaining differences](https://github.com/Weichwolf/outshine/issues/2083)

## Reference geometry

The initial integration covers the local DEM, mapped building shells, near recessed facade cells
and ground-level road surfaces. Separate water meshes fill the mapped polygons cut from the
terrain. Each mapped reach uses a shared DEM-inferred WGS84 level in the local and regional meshes.
Real DEM rings extend the terrain from the 250 m local square to 100 km along
each axis. The reference uses 240 rings with 128 intervals per side to retain Alpine
ridgelines in both renderers. Subdivision of the inner edges matches the original local
terrain. The far terrain currently has a uniform material.

The first twelve-image review shows missing steps at Strudlhofstiege, a missing passage at
Adlerhof and striping at Prater Hochstraße. The Schemerlbrücke camera looks from a mapped
bank path across the channel: the first integrated bank-to-bank recipe spans the water with a closed slab and edge guards.
Its steel truss, piers and graded approach tie-ins still need work.
Hauptallee and Dechantlacke show layered woodland. Hauptallee-Avenue looks along the
mapped bridleway and shows managed trees rooted in the grass verges of the shared
parallel route. Hauptallee-Entry shows the paved footway connection and recessed
entrance of an isolated low building. Dechantlacke adds sedges and reed
groups on the low natural bank, while the Asperner See park and access edges stay open.
Compare the patchy shoreline with the [Lobau photograph, 16 May 2015](https://commons.wikimedia.org/wiki/File:Lobau_Dechantlacke.JPG).
Franz-Karl-Fernsicht looks southwest from the mapped footpath over taller grass,
seed heads and wildflowers on the OSM meadow. The camera retains its 1.70 m eye height.
Use the [Sophienalpe summer photograph, 15 August 2006](https://commons.wikimedia.org/wiki/File:Wien_Sophienalpe.jpg)
to assess the ground cover and plant height.
Shore levels, road profiles, ground materials and light still need comparison with real scenes.

The November 2023 night photograph anchors the stair geometry. The paired flights,
landings and retaining walls remain legible around the fountain. The June 2017 bridge
photograph shows clearance below the painted truss, piers in the channel and the scale
of the stone pylons at the banks.

- [Strudlhofstiege, 23 November 2023](https://commons.wikimedia.org/wiki/File:Strudlhofstiege_2023-11-23_01.jpg)
- [Schemerlbrücke, 1 June 2017](https://commons.wikimedia.org/wiki/File:2017-06-01_AT_Wien_19_D%C3%B6bling_%26_Wien_20_Brigittenau,_Donaukanal,_Nussdorfer_Wehr,_Schemerlbr%C3%BCcke_(50699212833).jpg)

Mapped woodland uses the oak and beech stand recipes from the original biome study
(`9492ed2f7^`, `biome_recipes.py`). Shared mesh instances are rooted in canonical tile
coordinates. Road, water and building exclusion precedes placement. Established trees
take priority over smaller recruitment in the same stand. Recruitment varies in patches
and thins at paths, banks and clearings. Nearby stems compete regardless of the render
crop. The distant templates group leaves by branch and retain their represented leaf
area. From groups of sixteen leaves, a four-triangle blade uses the same placement
frame and one-sided area as the detailed representative. Dechantlacke-Woodland
looks into the stand from a mapped path. Compare crown gaps and woodland layers with the
[August 2022 Lobau woodland](https://commons.wikimedia.org/wiki/File:Lobau_near_Dechantlacke_in_Wien-Aspern_2022-08-09_03.jpg).
Wienerwald-Woodland follows a mapped trail by Franz-Karl-Fernsicht. This hillside
stand tests crown overlap, younger stems and terrain contact away from riverbank woodland.
Mapped woodland soil carries baked leaves, exposed humus and moss. Ground-level leaf
patches use the solved soil and reserve their full width along paths and water.


The thirteenth reference, Wien, reads the elevated camera and clock directly from
`test/outshine/data/places/Wien.scenario`. The native comparison image is
[`doc/images/wien.png`](../../../doc/images/wien.png) (commit `ab335a928`).
The current reference series enables vegetation for every camera, including Wien.
Wienerblick remains an independent, eye-level site.

The regional reference samples the mapped building heights, water, woods, grass and deck
footprints around each camera. Its height field extends to the distance at which a 12 m
building occupies one pixel (6–15 km range). Closer tiles retain mapped building outlines and facade openings.
Mapped woodland in the regional mesh uses a filtered canopy material. A shared
512 m tile describes overlapping crowns and their shaded gaps. Slower colour
variation across the stand helps distinguish wooded slopes from meadows. Both
renderers project the material horizontally on the existing terrain. The relief
affects shading. The terrain mesh determines the ridge silhouette.
Compact buildings can use a pitched roof within their mapped top height. The near and
regional generators share the same gable recipe, including the closing wall faces.
Courtyards, conflicting parts and incomplete tile footprints use the flat envelope.
Wienerblick also selects a pinned skyline sector out to 16 km. Its additional tiles
contribute height-bearing building shells above a two-pixel angular estimate. The
shells use the same geodetic ENU frame as the horizon terrain. The regional tiles
retain ownership of the buildings they already cover.
Mapped water crossings use a raised slab and piers where both banks can be resolved.
Local and regional shorelines, roads and paths partition the same terrain triangles. The planar
arrangement cuts triangles at material boundaries and grade contours. Terrain heights are interpolated
within the original faces before grading. At a natural shore, the terrain blends to the
water level within a 14 m band. Roads, footpaths, buildings and solved bridge approaches
constrain the grade. Trees take their root heights from the finished bank surface.
Water takes precedence over ground routes. Mapped footpaths
retain their footprint where an inferred road width overlaps them. Tunnels and elevated
links are excluded from the ground network.
The outer height field still quantizes roofs.

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

The reference cameras select up to two water levels for 640×360 planar reflection captures.
Clipped scene geometry supplies the reflected forms and the sky fills the uncovered rays.
GLSL captures the mirrors on the GPU. EEVEE prepares them once for each static camera pose
and reuses them for warm renders. The reported capture seconds are part of scene setup.
With `--repeat`, EEVEE also prepares the clear-sky compositing layer once in scene-linear
precision. The sky capture time is reported separately from PNG generation.
For a graded reach, the capture plane is fitted to the visible water samples.

Both paths sample the same 0.75 m nearshore transmission raster. In the first 10 m from
a mapped natural bank, a reference bed slope and RGB extinction approximate the fading
return from underwater sediment. Roads and quay contacts suppress the sediment response.
The band fades before the regional seam. The slopes and optical coefficients are estimates
for these comparisons. [Rendering Natural Waters](https://doi.org/10.1111/1467-8659.00548)

The Nußdorf entry in `water_controls.json` cuts the pinned Donaukanal polygon at the
mapped weir crest downstream of Schemerlbrücke. The upper reach runs beneath the
bridge. The two plateaus use Mapterhorn estimates. The canal below the weir uses the
relative drops from the KWD 2020 160 m³/s profile along the simplified OpenFreeMap
`Donaukanal` centreline. The curve is stationed
between the cut and the canal mouth. The gate face, shore grade and pedestrian clearance
use this same water model. A stationary overflow sheet joins the crest to the lower
reach in the reference. `Schemerlbruecke-Downstream` observes the crest from the lower bank.
The retaining reach uses the mapped water edges for the wall faces. Its inferred crest
follows Mapterhorn ground 9 m inland. The retained grade blends into the natural bank
at the ends of the reach and preserves the fitted transport surfaces.
[Nußdorfer Wehr, 29 May 2018](https://commons.wikimedia.org/wiki/File:Wien_-_Nu%C3%9Fdorfer_Wehr_(1).JPG)

```sh
build/blender-python/bin/python -m pip install moderngl
build/blender-python/bin/python test/experiments/vienna/glsl_render.py --place Wien
build/blender-python/bin/python test/experiments/vienna/compare_wien.py
```

The comparison writes all three views at the same scale to `build/shots/comparison/Wien.png`.
Check the bridgehead and approaches, riverbanks, city silhouette and mountain ridge before
judging material and lighting differences. Vegetation stays enabled in both generated views.
