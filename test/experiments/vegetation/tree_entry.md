# Compact whole-tree reference (#367)

Master requested a checked entry for replacing native POC crowns in
[#1](https://github.com/Weichwolf/outshine/issues/1#issuecomment-6202788291).
Use `outshine_ref.vegetation.PlantPlan`, `AxisLevel`, `Blade` and
`outshine_ref.vegetation.axis_surface.axis_mesh`. The existing unchanged
`plant_experiment.sapling(17)` supplies one small generic young broadleaf plant.
Its `branching-plant-2` identity is
`59fbc581bf126400b60223be7202cec8d231745e589f1a25b7a28ad05d012f73`.
It is a technical baseline, not an accepted mature oak or worldwide recipe.

Existing kernel/SpeedTree provenance remains with [#22](https://github.com/Weichwolf/outshine/issues/22)
and [#24](https://github.com/Weichwolf/outshine/issues/24); this handoff adds no new
generator algorithm or decompilation claim. The local SIGGRAPH1985
[Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
PDF-page11/Fig12–13 was opened: reusable stem-attached leaf configurations are
relevant context. Its photograph-mapped leaf is not used; all current surfaces
and material pixels are our existing procedural products.

## Fixed input and native handoff

`reference/tree-entry-sapling-17.json` contains the exact existing recipe,
identity, geometry subdivisions and metadata for its NPZ. The NPZ is a newly
admitted **fixed test input**, intentionally captured from fa1b29962 after the
existing independent `check_plant` controls. Normal tests never create/update
these reference files. Its74,081 compressed bytes hold16 columns/93,419 payload
bytes; decoding NPZ containers is not a proposed native asset ABI.

Existing pins remain unchanged:64 axes,571 leaves, eight variant counts
`[71,72,72,72,71,71,71,71]`, independent parent attachments and petiole/node
agreement within5e−16m. Recipe identity is also fixed explicitly. Source tests
and library implementations are unchanged. New fixed-row physical/frame
comparisons use absolute1e−14 against the historical Float64 data, while same
process recipe/partial replay keeps exact integer records and the original
stricter geometric controls. This does not relax an existing test or native gate.

Axes expose UInt64 IDs, Int64 parent IDs(-1 root), Int16 seven-slot paths(-1
padding), Float64 origins/frames/lengths/bends/parent parameters/radius pairs.
Leaves expose separate UInt64 IDs and owner-axis IDs, UInt8 variants, Float64
nodes/origins/frames/stem parameters. Matrices are row-major3×3 arrays whose
columns are the local basis; `world = origin + frame @ local`. Units are metres,
world Z up, organ-local X transverse/Y forward/Z normal. Petiole extends back
to the saved node. Camera and geometry subdivisions do not change these rows.

Native integration can first reproduce these complete static rows/identities
and the existing analytic surfaces, then compare images at the same camera and
light. This does not require importing the Python preview or its material API.
Keep source preparation, reusable prototype products and current shading
separate. World placement/reservations/source IDs belong to the caller. A shared
prototype's plant-local organ IDs still need a world-instance identity prefix.

PlantPlan currently uses circular axes. `CubicSpine`/`ArcMeasure`/`SpineField`/
`SpatialAxis` are separately checked public components, **not** a fully bound
spatial-spine PlantPlan consumer. Mature conifer work in draft PR #202 is not
this reference. Closed axis tubes meet at prepared parent sites; this is not a
welded branching solid or complete SpeedTree joint/architecture solver.

## Products, checks and actual picture

`tree_entry.py` restores the recipe, checks its rows and stores one combined
wood mesh plus eight canonical leaf meshes. The exact571 saved leaf placements
are grouped by their eight variants; there is no resampling.
There are25,600 wood triangles and260 per canonical blade,174,060 after leaf
instancing. Nine mesh products occupy4,249,632 array bytes, separately from the
93,419-byte plan. The renderer loads each mesh once; its preliminary array read
loads tables only, avoiding duplicate full geometry decode.

The existing own256×256 vein/pigment/physical-height field from the fern study
is reused, with metallic-roughness plus qualitative12% thin transmission.
Wood uses the existing procedural noise preview. Source CORNER normals feed
the object/world inverse-transpose normal transform before bump for both.
These materials are not species-calibrated bark/leaf optics, and this increment
does not change shared shader/native bindings or library APIs.

Four new controls plus six existing PlantPlan controls pass10 source and10
installed-wheel tests. They cover original whole-plant controls, fixed complete
rows, reversed partial requests, budget refusal before placement, corrupted
leaf ownership, actual petiole/node agreement, closed tube edges and all
face/corner orientations. Cache tests forbid plan/leaf/axis/RNG construction.
The wheel is the existing PR #329 environment; no new library build is claimed.

The whole1920×1080 EEVEE/llvmpipe plant image was opened. It shows actual closed
branches, attached stems and spatial individual leaves rather than a crown
body. The slender regular architecture, repetitive margins, sparse fine twigs
and simple preview materials remain visible limitations. No botanical, LOD,
wind, full forest, native runtime or device admission follows.

The first3.5m orthographic width clipped the crown. An independent projection
of wood vertices and every transformed leaf AABB finds vertical extent1.01655m
against half-view0.984375m. Corrected4.5m width gives1.265625m half-height;
the conservative complete geometry fits. Recipe, rows, materials and lighting
are unchanged. This framing counterexample is retained numerically.

## Completed costs

Product source4412df2e5; corrected renderer1d7640dc7. Fresh build includes reads,
reference comparison, meshes, compression and own maps:0.481s wall/0.478CPU-s,
59.92MiB peak RSS, terminal0. Internal plan/reference0.0898s, geometry0.0546s,
total builder0.2602s. One combined wood and eight leaf builders avoid571
individual leaf mesh calls; known array payloads are not all copied bytes.

Fresh process reads70 plan/geometry arrays/4,343,051 bytes in0.04261s while
source constructors are forbidden;0.369s process wall/0.355CPU-s/53.82MiB RSS,
terminal0. Procedural PNG maps are decoded separately by Blender, so this is
not a full renderer/material readiness or native asset-loader timing.

Six alternating fresh plan comparisons: full64-axis/63-leaf-group construction
and reference check median65.10ms; one requested twig plus its two ancestors
and one leaf group2.85ms. They request different content, so this is an avoided
work illustration, not an equal-task algorithm speedup.10,000 warm table calls
return the same immutable arrays, mean0.561µs. Separate untimed tracemalloc:
full357,859 retained/459,510 peak bytes/3,074 retained blocks; partial9,989/
17,055/106. These are live traced blocks, not all allocations/copies.

Corrected whole render:94.161s wall/95.624CPU-s/2117.75MiB RSS, terminal0.
The opened detail view uses the identical inputs:111.109s/112.307CPU-s/
2108.64MiB, terminal0; setup0.336s, render108.817s. Whole setup0.345s,
render91.989s. Both use1920×1080,16 samples and the same lights/materials.
Software-renderer time and this small tree do not establish a GPU/device/frame
budget. Original process/load/geometry receipts and current images remain in
`/tmp/outshine-vegetation-tree-entry-4412df2e5/`; research pictures stay local.

## Reproduce

Use `.venv` and pinned experiment dependencies, with
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation` and
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 LP_NUM_THREADS=1`.

```sh
.venv/bin/python -m unittest tree_entry_checks test_plant_plans
.venv/bin/python test/experiments/vegetation/tree_entry.py \
  --output /tmp/outshine-vegetation-tree-entry-products
.venv/bin/python test/experiments/vegetation/render_tree_entry.py \
  --inputs /tmp/outshine-vegetation-tree-entry-products \
  --output /tmp/outshine-vegetation-tree-entry-shot --view whole
```

The product output must be fresh. `--view detail` changes the diagnostic camera
only; both views consume the same complete near products. The literal recipe
and all old controls remain available independently of the new reference NPZ.
