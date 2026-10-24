# Actual body-ground point support — #1235

`prepare(body, contact_key)` snapshots one explicitly qualified ground group's
actual binary64 underside. `sample(model, xy)` returns its horizontal XY nearest
footpoint, distance and actual cap height. Inside the projected body the footpoint
is the query itself. This does not solve heights, infer terrain roles, select the
highest layer or alter the body. Caller selection precedes this finite experiment.

Selected vertices retain their original index identities through
`source_vertex_ids`; cap order and UInt64 source masks remain exact. Compaction
copies only referenced underside vertices, with no XY welding. Actual shared
indices establish continuity. Nonpositive/overlapping projected caps and touching
independent sheets are rejected. Disjoint projected components are permitted;
equally near witnesses at different heights remain locally unresolved (NaN,
`valid=False` and a reason). No zero-height or highest-hit fallback is used.
Same-height ties choose the existing deterministic cap order. Retriangulation
invariance concerns geometric support, not unchanged IDs after changing topology.

The implementation reuses the existing `SurfaceSampler`, its STRtree, gradients
and tolerances (XY 1e-8 m, height 1e-7 m). Private sampler members are reused only
inside this experiment; no library/native API is added. The immutable snapshot,
mask/map copies and optional boundary data remain owned by the model. No native
persistence or product schema is implied.

Three methods implement the same support definition: a full cap-distance scan,
the existing cap STRtree (`index`, the simple default), and a separate index of
actual external indexed edges (`boundary`) with the existing cap sampler inside.
The boundary strategy needs a prepared domain and second tree; fewer primitives
do not guarantee lower memory or cost. It is retained as an explicitly measured
alternative, not an unconditional replacement. Footpoint heights and final source
witnesses always come from the same actual cap sampler.

Defaults bound 8192 caps, 4096 input queries and 262144 face/query or emitted
witness pairs. The conservative face-count times query-count admission bound
applies even to indexed search. Qualification and tree batches reserve worst-case
candidate capacity; footpoint sampling has its own candidate bound. Cancellation
is checked between bounded stages/batches, not within GEOS operations. Failures
publish no partial result and leave caller buffers unchanged. GEOS internal node
visits, predicates, allocations and cancellation latency are not measured.

The driver constructs only the fixed World body from #1222 and two diagonals of
the same dyadic planar square. Source/part/port bindings and body words are exported
unchanged. Explicit controlled declarations qualify all World caps as one ground
group; this is not provider knowledge. The 3021 grid queries use 0.5 m spacing.

The square's old isolated `RoadBedAt` cap-height clamp plus single-group apron
algebra gives 3.1824059053 versus 3.1038840996 m at (4.5, 0.25): 7.85 cm solely
from the diagonal. XY nearest support yields the same footpoint and height for
both diagonals. This is a counterexample at zero planar support, not a reproduced
failure of the full native terrain, positive cell-support or World renderer.

`extend` is a separate controlled exterior recipe: actual nearest support blended
with the existing quintic fade to a flat prior of 25 m, declared relief 3 m and
height-adjustment limit 10 m. The old exterior primitive uses the same settings;
inside both fields use exact support. Different exterior recipes are not a
same-quality performance comparison. Their differences are not errors against
Earth, DEM or native rendering. Nearest-point extension alone does not guarantee
global continuity across medial-axis changes of nearest boundary or disjoint
components, nor a slope bound. Different-height ties are explicitly unresolved.

The diagnostic mesh uses the same lower-left/upper-right diagonal in every
ordinary grid cell for both fields. Probe errors at actual cap centres and used
vertices expose remaining cover/gap: exact point support does not make that mesh
conform to the body. These finite probes do not bound every triangle intersection.
No snapping, lowered body, relaxed tolerance or hidden cell halo masks the error.
Actual positive cell support/conforming terrain, other stamps, real DEMs and
native image/device admission remain consumer work; #1189/#826 retain that scope.
An exterior-recipe change needs agreement with master before native adoption.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/transport/body_ground_support_experiment.py \
  --repeats 21 --render --output /tmp/outshine-body-ground-support
```

Body/role construction and per-method model preparation are separate. Query
timings rotate the three methods for one first and 21 following invocations,
recording wall and process CPU time together. Allocation traces use separate
invocations after collection; live traced blocks are not cumulative allocations.
Selected array bytes distinguish temporary distance matrices, snapshot copies,
working boundary arrays and retained maps. They exclude Python/GEOS storage and
unmeasured internal copies. Process RSS includes all models, body and recipe
diagnostics before export/plot; it is neither per-method memory nor an app budget.
Current measured results and actual image review belong in the PR.

The local SIGGRAPH 1999 implicit-fairing paper (constraint discussion, p. 3) was
reviewed first; this experiment does not implement its PDE/fairing method.
Galin et al., *Procedural Generation of Roads* (2010), pp. 8–10 and Figure 20,
provide the existing closest-profile/terrain-blend background in the
[transport references](../../../doc/references/transport/README.md).
Our horizontal footpoint on an actual body underside is an adaptation, not that
paper's complete road-generation pipeline or a 3D closest-distance construction.
