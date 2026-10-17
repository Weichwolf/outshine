# Upper spruce crown: inclination, leader and foliage allocation

Owner: [#577](https://github.com/Weichwolf/outshine/issues/577), under #182.
This extends the existing [spruce profiles](spruce_growth.md), using the unchanged
[instanced portrait](wood_instances.md) for all lit comparisons. No native,
library, material, reservation or source-provider API changes.
The subsequent [whorl/fine-shoot study](spruce_whorl_balance.md) compares the
remaining lower gaps and supplies self-contained fixed source recipes.

## Opened image and decision

The complete baseline and both new2560×1440/32sample portraits were opened on
the same flat camera/light/material stage. The first `leader-0.9` form removes
the long bare leader but leaves conspicuous thin upper tiers. The selected
`density-0.5-0.76` form fills the upper crown more visibly at almost unchanged
total foliage area. Its lower tiers develop larger gaps. Retain these controls
and this tradeoff as research; **do not approve the whole spruce recipe**.
It remains much sparser/more mechanically tiered than the JRC isolated spruce.
Neither a source count nor the nominal LAI certifies botanical image quality.

Next compare more main-branch whorls with fewer fine shoots per main branch at
the same total foliage/work budget. Merely tilting branches or moving a fixed
number upward does not solve the whole crown. No eight-form/stand atlas run
before a convincing single form. #182/#131/#168/#1 retain the wider requirements.

The actually opened1920×1080 unlit comparisons are secondary form diagnostics,
not lit images. Their1024square source rasters have physical aspect restored.
The2560×1440 primary-axis diagrams show actual bounded spatial poses projected
from one fixed direction; they contain no foliage/material/shadow evidence.

## Existing controls and bounded selection

`upper_crown_profiles.py` authors the existing `Profile`, `ParentProfileGraph`
and `DistancePattern`; no new placement/curve kernel. The lower inclination is
preserved. Upper main branches change from about0.61rad to1.05rad from the local
stem tangent, still ascending. Last main attachment moves from0.96 to0.995:
unoccupied leader arc1.330→0.16625m on the same33.25m stem. A monotone remaining-
crown length curve has terminal fraction0.02 and exponent0.76.

The positive seeded64sample internode profile optionally multiplies spacing by
`1 + upper_bias*(0.5-t)`, then uses the existing bounded root normalization to
retain the interval and all36five-member whorls. Selected bias0.5 allocates more
of the same180main branches upward. Defaultbias0 preserves old tables exactly;
it draws no extra random values. This is an own authoring model, not measured
annual growth or a recovered vendor equation. Density/length parameters are
explicit optional authoring fields in the existing experimental recipe.

The initial256sample length table missed the declared terminal value; an
independent endpoint check caught it. A2001sample table includes the chosen
0.118/0.995knots exactly. Other parameter positions use ordinary interpolation;
no analytic exactness between samples is claimed.

Initial8cases isolate inclination and leader/length changes. Twelve coarse and
nine refined density cases bring the total to29; each prepares182root/stem/main
poses before descendant-length/count/area calculations. Angle-only cases retain
all counts/lengths. Broad profiles exceeding80,000windows are rejected. Small
area cases fail the unchanged5%target gate. No60,000axis/80,000window limit or
physical needle size/spacing/grouping changes. Only two full plants were built;
the first remained an insufficient counterexample. This bounded search is not
a global optimum or species calibration.

| Complete recipe | Prior control | Leader only | Selected redistribution |
|---|---:|---:|---:|
| Main branches in upper stem quarter | 50 | 55 | 65 |
| Expected total needle area, m² | 243.615298 | 239.901431 | 242.610567 |
| Area owned by those upper main branches, m² | 9.723890 | 9.492639 | 17.560360 |
| Axes | 48,927 | 47,665 | 48,511 |
| Finite windows | 77,698 | 76,257 | 77,175 |
| Physical needles | 13,895,173 | 13,683,344 | 13,837,866 |

The target remains250.072346m² (nominal width7.98m/LAI5). Selected error2.98385%;
total area decreases0.4124%, upper-owned area increases80.59%. `upper_crown_area`
attributes actual saved axis paths and global needle-variant cycles; its sum
reconciles with the producer. Ownership uses main attachment parameter>=0.75,
not final world-height bins or projected foliage. Area is half the cached closed
needle surface with expected scale squared, not observed LAI or a real-tree census.

## Complete product, cost and verification

Leader producer `1f6bf7674d503ca37ad72434177151f982ce86c5`; selected producer
`e15819565b09de58b618a892fbbe728b0e880d3c`. All29serialized graphs remain exact
after the authoring-list cleanup. Selected product identity:
`9b0ddc06782045160e56fdb9d81ea54da52bfda3d81491a30037477a7ac10131`.
Physical needle source and finite64direction catalog unchanged. Complete axes/
windows are saved before view selection; no full needle expansion or whole-tree
atlas bake. Existing products/defaults remain, new recipes have distinct hashes.

On the4logical-CPU/16GiB softwarellvmpipe host, selected complete build42.107s,
peakRSS282.336MiB; final axis/window arrays8,535,764/11,499,075bytes. Actual lit
tree2,791,610triangles, generated geometry/instance payload51,777,492bytes,
render399.414s, process400.970s, peakRSS2930.367MiB. First candidate build43.741s,
279.695MiB; lit396.667s/2920.984MiB. Runs include different small diagnostic
loads. No timing/device improvement claimed. Packing/copy behavior is inherited;
cumulative allocation events/internal copied bytes/GPU resources remain unknown.

23source and23installed controls pass without skips: scalar local-tangent angle
and length/site preservation, positive biased spacing/complete interval, exact
zero-bias tables against frozen own source, endpoints, invalid controls, existing
preflight/count, large paths, physical windows and real Blender instances.
Both products pass fresh source/installed cached three-twig replay with identical
array hashes, growth/whole foliage/needle meshes forbidden. Selected paths
`(179,)`, `(120,200)`, `(120,200,0)` return549/609/284needles; their owner ranges
match the complete ledger. This does not certify statistical-window/needle LOD
equivalence. Unchanged library permits the verified #562wheel to be reused.
Ruff, functions<=120lines and lint-docs pass; no native gate is claimed.

## Sources and reproduction

First consulted local SIGGRAPH1995 Weber/Penn §4.3/Fig.1 (PDFp.3 opened), then
JRC2016 *Picea abies* p.114 (PDF page opened again). PDFs remain under
`doc/references/vegetation/{siggraph,botany}/`. Read-only Ghidra inspection of the
local SpeedTree10.2executable: `CSplineProperty::EvaluateParentValue`, address`01fba5c0`, terminal
success. Conditional profile-input handling and variation/noise paths are
structural reference only; types/flags do not prove botanical meanings or
vendor optimization. No vendor code, constants, meshes or textures imported.

Use `.venv`, `PYTHONPATH=lib/src:test/experiments/vegetation`, explicit `/tmp`
outputs. `upper_crown_profiles.py --plant-product INPUT --catalog CATALOG
--needles MODELS --density --output /tmp/probe` emits all29graphs/preflights.
Build the selected graph with existing `spruce_growth_execute.py`, the explicit
authoring settings and unchanged physical source. Existing `window_portrait.py
--wood-mode instanced` renders it; `spruce_growth_replay.py` checks cache/detail.
`upper_crown_area.py` measures saved ownership; `upper_crown_plot.py` draws eight
named primary diagrams. Their `--help` lists required explicit paths.

Private proof root `/tmp/outshine-vegetation-upper-crown-proof-4d602e08a/` contains
`refined-density-preflight`, both complete products, both lit portraits, source/
installed/near/area/Ghidra receipts and opened diagrams. Selected PNG SHA256:
`96a0093178bb4ec83eb4935ef7e5d1cb86b2359a9aa7c54f657b8b882734fb59`.
No research images are published under `doc/`.
