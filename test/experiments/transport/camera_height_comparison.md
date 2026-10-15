# Shared camera-window bed heights

Owner: [#265](https://github.com/Weichwolf/outshine/issues/265), under #20.
Historical numerical/geometry stage from PR275. The subsequent installable
quadratic kernel, current public contract, source/wheel acceptance and updated
cost/image checks are in [weighted_height_library_comparison.md](weighted_height_library_comparison.md).
The native runtime remains a separate integration.

## Problem, scope and decision

The supplemental whole-camera transport windows previously used a single flat
height, although the retained coarse DEM varies substantially. Direct DEM
vertex draping creates invalid whole-face gradients (Feldkirch up to 45.7).
Fit shared vertex heights on exactly the same XY/topology, with the established
anisotropic longitudinal/crossfall constraints. Nearest retained source axes
direct corridor faces; junction/uncertain faces use the established conservative
2.5% isotropic recipe. These are procedural constraints, not measured road grades.

Keep the whole existing railway hardware support by fixing **all vertices of all
bed facets intersecting the complete verified rail-foot/sleeper footprints**.
Rebuild their coverage union and require full footprint coverage. This is stronger
than fixing a few contact anchors. The old Vienna C1 and Central Park flat windows
remain independently reserved; no newly coupled connection between windows is
claimed. Source usage/classification/IDs and part materials remain unchanged.

Compare the existing public minimax fit with a new lumped-area vertex least-squares
fit. The minimax objective minimizes maximum vertex deviation; the quadratic
objective minimizes a weighted sum of squared vertex deviations. Their objectives
are different; constraints, input targets, fixed contacts and solver budgets are
identical. Quadratic fitting is the promising next candidate: it produces valid
whole-window output in all four cases, with lower measured RMS prior error.
The default minimax API is retained; no global method superiority or optimum is
claimed. Timing-limited candidates can vary across runs; saved outputs are pinned
by their actual hashes, and every candidate must pass the same geometry gates.

## Mathematical construction and acceptance

Each triangle has an affine height gradient Gz. Use the same inequalities
`norm(F G z) <= 1` as the public minimax fit, and fixed contact rows. For normalized
lumped vertex areas w and centred prior t, the quadratic data are `P=diag(w)` and
`q=-w*t`. This is mass-lumped vertex quadrature, not the exact integral of a
continuous DEM residual. No smoothing or C1 term is hidden in the objective.

An incomplete/numerically marginal solver candidate is not accepted as-is.
Project fixed heights exactly onto their old datum d, then measure the maximum
normalized whole-face gradient M. The feasible flat reference has zero gradient,
so `gradient(d + alpha*(z-d)) = alpha*gradient(z)`. Choose
`alpha=min(1,(1-1e-5)/M)` and independently recheck the resulting mesh. This is an
explicit geometric contraction, not a relaxed tolerance or optimality argument.
Feldkirch's first uncontracted quadratic candidate exceeded the unchanged bound
by about 6e-6 and was rejected; that counterexample motivated this construction.

Acceptance still requires maximum normalized whole-face grade <=1+1e-7 and fixed
height error <=1e-7 m. The independent saved-output checker uses batched 2×2 linear
systems, not the producer's gradient implementation, verifies exact original
XY/face arrays, and reconstructs the union of **actual output facets still at the
old datum**. Every complete old hardware footprint must lie on that union.
All four selected quadratic results pass. Solver statuses include Solved,
AlmostSolved, InsufficientProgress and MaxTime; those statuses alone prove nothing.
Invalid minimax results publish no mesh; an explicit renderer fallback retains
the original valid bed and its reason.

## Frozen whole-window results

Fit/preparation/check source: clean `02f1ec181`. Inspection source: clean
`d05aafe06`; held-out probe source `11942e6e1`. No fit/solver/input mathematical
implementation changed between these commits. Eight controls cover analytic
quadratic projection, affine preservation, full-face versus edge limits, whole
footprint support, partial contacts, pair budgets, thin faces and tiny deadlines.
The frozen combined controls pass 24 tests; adding affected public height and
Place controls gives 42 passed tests. Ruff check/format, 120-line function limits
and `git diff --check` pass. No new package/wheel API is claimed at this stage.

| Place | Vertices / faces | Fixed vertices / facets | Footprints | Flat / quadratic vertex RMS m | Minimax accepted |
|---|---:|---:|---:|---:|---|
| Wien | 81092 / 84138 | 578 / 517 | 18993 | 11.663 / 2.088 | yes |
| Tokyo | 122759 / 128033 | 755 / 652 | 13855 | 13.557 / 3.422 | no |
| CentralPark | 118702 / 124896 | 49 / 47 | 1846 | 17.928 / 4.231 | no |
| Feldkirch | 97469 / 99373 | 1319 / 1217 | 25860 | 59.331 / 18.890 | no |

There are no new source omissions. The complete 60554 hardware footprints remain
supported; every one of 503616 baked hardware vertices is exactly unchanged in the
final ENU scenes. The old Vienna 191984 and CentralPark 151586 retained display
vertices remain exactly unchanged. Whole display indices/material slots and fixed
camera scene parameters/projection/view matrices are byte/value identical to PR260.
Only the intended supplemental bed heights change.

Four held-out samples per face (centroid and three edge midpoints) are evaluated
against the **same** retained DEM, identically for flat and all valid objectives.
They were not solver targets. These are sampled source errors, not whole-DEM
bounds or surveyed road error; some probes can coincide on shared edges.

| Place | Held-out probes | Flat / quadratic sampled RMS m | Flat / quadratic sampled max m |
|---|---:|---:|---:|
| Wien | 336552 | 11.669 / 2.103 | 16.953 / 13.691 |
| Tokyo | 512132 | 13.538 / 3.424 | 26.953 / 19.409 |
| CentralPark | 499584 | 17.916 / 4.240 | 30.849 / 22.207 |
| Feldkirch | 397492 | 59.040 / 18.854 | 248.790 / 104.659 |

On Wien, valid minimax held-out RMS is 2.442 m versus quadratic 2.103 m; their sampled
maxima are 13.675/13.691 m. Smaller RMS does not mean every error improves. Missing
terrain exterior and world classes remain missing. All new finite surfaces are
C0; common C1 ports, bank changes, tier/terrain coupling, rail vertical profiles,
turnouts, traffic, collision/LOD integration and devices remain separate work.

## Costs and boundaries

Same input per method; Clarabel 0.11.1, NumPy 2.2.4, SciPy 1.18.1, Python 3.13.5;
BLAS/OMP threads 1. Solver limit 8 s /100 iterations, address space 2 GiB /CPU 60 s per fresh
worker, 75 s external observation guard. Solver preparation and a step overrun are
measured outside/alongside the requested time limit. Input caps 131072 vertices,
160000 faces; fixed-bed queries 64 footprints/batch and 2000000 candidate cap.
The preparation is shared, not duplicated inside each compared fit phase.

| Place | Minimax wall s / peak child MiB | Quadratic wall s / peak child MiB |
|---|---:|---:|
| Wien | 9.215 / 453.7 | 7.813 / 345.8 |
| Tokyo | 10.115 / 660.5 | 7.912 / 497.3 |
| CentralPark | 10.017 / 644.0 | 9.915 / 486.6 |
| Feldkirch | 9.815 / 528.0 | 9.715 / 397.7 |

Feldkirch has three fresh normal workers per method, all independently replayed.

- minimax: 0/3 accepted; median measured fit 9.208 s.
- least_squares: 3/3 accepted; median measured fit 8.787 s.

The following separate one-call Memray profiles include Python/native allocations
for the entire fit after imports/input decode: sparse preparation, solver, wrappers,
projection and whole-face audit. Profiling changes timing-limited iteration
opportunity, so these are separate cost observations, not the normal timing
samples. Actual matrices were observed at the Clarabel call without copying them.

| Work/storage | Minimax | Quadratic |
|---|---:|---:|
| Constraint rows | 494376 | 299438 |
| Height/auxiliary columns | 97470 | 97469 |
| Constraint stored nonzeros | 1285552 | 895676 |
| Hessian stored nonzeros | 0 | 97469 |
| Constraint CSC B | 15816508 | 11137992 |
| Hessian CSC B | 389884 | 1559508 |
| RHS/objective B | 4734768 | 3175256 |
| Positive allocation events | 5748830 | 4860553 |
| Cumulative requested B | 986688163 | 767250412 |
| Tracked heap peak B | 468836821 | 332454279 |

Memray 1.20.0 with native and Python allocator tracing. Positive realloc requests
count; cumulative bytes are not unique/live storage. Constraint CSC counts include
explicit zeros retained by the shared assembly helper. Removing 194938 minimax
error rows reduces this measured sparse problem; it does not prove a global
complexity or optimality result. Both methods still process all 99373 face cones.

Known explicit `copy=True` payloads for these Feldkirch wrappers are at least
6711668 B for minimax input Mesh/grade-frame ownership and 779752 B for the quadratic
candidate-height ownership copy. These are **partial source-level copy counts**,
not all transfers or native memcpy totals. The public minimax wrapper constructs
owned XYZ/indices while the experimental quadratic path returns heights on the
existing shared XY/indices; this API representation difference contributes to
costs. Shared input payloads, solver scratch, matrix storage and allocations are
reported separately. Internal native copy traffic remains unobserved. No native
runtime/asset load/FPS/GPU/A18 claim follows from this offline comparison.

A minimal supervisor uses `Popen`/`os.wait4` for actual child CPU/RSS/terminal state,
with before-load snapshots; normal measurement workers keep normal priority.
Build/render work uses nice10. Direct inherited launcher high-water RSS was not
used. RSS is per process, not summed OS/GPU memory; profiling heap is a different
quantity. The full four-place preparation took 14.824 s /185.1 MiB child RSS and is
outside the fit phases. Final scene preparation took 10.617 s /188.6 MiB; these are
Python inspection/generation jobs, not prepared native cache loads.

## Pictures, references and reproduction

All four new full fixed-camera PNGs were rendered from the clean accepted outputs
at 1280×720 using Blender 4.3.2/Workbench, two threads. Each is freshly saved,
format/dimensions/hash checked, and actually opened against its PR260 predecessor.
Feldkirch/Tokyo/CentralPark now show coherent coarse relief; Wien's established
old C1 patch is untouched. Simple studio materials/lighting and absent terrain
are still explicit research limitations. No close-up or camera adjustment hides
unresolved geometry. The former research gallery is retired; new research images remain
local in system-temp directories. Only master publishes integrated Places.
Before-images are retained under `build/shots/reference/transport/heights-pr260/`
in both the Transport and main checkouts; current presentations are copied to
both `build/shots/experiments/transport/places/` directories.

SIGGRAPH first: Chen2008 PDF page2/Fig2 was optically inspected again; Galin2010
provides the profile/terrain separation. Local papers and primary URLs are in
`doc/references/transport/README.md`. The quadratic/conic API was checked against
[Clarabel's official Python formulation](https://clarabel.org/stable/python/getting_started_py/).
The contraction is the own elementary derivation above, not an imported algorithm
implementation. No foreign code, new package, ESRI call or SDK runtime dependency.

Reproduction CLI and explicit source/output variables are in [README.md](README.md),
section #265. `camera_height_dem_check.py --run <comparison-root> --place <Place>
--dem-sources <retained-cache> --output <fresh-report>` repeats held-out sampling.
The existing camera products remain at
`/var/tmp/outshine-transport-place-camera-inputs-209/`; old accepted rail products
remain at `/tmp/outshine-transport-rail-evidence-e4e9733af/products-source/`.
Complete fresh preparations, normal/repeat/profile reports, independent checks,
source/output hashes, load/process records, Memray traces and final scenes remain
outside the checkout in `/tmp/outshine-transport-camera-heights-evidence-02f1ec181/`.
All new private work uses root-ext4 `/tmp`, no tmpfs assumption.

### Pinned selected outputs

| Place | Prepared input SHA256 | Selected quadratic mesh SHA256 | PNG SHA256 |
|---|---|---|---|
| Wien | `5f5a672380f5462c9170b594d56e3721150bbad0cefbad71772409875c190af1` | `b792480629fb6f25c8107096c9b2b8126cd32b29b2765873ba708f76dacda536` | `45f0396c88c1bd5d9c7232ba8cf459ada2071b91553424a40acb1293d7ac16b5` |
| Tokyo | `ebb39ee767eec0232a0e861683deb79c50d8dd522c887f45de808bcc950e4c4c` | `6d36eeae0dadbac0c891c100e710fc3fa21f1600df30d98776159f0f39b2a9eb` | `429148ccfcadb9f1c69e1b48476f22234dbbf81616d712e7a886a6422e8bbefe` |
| CentralPark | `86ed9ee5ea8f09db53379f2b9668e5677b2d19432a482f6d0f9224df00234472` | `e9fbe8fc4e25bfa3390e838950c4c34b827a7393eb259756affb0242d8283179` | `c0028f03fafd1e12e4dabb8e707f3f4f626bf769a5d9ba12e06b8126ea8664b7` |
| Feldkirch | `92db85a92289d4314df1b6e43bd5619a9167b2b08d2584669d46dc4e17106c7c` | `d7e012359a2f20e64817e803264e8560552d8066ab46ce95b204d74737531093` | `bbe560ff9ada8b694816c6f8b134ad30c5b339d477ff3ef392a2698af50ab99d` |
