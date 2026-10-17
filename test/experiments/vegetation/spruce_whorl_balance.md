# Spruce whorls and fine-shoot allocation

Owner: [#581](https://github.com/Weichwolf/outshine/issues/581), under #182.
Extends [upper-crown profiles](upper_crown.md) and the existing complete cached
[spruce growth product](spruce_growth.md). Python experiment only; no common
library, native, material, provider or reservation API changed.
The subsequent [branch-plane study](spruce_branch_planes.md) tests the remaining
brush-like branching and broader crowns; its rejected whole-form candidates
retain useful explicit phase and curve-normal controls.

## Opened image and decision

Both complete2560×1440/32sample light portraits were opened on the unchanged
flat camera/light/material stage. The selected48whorl/1.6spacing form closes
some conspicuous lower tier gaps at almost identical total needle area. Its
upper crown is slightly thinner; main branches still form conspicuous tiers.
Keep this bounded improvement and its control. The whole tree remains too narrow,
sparse and mechanical against the isolated JRC spruce: **no whole-form approval**,
eight-form/350stand run, SpeedTree equality or native/LOD/device admission.

The opened1920×1080 unlit pair corroborates form only. The2560×1440 diagrams show
actual root/stem/main poses without foliage. Neither diagnostic substitutes for
the complete light image. Further work must change the remaining crown/branch
architecture, rather than continuing to optimize a fixed inadequate silhouette.
#182/#131/#168/#1 retain the wider requirements.

## Cause, controls and finite selection

The existing physical secondary placement advances from0.95 toward0.16 along
each main branch. Its201site cap truncates52of180long main branches before their
basal interval is occupied. More main whorls plus wider fine-shoot spacing removes
that truncation without increasing total foliage. This is an authoring decision,
not recovered vendor growth or measured annual branching.

`spruce_whorl_balance.balanced` uses the existing positive seeded64sample
internode profile over the same stem interval, varying only main-whorl count
and secondary physical spacing. Shape/radius/length/inclination profiles,
terminal branching, needle dimensions/material/spacing/grouping are unchanged.
The36/1control recreates its original graph exactly. Bounds remain48whorls,
256path slots,60,000axes,80,000windows and5%nominal-area error.

The25case search comprises one control,15coarse cases and nine refinements.
Each prepares182–242poses before descendant count/area prediction. Selected
48/1.6 matches the control's actual area more fairly than48/1.55, which reaches
250.039m² but adds about3%area. No global optimum or botanical calibration claimed.

| Complete product |36/1control|48/1.6selected|
|---|---:|---:|
| Main branches |180|240|
| Main branches truncated by201site cap |52|0|
| All axes |48,511|44,697|
| Finite physical windows |77,175|75,612|
| Represented needles |13,837,866|13,822,818|
| Expected needle area, m² |242.610567|242.346740|
| Main owners in upper stem quarter |65|86|
| Needle area belonging to those owners, m² |17.560360|14.414033|

Needle area decreases0.109%, axes7.862% and windows2.025%. The unchanged target
is250.072346m² from nominal7.98mwidth/LAI5; selected error3.089%. Area is half the
closed physical needle surface with expected scale squared, not observed LAI.
Upper-owner accounting uses actual main attachment>=0.75 and reconciles the full
ledger; it is not world-height binning or projected coverage.

## Reproducible own inputs

`data/picea-mature-study.json` contains16KiB of own fixed graph, needle, material
and finite catalog settings, with explicit provenance. No vendor asset or private
predecessor cache is required. `spruce_study_source.py` recreates three physical
needle meshes, procedural colour/roughness maps and the64direction/32×32 finite
family catalog. It does not build a complete plant or call PlantPlan-v4 parsing.
The plant seed815 and canonical catalog seed718 are intentionally distinct.

`family_geometry_for_seed` factors the existing canonical builder; the old
plant-taking wrapper and defaults remain. An independent frozen own wrapper
comparison preserves decoded geometry and metadata. Families are capped before
expansion at1,000,000source triangles. The source uses256groups and8subpixel
samples. The fresh source takes5.848s on this host; no whole-plant atlas is baked.

Optional `--verify-reference` confirms all26decoded mesh/catalog fields against
legacy own hashes on this host, separately from normal portable controls.
Cross-platform floating-point hash equality is not an admission rule. Consumed
binding metadata is preserved; order-specific pattern first/last differ honestly
and are excluded by the existing canonical pattern key. New minimal physical-
source metadata changes logical product identities. All20axis/window arrays of
the fresh control are dtype/value exact against #578, allowing its actual light
image to remain the unchanged baseline without relabeling that old product.

Frozen producer `0238d5cf844120a271b502c4e29dfa9903688f68`, clean source.
Control identity `a6eca4dd5b66e059933c46869010503c83375f6738482d2304a229c1710e22d9`;
selected `481411edefd956a9e3d74c28fe87e9ee02f27e48ab8669ef24c76156061441ee`.
Selected graph `4731ad577134903f76f8b67b2ff6a21f1d97b8e78cd6be8ea6ab65ff43dea4f5`.

## Costs and verification

Linux x86_64,4logical CPUs/16GiB, Python3.13, NumPy2.5.4, bpy5.2.2;
llvmpipe LLVM19.1.7, no hardware GPU claim. The two full builds ran concurrently:
47.445/44.916s, peakRSS282.316/271.578MiB for control/selected. Selected axis/
window arrays7,863,780/11,266,188bytes. These single timings are not a speedup
proof. Work reduction follows actual complete counts, not lower detail limits.

Renderer on clean `fd85f20288c3469aff403af16ae7ca13d8ccd68b`: selected tree
2,599,160triangles including separate camera/sun/fill window roles;
50,627,364geometry/instance payload bytes,2,359,296decoded catalog bytes.
Wood uses four existing prototypes,1,630,788output bytes/1,623,076explicit Blender
write bytes; fine wood remains fully tessellated. Render393.149s/process394.513s,
peakRSS2919.477MiB, partly alongside small diagnostic/tests. No render-time or
GPU-memory saving inferred. Cumulative allocation events, internal copied bytes,
driver memory and target-device times remain unknown.

28affected source and28installed controls pass without skips; the installed run
also passes four current reservation controls. Fresh wheel205Python files match
current source; SHA256`992c81e928ddfcdc1166ac10969a87ba65b2d5b43424792bc34b29945dfc3ff0`.
Fresh source and installed source independently pass explicit legacy26field
verification. Tests cover scalar counts/sites, unchanged profiles, invalid bounds,
family capacity before expansion, legacy wrapper, large paths and Blender instances.
Fresh source/installed cache replays agree for both complete products while growth,
whole foliage and needle meshes are forbidden. Selected paths `(239,)`,
`(239,0)`, `(239,0,0)` produce500/407/190needles with122000/99308/46360array bytes;
global owner intervals match the full ledger. Statistical-window/needle visual
equivalence remains unapproved. Ruff/format/functions<=120lines and lint-docs
are the affected static checks; no native gate claimed.

## Local sources and reproduction

First consulted local Weber/Penn SIGGRAPH1995 §4.3/Fig.1, then JRC2016 *Picea abies*
p.114; both PDF pages were also opened. They support relative branching and the
botanical image comparison, not these fitted constants. Prior read-only Ghidra
structural findings are recorded in [upper-crown research](upper_crown.md);
no new vendor equation or recovered botanical meaning is claimed here.

Use `.venv` with `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1` and
`PYTHONPATH=lib/src:test/experiments/vegetation`. All outputs must be explicit:

```sh
python test/experiments/vegetation/spruce_study_source.py --output /tmp/spruce-source
python test/experiments/vegetation/spruce_whorl_balance.py --source /tmp/spruce-source --output /tmp/spruce-probe
python test/experiments/vegetation/spruce_growth_execute.py --graph /tmp/spruce-probe/whorls-48-spacing-1.6.json --authoring /tmp/spruce-probe/whorls-48-spacing-1.6-authoring.json --plant /tmp/spruce-source --catalog /tmp/spruce-source/catalog --needles /tmp/spruce-source/models --output /tmp/spruce-product
python test/experiments/vegetation/window_portrait.py --plant-product /tmp/spruce-product --execution /tmp/spruce-product/execution --catalog /tmp/spruce-source/catalog --wood-mode instanced --output /tmp/spruce-lit
```

Repeat the builder with36/1.0for the control. Existing `spruce_growth_replay.py`
and `upper_crown_area.py` verify detail/ownership; their `--help` lists paths.
Private proof root `/tmp/outshine-vegetation-whorl-balance-proof-6df639bf9` holds
the actual products, input/implementation hashes, tests, installed-wheel receipt,
source equivalence, opened diagnostics and final light report. Selected PNG SHA256
`4f649d34968030e080593c01e1925a39ddca963a67818fdca88d16bb31c9efb1`;
baseline pin in [prior report](upper_crown.md). No research PNG under `doc/`.
