# Demanded geometric crown collars — #443

The joined cached crown from #440 still has sharp branch valleys. Local implicit
collars add physical wood at explicitly requested forks while every original
leaf, material input and canonical plant/stand plan stays fixed. The reference
consumer exports a complete joined tree for optical/correctness checks; this is
not a proposed native cache, incremental upload or runtime batching architecture.
Per-fork meshes are identical under subset/reversed requests and can be reused.

## Reference and construction

Local SIGGRAPH1985
[Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
pages5/6 were optically inspected: dominant limbs, outgoing tangent control and
ramiform saddle surfaces distinguish branch architecture from normal smoothing.
Existing Ghidra `WeldStart` shows parent lookup, a parent operation and conditional
root-position assignment. Property/virtual-call semantics remain partly unknown;
this is static evidence, not an executed numeric vendor weld oracle. No vendor
code/assets are copied. This implementation reuses our CurvedFork/tapered segment
envelopes and the dense/block grid methods documented at #28; it does not copy
the paper's parametric ramiform or change logical attachment points.

The current research cache schema has0.14m pipe-root radius and eight-sided limbs.
Restore its saved canonical plan, partition dominant paths, then resolve only
requested non-root side forks. Parent windows extend2.5parent radii on each side;
each side branch contributes the same chord-distance span. Unsupported complete
intervals fail. At most8 forks/15 side branches per fork are accepted.

Existing C1 Hermite centres and PCHIP radii are sampled into linearly varying
sphere envelopes. Radius factor0.96cos(pi/8) starts within the octagonal carrier.
Each branch takes the maximum over its segments; smooth mixing occurs between
branches, avoiding extra blending merely from inserting stations. Blend has m²
units: strength×parent radius×largest side radius, with strength in(0,2].

Centre/radius chord error is bounded by(max|c″|+max|r″|)Δu²/8. Cubic acceleration
and PCHIP second derivatives are bounded at active polynomial-piece endpoints.
Stations refine up to65 regular samples plus the fixed fork anchor, or fail;
default centre-plus-radius tolerance is1mm. This bounds the sampled carrier,
not the final mixed field or whole Boolean surface. The selected three fields
use104 segments and have maximum carrier bound0.7951mm.

Marching-Cubes resolution is explicit per fork. The existing conservative block
sampler retains all crossing-cell values; dense/block meshes match exactly.
Curved fields are not assumed globally star-shaped. Independent first-exit
bisections use256/512 sign scans and42 refinements, compared with direct
Float64 triangle-ray intersections. These finite probes do not certify all rays.
The fixed admission criterion is maximum2mm on those probes, decided before
measurement:96 cells fails fork13 at2.263mm. Only that fork is raised to128;
forks14/28 stay96. Selected944 probes give maxima0.589/0.851/0.486mm and scan
refinement change≤5.3e-16m. The earlier failed96 profile remains negative evidence.

## Whole product and optical result

Add closed local hulls to the finished cached wood with existing Manifold3D and
source-normal transfer; do not regenerate growth, whole axes, organs or textures.
Inputs are validated before kernel work. Only the existing explicitly bounded
subprecision flat-shell cleanup is allowed; other disconnected/nonpositive,
misoriented or oversized outputs fail before export. Kind2/source-node identifies
new collar surfaces; all canonical source IDs and the original plan remain stored.
No-demand calls avoid plan IO/fields/union, retain the original geometry identity
and use the original cached wood. Solver choice does not change geometry identity.

Opened matched1920×1080 views show physical widening and a rounded silhouette at
the lower main fork, with a second smaller fork checked separately. Foliage is
unchanged. This is a local geometric improvement, not completed SpeedTree quality:
octagonal parent skin, bark quality and some shading seams remain. Smooth C1
intersection with arbitrary parent skins is unproven. Optional ray-faded normal
blending fails input orientation on these fields and remains a rejected explicit
comparison; no invalid normals are repaired by flipping or silently published.

The accepted whole tree has155588 wood faces versus148066; complete instanced
tree1421842→1429364 (+0.529%). Independent topology checks give one connected
body,77766 single vertex fans, Euler−28/genus15 from existing physical contacts.
Centroid-relative signed volume0.340034550812477m³ adds0.000527677626700m³.
320 independent source-union material probes pass, including96 plant nodes,
128 offsets around new surface faces,96 points outside all collar bounds and61
new-material observations. These are selected probes, not a global self-intersection
or material-equivalence certificate. Whole/new local meshes have finite positive
triangles and unit outward-facing corner normals. All37464 leaf rows, eight blade
meshes, maps and complete plan/IDs remain byte-identical. Cache reads explicitly
forbid growth, placement, fields and union. No native Float32/device, forest/LOD,
wind, botanical-species or full worldwide coverage admission follows.

## Work, checks and reproduction

44 source/installed-reference-consumer controls cover exact parabola chord error,
piecewise PCHIP bounds, demand/station/output refusal, dense/block equality,
subset/order invariance, analytic sphere exits and existing plant/wood contracts.
Library source is unchanged; this is not a newly built wheel claim.

Same accepted fields/resolutions, three trials: dense median10.94s, blocked4.84s.
Point queries2094157→362657; segment-point pairs96480712→15094816, plus1548992
segment-block pairs/33353 blocks. Summed grid payload8376628B is unchanged;
blocked adds2094157B masks. Grids are processed sequentially: sums are not peaks.
Sampler-process peak129.3MiB. Final complete export8.06s/390.8MiB process peak;
union1.10s. Known12 kernel-input NumPy arrays copy8437368B geometry and write
1875088B face IDs. Internal allocator/traffic/normal-transfer work remain unmeasured.
Finished whole array payload23337528B. This is CPU generation work, not a frame
or device budget. Actual render receipts identify software EEVEE/llvmpipe.

Exact commits, commands, current images, negative cases and terminal receipts are
linked in the PR. Research images stay in system temp, not a published gallery.
Original complete source products and canonical inputs are retained.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest crown_collar_checks crown_union_checks limb_checks crown_shoot_checks test_spatial_axes test_plant_plans test_blades
.venv/bin/python test/experiments/vegetation/crown_collar_experiment.py \
  --inputs /tmp/outshine-crown-united --output /tmp/outshine-crown-collars \
  --nodes 13 14 28 --cells '{"13":128,"14":96,"28":96}'
.venv/bin/python test/experiments/vegetation/crown_collar_cost.py \
  --inputs /tmp/outshine-crown-united --products /tmp/outshine-crown-collars \
  --output /tmp/outshine-collar-cost.json
.venv/bin/python test/experiments/vegetation/crown_collar_replay.py \
  --inputs /tmp/outshine-crown-united --products /tmp/outshine-crown-collars \
  --output /tmp/outshine-collar-replay.json
```

The base producer and input exporters are linked in [crown_union.md](crown_union.md).
