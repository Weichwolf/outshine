Type: defect
State: active
Architecture: ready
Parent: 2105
Depends:
Priority: P1
Area: engine, world, streaming
Tags: scheduling, osm, realtime

# Streaming admits bounded compute work with backpressure

## Evidence

The 2026-09-21 cold floor-contact run with the bounded HTTP multi transport exhausted
15.040 s at 8/9 landed tiles. `BlocksUnder` then stopped serializing independent DEM
requests at the first missing field. The unchanged cold case exhausted 15.249 s with
8/12 landed, four queued and 1,164 structures left. Ground was resident after 7.084 s;
all requested vectors after 0.783 s. Worst range, finalization and task costs remained
0.734, 3.753 and 4.370 ms. Dependency fan-out therefore admits more work but does not
service it: the shared task pool starves short structure continuations behind unrelated
work. The request ledger can also report ground ready before stitched eight-neighbour
dependencies are ready. Measure both waits; do not tune slice sizes against this evidence.

## Decision

An engine-thread admission owner schedules existing vector decode, field ingestion and
structure continuations. Reuse their task/result owners; workers return private products,
never mutate the published world. Every item has identity, input revision, deterministic
priority key, cancellation and declared work units. Queue capacity and bytes are bounded.
At capacity, defer production or coalesce superseded work for the same identity; do not
silently discard a current required tile. Completion capacity is reserved before dispatch
so a worker can always return ownership. Cancellation releases products after worker return.

Wall-clock estimates may change when work runs, never its contents or merge order.
Sort accepted inputs by stable source identity before deterministic generation; do not
promise identical per-frame admission across CPUs or arrival orders. Stale revisions
are rejected at the existing candidate commit boundary. Publish complete products only.

A wall-clock deadline cannot preempt a synchronous call. Each admitted unit must have a
bounded count/size and measured tail cost. An oversized unit becomes a continuation, not
permanent starvation or an exception that bypasses the budget. Fairness ages ready work
using engine ticks, with stable tie breaks. Avoid waiting for all unrelated vector siblings.

## Implementation order and ownership

1. Measure first eligibility, height-dependency-ready, post, worker-start and land times
   for every structure tile. Attribute shared-pool queue delay by work kind. Reproduce
   floor-contact cold without changing 15 s.
2. Introduce one private admission record/owner around those existing queues. Preallocate
   capacity; establish explicit capacity and slice values from those measurements and
   label provisional choices. The frame target is 1000/60 = 16.67 ms, not a compute-only
   allowance. Record p50/p95/p99 and overshoot; no hard real-time guarantee from averages.
3. Test reversed completions, overload, cancellation, stale revisions and a large item
   among small ones with a controllable clock. Equal final native products and eventual
   service are required; identical wall-clock scheduling is not.
4. Integrate resumable ground work from WI 2234 when available. Admission tests and
   measurement do not depend on WI 2231's remaining public publication proof.

## Acceptance and commands

- [ ] Bounded queue counts/bytes, no blocked completion producer, eventual service under
      sustained admissible load; overload defers work without losing required coverage.
- [ ] Reordered completion produces identical accepted native data; an intentionally stale
      result is rejected and cannot overwrite the newer candidate.
- [ ] Unchanged floor-contact and Lattice cases meet their declared 15 s budgets; compare
      cold/warm runs and disclose IO separately. No sleeps or increased timeouts.
- [ ] make format; make suite SUITE=outshine/src/engine/streaming/StructureBuildTask;
      make suite SUITE=outshine/integration/places/ScoreTheLatticeMeetsItselfAtALevelBoundary;
      make suite SUITE=outshine/integration/places/ScoreAFootprintStandsOnALevelFloor;
      added admission cases through make suite; make lint.
