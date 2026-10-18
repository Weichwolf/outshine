# Attached short shoots replace crowded leaf fans — #491

The implementation now lives in the installable [library core](../../../lib/twig_clusters.md)
under #746. Experiment, replay, cost and renderer clients import that same core;
the method and historical image/cost findings below are unchanged.

The #412/#487 crown carries37464 physical leaves on1480 short0.16m carriers,
usually25/26 leaves per carrier. The resulting dense fans remain visible in the
whole tree. This experiment partitions those *same leaf IDs/variants* among7400
attached curved short shoots. Whole front/oblique views show more connected
foliage and fewer fan-shaped repetitions. No extra leaves, filled canopy, altered
blade size/material or species/biome default change supplies that improvement.

## Stable hierarchy and physical attachments

`TwigClusters` receives canonical host IDs/counts, source leaf variants, tree-local
metre origins and right-handed frames from the saved #487 pose. It preflights
65536 hosts/100000 leaves/16384 twigs, at most128 leaves/32 twigs per host. Source
buffers are immutable. Summary/twig requests and refused leaf budgets generate
no leaf poses. A host request preserves source global leaf IDs and variants;
reverse or partial queries reproduce the same complete tables.

For n leaves and limit6, create ceil(n/6) twigs and distribute the remainder so
counts differ by at most one. The actual source uses5twigs per host, each with
5/6leaves. Twig ID=hostID*32+ordinal is independent of request order. Attachment
fractions are stratified with bounded seeded jitter in the carrier's interior.
Existing `arc_frame` evaluates actual carrier positions/tangents. Golden-angle
azimuth plus bounded variation and inclination0.65..1.25rad place each child;
existing `normal_frame` supplies its rigid frame. No new phyllotaxis/frame solver.

The declared carrier radius profile is linearly interpolated at the attachment;
child base radius is0.65times that value, tip0.3times base. Profiles and the
slenderness bound are explicit inputs, not inferred wood allometry. The chosen
control retains its old0.5→0.12mm carrier radii. Larger valid profiles are covered
by independent tests; the batching algorithm has no fixed0.325mm radius limit.

Leaf-bearing shoots reuse public `Shoot`: bend0.35rad, nominal internode0.04m,
16bounded length/seed variations and the same0.004m petiole. The count-safe length
formula produces exactly the allocated count; physical internodes range3.4–4.6cm
instead of about5.6–5.9mm on the crowded carriers. These are authored study
parameters, not a new botanical observation or admitted species recipe.

A small bank keyed by(count,variation) caches local `Shoot.detail` rows. Only32
of the capacity96 prototypes are needed here. Each requested twig transforms
its prototype; source global IDs/variants replace prototype-local IDs. Cached
host arrays retain exact queried groups. Shoot→host→plant hierarchy remains
available without expanding all organs. No current camera or light is in it.

## Exact affine radius factoring

The first complete build spends5.67s in7400 individual `axis_mesh` calls. The
selected alternative retains that scalar kernel as its independent control and
factors fixed-length/bend/taper shapes into a batchable radius basis. No radius
quantization, artificial thickening or changed length is used.

For fixed tube centre C and base radius r, vertices are C+rV. Its unnormalised
side normals are N0+rN1: the curved axial factor and linear taper derivative are
affine in r. `radius_basis` obtains the existing scalar mesh at reference r=L/64,
recovers radial directions/unnormalised coefficients, and retains exact hard-cap
normals and topology. `apply_basis` evaluates actual radii and normalizes once.
The declared slender domain is0<r≤L/8. The whole preflight checks this conservatively
against the shortest possible declared child before any organ detail.

Group by(count,variation), apply32bases, scatter into canonical twig order and
transform vertices/normals by the same rigid child frames. The old wood arrays
are kept as an exact prefix. All7400new short-shoot solids remain separate closed
segments. Neither this procedure nor the older control is a welded-junction,
self-intersection or collision certificate; #429/#443 skins/collars are retained.

Before comparison, #491 fixed vertices≤1e-12m and unit-normal difference≤1e-10
against the original complete scalar export; faces/IDs/leaf arrays exact. Final
maxima are8.89e-16m /1.55e-14; full Float32 vertex and corner-normal arrays are
identical. Independent radius/bend/cap/frame tests include radii65µm..12mm,
positive/negative bends and wider carrier profiles, not just the sampled crown.

## Sources and actual optical evidence

Local SIGGRAPH1995 [Realistic Trees](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf),
PDF page6/§4.9 and grouped representation, was opened optically. Leaf orientation,
structural levels and reusable groups motivate the comparison; its paper does
not prescribe this balanced partition or our affine kernel factoring.
Existing local Ghidra exports `GetParentTemplateNode`0x209a270 and
`ComputeTopologicalLeaf`0x20b79b0 were reread: source/target generator links and
visible graph-child status are explicit. A graph leaf is not a biological leaf
count or evidence of our prototype scheme. No new vendor numeric oracle/Ghidra
run, web research, vendor source or foreign plant/textures are included.

