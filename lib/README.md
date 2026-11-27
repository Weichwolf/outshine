# Outshine Python reference

`outshine_ref` preserves the procedural research for the native rebuild. It is
a reference and experiment library. The target application is implemented in C++.

| Modules | Preserved methods |
|---|---|
| `transport` | Canonical ports, corridor and junction geometry, grades, layered connections and rail hardware |
| `buildings` | Footprints, roofing, facade rules, entrances and interior placement |
| `geometry`, `water` | Surface topology and patches, water levels, profiles and contacts |
| `vegetation`, `materials` | Habitat, morphology, leaf optics and surface composition |
| `places`, `rendering` | Reference data loading and Blender comparison helpers |

With Python 3.11 or newer, install in a virtual environment.

```sh
python -m pip install -e './lib[transport,water,roofs,solids,places,assets]'
python -m unittest discover -s lib/tests
```

Use the bundled tests and real-world fixtures as comparisons for native
algorithms. Keep tractable counterexamples, compare picture and cost on holdouts,
and remeasure when assumptions change. Research and remaining gaps are indexed in
[Outshine generation methods](../doc/references/procedural/outshine-generation.md).

Render a prepared reference scene on the GPU. The second render measures warm time
through the completed PNG write. Compare the Vienna working views at 1280×720.

```sh
blender -b scene.blend --python-exit-code 1 -P lib/src/outshine_ref/rendering/blender_render.py -- --engine eevee --repeat --output /tmp/outshine-preview.png
```

Use `--engine cycles` for lighting references. On the development Mac this selects
Metal and disables the CPU render device.
