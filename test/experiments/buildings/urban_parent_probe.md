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

## Ground views and direct conditioning

The exact query now uses closed slab intersections for horizontal/upward/downward
rays, with open opaque stop endpoints. Primary-miss conditioning uses the lowest
traversed height, which is at entry for upward rays. Sky rays have no ground receiver.
Camera origins are nonnegative relative to the flat base; a near point inside any
possible unresolved lot requires resolved geometry. The local phase filter still
requires downward macro rays; it has not been generalized to the horizon.

Long primary rays exposed quadratic shadow work: every caster query searched every
stored primary-miss cell. No history storage is necessary. A straight primary ray
visits each cell at most once. Intersect the caster cell's inset footprint with the
known primary prefix, clip to the actual traversed endpoint, and evaluate its lowest
height directly. Ignore camera-replaced cells and the known current hit cell as before.
A crossed footprint yields exactly the same height cap as the old miss history;
uncrossed cells retain an infinite cap. The endpoint owns exact opaque depth ties.

`primary_cap` performs one geometric check per candidate instead of a history search.
The current surface supplies its event depth; surviving background rays supply the
actual traversed prefix, including an optional early stop. Independent full XYZ-box
intersections check the conditioning for random directions and exact roof endpoints.
Parallel rays strictly inside a centred-lot gap skip the entire grid, with an ULP guard
that retains surface boundaries. This is a geometric skip in the declared model,
not an empty-world or native occluder certificate.

## Optional termination bound

`transmission_tolerance=0` keeps the complete reference and remains the renderer
default. With explicit tolerance epsilon, stop primary work once remaining T is at
most epsilon. Retain T rather than making it opaque; conditional receiver visibility
uses the prefix actually visited. For nonnegative radiance bounded by L, the missing
conditional tail and the substituted background each lie in [0,L], so per-channel
colour error is at most epsilon*L; transmission overestimation is at most epsilon.
This bounds only termination error in this model, not parent/source/LOD error.

For this shader, alpha=roughness^2, D<=1/(pi*alpha^2), G_light<=1 and
G_view/N_view<=2/alpha. With unit directions and albedo A, direct radiance is bounded
by `(A/pi + 1/(2*pi*alpha^3))*energy`; add ambient*A. For the point emitter above the
represented volume, its height clearance bounds inverse-square energy. Arbitrary
backgrounds need their own bound. The diagnostic uses ground/sky within the same
bound and epsilon=1e-8. Source heights remain priors, not original-world maxima.
The Horizon slide's alpha=1 early exit is a precedent; this finite-error derivation
is explicit here rather than attributed to that slide.

## Current evidence

General-ray control `8aa9686ed`; direct conditioning `d0807364b`; clean frozen producer
`725ee7d5` includes master `dd7864b0f`. **74 focused tests pass** (5.918 s). They include
81 independently enumerated four-cell scenes under four lights for elevated and
mixed horizontal/up/down ground rays, separate camera/caster replacement, opaque
stops, random closed-box prefix checks, optional error bounds and old phase contracts.
Ruff/format/function-size/diff checks and the docs gate pass.

Two frozen control runs and eight candidate/regression runs finish terminal 0.
**20 actual 1280x720 images were opened** with full-image/crop sheets. Day/back/night,
8-km displacement, 50/240-km target distances and the prior elevated composition are
covered. The original stored parent/owner inputs are unchanged; no source parsing or
child generation occurs. Ground eye height is 2 m, with four jittered samples per
axis. The 50-km day ROI is 338x6 pixels /32,448 rays, including 17,178 upward rays.
These are mathematical flat-ENU controls: omitted terrain/curvature means a 2-m
observer's real 50/240-km city visibility cannot be inferred from these pictures.

At the same epsilon, all four saved day colour/coverage arrays are **bit-identical**
between history and direct conditioning. Their primary/shadow visits are unchanged.
History comparisons 795,409,138 become 3,988,400 direct checks (different operation
types, not a claimed 199x speedup). The measured Parent time falls 104.83→31.81 s,
3.30x, under sequential normal-priority development-host runs. Named stored history
falls 34,258,944 B→0; named conditioning arrays peak 229,376 B. This is partial buffer
accounting, not full allocator/copy/process/device peak evidence or a general app-memory win.
The precomputed resolved CPU control is still much faster (.50 s); no native parent
runtime win is established. Original child preparation/storage is outside these timers.

| Flat ground case | Parent seconds | RGB region RMS vs inferred boxes |
| --- | ---: | ---: |
| Day, epsilon1e-8 | 31.81 | .00987 |
| Day, complete | 53.37 | .00987 |
| Night | 54.02 | .05266 |
| Backlight | 13.49 | .00505 |
| 8-km displacement | 27.68 | .00783 |
| 240-km target | 4.42 | .00126 |

The bounded/full day comparison differs at most 6.19e-10 in linear colour and
2.46e-9 in coverage, below the derived 4.69e-8 colour bound. A two-ray real-field
probe goes 42.50→8.03 s without truncation; prefix checks 9,792 replace3,001,244 history
comparisons. The optional bounded probe needs 25 instead of613 primary visits, with
1.31e-9 measured colour difference under the same 4.69e-8 bound. The parallel-gap ray
needs zero visits. Short prefixes have no universal timing-win claim.

All six elevated composition RGB crops remain bit-identical to the original
6f892d438 controls. The original signed-correction, phase/transfer and independent
night-reference evidence remains in the [previous method](https://github.com/Weichwolf/outshine/blob/3ba4d5a12/test/experiments/buildings/urban_parent_probe.md)
and its preserved receipts. No new convergence or temporal continuity claim: phase
aliasing, statistical shape/material error and the earlier night sensitivity remain.

Remaining: cameras below the flat base, mixed terrain/curvature, horizon pixel-phase
filtering, field-size-independent query cost, temporal transitions and native
World/device/p99 admission. No existing native child may be removed on this evidence.

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

The generic horizon comparison reuses those same saved inputs:

```sh
python -B -W error test/experiments/buildings/compare_urban_horizon.py \
  --parent /tmp/outshine-urban-control/parent.npz \
  --resolved /tmp/outshine-urban-control/reference.npz \
  --output /tmp/outshine-urban-horizon --altitude 2 --distance 50000
```

`--tolerance 0` selects complete traversal; `--ray-probe` runs the tiny real-field
cost/error pair. `--lighting night`, `--motion 8000` and `--distance 240000` vary the
controlled view; none supplies terrain/curvature or complete-world evidence.

`--lighting night --reference-check-samples 32` checks the brighter point-light case;
`--motion 8000` changes the view; `--whole-phase` retains the negative control.
Parent compilation precedes the offline reference bake. A stored `--parent`/`--resolved`
reuses finished data without source parsing or child generation.

Current raw evidence: `/tmp/outshine-research-urban-horizon-3ba4d5a12/receipt.json`.
Prior filtering: `/tmp/outshine-research-urban-layer-filter-86969ff1e/receipt.json`.
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
- [Horizon cloudscapes 2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF page 37 / slide 35: separate coverage/height density; page 81 / slide 79: alpha=1 early exit. Both inspected; no cloud/building equivalence or finite-error bound attributed to the slides.
- [Sloan/Kautz/Snyder 2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction / Figure 2, PDF page 2: separate transport from current light/BRDF; no SH/GI guarantees adopted.
- [Mueller et al. 2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page 1 / Figure 1 / Equation 1: baseline-plus-difference identity only; no neural implementation or unbiasedness guarantee adopted.
