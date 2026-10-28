# Whole small source cycles

[#1569](https://github.com/Weichwolf/outshine/issues/1569), under
[#1402](https://github.com/Weichwolf/outshine/issues/1402), extends the existing
[small ground construction](ground_network.md) to one bounded actual loop.
It is explicitly `outshine/experimental-coarse-ground-unicyclic/1`, not a
registered canonical Source/terminal/World recipe. Existing public recipes,
default tree topology guards, ports and source admission remain unchanged.

## Source, geometry and lifetime

`ground_cycle.py` requires a complete connected component of3–8 original
normalized spans, E=V, maximum degree6, no self/parallel edges and exactly one
simple cycle after leaf pruning. Leaves and the cycle use actual node IDs;
proximity does not create connections. Reject unsupported levels/profiles,
incomplete components and multiple cycles before DEM work. The input is bounded
by existing `PortBudget`; use its shared input/mesh/solver/time capacities.

Re-use existing whole junction cores, corridor/terminal bands, GEOS conforming
part triangulation, DEM lift, affine/C1 fitter and final finite quality checks.
Two private library helpers accept an explicit expected-hole count, bounded0–2;
default0 still rejects every hole. Only the experimental cycle caller requests
one; the explicit [two-cycle experiment](ground_two_cycles.py) requests two.
Coverage must be one valid polygon with exactly that many interiors; it
does not fill a hole or weaken unrelated overlaps/conformity checks. Reports
retain the old tree recipe only at the unchanged default. No native/public API,
asset format, source parser, scheduler or cache system is added.

All generated arrays/fields remain local caller-owned Python products. Source
read/decode/index construction is separate from the bounded geometry stage.
The total geometry/fit deadline and existing mesh/query/terrain capacities
remain binding. GEOS/SciPy calls are cooperative, not a hard frame deadline;
no frame integration or async lifetime contract is claimed.

## Actual case and separate authority

Existing versioned `fixtures/bridge-source-facts` and the original bound DEM
provide seed`(11372695559320524359,0)`: six spans, six normalized nodes, four
cycle nodes, two leaves, source width about1.8m and supplied gradient about0.15.
All six nodes remain Unknown. The strict old tree entry correctly returns
`bounded_ground_tree_topology_required`; it never reaches DEM for this request.
No unknown recipe is silently registered or admitted.

The standalone finite diagnostic produces32 vertices/32 triangles,12 entire
shared internal sections and two generated end sections. The86.216696m² hole
stays inside the123.461017m² original source-axis ring. The selected existing
affine control fits all32 original DEM observations within0.064019m; its
normalized continuous/finite grade is about0.243617. Existing0.5m prior and
1.0000001 normalized finite-grade /0.005contact-gradient limits are unchanged.
No dense-DEM, surveyed width/height or general optimality claim follows.

Generated ends are not original-qualified contacts. The report retains original
Unknowns, source/candidate/terrain bindings and the strict entry reason; zero
qualified original end contacts and zero World joins. Native omission/Way
authority remains #1530. Do not interpret a valid local surface as a legal route,
bridge clearance, vehicle sweep, terrain-deformation or whole-world result.

`ground_cycle_checks.py` independently checks actual triangle coverage,
nonoverlapping manifold edges/Euler characteristic0, exactly one hole contained
by the original source ring, unchanged fitted XY domain, full section inclusion
and finite grades. Existing field quality owns whole C1/finite-port checks.
New tests cover source order, filled holes, missing DEM, budgets, unsupported
cycles/levels, source closure and old strict defaults. Old actual three-arm and
five-span tree buffers are compared separately against their committed producer.

## Same-quality comparison and reproduction

Compare adaptive degree2 direct cuts plus branching LP against all-node LP.
Both use the same complete source component, widths/grades, DEM, prior cap and
full output checks. On this case adaptive uses two LPs and linear-only four;
report actual mesh equality separately rather than assuming it for other inputs.
No common global optimum or universal speedup is claimed.

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -W error -m unittest test_ground_cycle test_ground_network ground_network_checks ground_star_checks ground_terminal_checks test_canonical_ports test_canonical_bridge_ports -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/transport/replay_ground_cycle.py --output /tmp/outshine-research-ground-cycle-comparison --rounds 3 --trace --plot
```

Use a fresh explicit system-temp directory. Timing includes geometry/DEM/fit and
independent output checks; source prep, profiler work, NPZ/plots are separate.
Report constraints/LPs, elements, retained input/output bytes, known plan copies,
allocation requests/scoped high watermark and process peak separately. Total
copied bytes, finished asset-cache hits, native device/frame p99 remain unmeasured.
The local plot uses actual triangles and metric aspect, no vertical exaggeration;
no new repository images or packages. Fresh installed-wheel checks remain required.

Primary context: SIGGRAPH2008 *Interactive Procedural Street Modeling*,page2/Fig.2,
graph-to-geometry separation; Galin et al.2010 *Procedural Generation of Roads*,
page8/Figs17–18, explicit road profiles and distinct road/bridge/tunnel models.
Both local full pages inspected visually again. The bounded single-cycle/source
guard and exact-hole extension are our own derivation, not those full algorithms.
