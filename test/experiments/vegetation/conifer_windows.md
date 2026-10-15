# Bounded shoot windows and complete conifer products

Owner: [#168](https://github.com/Weichwolf/outshine/issues/168), under #23.
The complete four-species young-tree plans and physical tile from PR #151 are
unchanged. This experiment replaces whole-plant individual needle expansion with
a finite physical form catalog. It is an experimental far product, not an
approved near representation or a finished forest generator.

The shared `NeedledPlant.window_summary()` and `.windows()` methods count demand
before placement and partition every canonical needle range into bounded physical
windows. They expose owned immutable arrays: axis identity/order, nominal centre,
arc frame, unjittered phase, first needle ID, count and full-template capacity.
Whole-plant axis/window budgets reject before expanding axes. No needle placement
or meshing is needed. Existing recipes, identities and near-detail APIs remain.

`conifer_window_catalog.py` builds finite canonical windows from previously cached
closed needle prototypes and their procedural materials, at physical scale.
Pattern keys preserve spacing, cluster size and arrangement, excluding only the
occupied parent interval. The four plants require eight families in total.
Default windows span about 0.08 m and contain at most 64 groups; they do not stretch
needle length with the parent axis. Each family contains 64 unlit 32×32 views,
4×4 supersampled, with premultiplied colour, alpha, normal, leaf weight and depth.
Canonical angular jitter, needle variant assignment, stem radius and short-shoot
wood are approximations; matching counts do not prove matching coverage.

`conifer_windows.py` binds the shared plan to catalog families, accounting for
the canonical phase. Arc frames follow axis curvature. A partial last window
retains its exact source count but weights the complete template by that count
fraction. It may overhang the physical tail; it does not reproduce a near subset.
`conifer_window_render.py` combines those views with all simplified wood axes.
Colour, coverage and depth compose in subpixel samples before the final filter.
`plant_atlas.py` reuses the existing whole-plant atlas/depth-product packing through
an optional drawing callback; its previous cached-group path stays the default.
The wood-raster extraction preserves the previous output on an independent
opaque-wood/half-transparent-leaf control, including depth and material channels.

`conifer_whole_products.py --phase catalog` prepares and saves the finite catalog
and source recipe. A separate fresh `--phase build` process loads it and creates
all 64 complete tree views, with `NeedledPlant.foliage`, `NeedlePattern.detail`
and `Needle.mesh` disabled throughout. Coarse preparation, catalog generation,
catalog decode, atlas generation and final rendering have separate timings.
The recipe saved with the catalog supplies the later matching near details.
This is not a general asset cache implementation or an engine/GPU contract.

`conifer_whole_preview.py` uses the same four roots, identities, 10×10 m ground,
grass, camera, lights, 2560×1440 and 64 EEVEE samples as the complete physical tile.
Camera, sun and fill choose views independently. Four trees require 1,536 draw
triangles across the three roles; unchanged ground/grass bring the tile to
136,160 triangles. This draw count excludes the offline catalog/atlas bake.
`conifer_whole_cache.py` measures complete fresh-process product decode separately
from 60 CPU view queries, without rebuilding plans, meshes or textures. Resident
array bytes and host RSS are distinct; OS file caches may be warm. Upload, shading,
GPU allocations and native frame costs are not measured by this experiment.
Normals are raw mean controls and leaf roughness/transmission are approximate;
no equivalence to the procedural physical-needle shader or #134 is claimed.

The 128- and 256-pixel exploratory products were actually opened. The 256-pixel
tile retains the major forms but is visibly softer than the physical control at
this camera distance. It is not promoted over that control in the four-image
showcase. `conifer_window_comparison.py` compares one complete cached physical
shoot per species in 17 directions with identical bounds, wood-axis control and
subpixel sampling. It reports area and spatial coverage error at several image
sizes; botanical/shading/perspective/wind and LOD-transition approval stay open.

```sh
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/conifer_whole_products.py --phase catalog --output build/experiments/vegetation/conifer-whole --size 256
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/conifer_whole_products.py --phase build --output build/experiments/vegetation/conifer-whole --size 256
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/conifer_window_checks.py --whole build/experiments/vegetation/conifer-whole --products
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/conifer_whole_cache.py --products build/experiments/vegetation/conifer-whole --output build/experiments/vegetation/conifer-whole/fresh-cache.json
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/conifer_window_comparison.py --whole build/experiments/vegetation/conifer-whole --output build/experiments/vegetation/conifer-whole/shoot-comparison.json
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/vegetation/conifer_whole_preview.py -- --scenario build/experiments/vegetation/conifer-crowns/tile/tile.json --whole build/experiments/vegetation/conifer-whole --output build/experiments/vegetation/conifer-whole/tile/render.png
```

Use a frozen commit and one heavy run at a time. Do not elevate Blender upstream
deprecation warnings into exceptions: bpy 5.2.2 otherwise fails RNA node-tree
registration before rendering. Numerical checks do use `-W error`. Terminal
Linux RSS and wall-clock times are host measurements, not GPU or A18 evidence.

Method reference: [Deussen et al., Realistic Modeling and Rendering of Plant
Ecosystems, SIGGRAPH 1998](../../../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf),
PDF page 3, hierarchical and approximate instancing (opened optically).
This implementation uses explicit pattern families, not the paper's vector
quantization, and makes no global optimality claim. Previously inspected local
SpeedTree Ghidra exports show separate billboard generation/export stages;
their vendor code and numerical constants are not copied or bundled here.
Native integration and runtime budgets remain #48/#1.
