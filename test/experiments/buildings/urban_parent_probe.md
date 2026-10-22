# Direct urban parent probe (#1113)

Ongoing Python research under #1083 / Draft #1115. No admitted native parent/world
renderer, certified error bound, device result or temporal transition guarantee.
Images and raw diagnostics remain in system temp.

## Source and transport

The compiler clips coarse regions directly against urban/open-space evidence,
generating zero child cells and 6,264 array bytes. Circle/diagonal setbacks estimate
allowable centre area; their midpoint is a candidate, not a certified discrete/GEOS
bound. Parents store inferred height probabilities and six unlit orientation areas.
Missing original heights are unknown, not safe maxima/occluders. Zero probability,
XY evidence and asset existence never certify empty world or replacement readiness.
The resolved generator shares the recipe; all seven original owner arrays remain
byte-identical (247,731 masses / 5,962 patches). Coarse evidence lacks finer exclusions:
avoiding 1,531,626,496 hypothetical z5-parent child candidates proves bounded generation,
not equal-quality global coverage. Earlier source/compiler evidence is linked below.

The controlled renderer uses direct GGX/diffuse plus unoccluded ambient, flat inferred
boxes and one prescribed sun/point light. Resolved fields are offline controls, never
compiler prerequisites. Categorical transport treats a cell's heights as mutually
exclusive, skips its known hit cell in shadow queries, and conditions other cells
on previous primary misses. All 81 independent four-cell scenes match independent
box intersections under four light profiles. This exact reference still traverses
recipe cells; its cost is not an acceptable worldwide renderer qualification.

`urban_parent_layer.py` returns urban-only premultiplied linear colour C, remaining
transmission T and conditional parent receiver-shadow visibility S until an explicit
opaque stop in each ray's own parameterisation. Infinity means sky; opaque stops win
exact depth ties. Finite receiver normals and deterministic external casters are
explicit. Compose each subsample as `C + T*(S*B_direct + B_other)` before filtering.
B_direct already includes non-parent shadow visibility; ambient is not multiplied by S.
Averaging C/T/S separately would lose correlations.

Camera and caster replacement are independent complete ready cell rectangles in the
parent's canonical local grid, including ready empty cells. A camera-replaced cell
must not condition a remaining parent caster as a known miss; exhaustive independent
boxes exposed and verify that correction. Covered cells skip height hypotheses;
full-grid replacement skips the corresponding primary/caster traversal. Native
World bounds/readiness stay with #1102; existing resolved box paths remain independent.

## Filtering and retained alternatives

`urban_parent_phase.py` keeps macro ray directions but shifts XY origins so the
ground-hit cell samples its canonical phase. For fill f, edge e=(1-f)/2 and K samples,
each axis uses one midpoint per gap and K-2 Gauss-Legendre points inside the footprint,
with weights e, f/2 times the Legendre weights, e. The tensor product integrates
vertical footprint coverage exactly; oblique visibility/pixel filtering remain
approximate. Uniform four-point phases alias footprint edges. Independent X/Y macro
counts address anisotropic views. The sampled pixel-edge Jacobian must span at least
one recipe period; this guard is not a stationarity or error guarantee.

Transient shadow transfer stores visible mass M and visible/unshadowed mass J per
height/normal (four heights: 21 categories including ground). Interpolate M and J,
then use J/M as conditional visibility; evaluate live BRDF/energy at each event.
Zero mass is unsupported, not empty: use exact conditional shadows. Tables depend
on view/light direction/position, not intensity/ambient; no SH/GI or lit-colour asset.
Bounded packets avoid full-image coefficient gathers. Optional adaptive axes refine
support/overlap changes, with nonuniform bilinear lookup. Prior paired controls find
1.43x faster adaptive night sampling with 58.8% fewer nodes; four other cases are
slower, so uniform remains default. This heuristic is not a conservative error bound.
Those controls remain available, but are not silently reused across opaque stops.

## Joint filtering with an opaque baseline

`render_composed_phase` passes every shifted ray to the scene callback. Opaque depth,
normal, material and child/external-caster visibility are queried before composition.
Reusing the macro ray's depth/background fails an analytic half-cell foreground edge.

Filtering the whole scene also filters existing ready detail: the frozen whole-phase
day control adds .02271 region RMS to the all-ready child image. Instead retain an
independently filtered opaque baseline B containing actual ready children, ground
and external opaque surfaces, without the parent. Integrate only the signed parent
correction `D = C + (T*S-1)*B_direct + (T-1)*B_other`, then add its filtered value to B.
Negative corrections encode extinction/shadow and must not be clamped. With full
camera/caster replacement D=0 exactly, preserving the chosen baseline and bypassing
all phase queries. The baseline contains no unready children. Complete resolved
fields occur only in the offline oracle; the candidate queries the selected ready subset.

This applies the baseline-plus-difference identity in Mueller et al. 2020, page 1
Equation 1 / Figure 1, visually inspected. No neural network, learned sampler, path
truncation or unbiasedness claim is adopted. Stationary phase quadrature and finite
baseline integration remain approximations; no cached shaded-colour asset is added.

## Current evidence

Code `4bb7df3c2`, frozen image producer `35dea38c2` incorporates native-only master
`21782ef64`; Python is unchanged by that merge. **67 focused tests pass**: independent
boxes, analytic foreground edge, signed shadow correction, baseline preservation,
packet/old-phase equivalence, invalid contracts and prior transport/source checks.
Ruff/format, functions <=120 lines, diff check and docs gate pass.

