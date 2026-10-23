# Explicit fixed transport contacts (#1003)

The local shared profiles in PR #1006 solve the missing stair junction, but their
four outer profiles are proposals. This follow-up binds explicitly selected
source-owned points or whole XYZ polylines. It does not classify an observed
native point as an immutable outer boundary.

```sh
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -m unittest test_mvt_t_profile_contacts stair_metric_line_checks -v
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python test/experiments/transport/compare_mvt_t_profile_contacts.py --output /tmp/outshine-stair-fixed-contacts
```

Use the existing `.venv`: NumPy, SciPy/HiGHS, Shapely and Matplotlib. The fixed
input remains `fixtures/mvt_stair_t_junction.json`; no private previous output is
needed. Reports, one actual conditional body/plan and the inspected diagnostic
image stay in the explicit temporary directory. Parent PR #1006 stays frozen;
this is a separate dependent Python delivery.

## Ownership, full intervals and atomic refusal

Each contact names a retained source part, unique contact ID, explicit `fixed`
intent, provenance and one to 128 XYZ knots in the supplied frame. A proposed
export cannot become fixed implicitly. Folded/self-intersecting profiles,
unknown source owners, missing intervals/holes and unsupported positions fail.
Consecutive stations must be distinct and representable; there is no extra
metric minimum profile length and no nearest-height/source-node inference.

The entire point/line must lie in its owner's prepared domain. A shared STRtree
per used owner restricts candidate facets; a complete scan is the reference.
Reuse `stair_module_sections.line_pieces` and its affine interpolation. Its old
1e-10 m piece threshold remains the default for existing consumers; fixed whole
profiles explicitly keep every nonzero piece. Original profile knots are also
constraints, including a height kink inside a candidate triangle.

On each intersected facet, barycentric coefficients express the candidate
height linearly in its existing vertex heights. At all facet/profile breakpoints
impose the supplied height, using the existing local Z datum. Between consecutive
breakpoints both heights are affine, so their largest difference occurs at an
endpoint. Matching only the two overall endpoints is insufficient: an analytic
two-cell control has matching endpoints but a 1 m interior mismatch.

Preparation caps 64 contacts/4096 input knots, 128 knots per contact, 32768 constraint
samples and 200000 candidate facet tests by default. Caps are cooperative between
bounded geometry queries, not frame deadlines. Exhaustion refuses the whole
contact set before any pin is added; no truncated boundary or contact is emitted.
All previous height/grade/source/Float32 requirements remain in force.

## Independent physical audit and numerical conditioning

The independent check intersects the original profile with all facet boundaries
and uses `SurfaceSampler`'s normal-based planes. It includes original knots and
checks full-domain support. Its existing metric height tolerance is 1e-7 m;
the LP primal feasibility setting stays 1e-9. Neither threshold is increased.

The four generated outer profiles initially triggered a numerical HiGHS refusal
even though the original field matched them to about 1e-14 m. Contact equalities
are scaled by 1024, an exact power-of-two multiplication, and HiGHS presolve is
disabled for solves with fixed contacts. This conditions the same equalities;
no height interval or geometry tolerance is introduced. Unbound solves keep
their prior presolve setting. The comparison records both unscaled/default
presolve controls and the selected result; a future solver fixing those controls
is not a test failure. Every returned field still passes the independent audit.

This does not make an arbitrary profile representable by the current mesh. An
unrepresented height kink can require a different prepared mesh/transition.
Solver refusal alone is not a global geometric impossibility result.

## Actual native-point counterexample

Evaluating the parent proposal at the fixture's recorded native points changes
the stair by up to 2.030758 m and the first prepared way 432 point by 2.461507 m. These
are point observations, not complete qualified outer profiles. The two recorded
end heights of way 431 can be fixed while keeping all three sources and valid
actual treads. The native boundary roles are still unqualified.

If the caller fixes the first recorded way 432 point, the existing 2 m prior-change
recipe is infeasible. For one contact row `w·x=b` and finite vertex-prior bounds
`|x_i-p_i|≤d`, the triangle inequality gives
`d≥|b-w·p|/sum(|w_i|)`. Exact fractions of the actual binary64 LP coefficients
give 2.2890588514967423 m as a downward-rounded lower bound here. This is a bound
for this fixed mesh/prior model, not every mesh or possible world construction.
The budget is not silently increased, and old internal stair values are not
automatically frozen either. The integration needs qualified boundary roles,
actual complete profiles and, where needed, joint replanning with neighbours.

Four roundtrip profiles and one shifted-profile test are explicitly synthetic
handshake controls. They prove the binding/checking mechanism, not native world
admission. The diagnostic saves the actual conditional upper-point body and
matching consumer plan; all other unqualified outer contacts remain proposed.

## Cost and sources

The comparison checks identical complete contact coefficient bytes for index and
scan. It measures polygon/index build, candidates, samples, coefficient payload,
solve/validation times and a separate traced-memory worker. RSS covers the fresh
worker through evaluation, excluding plot/file export; live blocks are not total
allocations. No native IO, GPU, app RAM or frame-rate claim follows.

The SIGGRAPH graph-before-geometry and Galin cross-section/terrain references
reviewed for the parent remain the architecture sources in
[the transport review](../../../doc/references/transport/README.md).
The local Galin p.8/Fig.17–18 was inspected visually for that shared-profile step.
The finite contact equations and lower-bound derivation above are our smaller
operators, not implementations of those complete paper pipelines.
[Shapely 2.2 STRtree](https://shapely.readthedocs.io/en/stable/strtree.html),
[intersection](https://shapely.readthedocs.io/en/stable/reference/shapely.intersection.html)
and [SciPy/HiGHS options](https://docs.scipy.org/doc/scipy/reference/optimize.linprog-highs.html)
document the used primitives and solver controls. Frozen commit, actual package
versions, terminal results and measured numbers belong in the PR.
