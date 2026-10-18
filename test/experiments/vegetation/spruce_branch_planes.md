# Spruce branch planes and bend orientation

Owner: [#590](https://github.com/Weichwolf/outshine/issues/590), under #182.
Extends the [whorl study](spruce_whorl_balance.md). Python experiments only;
no public library/native/material/provider/reservation API changed.

## Decision and image limits

The first complete2560×1440/32sample light image was opened. Wider lateral fans
make a more legible branch skeleton, but produce thin flat shelves with larger
inter-tier gaps. **Reject this as an improved whole-spruce recipe.** Its unlit
projected coverage falls75.340→62.566m² despite almost identical physical area.
The parent-local curve normals point sideways, so the existing negative bend
bends in the fan plane instead of hanging downward.

The following world-up-normal alternative keeps the same counts and bends,
while directing secondary curvature downward. Actual opened main/fan/side
diagrams confirm this geometric change. The unlit complete comparison shows
hanging lower boughs, coverage65.724m², still below the radial control. Neither
counter nor candidate is a convincing complete JRC spruce. The complete opened
2560×1440hanging light image confirms the downward boughs but still has large,
mechanically tiered gaps. Keep the optional controls and both counterexamples;
do not replace the mature default with either candidate. After these two whole-
form failures, revisit main/inter-whorl branch distribution, size hierarchy and
the area assumption against references rather than another narrow profile sweep.
No species/stand/SpeedTree/LOD/native/device acceptance follows.

## Existing geometry, explicit orientation

`Placement.phase=None` retains the old seeded per-parent random phase exactly.
An explicit scalar sets the parent-frame azimuth phase; independent seeded
azimuth/pitch/scale variation continues in its original draw order. Recipes omit
the field by default; explicit values carry `fixed-parent-azimuth-1` and radians.
Boolean/nonfinite/vector/out-of-range values and unknown versions are rejected.
Roots reject parent-frame controls. Existing descriptor versions/defaults remain.

`Placement.normal_reference="world-up"` carries `world-up-axis-normal-1`.
After all subclass inclination/upward-bias placement, the existing library
`normal_frame` projects world-up perpendicular to the final axis direction.
Its existing deterministic near-parallel fallback remains. Origins/anchors,
direction, length and scale are preserved within floating-point normalization;
only roll changes. Negative secondary bend then curves toward world down.
This is geometric authoring, not elastic growth or recovered vendor physics.
No new normalization/placement kernel or special tree-only frame convention.

`spruce_branch_planes.branch_form` uses existing DistancePattern divergenceπ
for alternating sides and0.18rad seeded angular variation. Five bounded cases:
radial control, randomly rotated fans, parent-lateral fans, wider lateral fans,
and wider hanging fans. Main length×1.25, secondary spacing×1.25 and relative
secondary length ratio÷1.25 preserve the fine sites/lengths/needle counts.
Physical needle dimensions/material/spacing/grouping and wood radii are unchanged.

Only702poses are prepared for the spatial probe: root/stem/240main branches and
four actual complete secondary fans. Seventeen centreline samples per main
measure XY spans6.970/6.899→8.711/8.618m; maximum main length3.989→4.987m on the
same33.25mstem. These sampled skeleton spans are not continuous crown bounds.
The240pose main hierarchy is complete; fine whole-tree construction follows
count admission. Limits remain60,000axes/80,000windows and5%nominal-area error.

| Complete form |Radial control|Wider flat fan|Hanging fan|
|---|---:|---:|---:|
| Axes |44,697|44,697|44,697|
| Finite windows |75,612|75,683|75,683|
| Main-axis needles |64,402|80,472|80,472|
| Secondary/terminal needles |13,758,416|13,758,416|13,758,416|
| Expected needle area, m² |242.346740|242.628486|242.628486|

The wider forms add0.116%area, not new fine biomass. The250.072346m²nominal
target is deliberately held for this morphology comparison; it is not a newly
calibrated wider-crown LAI or measured isolated-tree canopy. Area means half the
closed physical needle surface with expected scale squared, not projected LAI.

## Independent compatibility and costs

Old recipes omit both new fields. The frozen own old GeneratorGraph comparison
keeps descriptors/identities and actual poses exact. A fresh complete radial
control under the final implementation keeps all20axis/window decoded fields
dtype/value exact against #588. Logical new control identities differ honestly
because their authoring metadata includes the new explicit study controls.
Source/installed cache-hit near replay is exact, with growth/whole organs/needle
meshes forbidden. Hanging paths `(239,)`, `(239,0)`, `(239,0,0)` return625/407/190
needles and152500/99308/46360array bytes with complete global owner intervals.
Statistical-window/individual-needle visual LOD equivalence remains unapproved.

Final hanging producer `0e5a0773a0e1c81909eb6211c4f1f90f8d464cb9`, clean.
Graph `75be103eeb2971dbdc94cbf1edfb84c9de90d38c01d0afe01341f8101c951ae1`;
product `1718f469f55867ba5f29539f6bb1065e020cf1a4a3b91eb8b45ba9a93d5dfabe`.
Flat counter producer `29616dea87135dff1099f5cccfe8ef96e3bd0a24`;
product `2ca614769f59faeb9547e0032bdb3013ba426d1505ffc61c384e2855c1c5a6e0`.
Source producer014b31ec2 rebuilds the own fixed inputs in5.876s; all26decoded
mesh/catalog fields are exact against #588 on this host. No private predecessor
is required to reproduce them from the committed16KiBinput. Optional strict
hash verification remains a same-host proof, not portable floating-point gating.

Linux x86_64/4logical CPUs/16GiB, Python3.13/NumPy2.5.4/bpy5.2.2, softwarellvmpipe.
Partly concurrent complete builds: radial42.418s/271.727MiBpeakRSS,
flat41.914s/271.836MiB, hanging43.510s/272.258MiB. No timing speedup claimed.
Hanging axes/windows7,863,780/11,276,767bytes. Fixed phase avoids one RNG creation
per secondary axis: independent seven-child probe14→7constructions,22,228fixed
secondary axes in the complete graph. This is object/work counting, not a count
of all allocator events or a hardware performance guarantee. Final frame alignment
adds work and an immutable pose replacement. Internal copies/allocation events,
driver memory and target-device times remain unknown.

Flat light counter:2,599,586triangles including camera/sun/fill window roles,
50,673,372geometry/instance bytes;394.465s/2955.281MiBpeakRSS. Same unchanged
four wood prototypes and complete fine wood; no triangle LOD optimization claimed.
Hanging renderer `aba477246` has the same triangle/payload counts,
431.837srender/433.302sprocess/2903.254MiBpeakRSS, partly alongside verification.
The loaded library and actual inputs are pinned separately from this software
renderer's wall time. No faster-render or smaller GPU-resource claim.
Controls cover scalar parent frames/sites and seeded jitter, late inclination/bias,
negative-bend endpoints, actual vertical fallback, invalid versions/root controls,
legacy buffers, span compensation, large paths and real Blender instances.
After merging current master,64source/64fresh installed controls pass:34branch/
growth/window controls and30surface/habitat/projected-surface consumer controls.
The new #589sampler implementation is inherited without API changes. All205Python
wheel files match current source; wheel SHA256
`c10c9eb902d919d4a69eb282c0a2d054c702d242acab304371b5976f5013e38d`.
Final installed hanging near replay is exact. Ruff/format/functions<=120lines
and lint-docs pass; no native gate claimed.

## Local sources and reproduction

First consulted local Weber/Penn SIGGRAPH1995 §4.3, especially PDFp.4's alternating
parent-aligned child distribution; PDFp.4 was opened. JRC2016 *Picea abies* p.114
isolated-tree andp.115twig/crown photographs were opened as botanical references.
They do not calibrate these fitted constants. Fresh read-only Ghidra on the local
SpeedTree10.2binary decompiles `ComputePlanarForce` at`020e4b90` and
`ComputeTwistForce` at`020e5640`, both terminal success. Local vector/matrix,
normalization and profile-dependent paths are structural reference only;
no botanical meaning, matching phase implementation or vendor optimization proven.
No vendor code/assets/constants imported.

Use `.venv`, `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1` and
`PYTHONPATH=lib/src:test/experiments/vegetation`. Rebuild own inputs with
`spruce_study_source.py --output /tmp/branch-source`; then
`spruce_branch_planes.py --source /tmp/branch-source --output /tmp/branch-probe`.
The existing [complete builder/render commands](spruce_whorl_balance.md) take
`hanging-lateral-fan.json` and its `-authoring.json`. `branch_plane_plot.py`
draws bounded actual poses; `spruce_growth_replay.py` verifies cached demand.
Private proof root `/tmp/outshine-vegetation-branch-planes-proof-360b58469` holds
the complete products, actual images, original failure logs, frozen references,
per-field hashes, package/near receipts and Ghidra outputs. No research PNG in doc/.
Flat PNG SHA256`44d6a9490df5f716ab5198c0beaf8c75c4ded2786a44f4ac5f26554f21c30d9a`;
hanging PNG`c01ca046189f0c9fc5a4cc25525b4180884553ba4eb9c3bcae47bcf20b27674a`;
unchanged baseline pin and source provenance in the [prior report](spruce_whorl_balance.md).
