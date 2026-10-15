# Cached needle shoot view comparison

Owner: [#154](https://github.com/Weichwolf/outshine/issues/154), child of #23.
This is an experiment and counterexample corpus, not an approved forest LOD or a
new native ABI. The four whole-tile showcase images remain independently verified;
the unlit diagnostic figure is deliberately outside that gallery.

The complete near shoots of Picea abies, Pinus sylvestris, Pinus cembra and Larix
decidua come from the same third-order plans and closed geometry as PR #151.
`needle_group_source.py` reads cached meshes/poses, preserves all organ IDs and
transforms them into a common shoot frame. It never places or meshes new organs.
There are respectively 123/67/85/94 organs and 14,180/22,184/34,376/69,500 triangles
in these four particular shoots; these are not whole-plant counts.

## Method and fair comparisons

`needle_group_views.py` uses the existing bounded triangle/pixel rasterizer to
store first-hit coverage, premultiplied linear colour, normals, roughness fourth
moment and depth. Every 64×64 texel also retains all sixteen unlit supersamples
as float32: normal XYZ, colour RGB, roughness, depth, coverage. The normal mean
is a comparison control, not a validated normal distribution or lighting kernel.
Full raw samples are expensive diagnostic data, not a proposed GPU texture ABI.

Actual sphere sets of 4/8/16/32/64 directions are baked independently; smaller
sets are not misleading prefixes of the 64-direction set. Seventeen different
directions use the closed mesh at eight sample rows per texel as reference.
Each direction has one viewport derived from the same complete source. A second
comparison changes that viewport to 16/32/64 pixels to expose screen-size effects;
it does not lower the resolution or contents of a whole-tile render gate.

Single planes, 4×4/8×8/16×16 first-depth reliefs and 1/2/4/8 organ depth bands use
the same physical geometry and source directions. Bands partition complete organs
by their area-weighted depth and composite their views; they are not exact LDI
layers and do not prove occlusion. Frontmost relief fragments use the existing
raster kernel. `plant_depth.depth_mesh(..., anchored=False)` retains shoot depth;
the existing default root anchoring remains byte-identical on the comparison
fixture against commit 44403307c for all four grid sizes.

The quantities are relative total coverage-area error, pixel coverage L1 divided
by reference coverage, and conditional first-hit depth RMSE where both products
cover a pixel. A small area error alone can hide large needle displacements.
The opened diagnostic shows this loss clearly. Do not approve close shoot LOD
from its total area or keep adding depth repairs without reviewing representation.
The current full closed-geometry tile remains the picture oracle.

## Scope and costs

Procedural needle textures are sampled at mesh corners before interpolation in
this CPU probe. This differs from Blender's per-fragment texture sampling. Wood
uses an explicit constant material control. The comparison therefore checks
geometry coverage/depth, not equivalence of procedural bark/needle shading.
R/T, metallic-roughness response, self-shadowing, wind, disocclusion, perspective
and actual moving whole-tile/native images need separate evidence.

The complete 64-view NPZ retains plan identity, axis path and organ IDs. Reports
bind source inputs, implementation and NPZ bytes with SHA-256. A fresh-process
decode check disables `Needle.mesh` and `NeedledPlant.foliage`, checks all views
and raw coverage, and rejects a different plan identity. This demonstrates a
cached view decode, not direct cold far construction from a compact recipe.
Report compressed bytes, decoded array bytes, view build samples, serialization
and decode separately. External Linux getrusage measures terminal process RSS;
none of these numbers is GPU memory, a native frame time or an A18-Pro result.

```sh
.venv/bin/python -W error test/experiments/vegetation/needle_group_checks.py
.venv/bin/python -W error test/experiments/vegetation/needle_group_experiment.py
.venv/bin/python -W error test/experiments/vegetation/needle_group_depth_experiment.py
.venv/bin/python -W error test/experiments/vegetation/needle_group_checks.py --cache
```

Inputs and results live under `build/experiments/vegetation/{conifer-crowns,needle-groups}`.
Freeze the implementation commit and run the steps sequentially. A failed or
changed-input run cannot approve or replace a showcase image.

## Primary references and independent implementation

[Billboard Clouds, SIGGRAPH 2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf)
motivates bounded plane approximations; page 4's density/greedy/refinement diagrams
were opened optically. [Layered Depth Images, SIGGRAPH 1998](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
motivates retaining depths rather than flattening all first hits.
[Han et al., SIGGRAPH 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf)
motivates preserving the normal distribution; #134 owns the material question.

Ghidra re-exported four local SpeedTree 10.2 frond routines read-only. Their call
relationships distinguish parent/guide assembly from skin emission and imported
mesh emission. That observation does not reveal or validate the needle view
algorithm used here. All four decompilation exports report success; subsequent
headless process termination was 143, not a clean exit-zero gate. Vendor code and
diagnostics remain in Downloads and are not bundled in this repository.
