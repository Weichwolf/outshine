# Constructive rail beds with explicit earthworks

Owner: #1671 under #1402. Python reference only; native geometry, source admission
and device acceptance remain separate. No `data/world` or native recipe changes.

## Input and product

The two actual whole rail components contain 13 and 7 original directed members.
Their native gradient words are zero (missing), not a measured zero gradient.
Hash-bound original MVT features identify ordinary rail and crossover service.
`rail_ground_profile.bind_profile` checks those actual features, capture/content
bindings, original width words, ground semantics and original object identity.
It supplies an explicit constructive cap of 0.025 while retaining every original
member, missing profile, node position, direction and Unknown/closure reason.
The factory requires a whole plain-rail component containing a crossover; the
physical cover does not invent switch connectivity or logical routing rights.

`rail_ground_bed.construct` creates one physical band per coincident axis and
unions complete band rectangles and endpoint bevel hulls. Physical overlaps are
explicitly allowed for this bed only. Existing default overlap/hole guards remain
unchanged. Existing 32-member/node, 50 m extent, 30 m span and mesh/work budgets
remain; the 13-member case still refuses `cover_extent_budget` before DEM work.

Original geographic positions define fixed ENU XY. Heights contribute only ENU
up; the ignored height-induced horizontal displacement is counted (0.000423 m
maximum in this example). All original full-width end profiles are exported in
original from/to and right/left order. This is a physical foundation product,
not detailed rails, a drivable railway or native EndContact publication.

## Height construction and its limits

Seventeen complete bed/axis/end observations require the southern DEM neighbor
14/8937/5679: the original geometry lies in the MVT buffer beyond the old tile.
The versioned `fixtures/rail-ground/manifest.json` retains the old input and pins
the new 60,344-byte Mapterhorn source by URL, acquisition time and SHA-256.
Normal replay/tests use these offline fixtures and the existing `Elevation`
boundary-post bilinear sampler. Provider revision/geoid equivalence is unknown;
source signed metres retain the documented model-geodetic interpretation.

The original affine 0.5 m DEM prior is infeasible at grade 0.025: an exact rational
pair witness satisfies `(abs(dh) - 2*0.5)^2 > 0.025^2 * dot(dxy,dxy)`, with positive
left unsquared term. This is an affine finite-observation contradiction; no claim
about arbitrary piecewise fields or true terrain follows.

The new construction minimizes sampled maximum cut/fill with a four-variable
HiGHS LP. A regular 64-sided polygon inscribed in the gradient disk gives 64
linear gradient constraints; 17 observations give 34 error constraints. This is
conservative and not the exact disk optimum. The earthwork reach is the existing
first collar margin times soil grade (5 m * 2/3); the old prior is never raised.
Solver success alone does not admit the result: finite heights, actual gradient,
sampled earthwork and time limits are checked. The actual maximum sampled
cut/fill is 1.608242 m; no continuous DEM error bound is asserted.

The existing whole-profile contact method keeps the new bed immutable, fits
complete outer chord profiles and bounds every soil triangle by grade 2/3.
Independent checks rebuild bands directly from original positions/widths, check
directions, full profiles, area, Euler/manifold and conforming upward faces, then
reuse `qualify_contact` for whole shared bed/soil edges and fixed outer XYZ.
This gives local C0 contact. Four refinement passes stop at their existing budget;
the reported maximum vertical LOD difference is 1.895047 m, not a passed terrain
accuracy certificate. Original Source/World/EndContact/closure stays unqualified.

## Actual result and equal-product comparison

Seven original owners / fourteen end profiles yield three physical bands,
47.000503 m², eight bed vertices and six triangles. Terrain has 52 vertices and
98 triangles. The alternative constructs seven per-source rectangles instead of
three shared rectangles: 28 versus 12 corners and nine versus five boolean
inputs. All fifteen exported arrays, including complete terrain, are bit equal.

At producer 038efca29, seven alternating warm Python rounds on Linux/i5-1135G7
give median CPU 35.204 ms shared versus 35.721 ms per-source. The small timing
difference is not a strong speedup claim. Memray separately counts 66,371 versus
66,742 requests, 13,557,642 versus 13,620,280 requested bytes, and 456,641 versus
445,833 bytes incremental tracked heap peak: sharing slightly increases peak.
Each path makes eleven DEM calls / 3,797 points. Source preparation (1.887 CPU s),
cold raster loading/construction (65.716 CPU ms) and plotting are separate.
Owned bed buffers are 264 bytes, final terrain 2,424 bytes. Guaranteed Mesh
constructor copies are a reported lower bound; total internal copy bytes remain
unmeasured. Python timings/heap/payload sizes are not native frame/UMA budgets.

## Reproduction and handoff

Use the repository `.venv` with NumPy, SciPy/HiGHS, Shapely and existing source
reader dependencies; plotting needs Matplotlib, cost traces need Memray.
Set `PYTHONPATH=lib/src:test/experiments/transport`, `OPENBLAS_NUM_THREADS=1` and
`OMP_NUM_THREADS=1`. Outputs must be fresh explicit system-temp paths.

- `python -m unittest test_rail_ground_bed test_constructive_earthwork_plane test_coincident_ground_cover mvt_source_profile_checks`
- `python test/experiments/transport/replay_rail_ground_bed.py --output /tmp/outshine-rail-replay --plot`
- Repeat with `--no-shared`; compare every NPZ field with `allow_pickle=False`.
- `python test/experiments/transport/measure_rail_ground_bed.py --output /tmp/outshine-rail-cost` (normal priority, frozen producer).

Thirty-five focused checks pass; the existing actual six-member default cover
retains all 44 saved fields exactly. Images are locally inspected in system temp,
not published. Native consumers must preserve original/constructed separation,
source-content binding and complete directed profiles; runtime Source index zero
in this fixture is not a portable identity. Remaining topology, generated rail
detail, terrain accuracy and native/device work belong to their consumers.

SIGGRAPH first: Chen et al., *Interactive Procedural Street Modeling* (2008),
`doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf`,
especially p.3 (locally rendered and inspected), motivates terrain-constrained
layout. This reference does not implement its tensor-field generator. The LP
above is explicitly derived here; existing SciPy/HiGHS is reused without new
solver dependencies or bundled external code.
