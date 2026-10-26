# Compact straight flights in the shared region (#1003)

The six-source proposal in #1012 leaves the observed short stair239 outside
the region. Its actual unchanged axis point conflicts with the2m prior budget.
The caller can instead include all seven original source parts and explicitly
select239 for a compact straight-flight recipe. There are no native IDs or
coordinates inside the procedure. Actual outer-boundary admission remains open.

`mvt_t_compact_caps.py` requires a straight flight with enough remaining run.
Each end receives a0.30m-deep flat strip. Its transverse span covers the whole
incident corridors, including the prepared outward grid boundary; it does not
widen those corridors. A nominal-width-only mask leaves duplicated faces at
the grid boundary. Shared compact ends need an observed, near-collinear endpoint
neighbour. Multi-leg/interior/T-junction patches need their separate recipe.

An unknown source end can receive an explicitly **unbound** geometric cap.
This creates no observed neighbour or free-end claim. Known outside contacts
must be bound or jointly included; they cannot become unbound caps. Preparation
copies the proposal and leaves the original capture, sources and proposal intact.
The old three-/six-source defaults do not opt into this recipe.

## Determine stair capacity before fitting heights

The actual prepared station cuts determine usable run. Compact flights use a
declared0.30m minimum uniform tread depth and0.17m per-riser height budget.
The maximum riser count is `floor(usable_run / 0.30m)`, capped at64. Its height
capacity bounds the difference of the two shared landing variables in the LP.
No supported run is a recipe refusal. These are conservative construction
choices, not a maximum over every possible stair layout.

This matters even at an identical minimax objective: without the capacity row,
IPM can select two risers on the short run. Their intermediate band is narrower
than the existing0.28m support disk. Fitting first and accepting that body would
violate the functional contract. The bounded model produces one supported short
step with both simplex and IPM. Existing grade,2m prior,0.18m actual riser,
source coverage and Float32 gates are unchanged.

## Compile representable caps

`stair_float32_edges.py` handles exact Float32-XY aliases at one identical logical
Float64 height. Candidates must be an existing interior edge of one source mask.
Vertex membership and legal links are checked separately in every incident tread
band; boundary curves may only lose a redundant segment while retaining each
band. Outer/source boundaries, different height planes and incompatible step
memberships are protected. No radius or coordinate frame is enlarged/changed.

The survivor must agree with every incident affine height plane within1e-10m
and keep all surviving Float64 faces positive. Surviving GPU XY coordinates
are unchanged because the two endpoints encode identically. Another existing
alias may still need contraction; only the final complete Float32/body/source/
support audit qualifies the result. At most64alias pairs are examined, over
at most8192faces and65536points. Counters state their face-query/copy scopes;
they are not complete scalar-operation or allocation counts.

Two frozen actual negative vertex stars in `fixtures/mvt_stair_float32_*_star.json`
test an interior alias and a two-riser boundary case. They are inputs, not newly
generated acceptance oracles. Tests also check unchanged band polygons, affine
height sampling, boundary/identity refusals, source ordering and a synthetic
quarter-turn plus translation of the complete seven-source case.

The legal-edge tests follow Hoppe et al.,
[SIGGRAPH1993, §4.2/p.4–5/Fig.4](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf),
whose equations, local transformations and boundary conditions were inspected
optically. This is a restricted exact-encoding cleanup, not their energy optimizer.
The profile-before-terrain Galin/Chen sources remain in `mvt_t_profiles.md`.

## Reproduce and consume

```sh
export PYTHONPATH=lib/src:test/experiments/transport
.venv/bin/python -m unittest test_stair_float32_edges test_mvt_t_compact_caps -v
.venv/bin/python test/experiments/transport/compare_mvt_t_profiles.py --input test/experiments/transport/fixtures/mvt_stair_boundary_neighbours.json --region-sources native-part:240 native-part:431 native-part:432 native-part:239 native-part:402 native-part:535 native-part:537 --compact-stairs native-part:239 --output /tmp/outshine-compact-flight
```

The common products retain both stair frames and explicit source-owned level
ranges, shared interfaces, navigation and terrain reservations. Four external
profiles remain unknown:239/first,432/last,402/last,537/last. Open `profiles.png`
and the separate actual compact-flight image. Fresh simplex/IPM/projected/
explicit-equality workers use identical geometry and quality; memory is measured
separately. Native attributes, cache/mesh/nav/terrain integration, full routes,
handrails, outer roles and device/Places admission remain separate obligations.
The bounded solver region may end at an artificial cut through a source corridor;
it need not expand until every original endpoint is classified. Master qualifies
the actual unchanged native interfaces; #1008 binds their complete profiles.
