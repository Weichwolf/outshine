# Actual twig coverage and finite window size classes

Owner: [#601](https://github.com/Weichwolf/outshine/issues/601), under #182.
Uses the unchanged complete [#588spruce](spruce_whorl_balance.md), its own fixed
physical needle recipes and the [cached portrait](window_portrait.md). Python
experiment only; no shared library/native/material/reservation/provider API.

## Finding and decision

Counts and matching needle IDs did not establish spatial fidelity. The old256group
prototype spans0.306m. Every partial last window used that entire prototype with
alpha multiplied by its count fraction. Actual small needle clumps became long,
faint strips. Nine real short/medium/long axes across all three needle orders,
17directions and matched projection/filter show only0.7–6.3%median needle-area
loss per axis, but appreciable spatial error. This does **not** explain all large
crown gaps or justify increasing the physical foliage area to compensate.

Select the smallest sufficient physical prototype from2/4/8/16/32/64/128/256groups
for each existing window. The complete opened2560×1440/32sample light image has
subtle finer clumps; the tree remains too sparse and mechanically structured.
Retain this representation experiment, not a whole-form approval or a change to
the species recipe. #182still needs the promised architectural/area reassessment;
no further narrow crown-profile sweep or350stand run follows from this fix.

## Real physical reference and independent validation

`cached_twig_source.py` reconstructs only requested closed needle/short-shoot
geometry from cached AxisState, actual scaled radius profile, seed815, global
first needle ID, original procedural prototypes/materials and exact variant cycle.
No PlantPlan-v4 parsing, full near plant, new growth or new needle meshes.
Requests admit at most1024needles/250,000triangles per twig before detail work.
First nine requests total2683physical needles/611,724triangles, processed serially.
Their cache intervals and local/world frames have independent controls.

`matched_images` reuses the existing raster/filter/composition pipeline. Main wood
is excluded so it cannot hide needle error; short-shoot wood and visible needle
coverage are measured separately. Physical/proxy use exactly the same bounds,
directions,128square raster and4×4subpixels. Comparisons include128/32/8/4pixel
rasters. The last number means raster width, not the twig's actual screen span.

The size rule was then checked on nine previously unused real axes, isolating
their actual tail intervals7..255needles. Exact subsets reuse the full requested
twig's seeded details; they do not restart its RNG. These tests prepare4527bounded
near poses and render943selected needles.17directions use8×8subpixels. In the
actual2560×1440portrait camera the subsets span about1.3–5.7pixels on their longest
screen axis. Whole2560×1440RGB/error and alpha comparison sheets were opened.

| Isolated tail needles | Median needle L1 at4pixel raster, old → matched |
|---:|---:|
|7|0.9071 →0.4009|
|15|0.6009 →0.2537|
|31|0.5776 →0.1391|
|63|0.4365 →0.1151|
|95|0.2818 →0.1510|
|127|0.1256 →0.0728|
|159/191/255|unchanged|

All six smaller held-out groups improve in median; three retain the original256
family exactly. Individual views can regress, for example95needles in displayed
view8. Seeded jitter, canonical stem radius, angular sampling and straight-template
curvature remain approximations. Neither this table nor the2×capacity bound is
a universal image-error bound, botanical validation, wind/shadow/temporal LOD
approval or proof for all species. Tiny1–6needle tails have scalar binding tests
but are outside this held-out physical sample.

## Product, preservation and demand

`window_capacity_catalog.py` reuses the old256bank decoded-exact and generates
only254additional canonical needles/shorts. Eight finite64direction32×32banks
need18,874,368decoded image bytes, versus2,359,296before:16,515,072extra bytes.
Build14.535s on this host, one cached catalog construction, not per-frame work.
The fixed own sources regenerate independently from the committed16KiBinput;
all26legacy decoded fields remain exact in a fresh source rebuild on this host.

`window_capacity_product.py` consumes a complete cached plan with growth, whole
foliage, needle detail and needle meshes forbidden. Only family/capacity/weight
change. All nine axis fields and eight other window fields remain exact; windows
stay75,612and represent13,822,818needles at the same physical area.21,124windows
choose a smaller family,54,488keep256. Scalar1..256controls prove smallest valid
selection, weights>=0.5and unchanged inputs; invalid catalogs/counts reject.

The plant identity remains
`481411edefd956a9e3d74c28fe87e9ee02f27e48ab8669ef24c76156061441ee`.
Representation version `capacity-matched-windows-1` and catalog version
`finite-window-capacities-1` distinguish the new product and input hashes.
Old products/default builders are preserved. The new catalog retains its full
family under the original key; only the explicit rebind selects smaller ones.
Fresh source/installed three-twig cache replay remains exact with real slot239,
no growth/whole near plant/needle mesh. This proves ledger/detail consistency,
not visual equivalence of every individual needle and its statistical proxy.

## Cost, implementation and verification

Linux x86_64/4logical CPUs/16GiB, Python3.13/NumPy2.5.4/bpy5.2.2, llvmpipe.
Binding75,612already loaded windows takes one observed2.696ms; load76.231ms is
separate. It retains1,209,792new array bytes and shares10,056,396unchanged bytes.
Memray1.20.0/Python-allocator tracking around the binding only:
1764allocation events,3,297,892cumulatively allocated bytes,2,331,951tracked peak.
Instrumented4.047ms is not an ordinary latency sample. Process peaks85.375/
92.875MiBinclude imports/input; internal copy bytes remain unknown.

The complete light image retains2,599,160triangles and50,627,364geometry/instance
payload bytes. Eight families increase texture/material grouping; reduced quad
extent does not prove GPU savings. Software render379.373s/process381.238s,
peakRSS2872.633MiB; mixed diagnostic/host load, no timing speedup/device claim.
First nine-axis comparison20.584s/290.391MiBpeak. Fresh installed held-out replay
98.346s/678.691MiBpeak includes repeated1024square subpixel buffers; it is an
offline reference, not runtime work. All held-out pixels and metrics match source.

Initial comparison producer `8a560f8ec`, catalog/product/portrait `36916b313`,
independent subset producer `51e29e438`. Complete producer/input/module hashes
are in the actual receipts. The extracted original comparison matches frozen own
#598metrics exactly. The128/8reference exposed a raster pair-budget mismatch;
the existing helper now requests sufficient bounded pairs, with a solid-quad
control; default32pixel catalog fields remain exact. Legacy comparison CLI now
requires explicit product/model paths. No source cache is silently reinterpreted.
41source/41installed controls pass,205Python wheel modules source-exact. The
installed wheel is the unchanged verified #598package, not a source-path fallback.
Ruff/format/functions<=120lines and lint-docs are the affected static checks;
native integration and device budgets remain unproved.

## Local sources and reproduction

First consulted local SIGGRAPH2003 *Billboard Clouds* §1.2/PDFp.2, then
Bruneton/Neyret2012 §8/PDFp.8; both pages optically opened. Geometry/coverage
error and opacity composition motivate independent comparisons, not a claimed
implementation of those papers or their performance. Fresh read-only Ghidra
decompile at`01f4d840`, SpeedTree10.2billboard-export worker, succeeds: scan-line
sample decisions are packed into bit fields. It does not reveal our size rule,
prove alpha semantics of the sampled channel or supply imported code/constants.

Use `.venv`, `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1` and
`PYTHONPATH=lib/src:test/experiments/vegetation`. Rebuild sources/products using
the [self-contained recipe commands](spruce_whorl_balance.md). Then run
`cached_twig_compare.py` with explicit `--product --catalog --models --output`;
`window_capacity_catalog.py --source SOURCE --output /tmp/capacity-catalog`, then
`window_capacity_product.py --product PRODUCT --original-catalog CATALOG
--capacity-catalog /tmp/capacity-catalog --output /tmp/capacity-product`.
`cached_window_holdout.py`, `cached_twig_plot.py`, `window_capacity_cost.py` and
the existing portrait/replay scripts provide the independent checks; `--help`
lists explicit paths. Profiling optionally uses an existing Memray installation.
Private proof root `/tmp/outshine-vegetation-window-fidelity-proof-813112870`
retains source inputs, complete outputs, pixels, raw allocation profile and logs.
Opened new portrait SHA256
`ae0e5cbe88483f700d9d700e82f86c2437a264e5fe7d4f7ccc1a80da493524dd`;
baseline pin in the prior report. No research PNG under doc/ and no server.
