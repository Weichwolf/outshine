# Directed branch posture on a saved crown — #487

The #412 spatial crown has a strongly upright, broom-like architecture. This
experiment adds coherent directional branch posture to that *same saved plan*.
Whole front/oblique comparisons show a wider crown with more varied limb
inclinations. It also opens visible gaps. This is an additional authored form
control, not a completed botanical recipe or an automatic improvement for every
species. No biome default, reference pin, library or native API changes.

## Definition and sources

Inputs are topologically ordered metre positions p, parents, a per-node mobility
m in[0,1], unit target g, curvature k in rad/m and a local turn cap. Root position
and source segment lengths stay fixed. Each node inherits its parent's rotation,
then turns its incoming segment toward g. Children inherit that complete rotation;
independent world-space displacement would break their attachments.

The own direction flow is dt/ds=k*m*(g-(g·t)*t). For inclination theta to g,

    tan(theta_final/2) = exp(-k*m*length) * tan(theta/2).

`atan2` evaluates the half-angle expression without division by zero. The turn
axis is t×g, with positive angle theta-theta_final, limited by the explicit cap.
SciPy Rotation supplies proper rotation matrices. Exactly parallel/antiparallel
segments have no chosen turning axis and remain collinear; the implementation
also leaves unresolved cross magnitudes≤1e-14 unchanged. No arbitrary random
axis is inserted. Zero total curvature/mobility returns bit-identical positions.

This is discrete forward kinematics, not elastic bending, light competition or
an inferred SpeedTree growth equation. Each straight segment retains its length;
its *orientation state* follows the exact direction flow before capping. Inserting
collinear segments preserves the final orientation for constant mobility when
the cap is inactive, but changes the piecewise-linear centreline approximation.
The source's maximum segment length is0.16m. No continuum geometry-error or
arbitrary-resolution invariance claim follows.

Local SIGGRAPH1984 [Global and Local Deformations](../../../doc/references/vegetation/siggraph/1984-global-local-deformations.pdf),
PDF page4, was opened optically: successive basis rotations and transformed
normals motivate coherent deformation. Our rigid-part transform uses its proper
rotation for normals, rather than treating a non-rigid Jacobian as a rotation.
Weber/Penn SIGGRAPH1995 [Realistic Trees](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf),
PDF page6/§4.8, was also opened: signed vertical attraction permits upright and
hanging branch character. Its published rule is not our ODE.
Existing local Ghidra exports `ComputeDirectionForce`0x20e5b30 and
`ComputeReturnForce`0x20e4a90 were reread: normalization/strength and optional
profile weighting are visible. Their incomplete signatures do not establish
where/how Modeler combines growth forces. No new numeric vendor oracle or
new Ghidra run is claimed; no vendor code/assets are included. No web research.

## Whole product, organs and scope

The unchanged source is the complete120m²-authored #412 crown,
`e513ebbd6a122d36431d5fb3f6ba7727d03c171c9acc6d829c31b188248f9c54`:
2100 nodes,1480 leaf hosts,37464 leaves/eight variants. Its actual triangulated
leaf area is126.003141944m², identically shared by both products. No extra leaves,
scale/color correction or crown envelope is rendered.

The study uses g=(0,0,-1),k=0.25rad/m,cap0.35rad and an authored mobility
clip(1-(pipe_radius/0.07m)^2,0,1). This mask holds the thick bole while permitting
thin limbs to turn. It is a shape recipe, not measured wood stiffness or a law
for a named taxon. Leaf-anchor XY ranges change from4.871×5.050m to7.464×7.717m;
height remains similar. A changed footprint needs fresh habitat/reservation
admission; no old source boundary automatically authorizes the wider crown.

Each saved leaf group rotates rigidly around its source host. IDs, variants,
physical meshes, petioles and within-shoot positions remain coherent. Leaves do
not readapt to new light. Existing `CrownShoots.group` preflights demand; an
insufficient request places nothing. Reverse host requests reproduce the whole
posed tables. Root/source identities remain distinguishable from pose identity.
Changed posture invalidates geometric visibility/transfer products such as #481.

Both sides deliberately use the identical older closed per-segment wood layout.
Binding is checked against its exact reconstruction once when building this new
product. Every triangle has one rigid owner; cross-owner faces are rejected.
This retains114528 wood faces/1388304 total instanced triangles. It does **not**
replace the connected/welded skins or collars from #429/#443; those require an
appropriate continuous binding/remeshing after this form decision. Centreline
attachments remain connected, not a welded-surface certificate. Self-intersection,
collision and smooth junction quality are not certified by preserved lengths.

