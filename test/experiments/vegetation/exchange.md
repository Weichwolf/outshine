# Reuse geometric diffuse exchange — #481

A geometric face/ground operator approximates the first neighbour term of
[#477](diffuse_bounce.md), with current light and material kept separate.
Four samples per side are selected after the same predeclared image bounds pass
for both four and eight samples. The saved geometric product is16.49MB; current
application is measured independently of preparation, direct shadows and images.
This remains a static finite-scene experiment, not a world/frame/device admission.

## Basis, operator and validity

The unchanged canonical stand is
`1cb40500dc435ebf78573379766dbeae51cb3f452bf76053184a2f78fe041a9a`:
2368 roots,8 existing unidentified grass forms and284160 blade triangles. The
same two-triangle6×6m ground remains actual geometry. Each leaf triangle gets a
centroid/flat-normal collocation node. A64×64 grid supplies ground field nodes,
without changing the ground mesh.288256 nodes have two outgoing hemispheres.
First-hit triangle IDs map to leaf nodes; ground hit coordinates map to grid cells.

For each node hemisphere, S cosine-weighted rays sample the first mesh contact.
A stored entry identifies the contact node and its outgoing side toward the
receiver; misses are-1 and contribute zero at this finite black boundary.
F_ij=count(i→j)/S is nonnegative with row mass≤1. Samples are powers of two, so
these weights and row masses have exact binary representation. No R/T, light
energy, current camera, shadow image or prepared illumination is stored in F.

Given separate current direct outgoing radiance D on both sides, A=F D is
normalised incident radiance. The receiver response is

    B_plus  = R*A_plus + T*A_minus
    B_minus = T*A_plus + R*A_minus.

This is a point-collocation approximation of KD; source illumination and first
contacts inside a triangle/ground cell are represented by the node's value.
It does not certify area-weighted reciprocity or global power conservation.
Positive row mass and existing bounded R/T preserve the white-furnace maximum
bound. The exact full geometry remains in every visibility/projection query.
Specular, detailed shading normals/BSSRDF, higher scattering orders, moving
geometry and the infinite surrounding world are not represented by this model.

SciPy CSR coalesces duplicate contacts and performs the RGB product. A bounded
NumPy gather/mean path independently evaluates the same links and agrees within
1e-14. Small explicit matrices, both sides, black/white inputs, the analytic
receiver→rectangle factor, sample packet independence and independent existing
leaf-response checks cover the algebra and sampling. R/T changes affect D and
the final mix; they do not rebuild geometric links. Common beam visibility is
shared among the four diagnostic light/material states.

`finite-face-exchange-1` contains only points, normals, signed links and geometric
metadata: mesh/stand identities, ground basis, sample budget/seed and metric bias.
The geometry fingerprint includes full vertices and ordered triangle indices;
changed geometry is rejected. Checksums, complete shapes/IDs and the exact header
key set are checked on load, including rejection of current-light fields.
Current D/B fields remain separate temporary frame diagnostics. The version is
an experiment format, not a proposed shared/native cache or shader contract.
Changing topology/deformation requires a new valid geometric product. Reuse for
wind, normal-map changes or moving neighbours has not been qualified. Sample
budgets have different identities; progressive/nested samples are not promised.

## Reference and source decisions

Ghidra work at #24 shows SpeedTree geometric visibility fields built from depth
views and sampled at vertices. The existing local `RenderVisibilityLookup`
0x24c1d90 decompilation was inspected again, including its64-view setup. This is
not evidence of a SpeedTree radiance-transfer matrix. Our collocation/operator
is independently defined; no vendor implementation or assets are bundled.
SIGGRAPH1995 Weber/Penn prototype/shadow reuse and the EGSR2007/Forests sources
remain linked in `diffuse_bounce.md`. The local SIGGRAPH2014
[Frostbite notes](../../../doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf),
PDF page60/printed59, were opened optically: a captured lighting probe represents
its capture state. This supports keeping our geometric product separate from
current radiance; it does not derive or validate this operator. No web research.

## Fixed image decision

Before running the comparison, #481 fixed mean indirect RGB error≤0.0005 in each
channel and mean absolute16×16-block RGB error≤0.0015. The block calculation
includes the final partial image strip and weights blocks by pixel area.
The complete1920×1080 four-sample primary positions and exact direct image from
#479 stay unchanged. Only the projected indirect field is substituted. Full
triangle projection preserves geometry/coverage; this is not a painted backdrop.

| S per side | Max absolute mean RGB difference, front/back | Block RGB error, front/back |
|---|---|---|
| 8 |0.0003830 /0.0003608|0.0004074 /0.0004418|
| 4 |0.0003402 /0.0004088|0.0005194 /0.0005695|

Both candidates pass without changing the bounds. Their direct nodal fields are
bit-identical across budgets. Four and eight sample candidate images were
actually opened against both original full ray images; shape and gaps remain
unchanged. Four is selected for lower preparation work/storage under these
specific bounds. This is not a claim that four is sufficient for other plants,
angles, sun directions, local light, materials, wind or LODs. The green mean bias
is larger than the reference's estimated whole-image sampling error around5e-6;
it is an observed approximation error, not excused as reference noise. Local
aliasing/noise and source/receiver collocation remain. No full-image per-pixel
or temporal acceptance follows from the block comparison.

`exchange_conformance.py` independently reconstructs64 stored full-stand sample
counters with Float64 closest triangles and scalar source-side/ground-cell mapping;
all agree. This audits actual cached contacts beyond the small explicit matrices.
It retains the sampled-conformance limit, including uncertified Float32 misses.
The image comparator also requires the original complete light/material states.

## Separated costs and fresh replay

Proof root `/tmp/outshine-vegetation-form-transfer-proof-b5f6e617d`. Receipts pin
commits, actual load and terminal0 results. CPU NumPy/SciPy/embreex on Intel
i5-1135G7; no GPU/device/frame claim. These are local observations, not p99 gates.

| Stage |4 samples,4dfd0f832|8 samples,0da052750|
|---|---:|---:|
| Geometry-link preparation |12.62s|23.85s|
| Transfer rays |2306048|4612096|
| Stored product |16493123B|20943744B|
| Signed link array |9224192B|18448384B|
| CSR entries / arrays |1774113 /23595408B|3419968 /43345668B|
| CSR preparation |0.068s|0.132s|
| Current direct light,4 states/6 shared beam directions |5.96s|5.87s|
| CSR+R/T application, one state |0.0487–0.0523s|0.0576–0.0608s|
| Gather propagation alone |0.182s|0.315s|
| Whole diagnostic build,wall/CPU/peak |27.17s/27.27s/407.19MiB|39.27s/39.34s/439.04MiB|

Application excludes optical-field creation, source/direct-field preparation,
IO, image projection and native upload. Gather timing excludes the material mix;
it is a conservative comparison, not an exact speedup ratio. The complete pure
CPU projection/PNG/comparison costs63.23s/395.82MiB for four,64.46s/395.73MiB for
eight; each projects8294400 primary rays and reuses the reference direct image.
No comparison of the~50ms application alone with #479's370s full renderer is made.

A fresh process at fe26268a4 loads the four-sample geometric product in0.219s,
prepares CSR in0.078s, and replays four exact fields without basis generation,
link tracing or BVH construction during application (these calls are forbidden).
Observed application range0.0497–0.0868s includes a first-use effect; no steady
50ms guarantee. RGB energy×2 gives exactly2×the indirect field. A different
R/T state also replays exactly from its separately prepared current direct field.
Afterward, a separate primary-only BVH supports two64×36 numerical camera queries
with distinct results and unchanged product hash. This is not an angular image
gate. The whole replay process terminates0 at3.175s/3.286s CPU/342.80MiB peak.

Identified four-sample query copies:64569344B geometric +48427008B current-shadow
inputs, three encoded buffers per packet (282/216packets). Node positions/normals
use13836288B; one two-sided RGB Float64 field uses13836288B. CSR stores values,
indices and row pointers. These counts do not cover all NumPy/SciPy/Embree
allocations, copied bytes or internal traversals; process peaks are separate.
The bounded gather limits its temporary to65536 contact triples. Existing ray
limits/bias2um and sampled Float32 conformance constraints remain as in #479.

## Reproduction and open work

Use existing `.venv` dependencies and explicit fresh system-temp paths:

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation .venv/bin/python -m unittest exchange_checks diffuse_bounce_checks
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python test/experiments/vegetation/exchange_experiment.py --inputs <saved463-products> --output <fresh-prepared> --samples 4 --ground-grid 64
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python test/experiments/vegetation/exchange_project.py --inputs <saved463-products> --prepared <prepared> --reference <479-full-images> --output <fresh-images>
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python test/experiments/vegetation/exchange_replay.py --prepared <prepared> --inputs <saved463-products> --output <temp-json>
```

Eight focused exchange tests, the prior diffuse tests and affected contact/camera
checks run with source and installed library as applicable; library unchanged.
No shared/native contract change, image publication, server or resumed paused
Material/Light work. Direct cold coarse-world products, whole-forest residency,
true continuous transport, detailed materials/plant diversity, wind/temporal/LOD
and native device acceptance remain open at #23/#451 and their feature owners.
