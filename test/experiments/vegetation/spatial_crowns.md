# Spatial crown growth with matched foliage (#412)

The existing recursive broadleaf produces repeated branch tiers. This experiment
connects the existing space-colonization skeleton to demanded public `Shoot`/`Blade`
products, preserving exactly the control's leaf variants and counts. It compares
whole plants, rather than accepting a naked skeleton or a filled crown envelope.
This is an additional generic research architecture; existing biome/species recipes,
public libraries, native APIs and reference pins are unchanged.

## Source and method

Local SIGGRAPH2009 [Self-organizing Trees](../../../doc/references/vegetation/siggraph/2009-self-organizing-trees.pdf)
motivates replacing uniform recursion with spatial competition. PDF page2/Figure2
was optically inspected. Local EG2007 [Space Colonization](../../../doc/references/vegetation/eg/2007-space-colonization.pdf),
PDF page2/Figure1, was also optically inspected: attraction points, skeleton,
geometry and organs are separate stages. Existing `colonization.py`, `nearest.py`,
pipe radii and public analytical surfaces remain the implementation base. This
does not implement the whole SIGGRAPH2009 bud/light/signaling simulation.

Prior actual Ghidra generator exports were reread locally: `CreateNodesFromTemplate`
delegates to the template's virtual function; `ComputeNodes` is a separate generator
stage. Those facts support retaining distinct structural and organ stages. They
do not establish that SpeedTree uses space colonization. No vendor implementation,
geometry, texture or scanned/photo plant asset is used.

The control is `match_area(architecture("oak",718,True), target)`: authored generic
dimensions,1105 axes, including its existing short leafy terminal order. The name
is inherited from that study, not a new botanical Quercus admission. The candidate
uses2048 authored attraction points,8m/5m growth-space height/spread, bole0.23,
0.16m steps,1.75m influence,0.22m kill, and4096-node/128-iteration capacities.
Actual dimensions differ from the recursive control; there is no same-allometry
or same-skeleton claim. Both sides have identical material, blade meshes, variant
counts, direction/scale and lighting within each image.

The first uniform ellipsoid plus tips-only foliage left naked branch webs. A
distal0.48m path-distance band adds short0.16m leafy shoots inside the crown, with
the same total leaves. Distances follow the saved parent graph, not world height.
The subsequent own asymmetric growth-space recipe uses nine smooth directional
Gaussian controls and bounded tanh radial variation. An affine normalization keeps
the source point cloud's exact per-axis bounds; point count/seed stay fixed. No
hull or envelope is rendered. This alters the point distribution, not just labels.
`--distribution ellipsoid` remains an explicit reproducible comparison.

`CrownShoots` is an experimental single-owner plan over saved Float64 XYZ metre
positions, topologically ordered Int32 parents and UInt8 source leaf variants.
Coordinates are tree-local east/north/up, Blender Z up. It retains immutable source
buffers; a recipe binds format/NumPy version, seed, shoot length/bend, petiole and
distal-band depth. Identity additionally binds all saved arrays. It does not rerun
growth or claim botanical observations when replayed.

Summary and per-host counts precede leaf placement. An explicit host-node request
returns stable global leaf IDs, variants, owner nodes, origins, stem points and
right-handed frames. Group seeds derive from the saved seed/node. Capacities
preflight complete requested leaves, with no partial group published; insufficient
capacity returns `None`. Full tables concatenate those same groups in canonical
host order. Host selection is capped by65536 nodes,100000 leaves and explicit
trait bounds. These are input/work capacities, not frame deadlines or RSS limits.

Tube bodies reuse `branch_tubes`, with pipe cross-section conservation. Separate
closed segments intersect at forks; they are not one welded manifold. The short
0.5mm-base leafy stems use3 longitudinal/4 radial subdivisions; original skeleton
tubes use8 sides. This is a local offline whole-view approximation, not a general
shadow/shading/LOD error certificate. Public axis/blade surfaces retain analytic
corner normals. Cache hits read complete arrays; growth/placement/meshing are not
repeated. Existing camera/LOD/wind/native integration is not replaced by this study.

