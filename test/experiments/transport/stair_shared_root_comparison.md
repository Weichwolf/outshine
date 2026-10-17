# Whole attachments at a shared old source root

Owners: [#595](https://github.com/Weichwolf/outshine/issues/595), bug
[#597](https://github.com/Weichwolf/outshine/issues/597), parent
[#434](https://github.com/Weichwolf/outshine/issues/434). The previous
[69-source world](stair_network_extension_comparison.md) is the unchanged input.
No new public library/native contract, SDK dependency or foreign code.

## Actual failure and correction

Old sources `12998091542:0` / `1580280942:0` share original node `[1496,2188]`.
The selector skipped the entire root node after choosing one old profile;
it omitted the other endpoint. Two whole proposals correctly failed reuse:
7.799538 / 11.388744 m² lay on that undeclared old cap. Choosing the other
root similarly failed 7.900251 / 11.090323 m². These are actual cap intersections
at a proven original node, not connectivity inferred from proximity.

`endpoint_bindings` excludes only the already declared root profile. Other old
endpoints at that exact original node become explicit `endpoint_contacts`.
The combined four-secondary-contact limit, two-to-four whole profiles per pad,
frame/ground/source checks and geometry tolerances remain. Near-but-distinct
nodes, shifted frames, wrong endpoint kinds and missing cap intervals reject.

The first corrected proposal generated a body, but its fresh reader rejected
two separately constructed partial pads. That output is discarded as a delivery.
The final constructor builds **one hull of every actual incident old and new
whole profile**, then constructs secondary pads only at other nodes. The reader
still independently reconstructs the required hull. Two new controls fail before
the selector fix; final controls also cover one complete hull, root deduplication,
distinct nodes and unchanged budgets. The [small actual source fixture](fixtures/stair-shared-root-endpoints-vienna.json)
retains MVT/ENU coordinates, properties, input hash and both old IDs.

## Verified 69→71-source result

Frozen producer, source/installed fresh readers, full control, costs and images:
`86c9816956ce00fd9a6c2c99390a874335654f46`.
[Fixed selected recipe](fixtures/stair-shared-root-module-vienna.json).
IDs have prefix `openfreemap:14/8937/5678:transportation:`.

| Whole source | Length, m | Existing procedural profile |
|---|---:|---|
| 1950369630:8 | 12.889659 | 1.8 m unpaved footway |
| 103612620:17 | 5.728743 | 5.5 m paved minor road |

Total **18.618401 m**; both whole originals connect through complete profiles of
both old roads. All old geometry and all 1657 original line parts / 71 areas
remain: 71 used / 1586 unused lines with IDs/attributes/geometry/reasons. Native
admitted IDs remain empty. PBF SHA256:
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
DEM, source catalogue and Vienna ENU origin/frame remain as in the linked report.
No source trimming, raster of old geometry or larger tolerance.

Surface 27 vertices / 23 facets, closed body 54 vertices / 100 faces;
Double/Float32 volumes 10.400482 / 10.400553 m³ and normalized grades
0.561926 / 0.561935 pass. Thirteen interface segments total 4.446796 m,
24856 wall face tests. Whole old/new internal/outer profiles, source ownership,
cyclic connected vertex links and both old reuse regions pass D/Q.

167 terrain facets become 225 / 129 vertices, 31 exact boundary edges / 111
pins; 30057 outside triangles remain unchanged. 98 complete source-bed facets
pass 7844 contact pairs, maximum Float32 error 14.868961 µm; 48 old body facets
pass 4818 pairs, maximum 14.574852 µm. All 124 old piers pass 7150 pairs,
maximum 15.170145 µm. No bridge underside overlaps this local patch: clearance
is inapplicable, not a new underpass. Complete control rechecks old bridges.
The larger six-source proposal still fails actual Float32 vertex uniqueness
after corrected root binding. Other actual refusals and untried alternatives
remain; no individual impossibility, solver or geometry budget increase.

## Complete owner-aware control and tests

Complete diagnostic control now supports attached modules as well as seeds:
ten old assets / 69 sources / 14068 closed body facets, 584736 B geometry and
48872 B explicit source-facet indices from verified immutable local owners.
`source_selection` uses those indices only in complete control; normal local
code retains the previous bounded mask. No global 65th-bit shift or missing
old body. The independent solve matches all **30282 XYZ facets/attributes**
and every module/layout/field array dtype, shape and byte. The diagnostic full
body matches that independent body. Controls select source65 from distinct
owner columns, compare all offsets to bounded masks, reject unknown sources
and retain 16-asset/32768-body-face and normal 64-local-source caps.

63 source / 63 installed controls pass with `-W error`:
`stair_endpoint_checks stair_growth_checks stair_frontier_checks stair_seed_checks
stair_local_checks stair_module_checks stair_module_chain_checks
stair_reference_checks`. Unchanged PR #592 wheel: 205 byte-identical source
modules; no library code/dependency change. Fresh source/installed automatic
readers verify selection, attempt window, body/field and dispositions. Original
receipts stay protected through disposable clones excluding receipts/display.
Ruff/format, functions≤120, diff-check and docs lint pass separately from native gates.

## Work, allocations and actual whole images

Linux x86_64 i5-1135G7 VM / Python 3.13 / NumPy 2.2.4 / Shapely 2.2.0, one
BLAS/OMP thread. Generation 6.576630 s / 133144576 B RSS; automatic proposal,
generation and reader 18.840649 s / 154378240 B. Fresh source/installed automatic
readers 13.613727 / 12.430536 s, peaks 152838144 / 152870912 B. Complete
diagnostic 38.985250 s / 187576320 B. These are different operations.

Equal-result construction after input preload, single serial shared-host runs:

| Mode | Normal seconds | Process RSS, B | Allocations | Allocated B | Tracked peak B |
|---|---:|---:|---:|---:|---:|
| local | 3.696103 | 128651264 | 11184323 | 1471928133 | 1415666 |
| whole | 30.207410 | 187551744 | 135998279 | 15385093881 | 50999674 |

All four normal/profiled module/layout/field results have the same byte hash
`2f07eb607371baa21f0530bf9bae7fdc2340ce6f1eb3b2a10e391150aaa2fe96`.
The local proof preserves unchanged outside; whole mode reconstructs/rechecks
it. Memray1.20 trackers exclude input/import/hash/export, instrumented times
9.032742 / 82.565907 s are not normal timings. Allocation churn is not live
RSS. Other Buildings/render work overlapped; no universal/native/frame claim.

Local context 490 terrain facets; one of ten assets / eight local sources /
856 body faces / 39240 B road arrays. Layout raw/unique/mapping arrays
9744 / 2064 / 4872 B; weld indexed/difference arrays each3664 B. Retained
terrain copying0 B, replacement mask26559 B; total internal copy bytes unknown.
Indexed/full-scan selection: identical roots/recipes/reasons and unchanged
resident geometry; 88 node lookups / 25 heap pushes each,125 versus132088
incidence tests. Observed0.119216 /0.210173 s, allocation counts616219 /812780,
allocated61984180 /69850523 B, tracked peaks1543463 /1296926 B. These single
selection runs exclude source preload and do not prove universal dominance.

Whole 1280×720 before/after pairs were opened with identical camera, projection/
view, lighting, samples and renderer helper hashes. Only **118 /141 pixels**
change (22 /26 above one level). The junction is barely perceptible; no
substantial visual upgrade. Coverage/material/light deficits remain. No
closeup, gallery/server or public image. EEVEE/llvmpipe49.753314 /17.012362 s,
process peaks2292002816 /1821151232 B, with concurrent software rendering;
no RTX/native/device/warmcache acceptance.

## Reproduction and accepted receipts

Use the retained 53-source resident plus ordered 55-,57-,65-,69-source modules
from the linked reports. `TMPDIR=/tmp`, one BLAS/OMP thread,
`PYTHONPATH=lib/src:test/experiments/transport`, fresh explicit outputs under
`/tmp/outshine-transport-<task>-<commit>`, never `/var/tmp` or private build
trees. Existing CPU120/300 s and AS3 GiB generator/reader limits remain.

1. `python -m stair_growth_experiment --resident <53> --prior-module <55>
   --prior-module <57> --prior-module <65> --prior-module <69>
   --proposal-offset 6 --output <fresh>`; default attached/roots-first/prefix,
   at most3 attempts, first succeeds after correction.
2. `stair_growth_verify` with identical arguments in disposable clones;
   replace `lib/src` with the wheel target for installed acceptance.
3. `stair_local_reference` with the same chain, `--baseline <independent69>
   --fixture <recipe0> --local <module0> --output <fresh-control>`.
4. `stair_local_cost` with that chain/baseline/fixture, `--mode local` or
   `--mode whole`, fresh `--output <json>`, optional `--profile <trace>`;
   `stair_growth_compare --profile` compares both selectors on identical input.
5. `stair_local_display` with the chain, `--local <module0> --reference
   <fresh-control> --before <69-display/after-display.npz>`; render emitted
   scene files through `render_transport_research`, compare with
   `test/scripts/pixels.py` and actually open complete PNGs.

Evidence root `/tmp/outshine-transport-attached-ac80c25bd/`:
automatic report `outshine-complete-root/comparison.json`
`cd7412ebc78ac61b4d3051829b11d1fc93061d8e49f52624da4120db4f25ccd8`;
accepted module `19ef2dabca7fc1fd2825b9a1349e0b6cbf870da19bb36bbe90282f5eb12d45b6`;
protected receipt `d6dff6bb919412e8d44bb817796a4929822d7517eac1dabaab81b9f2e18fcbac`;
complete `outshine-reference71/comparison.json`
`3e3b8bf310ffb33244dcdbf52e822c3598f7145e5fc49068528166c26535d57d`;
matched-image proof `a250d80d2f233fb9204cb49cd16e6ffcf6f145b1400f11b7a0b3ead1b3af2d82`.
Cost summary (four results also match the accepted saved arrays),
`cost-summary.json`: `00498de01783cf60c4129d20ef5f472f368a16c3c5a09156b3dd0f8f11ffa2e9`.

SIGGRAPH starting point: Chen et al.2008, [page2 /figure2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
visually reread: original graph proposal and actual3D admission remain distinct.
Existing profile hull and source contracts supply this correction, no new
tensor-field/vendor method. Larger thin junctions, worldwide coverage, affine
consumer callbacks and native delivery remain open; complete goal active.
