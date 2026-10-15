# Bounded port-height solver comparison

Owner: [#217](https://github.com/Weichwolf/outshine/issues/217), consumer #192.
Measured 2026-10-15. **Keep Clarabel. None of the tested HiGHS policies supplies
a valid complete plan inside the unchanged limits.** This rejects these policies
on this input; it does not prove that every HiGHS formulation is unsuitable.
No experimental backend is promoted to `lib/`, and no native implementation,
new terrain acquisition or visible image improvement is claimed.

## Fixed input and checks

The [captured Vienna corpus](reference/port-planes/README.md) retains 3478 source
axes, 3317 modules, 3888 ports, 30260 original height observations and 60514
original joint faces. `Vienna.npz` SHA256:
`05bae1944e0411fdf5497021a72bd06ea434bd0465441c17c7604acf6642d52b`.
No interpolated refinement samples enter the objective. Historical coordinates
remain unchanged scaled spherical Mercator; the fixture's historical `ENU` label
does not become a new WGS84-ENU certificate through this experiment.

`port_highs_experiment.py` scopes a replacement of `_fit.fit_planes` inside the
original public planner. All actual backend calls have byte-identical first-fit
height/gradient/equality arrays, checked by SHA256 of all three sparse buffers.
That fit has 4612 unknowns, 237427 paired cones, 201 equalities and gradient
nnz=2963649. The polygon path stops before this call at its unchanged sparse guard.
Original whole-face/finite grades, chord <=0.01 m, port gradient jump <=0.005,
coverage, anchors, source usage and contact checks remain mandatory.

All workers retain RLIMIT_AS=3 GiB, RLIMIT_CPU=90 s and an external 110 s wall
termination. Per-fit solver setting: 8 s and 100 iterations. HiGHS sums iterations
across its LP solves and subtracts measured solve time from the shared 8 s;
assembly/cut work remains separately timed inside the overall worker cap.
Clarabel's cooperative 8 s setting actually takes 8.77–9.09 s per solve here;
reported actual time is retained, not rounded down to the requested limit.

## Construction and small controls

For each paired normalized gradient `g=G_i*c`, the original constraint is
`norm(g)<=0.99`. Every unit direction `d` gives the valid outer cut
`d.T*G_i*c<=0.99`; `d=g/norm(g)` separates the current violating point.
Coordinate boxes are also outer relaxations. Sparse support grouping only chooses
cuts; it never replaces checking all original cones. The final seed selects the
first cone per column-index pattern: 11819 cones/23638 coordinate rows. Full-box
imports use 4096-row batches to avoid a second full sparse expansion.

SIGGRAPH/SCA context: [Tang et al. 2022](https://crl.ethz.ch/papers/Tang22SOCP.pdf).
The concrete separation source is [Coey/Lubin/Vielma, Appendix A.2](https://juan-pablo-vielma.github.io/publications/Outer-Approximation-With-Conic.pdf),
PDF page 41 visually inspected. API: [official HiGHS examples](https://ergo-code.github.io/HiGHS/stable/interfaces/python/example-py/).
Own implementation; no copied solver or SDK code. Ten focused controls cover
analytic circles/rotated ellipses, a conic boundary excluded by the old inscribed
polygon, equality contacts, grouped unequal cones, both box policies, zero/time/
sparse cases and refusal of a constant unverified initialization. With existing
port/slice controls: 26 tests pass; Ruff/format and functions <=120 lines pass.

## Whole-input results

Linux x86_64 WSL2 6.18.54.1, Intel i5-1135G7; Python 3.13.5, NumPy 2.2.4,
SciPy 1.18.1, Clarabel 0.11.1, HiGHS/highspy 1.15.1, one solver/BLAS thread.
Methods ran sequentially, with process snapshots before each worker; no other
heavy worker appeared in those snapshots. Continuous foreign load was not
recorded, so these are host measurements, without an isolation or device claim.
Wall/CPU/RSS include import, original input, planning and export, from `os.wait4`.
All rows have terminal status: Clarabel exit 0, other workers exit 2; no wall kill.

| Policy / frozen commit | Wall s | CPU s | Peak MiB | Whole-input result |
|---|---:|---:|---:|---|
| Clarabel / `044046fae` | 80.537 | 80.490 | 1099.89 | Accepted; independently verified |
| 16-direction polygon / `1175cda01` | 12.218 | 12.203 | 314.91 | Existing sparse guard; no fit |
| Unseeded IPM diagnostic / `044046fae` | 26.041 | 25.997 | 335.93 | Two LP optima, full norms 356.37/1322.68; no admissible field |
| Full box, IPM / `14cee8374` | 26.246 | 26.184 | 746.13 | Time limit before a usable iterate, 0 IPM iterations |
| Full box, simplex / `14cee8374` | 20.932 | 20.845 | 904.60 | 100-iteration limit, no usable iterate |
| Support box, IPM / `1175cda01` | 24.938 | 24.889 | 350.01 | Time limit, 10 IPM iterations, no usable iterate |
| Support box, simplex / `1175cda01` | 17.727 | 17.624 | 378.19 | 100-iteration limit, no usable iterate |

The unseeded diagnostic included a separately recorded 1.524 s first-fit snapshot
export; its stricter initial fit timer included setup. It was discarded before
the bounded variants. Their failed first solve also means the `all` versus
`support` later-cut choice has no effect on these measured failures; no separate
full-input `all` run is claimed. Simplex and IPM iteration counts are their own
algorithmic steps, not equivalent work units. Bounds were not increased.

Clarabel outputs 95989 vertices/191972 faces. Independent GEOS coverage error and
original-anchor displacement are both zero; original continuous norm 0.9818454,
maximum chord 0.00802893 m, finite port gradient jump 0.00418365, maximum original
DEM deviation 3.04868709 m. Every 34144 finite contact query succeeds with explicit
zero XY radius, max height discrepancy 1.01e-11 m. Larger-radius ambiguity remains
documented by the contact replay; defaults were not changed. Saved mesh SHA256:
`f436fea19936e979c3c73a2d63aa74eef9c5ac0b96f797e75a307a2ba637a395`.
Analytic C1, finite C0 mesh and physical contact remain separate checks.

## Work and limits of measurement

Full-box native LP nnz=3092119 versus support-box nnz=414224 (86.6% fewer).
Known frontend/native-import sparse occupancy peaks at 9170416 versus 6622982
entries, below the same 12000000 guard; this is not a count of solver internals.
Each policy keys all 237427 original cone supports, copying 11854596 index bytes.
The support-box frontend records 59 array/sparse payloads totaling 49039544 bytes,
36716988 explicitly copied value/key bytes and 6974388 API-submitted bytes.
Full box: 602 payload records, 91669992/59309884/49888492 bytes respectively.
Records can share storage and omit library intermediates: these are scoped work
counters, **not total allocations, total copied bytes or unique live memory**.
Native internal allocation/copy totals and Clarabel's corresponding totals remain
unmeasured; full allocation-cost acceptance is open. RSS is measured independently.

Only the discarded unseeded run reached separation: 6549 cuts, 41 LP iterations,
949708 cone checks and 11854596 gradient-nonzero visits. Bounded variants perform
no full-cone pass because HiGHS exposes no usable first iterate. The planner's
flat initialization still has geometrically valid triangles, but no successful
original-data fit; all failed runs correctly export no accepted mesh. No flat
fallback or conic global optimality is claimed.

## Reproduction and retained evidence

Use the explicit commands in the [experiment README](README.md); `--initial-box`
selects `support` (default) or `all`. Required extra package: `highspy==1.15.1`.
Recreate the exact commits above for historical policies. Run
`port_plane_verify.py <accepted-output>` and
`port_height_library_contacts.py <accepted-output>` from its frozen worktree.
The library kernel/input bytes are unchanged across these commits; experimental
driver provenance explicitly identifies each backend and source commit.

External retained roots are `/var/tmp/outshine-transport-delayed-cuts-evidence-`
plus `044046fae`, `14cee8374`, or `1175cda01`: terminal process JSON, startup load,
full worker logs/reports; first-fit matrices and accepted Clarabel mesh with
independent verification/contacts in the first root. No private outputs in the
checkout. Existing four transport images remain the latest inspected products;
this solver rejection does not improve their appearance.
