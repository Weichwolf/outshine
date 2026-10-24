# Exact source areas for numerically thin water triangles

Owner: [#970](https://github.com/Weichwolf/outshine/issues/970), parent #760.
`water_exact_membership.exact_overlap` computes each original source's positive
area in one supplied triangle, and the triangle area outside the source union.
It returns rational areas in input order. No coordinate, source ID, polygon,
triangle, island, height or solver constraint is modified.

## Geometry and bounded work

Inputs are finite stored binary64 XY coordinates and valid simple polygons or
multipolygons, including holes. `Fraction(float)` preserves those exact binary
numbers. The internal origin subtraction is rational; it does not round a new
translated mesh. A caller supplies all possible sources, or a conservative
complete candidate list such as the existing STRtree bounding-box query.

Every nonvertical ring edge is a rational affine y(x), restricted to its x range.
Triangle/source endpoints and all active-edge pair crossings partition the
triangle's x range into open slabs. Edge order cannot change inside a slab: any
change would require an omitted endpoint or pair crossing. Even/odd sorted ring
crossings produce the source intervals, including holes/disconnected parts.
Clipping them to the triangle and merging overlapping intervals gives per-source
and union slice lengths. Each selected endpoint remains the same affine edge
throughout the slab, so the slice length is affine. The trapezoid integral is
therefore exact; slab boundaries have zero area. Union coverage is counted once,
even when several sources overlap. Positive areas are never thresholded away.

`ExactBudget` defaults to 100000 counted operations, 4096 bits per checked rational
value, and 512 active edges. Configurable maxima are 10000000/16384/4096. Counters
cover input-point conversion, active edges, edge pairs, slabs, edge/slab tests,
line evaluations, source/slab visits, interval integration and union intervals.
Edges are capped before insertion. Refusal raises `ExactBudgetExceeded` and
returns no partial area result; prior independently completed results remain owned
by their caller. The same budget may be shared by several calls.

For E active edges there are at most 2+2E+E(E-1)/2 slab event values; pair building
is quadratic in this explicitly bounded local E. Slab sorting has additional
E log E comparison costs. This is a rare-conflict reference, not a global scan,
whole-network replacement for GEOS or a hard wall-clock deadline. Rational GCD/
intermediate arithmetic and comparison costs are not separate operation ticks;
checked-value bit limits are not peak-memory or every-intermediate-bit claims.
Caller input validity/complete candidate selection and GEOS/STRtree costs remain
separate contracts. No new native/cache/GPU format or dependency is introduced.

[Interactive and Robust Mesh Booleans, SIGGRAPH 2022, page 2, Figures 2–3](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf)
was inspected visually. It motivates exact classification when floating point
operations become inconsistent. The elementary 2D slab-area derivation above is
our smaller reference operation, not an implementation of that paper's 3D mesh
Boolean pipeline or a claim to its performance.

## Original counterexample and controls

The frozen native fixture, source table and WATER4 payload remain unchanged;
`inspect_source_membership.py` checks all acquisition hashes. GEOS3.14.1 reports
zero area against every individual source, while reporting about7.83084e-7 m²
against their computed union. Plain floating-point recentering changes that
classification, so it alone is not accepted as a proof or geometry repair.

The exact result identifies collection source46 (original native index1072):
7.830836439073031e-7 m². The exact uncovered remainder is positive:
4.328128757296415e-13 m². Returning a real owner does not mean asserting full
coverage. The native caller must preserve this residual and apply its existing
admission contract; this reference neither drops the face nor relabels it as
entirely covered. Native coupled-model/asset/Places admission remains open.

`compare_exact_membership.py` compares complete original-source evaluation with
STRtree candidate selection. Eight fresh normal/traced runs gave the same exact
fraction-valued memberships and uncovered area. On i5-1135G7/Linux/shared host,
three normal runs per method measured 34.62/56.75/44.66 ms for all48 sources versus
4.849/4.857/4.731 ms for the two candidates (medians44.66/4.849 ms). Indexed timing
includes index construction/query and the full exact computation; imports,
fixture decoding, numeric diagnosis, output and plotting are excluded.
Counted operations2925→271, input points2543→119, source/slab visits240→10.
Both paths use seven active edges/five slabs; peak checked value509bits.
Separate Python traced peaks150588→33612bytes; process RSS about45MB is unchanged.
Opaque index allocations and complete allocator/copy traffic are not measured.
No native/frame/device or full generation-time claim follows from these times.

Ten tests cover exact analytic rectangle integrals, independently counted integer
cell unions, source overlap, holes/islands/concavity, zero-area contacts, tiny
positive slivers, original-source index/full-pair agreement, rigid/dyadic coordinate
changes, immutable inputs and explicit budget refusal. Diagnostics show unchanged
geometry in physical coordinates plus a clearly labelled transverse nm enlargement;
the PNG was opened and remains private.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/water "$PY" \
  test/experiments/water/compare_exact_membership.py --indexed --image \
  --output "${TMPDIR:-/tmp}/outshine-exact-membership"
```

Omit `--indexed` for the full-source control; use `--trace` in a separate run.
This is a bounded Python source-area reference with a concrete native input,
not a completed native water/terrain or worldwide canonical-domain integration.
