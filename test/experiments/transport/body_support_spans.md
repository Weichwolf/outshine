# Body-owned source-path support — #1223

`body_support_spans.trace(body, path_xy, allowed_sources_mask)` consumes an
already qualified body and an explicitly selected source path. It never fits
heights. `top_faces`, `reference_faces`, `reference_vertices`, `face_sources`,
`face_step` and `levels_m` are the existing body products. Reference/actual cap
XY must match exactly in the supplied body-position encoding, and step labels
must match the actual flat cap heights. Selection is declared, not a highest-hit
or layer inference. The fixed experiment uses actual Float32 body positions.

The output stores cap IDs, source-segment IDs, exact ordered parameter intervals,
source arclength bounds, per-endpoint barycentric coordinates and cap-source masks.
Positions derive from those same indexed caps. The immutable body and source path
remain caller-owned and are retained by reference; indices belong to that body
version. No geometry is changed or independently recreated.

Whole path coverage is checked against the selected input cap domain, together
with an exact nonoverlapping parameter partition per path segment. Coverage is
not inferred from the union of rounded clipped line fragments. Literal input
endpoints receive their exact parameter words 0/1; there is no geometric epsilon,
nearby endpoint weld or changed height/plane tolerance.

Within a span, endpoint barycentrics interpolate on its actual body triangle.
At interfaces, the same cap/shared indexed 3D edge gives continuous support.
An indexed reference edge with different actual step labels preserves both
one-sided heights and the jump. Separate reference sheets do not join by XY
contact. Positive overlap is ambiguous; even equal-height independent sheets
require a qualified selection. A true shared indexed edge selects the lowest cap
ID, matching the existing surface-sampler tie convention.

The reference borrows the full body; callers must keep it immutable while using
the spans. Native retention/persistence and cache-version binding remain #1189.
No source access permission, agent width/height, swept body, foot animation,
trajectory, graph rerouting or native Nav admission is implied by support alone.
Existing World-node/gate heights and opaque integer IDs remain owned in the
captured source. The export attaches the declared World ports to support endpoints
by integer node identity, without an XY search or a second height solution.

`strategy="batch"` and `"scalar"` apply the same GEOS segment/triangle
intersections. Batch intersection/classification/coordinate extraction reduces
Python dispatch; parameter construction, body validation and topology are shared.
The exact arrays/connections are compared in both directions. Defaults bound
8192 caps, 4096 source segments, 262144 face intersections and 32768 spans.
Budget/cancellation/failure publish no partial table and leave inputs unchanged.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B -W error \
  test/experiments/transport/body_support_spans_experiment.py \
  --repeats 21 --render --output /tmp/outshine-body-support
```

The existing fixed World input and its source-direction reversal are the only
source cases. The old four ramp station priors are retained as a negative control:
two inner heights differ by about 0.069/0.381 m from actual tread support. This
does not assert that an already integrated native Nav consumer uses those priors.
The experiment exports existing body arrays, span arrays and source/part/port
bindings; it is not a new product format. Optional scientific figures compare the
actual body/profile with those old priors. Images stay in explicit temp outputs.

Selected work/copy counters and live traced blocks are not total allocations,
all GEOS/NumPy copies or UMA residency. Body preparation/Float32 position conversion
is separate; each compared trace includes geometry preparation and validation.
Traced peak is a separate invocation after garbage collection for each strategy;
process RSS is captured before export/plot.
Timed scalar/batch calls alternate order on each repetition. Wall and process CPU
times are recorded together; empty timer-pair overhead is reported without
subtracting it. This reduces ordering bias but does not create an isolated host.
Current measured results, actual image review and terminal checks belong in the PR.

Architecture reuses the existing SIGGRAPH 2008 street-graph/generated-geometry
separation and Galin cross-section review in the
[transport references](../../../doc/references/transport/README.md). This is a
finite body-consumer operation using the same GEOS intersections as existing
whole-section checks, not a new paper pipeline or general navigation-mesh system.
