# Limb-anchored attraction spaces

Owner [#736](https://github.com/Weichwolf/outshine/issues/736), child of the
unfinished [mature oak study](oak_scaffold.md). The [shoot-roll comparison](shoot_form.md)
did not fix the regular large crown. This experiment changes the distribution
of growth space while retaining the solver, initial scaffold and leaf inventory.
It supplies an authored form control, not a botanically admitted oak or a new
native/default recipe.

## Controlled inputs and procedure

Seed682, 18m authored height, 14m authored spread, 4096 attraction points,
28cm growth step, 2.5m influence and 38cm kill radius. All cases use the same
206-node cubic trunk/six-limb seed and GrowthBudget(16384,128). The existing
solver and its deterministic endpoint conflict rule remain unchanged.

The original single lobed cloud exactly regenerates every position/parent in
the fixed `data/crown682/` source. Its leaf-plan identity remains
`029f1af642d33a6aad5ba2adc640ce77ce7ce9f4505770e78800c18342c59d34`.
New growth deliberately changes later nodes, owner assignments and poses;
there is no claim that old per-leaf world locations survive. The unchanged
seed is an exact prefix in every case.

`subcrown_space.py` constructs six ellipsoids at primary-limb endpoints and
one upper central ellipsoid. Each limb ellipsoid has a radial/tangential/up
frame. The raised trial centres are .9 times endpoint XY plus .12–.16 of
height in Z; radial/tangential/vertical radii are .20–.25/.15–.21 of spread
and .16–.21 of height. The lower, laterally extended trial uses 1.1 times
endpoint XY, .04–.08 height offset and radii .25–.30/.12–.18 spread and
.12–.18 height. Upper centre/radii stay (0,0,.86H)/(.20W,.20W,.14H).
These are authored study values, not measured allometry.

For ellipsoid transform A, the exact extent on axis i is the Euclidean norm
of row i. Uniformly shrink each ellipsoid only as necessary to fit the shared
authored box [-W/2,W/2] in XY and [.23H,H] in Z. This preserves ellipsoid
proportions; it does not force the resulting silhouette to equal the control.
The supplied sampling-host IDs describe cohort provenance, not exclusive
growth ownership: all nodes still compete for all points with the same solver.

Allocate exactly4096 samples in proportion to absolute determinants using
largest-remainder rounding with stable ties. PCG64(SeedSequence(seed,736))
sets ellipsoids; a separate (seed,736,1) stream supplies normalised Gaussian
directions and cube-root uniform radii. This is uniform within each cohort,
not uniform in their union; overlap deliberately receives extra density.
The sampler is bounded by8192 points and seven cohorts.

No leaf geometry or original leaf groups are created during the skeleton pilot
or compact-product preparation. After the pilot, `subcrown_products.py`
reuses the cold-source adapter, Float64 compact poses, conservative host bounds,
null ShootForm controls and the established C1 wood. Trusted experimental
NPZ files are complete for replay; they are not a proposed production format.

## Long dominant paths

The new source contains a68-station dominant path, exceeding the earlier
65-station experimental path limit. `limb_spine.py` composes existing validated
CubicSpine instances with at most64 segments each. Both adjacent pieces receive
the same endpoint derivative; piece length and derivative chain factors
preserve the global normalised parameter. PCHIP radius and transported frames
are still calculated along the complete limb. No cut/cap is introduced at
piece boundaries. Paths of2–65 stations retain the exact original code path;
the extended path is explicitly bounded to1025 stations, at most16 pieces.
Existing face/vertex capacity refusal still precedes curve or frame work.
The installable library and its64-segment CubicSpine limit are unchanged.

## Local sources and independent checks

Runions et al., EG2007 *Modeling Trees with a Space Colonization Algorithm*,
local PDFpage2/Figure1 optically inspected: the attraction-point distribution
is an authored form control. Palubicki et al., SIGGRAPH2009 *Self-organizing
Tree Models for Image Synthesis*, local PDFpage3/Figures3–4 optically inspected:
environment competition, explicit buds and metamer development are distinct
from merely perturbing an envelope. The latter bud-development method is not
implemented here. Both papers are already in `doc/references/vegetation/`.

Actual Ghidra12.1.2 read-only/no-analysis run completes CZoneNode::ComputePosition
0x1a3d240(sizee62hex) and ComputeFrame0x1a3e0b0(size1d9hex).
Position reads editable/profile-derived offsets and applies local rotations;
Frame fetches start data, invokes Position, then selects disk/mesh construction
and extents. This supports a distinction between authored spatial controls and
their generated representation. It does not establish a Space-Colonization
implementation or the meaning of every unnamed field. Binary SHA256 remains
`9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c`.
Diagnostic C hashes are b69a3e7c83c5282836ca3d0b40d117378e7447adfd0c49f746a52e1537ae63eb
and17d149435b5800c3db4ec50bd233bfc9995fea2186ff124a7a515f506f555991;
decompilation and all private inputs/outputs stay under/tmp. No vendor code,
texture or mesh enters the implementation.

The supplied Oak_Hero_Field preview is opened again as a quality reference:
broader uneven subcrowns, mature forks, rich bark and fine foliage remain
unmatched. Modeler ScientificNames=Quercus robur metadata is not independent
living-tree evidence. Botanical reference supplementation remains necessary.

Seven new tests cover analytic ellipsoid containment, inverse-transform
containment and ball moments, exact sample counts/rounding, deterministic seeds,
input refusal, independent brute/indexed growth, seed/capacity preservation,
global SciPy Hermite comparison through1025 stations and closed regular mesh
topology. Together with affected growth/wood/spine and current compact/material
checks:144 source and144 installed tests pass. At the b7e7c9aff study base,
all212 installed Python library files equal source byte-for-byte; no package
migration is necessary for this experiment. Integration checks follow in the PR.

The full4096-point inputs also reproduce positions, parents and remaining
attractors bit-for-bit with brute nearest queries in all three cases.
A fresh saved-product reader prohibits growth, source groups, form compilation
and random generation. Both complete90000-row products then match the
independent original CrownShoots pose path bit-for-bit. All5.04M actual source
vertices fit their conservative host bounds. Original pose digest remains
96e7b4d4e87ee03f5beccd82c04bc6e69333f00cb8cd519ce8977695d1f16f30;
new limb digest580b460c63096cfc5185d7a5af0f87dfdb5742b320e4346e70544fabfaebb309.

## Work and retained data

| Measured item | Original | Raised pilot | Limb pilot |
|---|---:|---:|---:|
| Skeleton nodes | 5472 | 3922 | 3895 |
| Leaf hosts | 4301 | 3134 | 3224 |
| Terminal nodes | 1301 | 957 | 993 |
| Nearest-query points including endpoint checks | 65519 | 61300 | 54797 |
| Growth iterations | 31 | 30 | 29 |
| Unreached attractors, explicitly retained | 122 | 72 | 52 |
| Skeleton array bytes | 153216 | 109816 | 109060 |
| Growth and summary, seconds | .34394 | .25573 | .24182 |

Initial growth arrays have the same fixed16384-node capacity in every case.
Returned skeleton arrays are copies, not a claim of zero-copy growth. Nearest
query counts include endpoint checks but do not count internal KDTree visits,
its allocations or all memory traffic. Allocation totals/peak live growth
bytes are unmeasured here; this is not a solver optimisation admission.
Single local wall times are diagnostic, not an isolated speed ranking.

The original/limb compact arrays occupy799316/624032B, C1 wood arrays
19651248/14097264B and195888/140496 triangles. The exact90000-blade inventory,
eight variants with11250 blades each and392.735145m² analytical area are equal.
Whole-view8×1 blade geometry has411.886847m² faceted area and3.06M triangles
in both cases, with357840000B expanded diagnostic arrays. Thus fewer branches
do not solve the dominant foliage expansion/draw cost. These Blender buffers
are not proposed runtime data structures or a device-budget proof.

## Images and remaining gap

The raised pilot is rejected before foliage expansion: it narrows the lower
crown and exposes overly long bare seed limbs. The lower limb-centred pilot
is carried through the same source material and whole-view geometry.
Final picture evidence and rendering costs follow in the delivery below.

All four original/limb front/oblique2560×1440 pictures have actually been opened.
Both viewpoints have equal50° vertical FOV,26m horizontal camera distance,
eye Z10m and target(0,0,9). Same original procedural leaf colour/height, source
normals, .12 qualitative translucency, neutral studio world, sun/fill and
32 samples. Wood is generic smooth brown. All shadows are disabled, so
camera-only host/leaf culling is allowed for this static form comparison.
Every whole view draws all90000 leaves. The comparison does not preserve
woody volume or certify shadows, wind, forest appearance or temporal LOD.

| Case/view | Scene build s | Render s | Process peak RSS B |
|---|---:|---:|---:|
| Original front | 17.0602 | 82.7230 | 3774738432 |
| Original oblique | 17.4478 | 83.2127 | 3661656064 |
| Limb front | 16.5254 | 76.4314 | 3714506752 |
| Limb oblique | 16.6738 | 76.3491 | 3726077952 |

These are terminal exit0 runs using Mesa llvmpipe/LLVM19.1.7 software rendering
under WSL, not RTX3060 measurements. Full-input brute verification and Ghidra
overlap some front rendering; other host work is uncontrolled. Do not infer a
speed ranking from these times. All four logs emit an unavailable geometry
velocity-buffer warning; static PNGs and reports complete, but movement is
unverified. EGL_BAD_MATCH fallback messages are also retained in the logs.

The limb case visibly separates side crowns and creates asymmetrical gaps in
both directions. The original remains a more uniformly filled egg-shaped
envelope. This is a useful architectural form change with unchanged leaf area,
not sufficient mature-oak quality. The upper central cohort still dominates;
bare sweeping lower seed limbs, radial leaf fans, smooth bark and weak mature
forks remain conspicuous. The raised pilot is not retained as a quality winner.
Neither case is admitted as a new Quercus default. Further quality needs
branch/bud/metamer organisation and mature wood structure; adding more noise
or more taxonomic labels would not address the opened images.

Current PNG SHA256 values, original front/oblique then limb front/oblique:
fe154066aaf49bf03c0d1c8f47c35c8ebbf5c49b0f9faf266b360fdedb85fe4a,
cc08298eea8020b50b31e9b19bce3435c1773f6ef189313b4d0d2327835ffbfa,
ba87eb460bca72deaec57774fc51b4242244d6dfb857464bb215c37a745d86c8,
637a6bdda707ad9a7f1fe8d6d026707006287f5ef78fc4985f4cb5d63aed37a1.
The reports identify b7e7c9aff plus the working experiment delivered by this PR;
formatting before commit does not change the studied construction or shaders.

## Reproduction

Use the project `.venv` with NumPy, SciPy, Matplotlib, Pillow and bpy5.2.2.
Set TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, OPENBLAS_NUM_THREADS=1,
OMP_NUM_THREADS=1; PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
Put MPLCONFIGDIR, XDG_CACHE_HOME and MESA_SHADER_CACHE_DIR under/tmp.

```sh
.venv/bin/python test/experiments/vegetation/subcrown_probe.py --output /tmp/outshine-vegetation-subcrown-replay/pilot
.venv/bin/python test/experiments/vegetation/subcrown_products.py --pilot /tmp/outshine-vegetation-subcrown-replay/pilot --original test/experiments/vegetation/data/crown682 --output /tmp/outshine-vegetation-subcrown-replay/products
.venv/bin/python test/experiments/vegetation/subcrown_verify.py --pilot /tmp/outshine-vegetation-subcrown-replay/pilot --products /tmp/outshine-vegetation-subcrown-replay/products --output /tmp/outshine-vegetation-subcrown-replay/verified.json
LIBGL_ALWAYS_SOFTWARE=1 .venv/bin/python test/experiments/vegetation/render_subcrowns.py --inputs /tmp/outshine-vegetation-subcrown-replay/products/limb --output /tmp/outshine-vegetation-subcrown-replay/limb-front --view front
.venv/bin/python -m unittest subcrown_checks limb_checks oak_scaffold_checks oak_fork_checks source_limb_checks spine_checks test_growth_spine
```

Repeat the render with `original`/`limb` and `front`/`oblique` into distinct
empty directories. Private current evidence is
`/tmp/outshine-vegetation-subcrowns-b7e7c9aff/`: pilot-final, products-final,
verified.json, render reports/PNGs and terminal logs. No gallery/server or
diagnostic images are committed. All temporary writes use/tmp, never/var/tmp.
