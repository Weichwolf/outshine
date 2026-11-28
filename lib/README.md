# Outshine Python reference

`outshine` prepares a geographic scene and renders one PNG per camera. It contains
sources, procedural generators, materials and the Blender and GLSL renderers.
The C++ realtime application lives in `src/` and its tests live in `test/`.

| Path | Contents |
|---|---|
| `outshine/` | Python implementation and reference shader sources |
| `tests/` | Python tests, pinned camera and source fixtures, image comparisons |
| `experiments/` | Standalone Python research probes |

Install from the checkout. For Blender, use a virtual environment with the same
Python major and minor as Blender. The reference launches Blender with Metal on
macOS and Vulkan on Linux and Windows. The GLSL backend uses the shared native
shader includes from the checkout. It uses a hardware OpenGL 4.1 context, via
EGL on headless Linux.

```sh
nice -n 10 python -m pip install -e './lib[render,assets,roofs]'
nice -n 10 python -m unittest discover -s lib/tests
nice -n 10 python -m outshine --latitude 48.13737 --longitude 11.57574 --heading 290 \
  --absolute-height --height 565 --pitch -14 --time 2026-06-21T10:00:00Z --backend blender --output build/shots/Munich.png
```

Use `--backend glsl` for the alternate reference renderer or `--engine cycles`
for Blender light transport. Camera height defaults to 1.7 m above the solved
surface. `--absolute-height` selects a height above sea level. Coordinates use
the Web Mercator domain of the elevation and OSM providers. Source cache and
snapshot can be selected via `--cache` and `--snapshot`.

```python
from outshine import Camera, render_png

render_png(Camera(48.13737, 11.57574, 290, -14, 565, above_ground=False),
           "build/shots/Munich.png", backend="glsl")
```

Run the [pinned pedestrian comparisons](tests/fixtures/vienna/) with
`nice -n 10 python -m lib.tests.render_places --place Adlerhof`. These fixtures
own their camera catalog, source receipts and survey controls. The camera API
uses the requested position and source snapshot directly.

Use the smallest fixture that exposes the change. Open the renders and compare
with dated photos and earlier results. See the
[generation methods](../doc/references/procedural/outshine-generation.md) for
research and known gaps.