Five frozen runs, all terminal 0: day, backlight, night, 8-km camera displacement,
and the whole-phase day negative control. **54 actual 1280x720 images were inspected**
with full-image/crop sheets; a saved 32x32 night reference was also opened. The unchanged
parent/owner inputs, true 240-km camera-target distance and fixed 66x31 diagnostic
region are shared. This is not a complete world/Places/device gate.

Each pixel uses 2x2 macro rays and 6x6 phase nodes (144 expensive parent queries),
versus 16x16 jittered rays in the dense control (256). The opaque baseline separately
uses those 16x16 rays and is fully counted. Its shared sample set makes all-ready
control equality a preservation check, not exact physical/independent-reference accuracy.
The 120-m foreground plane also casts shadows, with its edge displaced inside a pixel.

| Light/view | Pending RMS | Half-ready RMS | Full-ready added error |
| --- | ---: | ---: | ---: |
| Day | .01236 | .00950 | 0 |
| Backlight | .01035 | .00693 | 0 |
| Night | .07223 | .04194 | 0 |
| Movement | .01281 | .00960 | 0 |

Day same-process totals include baseline and all child/parent queries. Filtered then
dense, normal priority, sequential shared-host runs with unrelated development load:

| State | Filtered seconds | Dense seconds | Filtered / dense RMS |
| --- | ---: | ---: | --- |
| Pending | 17.78 | 31.86 | .01236 / .01182 |
| Half-ready | 17.93 | 29.56 | .00950 / .00907 |
| Foreground | 17.24 | 27.81 | .00950 / .00907 |

These are 1.61-1.79x Python speedups with slightly higher error, not equal-output,
universal or native/device wins. Pending primary visits fall 1,787,938→1,022,374;
shadow visits 8,552,605→4,299,419; history comparisons 40,153,133→20,493,765. Child
primary/shadow work and baseline work are recorded separately. The faster whole-phase
negative control loses the required ready-detail preservation and has .01820 half-ready
RMS versus .00950 for signed correction. No final composite has negative channels in
these runs; the signed correction itself is deliberately allowed to be negative.

Night is not converged: independent seed 1114 at 32x32 differs from seed 1113 at
16x16 by .05344 RMS. Against that saved denser reference, pending/half-ready/baseline
errors are .06012/.05742/.05344. Small night rankings remain unproved. Hard readiness
changes can still change brightness/structure; no temporal continuity claim.

Named phase-packet arrays peak at 442,368 B for rays, 368,640 B for returned C/T/S,
and 884,736 B for background ray arrays. The separate dense baseline currently still
allocates 20,951,040 B of C/T/S and 50,282,496 B of named background arrays. These are
partial logical sizes, not total peaks/copies/allocator/driver/device accounting or
an application-memory saving. IO/source preparation/output are outside render timers.

Remaining qualification: arbitrary ground/horizon views (currently downward rays
from above), terrain/curvature, field-size-independent cost, temporal transitions,
native consumer/readiness and complete World/device/p99 admission. No existing native
child may be removed on the strength of these Python assets or pictures.

## Reproduction and previous evidence

Use the existing `.venv`, fresh system-temp outputs and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-control --samples 16 --jitter-seed 1113
python -B -W error test/experiments/buildings/compare_urban_layer_filter.py \
  --parent /tmp/outshine-urban-control/parent.npz \
  --resolved /tmp/outshine-urban-control/reference.npz \
  --output /tmp/outshine-urban-layers --paired-dense
```

`--lighting night --reference-check-samples 32` checks the brighter point-light case;
`--motion 8000` changes the view; `--whole-phase` retains the negative control.
Parent compilation precedes the offline reference bake. A stored `--parent`/`--resolved`
reuses finished data without source parsing or child generation.

Current raw evidence: `/tmp/outshine-research-urban-layer-filter-86969ff1e/receipt.json`.
Earlier source/compiler/conditional controls: `urban-parent-1edb4404d/receipt.json`;
phase: `urban-phase-6f84f25f3/receipt.json`; transfer: `urban-transfer-c2bc91d25/receipt.json`;
adaptive: `urban-adaptive-cf6a67e95/receipt.json`; opaque composition:
`urban-layer-897451cc6/receipt.json`, each below `/tmp/outshine-research-`.
All original fields, controls and needed negative cases remain preserved.

## Primary sources read and visually inspected

- [Feibush/Levoy/Cook 1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section 2.4 / Figure 2: joint filtering of polygon edges and texture footprint.
- [Enderton et al. 2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page 2 / section 3.1 / equations 1-2: depth-ordered premultiplied contributions and remaining transmission; no stochastic GPU/OIT implementation adopted.
- [Bruneton/Neyret 2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section 3 / Figure 2 / equations 1-6: correlated visible/lit proportions; no Poisson ellipsoid forest implementation adopted.
- [Han et al. 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures 1-2 / section 3: BRDF normal filtering explicitly excludes occlusion, geometry simplification and cast shadows.
- [Horizon cloudscapes 2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF page 37 / slide 35: separate coverage and height density; no cloud/building equivalence claimed.
- [Sloan/Kautz/Snyder 2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction / Figure 2, PDF page 2: separate transport from current light/BRDF; no SH/GI guarantees adopted.
- [Mueller et al. 2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page 1 / Figure 1 / Equation 1: baseline-plus-difference identity only; no neural implementation or unbiasedness guarantee adopted.