## Matched physical quantities

Each pair preserves the exact variant vector, source leaf IDs and leaf mesh arrays.
Therefore analytical and triangulated leaf area match exactly within a pair. The
triangulated area differs from the analytical Blade area at eight subdivisions;
both are reported. Changing the authored target between pairs is a density study,
not an architectural gain.

| Requested area | Actual analytical area m² | Leaves on each side | Recursive triangles | Spatial triangles |
|---|---:|---:|---:|---:|
|55|54.954697648|17136|675444|697152|
|120|120.146054662|37464|1366596|1388304|

The final saved skeleton has2100 nodes and1480 foliage hosts. Growth ends after45
iterations/59958 nearest query points with44 unreached authored attractors. They
remain saved and reported; this is not complete attractor consumption. No source
OSM habitat is silently discarded. Growth/summary demand never builds these leaves.

Whole front/oblique1920×1080 pairs were optically opened at both leaf budgets.
The spatial form removes the repeated tiers, has connected curved-looking branch
paths and spreads foliage through a distal band. The crown remains strongly
controlled by its growth space, has exposed inner wood and lacks botanical
asymmetry/age/damage/detail. The denser pair fills more of both crowns; it cannot
be used as evidence that topology alone supplies the extra density. This admits
a local architecture alternative, not SpeedTree quality or a default species change.

## Controls, costs and persistence

Seven new controls plus existing broadleaf/PlantPlan/Blade checks pass,23 total;
the same23 pass using the installed reference wheel without `lib/src`. No library
code changes, so that is an installed-consumer check, not a newly built wheel claim.
Controls cover exact counts/IDs, reverse/subset replay, no placement on coarse or
insufficient demand, rigid frames/petiole anchors, immutable source arrays, invalid
topology/variants/budgets, distal graph distance under rigid motion, and deterministic
asymmetric point distributions with preserved macro bounds across three seeds.

`spatial_crown_replay.py` independently reads finished products with growth,
PlantPlan/Shoot placement and Blade meshing forbidden; separately restores saved
parents and reproduces all requested groups in reverse order. Entire cached mesh
indices, positive areas, normal orientation/unit lengths and exact shared leaf
areas are checked. No new pin or oracle is generated by ordinary tests.

`spatial_crown_cost.py` compares brute/KDTree on identical actual attraction points,
then summary/one/full foliage demand on the same prepared crown. The latter are
different requests, measuring avoided work, not equal-output solver competitors.
Actual query inputs and brute distance-pair/temporary-payload work are counted;
KDTree internal visits/allocations and shared duplicate-endpoint tree work are
explicitly unmeasured. Known result backing/concatenation copies are lower bounds,
not complete memory traffic. Traced live blocks/peak exclude native allocator RSS.
Process CPU/wall/RSS and load are separately recorded. Device/frame performance,
native assets, global optimality and forest coverage remain open.

Exact frozen commits, current images, terminal receipts and numeric values are
linked in the PR. Research output remains in the system temp directory. Keep
the current55/120m² inputs and needed counterexamples; do not stack full old exports.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest crown_shoot_checks broadleaf_crown_checks test_plant_plans test_blades
.venv/bin/python test/experiments/vegetation/spatial_crown_experiment.py \
  --output /tmp/outshine-spatial-crown-120 --area 120
.venv/bin/python test/experiments/vegetation/render_spatial_crown.py \
  --inputs /tmp/outshine-spatial-crown-120 --output /tmp/outshine-spatial-front --view front
.venv/bin/python test/experiments/vegetation/render_spatial_crown.py \
  --inputs /tmp/outshine-spatial-crown-120 --output /tmp/outshine-spatial-side --view side
```
