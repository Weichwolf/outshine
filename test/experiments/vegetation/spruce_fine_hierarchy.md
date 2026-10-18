# Branching terminal twigs and directional foliage coverage

Owner: [#619](https://github.com/Weichwolf/outshine/issues/619), under #182.
This changes the twig hierarchy, not another main-whorl/upper-profile sweep.
No shared-library/native/material/provider/reservation API. The whole spruce
form, stand and SpeedTree-quality admission remain open.
Both recipes are rejected as species defaults; the completed radial light
portrait has a more compact lower crown but an even sparser upper crown.

## Construction and bounded execution

The #588control has22,228secondary axes and one terminal per secondary. The new
recipe keeps its trunk and240main branches, physical needles and scene. Use
0.15mphysical spacing for supporting secondary branches and0.045mfor their
terminal branchlets. Lengths depend on actual parents and local position;
these are multiannual branchlets, not claimed single annual shoots. Support/end
ratios0.5/0.7replace1/3and7/15. Prototype radii compensate those ratio changes
so equal parent length/local scale keeps the old physical attachment radii.
The existing continuous parent-radius fit is retained, not relaxed.

`spruce_fine_hierarchy.py` uses existing physical pattern tables to pre-count
both descendant levels from actual lengths. It prepares242root/trunk/main poses,
not the complete fine geometry. Per-parent clipping rejects. The original
60,000axis/80,000window and100,000stored-graph-pose limits remain. Coarse0.035/
0.04end-spacing candidates exceed window demand;0.045fits the existing nominal
250.072346m²±5%author-area admission. This is feasibility, not an image optimum.

The conservative graph capacity for terminals is205,920; actual pre-count40,789.
`fine_hierarchy_execute.py` requests existing individual paths with that complete
pre-count, checking each stage before its next pose. It does not call a bulk
request whose pessimistic bound would reject this smaller actual hierarchy.
Stored poses finish at43,546including the logical root. No global limit raised,
no whole foliage, individual needle poses or needle meshes during full build.
No second growth/curve implementation or global cache.

Two real Float64 counterexamples corrected the new count helper:

- Parent0.6265822784810127mhas12terminal sites under the existing closed-endpoint
  tolerance; raw floor predicts11. Reuse the existing capacity contract with a
  non-clipping ceiling before requesting actual tables.
- Parent0.0640987595703009mproduces an unrounded first child0.013012048192771084m.
  Actual prototype-scale→length gives0.013012048192771082m: physical needles10→9.
  Pre-count now performs the same Float64 roundtrip as actual placement.

## Physical inventories and an isolated angular comparison

| Quantity | Control | Both branching variants |
|---|---:|---:|
| Main axes |240|240|
| Supporting secondary axes |22,228|2,515|
| Terminal axes |22,228|40,789|
| Complete physical axes |44,697|43,545|
| Windows |75,612|76,443|
| Needles |13,822,818|14,167,937|
| Expected half closed needle surface [m²] |242.346740|248.397494|

Hanging terminals share a downward azimuth. Their actual mean initial world-z
direction is−0.9512. The radial countercase changes only terminal angular spread:
ordinary seeded phase and golden-angle divergence, retaining world-up curve
normals/downward curvature. Complete cached comparisons prove all other axis
fields, global IDs, counts, paths, scales, bends, window phases/capacities/families/
weights and all earlier-level world poses exact between variants. This isolates
layout from physical mass. No silent needle-size or alpha adjustment.

Hanging product`0d7516af55d170343ac30c818543ea6214d7708422046844d40864d41427ccca`,
graph`1af43d204ed2b6c4751d45146642b99508507018b438139e14e406ec5e8db04e`.
Radial product`147fafbb814a9d60e16afc0302c020915cb8aa8052cc80979dc6d2b13fbef6e8`,
graph`632c9963577348e6171c7311a15631b6ae3c694a14cccf758645bd9e7ae94715`.
Recipe`experimental-branching-needle-hierarchy-1`; control/default builders and
graph`4731ad577…`retain their original identities.

## Projected coverage and optical findings

`fine_hierarchy_coverage.py` reuses the existing finite-window raster/compositor.
Each pair uses exactly shared projection bounds and a common convex envelope of
both window-centre sets. Three horizontal azimuths15/75/135°,512×512rasters with
2×2subsamples. Reference pixels may be rectangular in physical space; the complete
2560×1440plots restore their true physical aspect. Main wood is excluded; canonical
short-wood occlusion remains. Channel7measures leaf-weighted coverage separately
from total alpha; a closed-form two-layer control checks that distinction.

| Variant | Mean leaf coverage in pair envelope | Envelope pixels with leaf<0.05 |
|---|---|---|
| Hanging pair control |0.4207 /0.4147 /0.4225|20.4% /20.6% /18.4%|
| Hanging |0.3989 /0.3924 /0.3979|27.5% /29.2% /26.3%|
| Radial pair control |0.4094 /0.4070 /0.4127|22.5% /22.0% /20.3%|
| Radial |0.4086 /0.4074 /0.4111|23.0% /23.0% /21.0%|

Both complete plots were actually opened. Hanging forms regular shelves and is
rejected. Its expensive unfinished light process was deliberately terminated
after that optical/coverage result; no finished hanging light image is claimed.
The radial hierarchy restores approximately control coverage, but neither these
numbers nor its projection establishes a convincing dense whole spruce. Pair
envelopes differ; absolute means must not be ranked across pairs as if masks were
identical. Directional proxy coverage is not observed gap fraction/effective LAI,
all-view physical-needle fidelity or temporal/LOD approval.

The complete radial2560×1440/32sample light portrait and the unchanged control
were actually opened at full resolution. Same scene/camera/light/backend. The
lower crown is more compact, the upper remains thin and has less usable foliage.
No convincing whole-form improvement, and no species default selected. Portrait
SHA256`eb99aeae92ccff90ff9f6c9d8ce51c6fb7d5268c32d44c2575044ffcd268833b`;
control`ae0e5cbe88483f700d9d700e82f86c2437a264e5fe7d4f7ccc1a80da493524dd`.
Further narrow tweaks to this fixed-depth recipe are not the next step: #182
needs a different foliage/count-allocation rule, checked against the whole crown
and local primary literature, not total counts alone. Grouping actual window
area by its originating main-branch attachment confirms13.9783→6.7705m²above25m,
and43.1376→27.0859m²above20m, despite increased total area. Both new orientations
have identical values: angular spread cannot restore that missing upper mass.
With two uncapped constant-spacing descendant levels, the continuous count×
count×length approximation grows cubically in primary length; the one-terminal
control is quadratic. Integer endpoints and caps bound that approximation.
Next compare bounded counts that cover the whole parent interval with a physical
minimum spacing, rather than merely truncating a dense physical sequence.

The nominal area is still an author input. Measured window-centre XY hulls are
41.7154/43.0565/46.5824m²for control/hanging/radial. They omit physical leaf extents
and are not botanical crown boundaries. They do not support the hypothesis that
our current crown footprint is much larger than the nominal50m²; do not invent a
factor-of-two area explanation from the unobserved LAI target.

## Sources, work and verification

First local SIGGRAPH1995 Weber/Penn §4.3/PDFp.4 and JRC2016 Picea abies p.1 were
read and optically opened. Local Chen et al.1997 §2.1–2.2still separates physical
half-surface from directional gap effects; no biological calibration inferred.
Fresh read-only Ghidra succeeds at SpeedTree10.2`020a4220`ComputeNodes and its
`02094cf0`parallel worker: per-node computation, completion/dirty bookkeeping
and bounds are visible. This does not provide a Picea recipe, our spacing/radius
rules or proof of a particular vendor parallel speedup. No vendor code/assets.

Linux x86_64/4logical CPUs/16GiB, Python3.13.5/NumPy2.5.4/bpy5.2.2, software
llvmpipe. A large unrelated kernel build ran concurrently. Actual full hanging
build138.772s/259.699MiBpeak; radial264.213s/259.961MiBpeak. Different host load,
no speed comparison. Retained axis/window arrays7,814,124/11,390,007bytes.
Hanging six-view projection258.778s/1122.816MiBpeak; this is offline reference
work, not per-frame execution. Primitive raster/fragment budgets remain active.
Radial projection57.174s/1025.313MiBpeak used ordinary measurement priority,
whereas the earlier run had nice10; no algorithm speedup follows. Full radial
light render387.452s/process390.743s, peak3236.551MiB, also ordinary measurement
priority under mixed load. Geometry2,548,850camera/shadow triangles and
51,124,380geometry/instance payload bytes; catalog18,874,368decoded bytes.
Allocation/internal-copy totals for this new full build remain unmeasured; do
not reuse the old #610allocation total as a new result.

Initial54source/56installed controls pass; final eight independent focused cases
pass after adding angular-mass invariants and the two actual numeric countercases.
Fresh source/installed cached twig replay is exact for the hanging variant;
the radial replay uses the same original-demand limits and forbids growth/full
near/needle mesh. Existing installed wheel is the verified current #615package;
affected library code is unchanged. Source/producer hashes and terminal logs
are retained with actual products, not rewritten as a new whole-plant result.

Rebuild the existing self-contained source, then `spruce_fine_hierarchy.py` with
explicit`--source --output`; execute admitted`hanging-0.045.json`or
`radial-0.045.json`using`fine_hierarchy_execute.py`. Existing capacity rebind,
portrait and demand-replay CLIs consume these complete products. Coverage takes
explicit control/candidate/catalog/output; its`plot`helper can redraw the saved
NPZs at physical aspect without rerasterizing them. Use`.venv`,tmp/cache/config
paths under/tmp, and the ordinary experiment import path. Actual inputs, products,
opened plots, stopped-run receipt, references and logs are in
`/tmp/outshine-vegetation-fine-hierarchy-proof-47263bf01/`.