Both complete1920×1080 front/oblique pairs and an actual host detail were opened.
Skeleton, #487 pose,37464source leaf IDs/eight blade meshes and materials remain
unchanged. Physical triangulated leaf area is126.003141944m² on both sides, not a
claim of measured stand LAI. Candidate geometry adds236800wood faces:1388304→
1625104 total instanced triangles (+17.1%). This is a quality/cost tradeoff.

The close view uses source host208/leafIDs2600..2625 and its actual cached
carrier/new children, with a common rigid transform only. Both subjects share
camera/light/scale. An optional presentation camera in the existing renderer
allows this partial inspection; whole-view defaults remain identical. The detail
uses0.01..20m clipping. Some very thin supporting twigs are subpixel; generic
leaf shape/materials, carrier allometry and junction/bark detail remain quality
limits. Two views are no global angular/LOD bound. No new Quercus/Salix admission.

39source/39installed-reference-consumer controls pass. Library unchanged: no new
wheel claim. Fresh finished-product read forbids growth/placement/wood building;
separate reverse replay checks all1480hosts/37464leaves, petioles, outward unit
normals, positive wood triangles, exact old wood prefix and physical blade meshes.
The old posture exporter now shares one paired-product writer; a fresh old-product
regression preserves its arrays/maps/metadata. Neither cache recipe is a common
portable catalogue definition: these saved plans already contain concrete poses.
#483's small shared recipe/operator pilot remains Buildings/master-owned.

## Separated local costs and reproduction

Frozen b64366538 on Intel i5-1135G7/16GB, numerical threads1. Three alternating
trials, comparisons outside timing, identical physical outputs:

| Work | Scalar/individual | Shared/batched |
|---|---:|---:|
| Whole leaf placement |4.2206s|0.7635s|
| Whole extra wood |4.3698s|0.7208s|
| `Shoot.detail` or scalar axis calls per corresponding stage |7400|32|

Whole verified build terminal0:4.043s wall/4.038CPU-s/188.27MiB RSS. Stages:
source+plan0.0993s, leaves0.7669s, wood0.8207s, export1.7633s. Fresh paired product
read0.2750s; process including full geometry/reverse replay2.280s/222.65MiB,
terminal0. Timing is local offline CPU work, not native/frame/GPU/A18 admission.

Prototype rows23232B; full returned leaf tables5132568B. Reusing host groups
retains that payload plus Python objects: traced live/peak13.30/13.34MB and51056
live blocks, versus5.15/13.15MB and133live blocks without group retention. These
are live/traced quantities, not cumulative allocations or complete native traffic.
Geometry results51469552B versus old wood16778352B; the paired compressed export
is about20.5MB, not a proposed runtime format. Kernel work calls drop; transformed
outputs still scale with demanded organs/triangles. Sort, temporary arrays,
concatenations, internal allocator/compression traffic remain real and incomplete.

Original scalar-input renders at6338038ee: front59.616s/3707.90MiB, oblique
58.848s/3686.05MiB; both terminal0. Shared host detail at ecdd9e8a1:32.993s/
2013.19MiB, terminal0. Blender5.2.2 EEVEE4samples on the previously verified
Mesa llvmpipe software host. Whole render load was high/uncontrolled (about8–10);
final comparison load about1–2. No cross-load renderer speedup is claimed. All
physical blade/table inputs and complete Float32 wood inputs match the final
batch product, so existing whole views also inspect its actual rendering inputs.

Exact commits, hashes, retained inputs and terminal receipts are linked in the PR.
Keep research images in system temp; publish no doc/ images or replacement gallery.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest twig_cluster_checks posture_checks crown_shoot_checks broadleaf_crown_checks test_plant_plans test_blades -q
.venv/bin/python test/experiments/vegetation/twig_cluster_experiment.py \
  --source /tmp/outshine-vegetation-spatial-crown-a71e9857f/dense-products \
  --posture /tmp/outshine-vegetation-crown-posture-proof-2e61b6f8d/final-products \
  --output /tmp/outshine-vegetation-twig-cluster-products-ba08158e4
.venv/bin/python test/experiments/vegetation/render_spatial_crown.py \
  --inputs /tmp/outshine-vegetation-twig-cluster-products-ba08158e4 \
  --output /tmp/outshine-vegetation-twig-cluster-front-ba08158e4 --view front
.venv/bin/python test/experiments/vegetation/twig_cluster_replay.py \
  --inputs /tmp/outshine-vegetation-twig-cluster-products-ba08158e4 \
  --output /tmp/outshine-vegetation-twig-cluster-replay-ba08158e4.json
```
