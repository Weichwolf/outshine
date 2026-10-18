# Two primary populations at the same foliage budget

Owner: [#610](https://github.com/Weichwolf/outshine/issues/610), under #182.
Python experiment only. **Rejected as a new species default:** the complete
opened2560×1440image changes local lower-crown gaps but does not produce a
convincing whole spruce. Preserve the finite counterexample; no more small
main-whorl/profile sweeps follow from this experiment.

## Question, sources and construction

The [actual-twig comparison](cached_twig_fidelity.md) found spatial errors in
small windows, but insufficient total needle-coverage loss to explain the open
crown. Compare the unchanged #588growth architecture with a genuinely different
primary population. Same stem, primary shape prototype, physical needles,
catalog, scene, lighting and expected physical-area admission; no opacity boost.

First consulted local SIGGRAPH1995 Weber/Penn §4.3/PDFp.4 for parent-dependent
length/position, and SIGGRAPH1988 *Developmental Models of Plant Architecture*
§4.5/§5/PDFp.7 for variation and longitudinal/angular branching. Pages optically
inspected, including the whorl/alternating illustration. The JRC2016 Picea abies
PDFp.1 was opened as a whole: its isolated field spruce has a substantially fuller
and broader crown than either experiment. The photo does not measure our tree's
age, dimensions, leaf area or an exact branch population.

Fresh read-only Ghidra of local SpeedTree10.2 succeeded at `0209a270`
`CGenerator::GetParentTemplateNode` and `020b6900`
`CGenerator::CreateNodesFromTemplate`. Source/target link lookup and dispatch
through a parent/template are visible. These functions do **not** establish a
Picea recipe, numerical branching law, support for our two-input construction or
vendor optimization. No vendor code/assets/constants copied.

The candidate uses existing immutable generators and distance/parent profiles.
Replace one of five members in each of48primary whorls with47smaller primaries
distributed independently over the same height interval. These are not claimed
to occupy every exact irregular-whorl midpoint. Their prototype is identical;
its inherited scale is0.7times the ordinary primary scale. Both generator2and5
feed existing fine generator3; generator4continues those fine parents. Distinct
full generator/slot paths retain both owners. No new general growth/curve kernel,
shared library or native/material/provider/reservation API.

`spruce_interwhorl.py` counts primary/fine/terminal lengths before executing the
full hierarchy. A three-value physical-spacing bracket admits1.45:1.4would leave
unsupported capped sites;1.5misses the unchanged5%area admission. This selects
area feasibility, not an appearance optimum. The per-primary fine limit201→208
allows every planned site; complete60,000axis/80,000window and100,000graph-pose
limits remain unchanged. Any capped parent rejects rather than silently omitting
support. Main/fine/tip lengths continue to use actual parents and local position.

## Complete products and image result

| Quantity | Whorl control | Two populations |
|---|---:|---:|
| Main / smaller primaries |240 /0|192 /47|
| Fine / terminal axes |22,228 /22,228|23,223 /23,223|
| Total axes |44,697|46,686|
| Windows |75,612|76,020|
| Represented needles |13,822,818|13,705,976|
| Expected half-surface area [m²] |242.346740|240.298223|
| Geometry/instance payload [bytes] |50,627,364|50,963,352|
| Camera/shadow geometry [triangles] |2,599,160|2,697,080|

Nominal target250.072346m²and5%admission are unchanged. Candidate expected area
is0.845%below control, not exactly equal mass. `interwhorl_execute.py` produces
all admitted axes/windows with complete foliage, needle poses and needle meshes
forbidden. The existing size-class rebind then uses the exact #609catalog;
18,874,368decoded image bytes in both cases. Four repeated wood prototypes remain
valid because both primary generators use precisely the same shape prototype.

Candidate identity
`a9fb6154552938c35902c8f04f95dc8d45126a0e22b4113c38c5f9f676b04589`,
graph`c1d7708a4cee230cbc2e427120409e8fc1e94fa737c159ce6913ba365aa32a60`.
New plant recipe version`experimental-intercalated-needled-1`; old products and
the control graph`4731ad577…` are unchanged.

The complete candidate and original control were actually opened at2560×1440,
32samples, identical scene/camera/lighting and llvmpipe backend. Local lower
coverage changes, but the whole crown stays thin, tiered and mechanical. The
candidate is not selected as a morphology improvement. No eight-form/350stand
bake, gallery, server, botanical/SpeedTree/native/LOD approval or shader change.
Candidate portrait SHA256
`0c71f012cf7a1335f8c8796dcfcf08dbb06a5805dbaa09c87b11c44080edf98f`;
control`ae0e5cbe88483f700d9d700e82f86c2437a264e5fe7d4f7ccc1a80da493524dd`.

## Spatial area, scope and next decision

`interwhorl_distribution.py` assigns each window its expected physical area,
including the original global three-needle variant cycle and scale²expectation.
Half-metre height bands and0.1mXY-radius bins use accurate `math.fsum` reductions;
the first weighted histogram exposed cancellation at8.3e-11m², so the diagnostic
now avoids subtraction of accumulated totals. The original mass-closure check is
retained. Its whole2560×1440plot was opened.

Area beyond2mfrom the world XY origin decreases74.541242→66.166819m²
(30.76%→27.54%of total). This is window-centre inventory, not exact intra-window
position, trunk-relative radius or projected coverage. Much of the additional
population replaces outer mass with inner mass; counting more vertical supports
does not establish a fuller crown. Height-band variation is not a gap-fraction
or botanical quality metric.

The nominal LAI×ground-area target is an author input, not an observed isolated
spruce measurement. Chen et al.1997
[*Leaf area index of boreal forests*](../../../doc/references/vegetation/botany/1997-leaf-area-index-of-boreal-forests.pdf) §2.1–2.2,
PDFp.2, also optically opened, distinguishes half total green surface from
effective LAI inferred from directional gap fractions; clumping matters at
multiple scales. The present experiment neither estimates that clumping index
nor fits an observed LAI. #182must next address fine-shoot spatial hierarchy and
projected gap/coverage together, instead of treating total area or more primary
locations as sufficient. The physical-area convention itself is not an excuse
to raise opacity or silently change needle dimensions.

## Verification and cost

Producer`496f10dea`: actual complete build40.098s/peakRSS280.332MiB, including
count preflight and file publication. Exact retained axis/window buffers
8,213,856/11,326,980bytes. Extra graph work is visible in the table; internal
copied bytes remain unknown. Separate Memray tracking surrounds actual execution
only, excluding source decode, comparison and output. Its regenerated axis/window
arrays are all decoded-exact. Instrumented latency89.358s is not ordinary runtime;
process peak289.785MiBincludes interpreter/imports and tracking.
128,635,251positive allocation events total11,861,883,254allocated bytes;
tracked peak149,682,237bytes. The257,252,629raw allocator operations include
frees and must not be reported as positive allocations. This exposes substantial
temporary churn, including repeated per-axis/per-window recipe serialization;
reuse at the existing pattern/order boundary is a separate measured cost task,
not a claimed benefit of the rejected architecture. No global CPU optimality.

Complete software render238.409s/process240.558s, peakRSS2836.531MiB. The older
control timing had different host load; no render speedup or GPU/device/frame
claim follows. Linux x86_64,4logical CPUs/16GiB, Python3.13.5/NumPy2.5.4/bpy5.2.2.
Five independent new controls cover scalar descendants, both real owners, global
ledger/detail demand, original graph/control preservation, pre-work refusals and
area weighting. Four initial controls plus41existing ones pass from source and
installed wheel; the final five new controls pass in both environments. All205
installed Python modules are source-exact; no shared library changes. Six actual
cached twigs across both primary populations and all needle orders reproduce
byte-exact in fresh source/installed readers, without growth/full-near/needle
meshes. Static checks: Ruff/format, functions<=120lines, lint-docs, diff.

Use the self-contained [source reconstruction](spruce_whorl_balance.md), then
explicit `--source`/`--output` with `spruce_interwhorl.py`. Execute the admitted
`intercalated-1.45.json` using `interwhorl_execute.py`; use existing
`window_capacity_product.py` and `window_portrait.py --wood-mode instanced`.
`interwhorl_replay.py` and `interwhorl_distribution.py --help` expose the actual
cache/plot inputs. Use `.venv`, `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1`,
`MPLCONFIGDIR` and `RUFF_CACHE_DIR` under/tmp; experiment import path as above.
Original inputs, complete product, actual PNGs, raw profile and terminal logs are
in`/tmp/outshine-vegetation-interwhorl-proof-c064d9075/`.
