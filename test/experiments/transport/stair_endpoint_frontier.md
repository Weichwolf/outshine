# Automatic whole sources at verified old endpoints

Owner/integration: [#1738](https://github.com/Weichwolf/outshine/issues/1738),
parent [#434](https://github.com/Weichwolf/outshine/issues/434).
The physical old-end/new-through product is delivered by
[#1736 / PR #1737](https://github.com/Weichwolf/outshine/pull/1737).

The preceding discovery inspected only old interior profiles. In the actual
107-source context it found zero roots despite the valid new through source.
`stair_frontier_roots.py` now also proposes unique original through occurrences
at actual old outer Ground profiles. Each new contact binds its first whole
source. Existing interior roots retain their identities and precede the new
contacts under the same 64-profile budget. Source/frame and actual old cap
checks precede graph proposals. Original MVT occurrences establish the proposed
constructive contact, rather than original OSM-node or native routing authority.

`frontiers.py` accepts optional strictly boolean `endpoint` and explicit
`initial_source_id` root metadata. Absent metadata preserves the former interior
semantics. A bound first source must be eligible and uniquely meet that role;
connected-prefix halving retains it. Eight proposals/sources, visit budgets and
three physical attempts remain unchanged. The new family uses the existing
`free_region` terrain recipe. Selection reasons preserve deferred sources.

The geometric request uses the existing bounded neighbourhood/asset window.
All 107 resident source IDs determine eligibility; only 13 actual local owners
determine local body masks. This avoids proposing an already resident source
without making a global bit mask. The previous source-specific request delegates
to the same `state_for_axes`; its arrays and work remain equal in controls.

Reference: Chen et al., *Interactive Procedural Street Modeling*, SIGGRAPH2008,
sections6/6.1 and figures7–8, [local PDF](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
Page5 was visually inspected for the distinction between graph connectivity and
realized 3D geometry. Proposed graphs still require the whole physical product.

## Reproduction and result

Use `.venv`; `PYTHONPATH=lib/src:lib/tests:test/experiments/transport:`
`test/experiments/assets:test/experiments/render:test/experiments/buildings`,
BLAS/OMP threads1. Existing NumPy/Shapely/Clarabel/Memray dependencies suffice.
`fixtures/stair-endpoint-frontier-query.json` contains only the geometric request,
frame and fixed original input reference, with no selected source or old port.

Run `stair_endpoint_frontier_study.py --query <fixture> --resident <resident>`
with ordered repeated `--prior-module` and fresh system-temp `--output`. It
compares indexed/scanned proposals and original parent witnesses, writes recipes
automatically, and runs the existing local generator and fresh reader per attempt.
On `e4d2c95d4`, 1657 source records/6014 node occurrences yield one verified root.
The six-/three-source attempts retain the old baseline after invalid terrain
coverage. The third, whole `80922970:255`, succeeds:107→108 sources, unchanged
27 old assets/186 arrays/957042B and124 support contacts. Remaining sources retain
their joint failure/deferred reasons. The geometric output matches #1736 exactly.

Actual new full before/after exports, all24 body/field/selection array fields and
the four camera/material scene descriptors match the previously inspected images
exactly. Ten exported array fields/17634888B are compared. Those four actual
1280×720 images are reused; their closed root, coarse terrain/diagnostic materials
and two open outer ends remain the image assessment. No new gallery is published.

Run `stair_endpoint_frontier_cost.py` with the same request/parents, `--study`
and a separate fresh temp `--output`, at normal priority. Optional
`--reference-commit e4d2c95d4c02113ee2ff8ecf2b73029517edf6e2` loads that frozen
historical procedure into an isolated reference module for the same input.
On `b7eb74e1b`, nine rotating rounds, Python3.13.5/NumPy2.5.4/Memray1.20.0,
load2.25/3.89/6.02: index/scan/historical-index CPU medians6.996/9.554/20.314ms;
events69225/70636/183608; requested bytes5651852/5686428/21009180; scoped peaks
907824/902696/912320B. All produce equal source/witness/disposition tables.
Index/scan incidence tests34/1050,21 node lookups and6 heap pushes each;
43 indexed nodes/50 entries. The common plain-int path removes repeated abstract
integer checks while preserving NumPy integral, bool rejection and int64 bounds.
IO, eligibility/profile creation, geometry/readers, total copies, process/UMA and
native times are outside this discovery scope; peak memory is not greatly reduced.

Source and separately installed Wheel controls:90 tests/0, including original
roles, initial source, permutations, budgets, actual caps, frame/Ground/axis
counterexamples, global resident/local owner distinction and integer edge cases.
Only local constructive Python products are admitted. Native source admission,
global source completeness, public routes, World, device/UMA/p99 remain open.
