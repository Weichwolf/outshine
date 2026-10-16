# Continuous dominant limbs — #423

The cached spatial crown from #412 contains visible rings, abrupt radius steps and
overlapping caps along otherwise continuous growth axes. This experiment replaces
only that wood with complete dominant limb sweeps. All canonical nodes, leafy
stems, organ rows, eight leaf meshes and procedural material inputs stay fixed.
It is additional Python research, with no new library/native/species default API.

## Reference and construction

Bloomenthal, SIGGRAPH1985,
[Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
pages3/5 were optically inspected: moving section frames and dominant continuation
precede separate ramiform junctions. This implementation uses existing public
`CubicSpine` C1 Hermite interpolation and rotation-minimizing transport frames,
rather than reproducing the paper's C2/Frenet construction. Existing local Ghidra
exports of `ComputeWeldRing` show parent/start-ring delegation; they do not supply
an executed numeric vendor oracle or justify calling overlapping limbs welded.
No vendor implementation or asset is bundled.

`limb_paths.py` partitions every directed source edge exactly once. The strongest
child continues the parent; relative1e-12 strength ties use incoming alignment,
then canonical child ID. Limb identity is its first child ID. Nodes/radii are
bounded and invalid parents or collapsed edges fail explicitly.

`limb_surface.py` interpolates the original node positions, averaging adjacent
unit chord directions with the shorter local chord length. Interior derivatives
are shared. SciPy PCHIP provides a positive monotone C1 radius for the tested
nonincreasing pipe radii; a side limb starts with its own child radius, and the
terminal radius uses the existing0.35 taper. This intentionally changes the old
independent-cylinder solid. It does not preserve its volume or swept envelope.
At most65 anchors,16 subdivisions per edge and64 sides are accepted; face/vertex
capacities precede curves and frames. Longer paths are rejected, not truncated.

The existing transport/sweep/cap functions share interior ring vertices and side
corner normals. Each limb has only two hard terminal caps. Sampled radius×curvature
and positive facet/corner-normal orientation reject underresolved products;
neither is a continuous regularity or global self-intersection certificate.
Non-dominant limbs remain separate closed bodies. Their caps/intersections at
forks are still visible: #28 retains the actual junction/union requirement.

## Fixed whole-tree experiment and observed image

`limb_experiment.py` consumes the stored dense spatial product, verifies the old
wood prefix independently, and replaces only that prefix. Existing leafy twig
vertices/normals and relative indices are retained exactly. Finished products
keep the logical source identity and receive a separate geometry-version identity.
The paired preview reuses its existing folder slots: `recursive` now means old
independent segments, `spatial` means continuous limbs, as labelled in the report.
The shared renderers gain optional saved detail-camera/sample metadata; defaults
and existing content remain unchanged.

Actual source:2100 nodes/2099 edges,499 limbs, longest path55 nodes,37464 leaves.
Source identity `e513ebbd6a122d36431d5fb3f6ba7727d03c171c9acc6d829c31b188248f9c54`;
geometry identity `39c4ae9207795846fd8df4f092576e52bf44b7fb4f65999ab258cd44cdbf2e20`.
Two subdivisions per edge produce75152 body triangles versus67168 before.
The complete instanced tree changes1388304→1396288 triangles, about+0.575%.
Terminal cap faces drop33584→7984; stations4198→4697. This is a quality change,
not a smaller complete product. One subdivision fails orientation on7 limbs and
is explicitly rejected. Four subdivisions produce142320 body triangles.

Opened whole pair and both matched node13 closeups at1920×1080 show unchanged
foliage placement, removal of transverse seam rings and smoother major limbs.
Whole-tree improvement is modest; side-branch caps and intersections remain.
The generic controlled crown envelope, bark quality and botanical architecture
still fall short of SpeedTree. No new species/forest/LOD/wind admission follows.
EEVEE uses software llvmpipe: whole pair43.16s/3540MiB process peak, closeups
54.30/54.79s and2409/2412MiB. Those are offline diagnostic costs, not GPU or
native frame/device evidence. Four whole-view/eight closeup samples keep noise.

## Controls, cost and reproduction

32 source and32 installed-reference-consumer checks pass, including exact edge
partition/ties, analytic frustum volume, anchors, radius, shared normals, capacity
preflight and existing spatial-axis/foliage contracts. No library source changes,
so this does not claim a newly built wheel. `limb_replay.py` independently checks
all499 closed oriented components, edge incidence/Euler characteristic, positive
volumes/facets, exact complete replay, unchanged leaf/material file hashes and
twig arrays. Cache reads forbid growth, foliage assembly and limb meshing.

`limb_cost.py` measures three untraced builds and a separate traced build, not
equal-output solver competitors. Medians: old independent tubes11.07ms,
two-subdivision curves1110.66ms, four-subdivision curves1126.77ms on this host.
The499 Python spline/frame calls dominate relative to vectorized straight tubes;
no generation-speed improvement is claimed. Cache hits consume finished geometry.
Known payloads: old3677448B (F32 geometry/I32 indices), new7539152B (F64 geometry,
I32 indices and U32 owners); traced peaks14.25/15.87MB, process measurement146MiB.
New aggregation alone reads+writes at least15078304B; this excludes curve/frame,
allocator, serialization and other traffic. Live traced blocks are not allocation
event counts. These are research build costs, no runtime/native budget clearance.

Exact commits, commands, inputs, images and terminal receipts are linked in the PR.
Research output is local in the system temp directory, not a replacement gallery.
The saved input is reproducible with the #412 dense exporter; normal controls do
not rewrite pins or oracles.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest limb_checks crown_shoot_checks test_plant_plans test_blades test_spatial_axes
.venv/bin/python test/experiments/vegetation/spatial_crown_experiment.py \
  --output /tmp/outshine-limb-source --area 120
.venv/bin/python test/experiments/vegetation/limb_experiment.py \
  --inputs /tmp/outshine-limb-source --output /tmp/outshine-limb-products
.venv/bin/python test/experiments/vegetation/limb_replay.py \
  --source /tmp/outshine-limb-source --products /tmp/outshine-limb-products \
  --output /tmp/outshine-limb-replay.json
.venv/bin/python test/experiments/vegetation/render_spatial_crown.py \
  --inputs /tmp/outshine-limb-products --output /tmp/outshine-limb-whole --view front
.venv/bin/python test/experiments/vegetation/render_tree_entry.py \
  --inputs /tmp/outshine-limb-products/spatial --output /tmp/outshine-limb-detail --view detail
```
