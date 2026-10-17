# Retain connected whole-source prefixes after physical failure (#539)

The automatic 51-source parent from [#537](stair_frontier_comparison.md) has six
more original-node module proposals, all locally rejected by existing physical
gates. Its largest four-source proposal contains an entirely fixed triangle
whose normalised whole-face gradient is 3.034239, above one. Removing the fixed
DEM junction boxes does not fix it. Pairwise fixed-edge tests alone miss the
problem. The adjacent single original minor road, however, passes whole local
source/body/profile/terrain admission. A failed joint recipe does not establish
that every constituent original source is unusable.

The reusable `transport.frontiers.connected_prefixes` validates discovery
parent-order witnesses and returns whole-source count prefixes, halving to one.
The opt-in `stair_frontier_experiment.py --strategy prefix` tests them in order
at the same root. Root references, minor profiles and all secondary contact
declarations are regenerated for each source choice. Existing `whole` strategy
tests complete proposals for comparison. Both retain a maximum of three actual
geometry attempts, eight sources per attempt, four parents and 64 source bits.
No height, Float32, interface, terrain or pier-contact gate is weakened. Invalid
joint recipes retain the verified old world and original reason/source ledger.

Source dispositions distinguish physically valid candidate sources, unadmitted
sources present in failed joint recipes, and sources outside the bounded
geometry attempts. Failure reasons remain attached to the actual attempts;
omitted sources stay eligible original inputs for later recipes. This is a
bounded valid-partial-network method, not a maximal subset solver. Halving can
skip a larger valid alternative; an eight-source proposal needs four candidate
sizes including one, while the unchanged cap allows only three attempts.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5 figures 7–8 and section 6/6.1, visually inspected. Graph/seed and
geometry separation is relevant; the present halving rule is independently
defined for original provider sources, not the paper's tensor-field tracing
algorithm. No third-party code or SDK dependency is introduced.

## Frozen comparison and physical products

Both automatic generators and independent readers use frozen runtime code
`fd572730a44ac4fcca44887411b2542e891a2cbe`. Counter/control-only commit
`004146b5f` adds the real fixed-face fixture and independent cross-product test;
it does not change that runtime code. The actual versioned counter is
`fixtures/stair-prefix-fixed-face-vienna.json`: captured fully fixed face 58,
its three actual XYZ values, grade frame, original input hash/root/module IDs.
The counter is specific to the rejected triangulation, not a proof that all
possible geometry for those sources is impossible.

Same exact verified 51-source parents and original PBF as #537, SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
The indexed original-node plan has six roots and 457 eligible new ground
sources. `whole` spends its three attempts on modules of 4/6/5 sources and
retains the valid 51-source baseline: infeasible fixed/grade constraints,
a frozen-contact level conflict, and a facet collapse after coordinate casting.
`prefix` tries 4 then 2 sources at the first root, succeeds on the second
attempt and stops. The one-source choice and remaining roots are not tried.

Accepted original IDs, prefixed `openfreemap:14/8937/5678:transportation:`:
`103612620:11` (minor road, 214.771049650 m) and `97041640:1` (cycleway,
204.198018522 m), total 418.969068171 m. Root is cached `103612620:19`,
original interior vertex 1. The road uses the existing explicit procedural
5.5 m/two-lane unclassified recipe; original tags/direction are preserved,
missing data remains an assumption. Whole new road gradient/crossfall and all
profiles pass Double/Float32; maximum longitudinal grades 0.009103712 /
0.009105302, crossfalls 0.011072962 / 0.011071952. This does not establish
complete motor access, endpoint turns, driving dynamics or native admission.

New surface: 34 vertices/38 facets, closed body 68 vertices/132 facets.
Two whole new inner endpoints, one actual cached interior cross section and
two outer profiles pass. Preserved history plus this addition gives 53 sources,
86 inner endpoints, three cached interior contacts, twenty outer profiles;
the four previous new-source through checks remain separate. Actual old
road bodies/display prefixes and source identities are unchanged. The two new
source caps have no positive-area overlap with the existing whole bridge
surface, in both precisions. Existing upper structures and all 124 whole pier
contacts pass, maximum existing Float32 error 15.170 micrometres.

Local terrain: 166 selected old facets, no protected parent/pier affected,
204 points/358 facets, 48 exact boundary vertices and 173 fixed heights.
Welding: 312 input/206 unique vertices, 106 candidate pairs, zero displacement,
two unused points removed. Twenty split edges add eighty conformance facets.
Whole field: 14160 points/27962 facets; closed world body 28320 vertices/56636
facets. Outside the patch 27604 old XYZ triangles are byte-exact. Same
16384/32768 terrain, solver, interface, grade, owner-bit and parent limits.
Four original Terrarium files reproduce every versioned crop and all 14406
whole-world/local/module/original-axis prior samples bit-for-bit.

Both strategies' readers terminate 0 without old planning, CDT, solver, body
generation or array export. They replay original selection/prefix recipes,
actual saved whole products, original reasons, source dispositions and hashes.
Rejected modules' readers verify retained parents and original refusal ledgers;
they do not rerun the failed solver. The independent fixed-face witness above
substantiates the first recipe conflict separately. Accepted automatic report
SHA256 `a892f71947d6fc00faddf92d8b7f07d0554598d20f725f0108e601ea11ed6015`,
chosen module report
`d8bd3caffacd9f6328f4b8608263189d3da5946ef5435e068a339153ab61dabe`.
Whole-strategy report
`85239090dfca7468be6be8d0b36eda49d21e019c5a25e8328b75edc2b1771ac0`.

112 source / 112 fresh-installed controls pass. All 201 installed Python
modules match source; the previous 200 outside `transport/frontiers.py` are
byte-identical. That one module adds the prefix helper without changing the
existing discovery function. Wheel SHA256
`cd477c61e755d4f7efbd049af08d1733afd2b885673f63c403742bb620d38002`.
Additional cached Zürich/Basel/Midtown original inputs verify sixteen/sixteen/
fourteen connected prefix choices from four roots each: actual node membership
and root/parent connectivity, identical index/scan choices. No physical city
or 3D admission is inferred from those topology controls.

## Costs and actual images

Same Linux VM as #537: four exposed logical CPUs, Intel i5-1135G7, Python
3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0; BLAS/OMP one thread,
nice 10 and concurrent Buildings CPU/render work. Whole automatic run includes
original selection, failed attempts, parent loads, all physical checks, original
source ledgers and serialization. Subprocess/render/reader allocations excluded.

| Strategy/output | Normal seconds / peak RSS B | Profiled seconds / peak RSS B | Allocation events / cumulative requested B / tracked peak heap B |
|---|---:|---:|---:|
| whole / retained 51 | 3.306933 / 126259200 | 7.166753 / 136630272 | 12199981 / 2038817498 / 23302753 |
| prefix / valid 53 | 22.955816 / 186236928 | 57.314111 / 209240064 | 116866356 / 13441762818 / 60695582 |

These produce different physical outputs: this is no equal-quality speed
ranking. The prefix method retains useful original sources within the same
attempt cap, and pays whole-world diagnostic/export costs. Its successful module
takes 21.387568 s; independent successful module reader 18.476152 s. Memray
1.20.0 captures native/Python allocation events with Python allocators enabled,
native call-stack unwinding disabled. Official statistics CLI returns normally;
fresh complete JSON is decoded/validated, then stdout/stderr are flushed before
explicit process exit 0, excluding interpreter teardown. Requested bytes are
allocation churn, not live/unique memory; tracked heap and process RSS differ.
All nine normal/profiled accepted NPZ products match in every array's shape,
dtype and bytes. Explicit local raw XY 13728 B, unique XY 3264 B, labels 6864 B;
global XY/labels 340704 B, outside XYZ comparison 3974976 B, display-owned arrays
10864080 B. These counters do not observe every internal native copy. Global
physical diagnosis remains costly; no frame/native/device bound is established.

Four fresh actual Eevee images were fully decoded and opened: before/after
whole overview and moderate network view, all 1280×720. Receipts bind mesh,
scene and PNG hashes. Software llvmpipe LLVM 19.1.7, render times
12.819311/12.236585/14.394094/13.227936 s. The new dark road branch is visible
on the left; its adjacent cycleway remains thin. `pixels.py` counts 1565
changed overview pixels (0.1698%) and 2074 network pixels (0.2250%). This is a
small coverage improvement; the scene remains sparse/pale and routes/outer
nodes angular. One last-frame launch was rejected because the counter/control
was being recorded during rendering; it produced no image. After freezing
`004146b5f`, that missing frame was freshly rendered and all four receipts
validated. No public gallery or native image is claimed.

## Reproduction, evidence and remaining work

Authoritative private root `/tmp/outshine-transport-expansion-d35560682/`:
`automatic.py` retains exact guarded whole/prefix/profile/replay commands;
`cost.json`, `normal-profile-array-proof.json`, `installed-proof.json`,
`raw-proof.json`, `city-prefixes.json`, `render-completeness.json`, original
Memray traces, terminal logs and frozen height-input counterexamples are retained.
Accepted complete output
`/tmp/outshine-transport-prefix-prefix-fd572730a/outshine-module-1/`;
full original-source plan, recipes, attempts and actual source dispositions live
in its parent directory. The failed four-source attempt remains alongside it.
Whole-strategy retained baseline/refusal products live in
`/tmp/outshine-transport-prefix-whole-fd572730a/`. Duplicate profiled exports are
removed after exact-array proof; normal products and compact cost evidence remain.

Install `./lib`; reproduce #537's three cache parents, then its actual 51-source
output as the fourth ordered `--cached-module OUTPUT RECIPE` pair. Use the
same `--base`, `--supports` and explicit fresh system-temp `--output` with
`stair_frontier_experiment.py --strategy whole` and `--strategy prefix`.
`stair_frontier_verify.py` takes the same arguments and verifies both successful
and retained-baseline products without geometry generation. `--max-attempts`
remains 1–3. Source, roots and secondary contacts are derived afresh from
original inputs; no handpicked admission fixture is needed. The installed
prefix helper and existing `stair_frontier_compare.py` allow replaying other
cached provider graphs; `city-prefixes.py` in the evidence root also checks
actual source-node membership for each parent witness.

Original inventory retains 1657 line parts and 71 areas, 53 candidates, 1604
unused lines and every original/ID/reason. The two omitted original sources
`80922970:163` and `14638079782:0` remain available; joint failure is not an
individual exclusion. No native `admitted_source_ids` are added. In particular,
the four-parent diagnostic import is now exhausted: this output is a fifth
module, not evidence of a scalable cache graph. Further world growth needs an
appropriate bounded spatial asset representation, not a higher legacy cap.
Worldwide module/cache/LOD/reservations, remaining sources/levels/classes,
complete routes and native/structural acceptance remain #434/#50/#48.
Format/Ruff, changed functions ≤120 lines, diff and docs lint pass; native engine
gates are not run for this pure Python delivery.
