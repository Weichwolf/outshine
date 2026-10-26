# Native 6.5-metre broadleaf study — #1 / #25 / #508

This ports the **existing authored oak study**, not an admitted mature species:
`broadleaf_crowns.architecture('oak',718,terminal)` and `match_area` from
`test/experiments/vegetation/`. Its trunk is6.5m, with actual smaller branches and
0.12m blades; the1.55m technical portrait is not enlarged. No plant algorithm,
recipe or accepted taxonomy was changed. The native JSON/plan/product path is
[documented separately](native_plant_recipe_json.md).

## Actual matched input and image paths

Three whole plants were built natively from canonical recipe JSON, prepared and
stored through the common asset cache, then rendered with Metal. A second native
consumer independently receives Python's complete canonical axis/leaf rows and
uses the same product/renderer. Separate cache databases and cold generator calls
prevent either consumer from substituting the other's product.

The density-only control is matched to the terminal18 plant's **actual** physical
area before poses/meshes are generated. Discrete leaf counts leave an explicit
residual:17.741m² versus17.779m², about0.22%. The third plant targets55m² and
actually has54.955m². More leaf area is a separate comparison, not free coverage.

| Actual product | Density control | Terminal18 | Terminal55 |
|---|---:|---:|---:|
| Axes / leaves |97 /5532|1105 /5544|1105 /17136|
| Unique prototype vertices |6932|75476|75476|
| Unique prototype triangles |10748|119612|119612|
| Whole-instance triangle references |198564|307836|701964|
| Encoded product bytes |1099414|4068694|5737942|

Triangle references count wood once plus blade triangles times leaf placements;
they are not measured GPU invocations. Geometry uses8axis segments,6sides and
8blade segments/1across. Each of the eight shared blade prototypes has34triangles.
The extra1008twig axes add108864wood triangles; more branches have a real cost.
These counts do not prove optimal tessellation or an appropriate world LOD.

A matched640×720camera uses local Y-up metres: eye(0,4.3,14), aim(0,4.3,0),
vertical FOV0.8rad. Both input paths give exactly the same PNG bytes for all
three whole plants. Native hashes start `8c0af8ac`(matched density control), `35ca47b2`(terminal18),
and `54d4e526`(terminal55); full hashes are retained in the private full-camera
receipt. The first closer
camera clipped the top; those views were rejected and replaced. All final whole
native views were opened. Black background/direct light isolate geometry;
there is no ground contact, sky illumination, wind or cast-shadow admission.

## What the images show

Moving leaves onto short terminal shoots improves spatial occupation compared
with piling leaves along the sparse old branches. Terminal18 reads as a small
broadleaf tree. Terminal55 fills its crown more, but does not fix the growth form:
the main stem remains unusually straight/dominant, repeated branch spacing and
thin woody hierarchy remain visible. Neither is an accepted mature Quercus robur.
Increasing leaf count alone must not substitute for a better crown architecture.
These native comparisons identify a source-form gap; they do not demonstrate a
C++ pose reconstruction failure or a Place improvement.

## Numerical scope and retained counterexample

IDs, parents, paths, owners, variants and counts agree exactly. Metre-position
errors stay below1.9e-15 in this finite cohort. The existing small-plant2e-14
Float64-component diagnostic **refuses** these larger inputs: frame components
differ, with a maximum5.51e-13. Its tests/tolerance were not changed.
At the actual Float32 interface, every axis/leaf position agrees exactly; only
near-zero frame components differ by at most2.28e-16. The separate original-row
Metal renders above establish byte-identical actual pixels for these views.
This does not establish global bit identity, a new LOD/surface-error allowance,
botanical correctness or another-view/device performance acceptance.

Original recipes, canonical rows, both consumers/commands, separate cache files,
actual logs, complete hashes and full-camera receipt remain privately under
`/tmp/outshine-native-plant-recipe-39bf9e07d/broadleaf/`. No expected table, pin,
or oracle was rewritten. No images were added to the repository/gallery.
Next work: improve/admit source growth forms and choose detail before meshing;
then integrate matching near/far products into the actual world and its budgets.
