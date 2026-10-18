# Wider spruce crown from existing parent profiles

Owner: [#562](https://github.com/Weichwolf/outshine/issues/562), under #182.
This is a bounded Python growth experiment, separate from Draft #202 and native
integration. The prior [finite-window portrait](window_portrait.md) remains the
same-source control. The new recipe changes crown proportions and the nominal
foliage-area target explicitly; it is not the same tree with a cheaper drawing.

## Image and decision

Both complete lit portraits were opened at2560×1440 on the same flat stage,
camera, sun/fill, materials,32samples and finite twig catalog. The candidate has
a wider lower crown and less uniform vertical tiers. The upper crown remains
too narrow and sparsely occupied; gaps expose the trunk and the repeated branch
organization is still conspicuous. Keep the improved authoring controls as a
research increment, not an approved spruce recipe. SpeedTree/botanical quality,
near/far continuity, stand appearance, wind and native/device costs remain open.

The intermediate unlit1920×1080 comparison is only a form diagnostic. Its source
raster is1024square with physical aspect restored; it is not the lit acceptance
image. Its padded projected bounds are not botanical crown-width measurements.
Do not run another eight-form atlas before the single spruce is convincing.

## Method and unchanged limits

`ParentGrowthGraph` binds child length to actual parent length, parent-position
profile and explicit relative factors; `ParentProfileGraph` adds inclination
profiles/world-up bias. These selectively promoted experiment modules use the
existing generator, frame, profile and physical `DistancePattern` implementations.
No second curve/site kernel, all of Draft #202 or native code is introduced.

`spruce_growth_probe.py` computes child lengths, physical needle counts, finite
window counts and expected area before expanding descendant poses. The selected
preflight prepares182root/stem/primary poses instead of48,927complete poses.
Needle shapes, absolute spacing, grouping and scale distribution are unchanged.
All60,000axis/80,000window limits remain. Candidates exceeding them are rejected.
Expected area uses half the cached closed needle surface, deterministic variant
cycling and the expected uniform scale squared; it is not observed stand LAI.

An own seeded64-sample correlated spacing profile varies internodes. A bounded
Brent solve normalizes its scale so the same crown interval holds36complete
five-member whorls. The final endpoint is inside that interval within1nm; the
spacing profile, not a new placement algorithm, is serialized in the graph.
The0.35noise amplitude is not its measured coefficient of variation. Actual
mean whorl gaps are0.799m, range0.592–1.098m, CV0.142. Attachment spread0.08m
and primary length variation0.12 alone left tiers too regular (CV0.031).

| Recipe / full execution | Existing control | Selected candidate |
|---|---:|---:|
| Stem height, m | 33.25 | 33.25 |
| Nominal crown width, m | 5.7 | 7.98 |
| Nominal LAI authoring input | 8 | 5 |
| Nominal target area, m² | 204.140691 | 250.072346 |
| Expected area, m² | 203.870941 | 243.615298 |
| Target relative area error | 0.132% | 2.582% |
| Primary whorls | 48 | 36 |
| Complete axes / windows | 52,811 / 70,990 | 48,927 / 77,698 |
| Represented needles | 11,628,260 | 13,895,173 |

The five-percent area admission is unchanged. A35-whorl irregular candidate
misses it (5.321%);37whorls fit (0.162%) but use79,870windows. Choose36for more
window margin, not minimum area error. The16coarse/refined uniform cases and
three irregular whorl counts are counterexamples, not a botanical calibration.

## Complete product and requested detail

`spruce_growth_execute.py` preflights again, builds all actual axes/windows and
requires per-order counts to match. Complete needle placement/mesh calls are
forbidden. A tiny independent ancestor replay checks actual poses. The explicit
`cached-window-plant-1` product stores axes, windows, graph and recipe/provenance;
its recipe is `experimental-parent-needled-2`. It reuses the existing finite
catalog, without64whole-tree atlas renders or an individual-needle expansion.

Real graph sibling slots reach200/239; `AxisState` now accepts an explicit
`path_capacity=256`. Default64, existing plans, recipes, IDs and bounds remain.
Paths are not truncated, remapped or fabricated. This additive library contract
was announced in #47; Transport confirmed it has no conflicting consumer.
The shared internal window formula is extracted without changing the default
window wrapper. The baseline reproduces **all9axis and11window arrays/dtypes
exactly** against the previous complete cached tree.

`spruce_growth_replay.py` starts from the saved product, forbids graph growth,
whole foliage and needle meshing, then requests only three bounded twig details.
Actual paths `(179,)`, `(120,200)`, `(120,200,0)` generate549/616/288needles.
Their complete window ranges match axis ownership. Requests below each twig's
count allocate no detail. Two fresh processes, source and installed wheel, give
identical arrays/dtypes/IDs/hashes. Total returned detail arrays:354,532bytes.
This proves deterministic cached ownership/detail, not that statistical window
coverage is the raster of those individual needles or every LOD is correct.

## Costs and verification

Frozen producer/renderer: `8c9b56a64a075860298ce4b0b0165338ae5d49bc`.
On this4-logical-CPU/16GiB software-llvmpipe host, complete build44.598s,
peakRSS283.547MiB; count-only0.114s. Final axis/window arrays8,608,980 /
11,577,002bytes. Packing explicitly materializes9axis arrays and concatenates
then binds window columns; these are not zero-copy claims. Total allocation
events, cumulative copied bytes and incremental-build/device times are not
measured. No runtime/frame-budget conclusion follows from this offline build.

The cached lit draw decodes20,185,982axis/window bytes and2,359,296catalog bytes,
returns178,343,064geometry-array bytes, and draws2,814,716tree triangles including
all77,698windows in each of3camera/shadow roles. Decode0.224s, wood assembly/
binding2.423s, complete population3.573s; render387.013s, process390.660s,
peakRSS3377.137MiB. This is a still-image software-render measurement under
concurrent small checks, not RTX3060/A18 evidence. Blender reports no geometry
velocity buffer; motion/wind is unproved. Thin wood still needs demand selection.

Source and installed-wheel41affected tests pass without skips: independent
scalar parent/count controls, bounded preflight, irregular normalization, actual
large paths, legacy window equality, physical needles/plans and portrait guards.
All54loaded installed `outshine_ref` modules match source hashes. Wheel SHA256:
`a1d5a7cbf8e91ff6718d6c595f2a7d22cb5eed7f48d3fba86c7cbb78bdadc7bb`.
Ruff, function-size checks and `make lint-docs` pass. No native gate is claimed.

Private proofs: `/tmp/outshine-vegetation-spruce-growth-proof-8bbcb5503/`:
`product-irregular`, `lit-irregular/portrait.png`, `execution.json` inside the
product's execution directory, `baseline-equivalence.json`, `near-source.json`,
`near-installed.json`, `wheel-module-origins.json`, `internode-statistics.json`.
Product identity:
`61979097bd1a9c4587bedab007fd928b62985ed88113799cd8eca5e03a62fcc8`.
No research images are published under `doc/`.

## Sources and reproduction

First consulted local SIGGRAPH1995 Weber/Penn, §4.3/Fig.1 (PDFp.3), then JRC2016
*Picea abies*, pp.114–115. Both PDF page images were opened again. See
`doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf`
and `doc/references/vegetation/botany/2016-european-atlas-picea-abies.pdf`.
Read-only Ghidra inspection of the local SpeedTree10.2 executable's
`CGenAlgPhyllotaxy::CreateProperties` at`01e27c60` and
`CGenAlgInterval::CreateProperties` at`01e3bd70` supplies structural control
reference only. The local Conifer sample confirms parent-length/internode
controls. No vendor code, constants, geometry, textures or optimization claims
are imported; own noise/normalization/recipes implement this experiment.

Use `.venv`, `PYTHONPATH=lib/src:test/experiments/vegetation`, explicit `/tmp`
outputs. `spruce_growth_probe.py --help` lists graph/plant/catalog/needle inputs.
For the selected refinement call `variant(original,1.4,36,internode_variation=0.35)`
and `measure` with the unchanged physical source/catalog. Serialize graph and
authoring fields including graph identity and `5*pi*(7.98/2)**2` target area,
then use `spruce_growth_execute.py --graph … --plant … --catalog … --needles …
--authoring … --output /tmp/…`. Frozen cached inputs are preserved in the proof's
source receipts. `window_portrait.py --plant-product … --execution … --catalog …
--output /tmp/…` renders the lit product. `spruce_growth_replay.py --plant-product
… --catalog … --output /tmp/…json` runs the isolated cache-hit/detail check.
