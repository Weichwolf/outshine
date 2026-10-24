# Immutable per-stamp height ranges — #1227

The existing native `RoadBedAt` in `EarthworkDeformation.cpp` evaluates `WantsAt`
at every ring corner on every probe, folds an ordered minimum/maximum, then clamps
the probe height. Those ring heights are constant for an immutable stamp.
`earthwork_height_ranges.prepare` computes that same range once and retains its
immutable stamp by identity. This is partial evaluation of the existing expression,
not a new terrain algorithm or cache subsystem.

The Python `Stamp` is a finite immutable snapshot of the consumed non-profiled
fields and ring. Construction owns its tuple of ring points. Range preparation
does not copy that geometry; it adds two binary64 numbers per clamped stamp plus
Python object/reference overhead. The complete table is returned only after work
budgets and cancellation succeed. Any change to ring, origin, slopes, plateau,
sag, kind or contact key requires a new snapshot and new ranges. Nothing is
persisted, and no asset, native interface or GPU contract changes.

Direct and prepared evaluation share the same ordered scalar expression:

```text
dE = east − AtE; dN = north − AtN
WantsAt = Plateau + SlopeE*dE + SlopeN*dN − 0.5*(dE*dE + dN*dN)*SagInv
low = 1e30; high = −1e30
for each original ring corner: low = min(low, WantsAt); high = max(high, WantsAt)
RoadBedAt = clamp(WantsAt(probe), low, high)
```

The finite sentinel and ring order match the native source. Non-corridor stamps,
zero contact keys and fewer than three ring corners remain unclamped. Profiled
corridors have a separate existing consumer and are outside this experiment.
Finite nonoverflowing queries and qualified finite ring heights are required;
this is not a change to native invalid-input handling. Identical Python words do
not establish compiler/FMA or cross-platform word identity.

The fixed World body from #1222 supplies its actual binary64 underside caps.
Stamp plane construction follows existing `SurfaceEarthworks.cpp`; all 58 caps
keep their original order and separate clamp range. Contact key 1 is an explicit
ground recipe for this controlled consumer, not an inferred provider fact. The
body, source/port capture and owned heights are unchanged. No new source case,
DEM download, topology, height fit or provider-role inference is added.

The driver compares one centroid per stamp and a controlled 0.5 m grid restricted
to per-stamp bounding boxes expanded by 3 m. These are finite probe workloads,
not actual Tokyo DEM nodes or a claim about native bucket/candidate counts. Each
prepared timing includes preparing the entire range table. Scalar operation counts
include probe and ring `WantsAt` calls; the one-probe case saves no such work.
Body/stamp construction and common probe generation are separate. Calls alternate
order; raw wall/process CPU samples and timer-pair overhead are recorded. Traced
peak/live Python blocks use a separate invocation after garbage collection. RSS
before export/plot includes imports/body/probes and is not per-operation memory.

Removing the clamp is a negative control. Coplanar stamp merging also fails in
general: adjacent rings on the same plane can have different height bounds, and
`CorridorApronBlend` combines corrections per stamp. This comparison preserves
those distinctions. No signed-distance/broadphase, apron, terrain-decision,
deformed-mesh or full-image equivalence is claimed from this isolated operation.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B -W error \
  test/experiments/transport/earthwork_height_ranges_experiment.py \
  --output /tmp/outshine-height-ranges --repeats 21 --render
```

The existing [transport references](../../../doc/references/transport/README.md)
locate the SIGGRAPH 2008 graph/geometry separation and Galin terrain-modification
background already used by these body consumers. No new paper method is proposed.
Current measurements, actual diagnostic image review and terminal checks belong
in the PR. Master/#1189 owns native profiling/integration: Tokyo's cold timeout
motivates examining repeated work but does not identify this operator as its cause.