`spatial-crown-posture-1` is only a private experiment product. It binds source,
parameters, mobility and a payload checksum. Finished arrays load with growth,
kinematics and leaf placement forbidden; a separate check replays demanded
organs. This proposes no shared asset/cache contract or new catalogue system.
Wind, seasonal response, full forest/LOD/native/device admission stay open.

## Checks, actual images and costs

31 source and31 installed-reference-consumer controls pass; library unchanged,
no new wheel claim. Controls include independent `solve_ivp` direction integration
and scalar Rodrigues/FK at a fork, analytic subdivision, rigid-motion equivalence,
proper rotations, zero/parallel/antiparallel/capped cases, invalid sources,
unchanged identities/petioles, rejected cross-owner geometry and corrupted or
stale saved products. The whole reversed replay checks all1480 hosts/37464 leaves,
all wood areas, unit outward normals and unchanged physical blade meshes.
Maximum segment-length change5.56e-16m; maximum wood-face area change6.92e-17m².
These finite floating checks are not a universal collision/geometry proof.

Both full1920×1080 front/oblique pairs were opened. Blade meshes, populations,
materials, light, camera and scale match; only the skeleton pose and attached
rigid parts change. The wider form is visible, while bare interiors, repeated
terminal fans, generic bark and segmented junctions still limit realism. Neither
form is accepted as a new Quercus/Salix species. No images are published to doc/.

Build/replay commit f03fe4086 on Intel i5-1135G7/16GB, single numerical thread:

| Stage | Local measured cost |
|---|---|
| Source decode / exact binding validation |0.0867s /0.2606s|
| Skeleton posture / full rigid geometry transfer |0.01075s /0.04821s|
| Compressed export |1.1465s|
| Serial versus depth-batched posture,3 alternating trials |median0.30531s /0.01017s|
| Complete diagnostic build, terminal0 |3.137s wall,3.130CPU-s,154.19MiB RSS|
| Fresh finished-pose+two-product read |0.17698s|
| Fresh process including all independent replay checks, terminal0 |2.853s wall,2.842CPU-s,145.38MiB RSS|

Both solvers evaluate2099 edges and agree to3e-14. Layer batching reduces
Rotation calls2099→55; source/sort/depth setup is included. Skeleton outputs are
201600B, source buffers75600B, depth8400B, index order16800B. Explicit new rigid
geometry results total13388688B; unchanged IDs/faces/UVs are shared in memory.
Total internal allocation/copy/traversal counts are unmeasured; SciPy kernels,
sort temporaries and compression are not free. No strict linear sort guarantee
or application-only versus complete-render speedup is claimed.

The private paired export is12.78MB (duplicates needed only for comparison), not
a proposed runtime format. Original render commit e95ec2f71: front47.817s wall /
56.301CPU-s /3648.97MiB, oblique43.890s /52.025CPU-s /3634.15MiB; both terminal0.
Blender5.2.2 EEVEE,4 samples; separate initialized backend probe confirms OpenGL
Mesa llvmpipe/LLVM19.1.7 software rendering. EGL fallback warnings precede the
successful frames. Exact arrays/dtypes and maps for24 paired products match the
final build; metadata-only checksum/replay additions required no image rerender.
These are local offline CPU costs, not GPU/frame/A18 measurements.

Reproduction uses the retained original complete #412 inputs; private outputs
must be explicit system-temp paths. Exact commits, image hashes, terminal
receipts and retained source paths are linked in the PR.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest posture_checks crown_shoot_checks broadleaf_crown_checks test_plant_plans test_blades -q
.venv/bin/python test/experiments/vegetation/posture_experiment.py \
  --inputs /tmp/outshine-vegetation-spatial-crown-a71e9857f/dense-products \
  --output /tmp/outshine-vegetation-crown-posture-products-2e61b6f8d
.venv/bin/python test/experiments/vegetation/render_spatial_crown.py \
  --inputs /tmp/outshine-vegetation-crown-posture-products-2e61b6f8d \
  --output /tmp/outshine-vegetation-crown-posture-front-2e61b6f8d --view front
.venv/bin/python test/experiments/vegetation/posture_replay.py \
  --source /tmp/outshine-vegetation-spatial-crown-a71e9857f/dense-products \
  --inputs /tmp/outshine-vegetation-crown-posture-products-2e61b6f8d \
  --output /tmp/outshine-vegetation-crown-posture-replay-2e61b6f8d.json
```
