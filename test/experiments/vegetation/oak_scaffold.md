# Mature-size oak scaffold: connected hierarchy before foliage detail

Owner: vegetation, [#682](https://github.com/Weichwolf/outshine/issues/682),
parent [#25](https://github.com/Weichwolf/outshine/issues/25). This addresses
master's request for an adult European tree interpretation beyond the1.55m
sapling. It is an executable form comparison, **not an admitted Quercus robur**.
Library/native interfaces and existing species defaults remain unchanged.

## Sources and hypothesis

First local SIGGRAPH2009 [Self-organizing Trees](../../../doc/references/vegetation/siggraph/2009-self-organizing-trees.pdf),
PDFpage2/Figure2, and EG2007 [Space Colonization](../../../doc/references/vegetation/eg/2007-space-colonization.pdf),
PDFpage2/Figure1/equation1, were opened optically. The former contrasts uniform
recursion and spatial development; the latter separates skeleton, later curve
operations, pipe sizing and organs. We reuse the existing Runions-style spatial
competition, not the complete SIGGRAPH bud/light/signaling model.

SpeedTree10's supplied `Oak_Hero_Field.spm` preview was actually opened.
Its ScientificNames metadata says Quercus robur. It shows spreading primary limbs,
uneven subcrowns, gaps and a much richer trunk/branch surface. This is a Modeler
quality oracle, not an independently verified living-tree observation. Local
botanical references for this adult broadleaf still need supplementing before
species admission; no web research was performed. Vendor photos/assets are
never used in generated geometry/materials. The existing Ghidra ComputeSpine
export at0x1cd2950 was revisited in the context of #24's established profile/
local-rotation findings; no new numeric vendor oracle or proof that SpeedTree
uses this seeded-colonization method is claimed.

## Procedure and physical source

`OakRecipe(seed=682)` declares18m growth-space height,14m spread,0.325m basal
radius,0.0015m terminal support radius,0.28m growth step,4096 attractors and90000
leaves. These are authored dimensions, not measured oak allometry. Reuse the
existing asymmetric attractor field, scaled from its8×5m source. Both compared
plants receive exactly the same field and leaf inventory. The control starts
from a shared slightly curved cubic trunk only; the candidate additionally has
six cubic main limbs at distinct stem positions. The old metadata's
"straight-trunk" label means trunk-only, not a perfectly straight analytic line.

Public `CubicSpine` checks regularity. Sampling uses the cubic derivative-control
hull bound:3max(control-edge length)/sample-count≤step. Every source seed node/
parent remains an exact prefix. Existing nearest-node growth, tie rules,
occupied-endpoint handling and finite node/iteration limits are reused. Optional
seed validation precedes full growth allocation/query work. Default unseeded
positions/parents/rest-attractors and default whole wood match actual #412
original products exactly. No growth RNG, endpoint policy or old recipe changed.

Existing `CrownShoots` attaches0.35m leafy shoots through a1.1m distal path band.
Each of eight original Quercus-profile Blade variants has11250 leaves; lengths/
widths are7/6 of the earlier12×6cm maximum, petioles remain4mm. IDs, variants,
placements, frames and physical blades match between the two radius trials.
Analytic area392.735144555m²; each8×1 whole-view mesh inventory has411.886846728m².
The discretization difference is explicit; this does not admit a coverage-
conserving LOD or silently retune density. Whole-view IDs0..89999 remain stable.
The existing short-shoot mesh uses3 longitudinal/4 radial subdivisions. For its
0.35m/.4rad circular centreline, the segment sagitta bound is
`(.35/.4)*(1-cos(.4/6))=1.944mm`, or.119px at the whole-view42m/2560px span.
This is a centreline bound only; the same stem subdivision is insufficient as a
near-surface quality certificate. Organ poses do not depend on mesh subdivision.

The initial n=2 tip-count pipe radii gave about9mm outer support radii: visibly
too coarse next to these blades. Retain that actual source/image counterexample.
The authored alternative sets `r=root*(pipe_radius_for_unit_root)^e`, with
`e=log(terminal/root)/log(minimum_unit_pipe_radius)`. It preserves the declared
root/terminal radii and monotone parent radii; it **does not conserve squared
child areas**. This is a separate taper control, not a newly established botanical
pipe law. Both plants use the same endpoint constraints. Old pipe mode remains
default for all old callers. Wood is still separate capped segments, not welded
forks. Full product identity binds the physical recipe and wood/blade models;
leaf-plan identity alone intentionally remains unchanged by radius selection.

## Actual pictures and present verdict

Opened2560×1440 front and actual oblique pairs, plus the same candidate's near
view. Both whole plants share camera, material, lights and source leaf geometry;
translations only separate subjects. Candidate coarse geometry bounds are
14.322169×13.989837×18.057008m; control14.122697×14.294716×18.129863m.
The main limbs break the trunk-dominated architecture. Taper reduces the outer
wood's visual weight. The shape is still strongly ellipsoid-constrained,
transparent and too evenly distributed; the mature field-oak reference has a
more distinctive uneven architecture. This is not a finished adult species.

Near detail preserves the complete coarse context and its shadow casters.
The first target-ball selection left visibly coarse leaves near picture edges;
it is replaced by conservative orthographic camera-plane tests. A Blade sphere
has radius `max(length,petiole)+width/2*sqrt(1+fold²)`: centreline length and the
orthogonal transverse displacement bound the whole current analytic surface.
Sphere support against the two horizontal, two vertical and near/far planes
selects2274 potential leaves before any fine mesh. The other87726 remain coarse.
Only selected variants receive32×4 geometry; all source IDs/poses remain fixed.
This is conservative potential visibility, not occlusion or shadow-LOD proof.

The near image still exposes repetitive terminal fans, lobes needing botanical
calibration, capped-segment lines at joints, smooth generic bark and simplistic
pigment/material response. Existing procedural fern-preview maps/materials are
shared unchanged on both sides; they are not calibrated oak venation/R/T or the
new #681 numerical GGX reference. Next visible work needs irregular subcrowns,
existing continuous skin/join procedures and properly shaped leafy shoots;
additional names/catalog bakes cannot resolve these defects.

## Independent checks, cache and cost

47 source and47 fresh installed-library tests pass: seeded KDTree versus brute
queries, exact seed/default preservation, refusal before queries, invalid/cyclic/
collapsed seeds, conservative silhouette demand, analytic taper endpoints and
existing plant/blade/spatial/posture controls. Fresh replay forbids growth,
placement and meshing during finished-product decode; separately reverses every
host demand, checks complete original rows/petioles/frames, reconstructs wood
binding, verifies positive outward mesh geometry and checks product recipe IDs.
No source pin is rewritten. The fresh wheel matches all207 current Python
module files; installed runs exclude `lib/src`. Wheel SHA256:
`2cf7a918b991621550b98dcbbc91ee94c92926b4ec2867cbc81f13491021dc3d`.

| Work/product | Trunk only | Six main limbs |
|---|---:|---:|
| Initial nodes / final nodes |40/5217|206/5472|
| Growth iterations / counted nearest query points |37/97425|31/65519|
| Unreached attractors / foliage hosts |130/4014|122/4301|
| Instanced whole triangles |3355360|3372704|
| Wood array bytes |43270240|45811136|
| Complete rows / cached group rows, each B |11970000|11970000|
| Skeleton position/parent B |146076|153216|
| All NPZ payload B including skeleton diagnostics |55435692|57988184|
| Compressed complete folder B |16806586|17209022|

Final build commit ef5423a8b; growth/summary.355/.335s, placement3.062/2.961s,
wood.702/.666s, export1.692/2.019s; process peak215842816B. Known group→whole
copies11970000B per product, plus output/compression/Blender temporaries. Total
allocation/traffic counts are not instrumented. Skeletons precede leaf expansion;
coarse summary and insufficient demand call no `Shoot.detail`. Remaining
attractors are reported, not dropped from provenance or treated as consumed.

Finished two-product decode.533s source/.534s installed; full independent replay
17.803/17.250s, process peaks353447936/353570816B. Near commit87620e12f:
2274 fine leaves/1182480 fine triangles,4477868 total instances,2.273s setup /
61.137s render,3485585408B process peak. Front/oblique render49.559/52.384s
plus4.615/3.813s setup. EEVEE4 samples whole,8 near; Linux/WSL x86_64,4CPU
affinity, Python3.13.5/NumPy2.5.4/Shapely2.2.0/GEOS3.14.1, one numerical
thread, nice10 under shared host load. Confirmed renderer is Mesa llvmpipe,
not RTX3060. These are offline diagnostics, not runtime/CPU-speed rankings,
whole forest, cache-format, dynamic LOD/wind, native or device acceptance.

## Reproduction

Use `.venv`, `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`. Run
`oak_scaffold_experiment.py --output /tmp/<fresh-products>`,
`render_spatial_crown.py --inputs <products> --output /tmp/<fresh-front> --view front`
(or `side`), `oak_scaffold_near.py --inputs <products> --output /tmp/<fresh-near>`
and `oak_scaffold_replay.py --inputs <products> --output /tmp/<replay>.json`.
The additional functions remain experiments until broader form/consumer evidence
justifies promotion. Private original inputs, current pictures and reports are
under `/tmp/outshine-vegetation-oak-scaffold-proof-6db2a15ea`; no repository images,
server, gallery, vendor assets or native implementation were added.
