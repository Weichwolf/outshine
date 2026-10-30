# Whole elevated-rail piers without blocking the retained network

Owner: [#1771](https://github.com/Weichwolf/outshine/issues/1771), parent #434.
The whole elevated slabs from #1768/#1770 still float above the retained114-source
transport world. This experiment adds actual whole terrain-to-deck support
volumes using the existing installed `SupportVolumeBuilder` and physical audit
kernels. Terrain, deck, old roads and124 old piers remain unchanged.

## Bound inputs, stations and admission

Read the exact accepted constant-deck archive, original three source IDs/roles,
frame and prior report hashes. All200 original road-array hashes must still
match. `elevated_rail_inputs.read_sources` reads the supplied metadata/axes without
rebuilding the deck; existing `sources` callers retain their unchanged layout
behavior. This generation reference still decodes source axes: it is no
source-free/native asset-cache-hit implementation.

Nominal stations start6m from each source beginning and repeat every12m, retaining
both open source ends in the spacing ledger. At most128 selected stations,
640 candidate attempts and8 cooperative seconds per variant; the existing
uniform selector preserves source ends and explicit budget reasons if needed.
Footprints are1.2m along/.6m across the local source tangent. This is a procedural
spacing/shape recipe, not a load/foundation/structural specification.

Fixed centres are compared with bounded longitudinal shifts0/−2/+2/−4/+4m.
A lateral alternative produced exactly the same accepted bodies here and is
removed from the delivered planner. Complete recipe spans are required before
preferring the greater accepted station coverage; timing noise does not choose
the stored result. Partial variants retain every local omission and the deck.

Before contact construction, old asset metadata selects the relevant complete
working context:2/29 assets,33 owners/9560 body facets within existing16/64/32768
caps.291 actual traffic triangles per precision enter two shared exclusion
unions. All124 old pier bounds are checked; none enter this deck neighbourhood.
Old selection includes XY rounding reach before Float32 conversion. The
selected terrain has567 faces, deck underside22; no old geometry is replanned.

The existing whole-contact builder returns paired contact meshes and a closed
body. Existing `body_caps`/`Reference` checks validate topology, positive actual
heights, whole Double/Float32 contacts and their derived rounding envelopes.
An additional check binds the entire footprint to its declared source station.
Old traffic/piers and every earlier new pier retain0.2m foreign clearance in
both precisions. Source/frame/role checks precede construction; no Ground-only
replay or source/routing identity is inferred from the resulting piers.

## Actual complete result

| Variant |Accepted/nominal |Attempts |Body builds |Maximum source gaps |
|---|---:|---:|---:|---|
|Fixed |89/94 |94 |89 |12/24/24m |
|Longitudinal, selected |94/94 |102 |94 |12/14/14m |

Five centres move by2m to avoid existing traffic. Every complete original source
retains all its nominal stations; no station is silently dropped. The new94
bodies have1018 vertices/1660 faces, minimum actual Float32 height3.08859m.
Stored body/cap/count arrays total75680B. Maximum actual lower contact error
15.062µm remains within its individually derived envelope; upper contact error0.

An independent full old-world union, without deck-neighbour preselection,
checks all7514 old top triangles and124 old support bounds against every actual
new footprint. Minimum old foreign distance is.557415m Double/.557428m Float32;
minimum new-new distance3.051036m/3.051034m. Thus the complete old traffic and
all new pairs exceed.2m. Filtered unions certify this exclusion range; distances
to irrelevant far geometry need not equal a full nearest-neighbour query.

A fresh saved-product reader repeats body/contact/source-station/traffic/span
admission without builder, planner, deck layout or CDT. It also succeeds with
all of those constructors/functions replaced by failing sentinels. Rebinding a
corrupted body container SHA still fails body/cap validation; changing an original
station fails identity validation. No regenerated output is used as an oracle.

## Same-quality work and costs

Frozen bd1769a0d, Linux/i5-1135G7, Python3.13.5/NumPy2.5.4/Shapely2.2.0,
BLAS/OMP1. Seven alternating normal-priority rounds after warmup, concurrent
host load2.36/5.10/11.68. Compare early foreign rejection with constructing first
and applying the unchanged physical checks afterward. Both return the same94
accepted station/body arrays exactly; rejection reason precedence may differ.
Preparation, archive IO, rendering and independent replay are outside this timing.

| Planner |Early foreign filter |Build before foreign filter |
|---|---:|---:|
|CPU median |1208.026ms |1353.135ms |
|Wall median |1219.157ms |1372.588ms |
|Body builds/successes |94 |102 |
|Contact-pair checks |1705 |1705 |
|New-pier pair comparisons |8742 |8742 |
|Allocation events |3738337 |4041968 |
|Requested allocation B |463573742 |494647249 |
|Tracked heap peak B |866185 |868833 |

Separate Memray1.20 traces include Python allocators and complete94-output
paths within the unchanged budgets. Eight unnecessary builds are avoided;
about10.7% CPU and6.3% requested allocation bytes are saved in this experiment.
Known accepted Mesh-constructor copy subset76560B is unchanged. It excludes
failed builds, temporary/reference/NumPy/GEOS/IO copies; total traffic is unknown.
Requested allocation bytes are not retained payload, copied bytes or RSS.

A separate normal whole study uses1.318CPU/1.323wall s input/context preparation,
3.578CPU/3.584wall s total including both variants/serialization,138203136B OS
peak. Instrumented/report-reading process peak162754560B is separate. Python
reference times and heap peaks are no native/device/World/p99/8GB/UMA proof.
Earlier low-priority probes under competing builds exhausted the same8s wall
budget locally; their explicit partial ledgers remain counterexamples, not
reasons to increase the limit.

51 focused checks pass: existing contact/traffic/rail controls plus source-role
refusal, missing/extra original stations, capacity-preserving partial selection,
wrong-station contacts, source parsing without layout, rounded foreign-neighbour
selection, whole traffic exclusion, new-pier spacing, coverage-first selection
and equal accepted geometry with/without early rejection. SIGGRAPH first,
optically inspected: [Chen2008 PDFp2/Fig2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
separates graph and geometry; [Galin2010 PDFp8/Figs17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf)
shows profile/terrain-relative bridge generation. Whole support-contact
construction remains Outshine's existing reference, not a structural solver.

Four actual matched whole/detail1280×720 images were rendered and opened:
old scene arrays remain exact prefixes, new piers visibly meet terrain/decks
beside the old paths. Blender5.2.2 LTS/EEVEE/IrisXe OpenGL,16samples/two threads,
all render processes terminal0,2.51–3.04s. Detail9276 changed pixels,8528 over1/255;
whole449 changed pixels. Coarse terrain, simple family materials, missing rail
hardware and open outer routes remain visible. No published images or galleries.

## Reproduce and remaining scope

Use the existing.venv; set
`PYTHONPATH=lib/src:test/experiments/transport:test/experiments/render` and
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. Freeze sources; fresh temp outputs only.

```sh
python -B -m unittest rail_supports_checks stair_support_checks stair_support_traffic_checks elevated_rail_checks
python -B -m rail_supports_study --deck <verified-1768-deck-study> --output /tmp/outshine-rail-supports/run
python -B -m rail_supports_verify --deck <verified-1768-deck-study> --output /tmp/outshine-rail-supports/run
python -B -m rail_supports_display --study /tmp/outshine-rail-supports/run --baseline-scene <verified-1768-display>/whole-flat-scene.json --output /tmp/outshine-rail-supports/display
python -B test/experiments/transport/render_transport_research.py /tmp/outshine-rail-supports/display/after-detail-scene.json
```

The bound deck report records every concrete prepared114-source input and hash.
Current raw/negative/cost/image evidence:
`/tmp/outshine-research-rail-supports-5d44efcbb-evidence/` (`outshine-final`,
`outshine-display`, `support-costs.json`, `forbidden-reader-terminal.json`,
`full-foreign-control.json`, terminal logs and preserved source inputs).
This is a whole geometric support candidate. Foundations/load capacity,
rail hardware/curvature/C1, outer routes, correct elevated-product replay,
Buildings/Plants consumers, native/Source/World admission and both devices'
p99/UMA/full-session budgets remain open under #434/#21/#5.
