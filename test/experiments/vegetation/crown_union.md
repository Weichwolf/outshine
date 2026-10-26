# One connected cached crown wood body — #429

The #423 crown has continuous limbs but separate overlapping wood solids. A
single outer surface is needed before physical fork collars and coherent bark
joins can be added. This experiment unites the complete saved canonical crown
and its leafy shoots without changing any leaf placement, mesh or material.
It is Python research, with no library/native/default species API change.
The accepted whole/closeup images look almost unchanged: topology alone does
not produce natural forks. No large visible-quality improvement is claimed.

## References and representation

Local SIGGRAPH2022
[Interactive and Robust Mesh Booleans](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf),
page2 optically inspected, motivates conforming intersection patches and warns
against treating floating-point success as a geometry certificate. Manifold3D
3.5.4 is the existing dependency, not an implementation/oracle of that paper.
SIGGRAPH1985 [Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
pages3/5 and existing Ghidra `ComputeWeldRing` parent/start-ring delegation were
also consulted. Static vendor evidence does not provide executed numeric weld
parameters. No vendor implementation/assets are copied.

`crown_union.py` consumes the saved eight-sided dominant-limb rings and existing
four-sided short shoots. It is deliberately limited to this prepared product
schema. Each pair of adjacent rings becomes one temporary closed loft cell;
original side triangles/normals stay bit-identical. Manifold batch union removes
their shared caps. Direct whole-limb union is retained as a rejected comparator:
the same complete-output check finds flipped source-normal/facet orientation.
Reordering alone did not fix the earlier isolated bare-crown counterexample.

Exactly coincident shoot roots also leave problematic cap remnants. Prepending
a one-radius overlap into the parent preserves every old shoot side triangle,
leaf anchor and tip. All5920 new prefix-ring vertices are independently inside
the original crown cells. This is added hidden wood, not moved foliage.

The cell result includes roundoff-scale flat closed components. Only components
with at most8 faces/6 vertices and maximum fitted-plane distance at most
128×Float64 epsilon×max(1,absolute coordinate) may be removed. All89 discarded
components retain their coordinates/faces and measured threshold in the report.
This is an explicit numerical cleanup criterion, not a global error or robustness
proof; it would be wrong to discard arbitrary small-volume components.
The remaining result must be one positive body. Non-root limb caps, intermediate
ring caps and shoot-root caps are forbidden by source face ID. Output and source
budgets are explicit; failed/oversized results never become partial assets.

Per-face source kind/node identifies exposed canonical limbs/shoots. Covered
sources need no exterior face:75 shoots are completely internal. Their IDs and
the complete saved spatial plan/recipe remain in the product root. The logical
source identity stays fixed; the geometry identity includes source geometry,
recipe and Manifold version. This is a reference product, not the native asset
cache format or a complete new plant/species/LOD contract.

## Shading comparison and optical limits

`--join-shading angle` applies the existing angle-weighted source-normal method
only at actual source-contact vertices, with its45° and orientation rejection.
The shared helper was extracted without changing existing graph consumers.
70547 corners change on this case, but the opened closeup shows stretched light
streaks at sharp geometric forks. This variant remains an explicit comparison;
the default preserves source normals. It is not a substitute for geometric
collars. Uniform/area/angle controls still compare against independent scalar
angles and split invariance; hard boundaries remain preserved.

Default1920×1080 whole pair and matched node13 closeup were opened. The final
default's entire render-input directory is byte-identical to that frozen export,
so those original images are reused. Software EEVEE/llvmpipe, four/eight samples:
whole45.11s/3729.6MiB process peak; closeup54.93s/2449.6MiB. No GPU/frame claim.
Blender Exact was also probed locally as an alternative:61.3s,3897MiB and2348
degenerate triangles after its Float32 output; not admitted. This is a diagnosis
on these inputs, not a general ranking of Boolean libraries.

## Controls and costs

38 source and38 installed-library consumer checks pass; no library source change
or new-wheel claim. Independent controls cover analytic union/prefix volumes,
source-side preservation, scalar weighting, occupancy, flat-shell rejection and
capacity/incomplete-output refusal. `crown_union_replay.py` checks all148066
triangles, positive finite facets and unit oriented normals, edge incidence and
opposite directions, all74005 single vertex fans and one connected component.
Euler−28/genus15 comes from physical source intersections, not logical branches;
the genus-zero contract of an isolated fork is deliberately not reused here.

All3580 node/shoot interior probes,128 source-OR probes and5920 contained-prefix
vertices pass independent Float64 solid-angle tests. Those exhaustive selected
anchors plus finite samples do not certify every surface point or self-intersection.
Full replay takes219.54s locally: a deliberately expensive verifier, not generation.
It also reads finished products with growth/placement/union forbidden and checks
byte-identical leaf/material files plus saved covered-source identities.

Complete wood122512→148066 faces; whole instanced tree1396288→1421842 (+1.83%).
5678 source components/193536 temporary source faces. Under recorded concurrent
verification load, direct limbs fail in2.16s; admitted cell-union medians5.88s,
plus1.77s source preparation. Final export7.99s,432.8MiB process peak; comparator
process494.3MiB. Known input NumPy arrays17034; geometry copies7239840B and face-ID
writes1548288B. Kernel work/allocations/traffic remain unmeasured. Finished array
payload22209228B, including normals, provenance, surface and corner UV.
Cache reads avoid all these union/cell costs. No equal-output speed improvement,
native Float32, runtime/device, botanical, wind or forest/LOD admission follows.

Exact frozen commits, terminal receipts, hashes and original temp paths are in
the PR. Keep current inputs/images and compact negative evidence; do not stack
full obsolete union exports. Existing requirements already pin Manifold3D.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest crown_union_checks limb_checks crown_shoot_checks test_spatial_axes test_plant_plans test_blades
.venv/bin/python test/experiments/vegetation/crown_union_experiment.py \
  --inputs /tmp/outshine-limb-products --plan /tmp/outshine-limb-source \
  --output /tmp/outshine-crown-united
.venv/bin/python test/experiments/vegetation/crown_union_replay.py \
  --inputs /tmp/outshine-limb-products --plan /tmp/outshine-limb-source \
  --products /tmp/outshine-crown-united --output /tmp/outshine-crown-replay.json
```

The input exporters and their full commands are in [continuous_limbs.md](continuous_limbs.md).
