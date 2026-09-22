Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2234
Depends:
Area: engine, ground, streaming

# Terrain candidates poll height fields without waiting on workers

## Evidence at 1af66932a

Native sample after atmosphere-cache repair: HeightSheets::RefineByError has
159/221 samples in GroundStream::StitchedFieldAwaited -> TilePool::FieldAwaited
-> condition_variable::wait. Halos also waits through HeightSheets::FieldAt.
This is forbidden blocking in Engine::advance, not merely expensive arithmetic.
Profile: /tmp/outshine-after-cache-sample.txt. Unprofiled simulation p99 34.08 ms,
worst 265.85 ms; current PNG 8dd84aa7. Samples do not establish frame maxima.

## Decision and ownership

- GroundStream must expose nonblocking stitched-field status and owning shared
  field together. Reuse TilePool::Reply (Ready/Pending/Deferred/Absent/Refused/
  Undeclared); the existing pointer-only accessor loses Pending vs Absent.
  Keep useful pointer-only callers via delegation. Never wait on the frame thread.
- Add a field-preparation phase before NeedsRefinement in GroundBuildSchedule.
  Candidate HeightSheets owns the request set and resolved immutable fields.
  Enumerate unique source tiles plus the one-tile neighbours required by halos,
  using SourceZoomOf/AsksFields conventions (wrapped X, bounded Y). Selected
  child patches use the same source regions; verify coverage with a boundary case.
- Poll bounded batches and retain resolved owners across ticks. Pending/Deferred
  leaves the candidate pending and the published world usable. Do not cache a
  pending null pointer as terminal absence or drop its sheet. Refused propagates
  a candidate error. Only terminal missing coverage permits existing fallback/rim
  behavior. Keep source identity/quality contracts from 2245/2248 intact.
- Refinement and halo construction then consume only prepared fields. FieldAt
  must not secretly enqueue/wait or substitute missing results for pending work.
  Remove StitchedFieldAwaited/FieldAwaited if no callers remain. No global cache.
- Reset preparation on candidate cancellation/source invalidation. Preserve
  published owners. Audit HeightSheets copying and ForgetsFields after corridors;
  copied stale preparation must never make a new candidate look ready.
- Do not introduce a general scheduler or first slice the numerical refinement:
  this step removes the evidenced wait. Remaining compute/halo work can be paced
  next using a fresh profile, without lowering resolution or hiding frame samples.

## Acceptance

Delayed-source test: advance returns while release is withheld, publication stays
old, and no missing sheet/rim is finalized. Release -> identical completed fields,
native products and halos as an already-resident run. Cover deferred admission,
terminal absence, refusal, cancellation and replacement at a source boundary.
Use deterministic synchronization/watchdog, not a fragile microsecond threshold.
GroundBuildSchedule and relevant terrain/source/publication tests remain green.
make format; focused suites; make lint (clang-tidy zero).
Preserve build/shots/places/Malcesine-8dd84aa7.png before rendering. Run Malcesine
without vegetation through full refinement, open both PNGs and use pixels.py;
report CPU/draw p50/p95/p99, maximum and count over 16.67 ms. No visual degradation.
