# Source grass uses graminoid forms, not the tree-height heuristic (#208)

The opened whole Koerbersee diagnosis had conifers scattered over alpine grass.
`place_stand.py` previously selected conifer kind for every site above900m at
mid/high latitude, regardless of its source landcover. The correction keeps the
existing wood form proxy rule but assigns source grass to kind2. Wood wins at
wood/grass overlap, matching the existing source-membership priority.

`source-site-plan-2` declares the new kind meanings. Original source IDs, contact
positions, frames, source indices and normalized scale variation are retained;
only the inappropriate grass form indices change. No logical site is removed.
Consumers must provide a product for every used kind rather than silently lose
sites with an old two-entry catalog. This does not identify a botanical species.
The original3.5%-of-cell grass sampling is still a sparse form control, not full
meadow or understorey coverage. Continuous fine grass density remains open.

## Shared cached form product

`grass_tuft.py` reuses the existing analytical `Blade` from #52. Its base grass
recipe is also imported by the older organ experiment, preserving those controls.
Eight seeded12-blade variants have authored lengths0.25..0.48m, widths6..12mm,
curved/folded ribbons, azimuth/lean variation and roots embedded8mm. These are
our form choices, not measured morphology of a named accepted species. Segments
change surface sampling, not the seeded blade parameters or child identities.

Export an explicit temporary product with `grass_tuft.py --output <temp>`.
The manifest, recipe and shared mesh arrays retain all variants and stable blade
indices. A canonical recipe hash identifies the form plan. The existing complete
Place export loads these arrays; no Blade meshing belongs on a cache hit.
The explicit Place product list adds this manifest directory as the third entry.

Before Blender instances, the existing physical bounds/frustum/sun-sweep demand
selects possible camera or shadow influence. Spatial grass uses one union of
both masks and one actual mesh instance per selected site, rather than duplicated
camera/sun sheets. Original site keys choose the shared form variant through
`species_site_uniforms(...,9417)`; filtered row/query order is not a seed.
Its procedural rib/reflection/transmission material reuses the existing leaf
preview. Saved form variants own their tint, removing the old ObjectInfo random
input so filtered instance order cannot change colour. This is a static CPU
reference composition, without new native/shader/LOD or wind acceptance.

The current prototype still requests physical tufts for all potentially relevant
sites; screen-error-selected near/far grass layers are not solved here. One
shared geometry product is small, but large-scale grass aggregation and its
coverage/shadow/material equivalence remain required before runtime promotion.

## Verification

`grass_place_checks.py` tests source grass/wood overlap above the old tree-height
threshold, stable sources under camera rotation, positive physical triangles,
consistent unit corner normals, sub-metre bounds, seed variation and shared blade
IDs/surface points across subdivision. Existing organ `check_blades()` remains
unchanged numerically after moving the shared grass recipe. Run camera, scene,
water and demand controls alongside these tests. Real Koerbersee cached export
must reject any organ reconstruction, retain all53,200 source IDs/contacts and
change only grass form kinds. Then render and open the same whole2560×1440
private one-sample profile before/after; do not replace the public four shots or
claim the original64-sample cost gate passed. Actual-source results pending.

## Sources and scope

SIGGRAPH first: Deussen etal.1998, population versus shared plant models,
`doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf` (previously opened
pipeline/population figures). Analytical folded blade geometry and its original
primary references/independent metrics belong to #22/#52; no new standard surface
algorithm is duplicated here. Perbet/Cani's I3D2001
[author project page](https://www.inrialpes.fr/sed/people/boissieux/PRV/PUBLIC/PRAIRIE/plaquette_pub.html)
provides the relevant geometric/volumetric/texture grass hierarchy comparison;
its prairie LOD/wind implementation is not reproduced or accepted by this fix.
No photographs, source implementation or native Place PNG are used as assets.
