Type: defect
State: active
Architecture: ready
Parent: 2228
Depends:
Priority: P1
Area: world, streaming, memory
Tags: residency, tilepool, realtime

# TilePool scheduler counts exact owned storage

## Problem

Queue-owned indexes now use FlatMap and completed-key windows use bounded rings.
Allocation-failure admission has a regression test. This removes guessed tree-node
layout accounting but does not yet prove complete exact retained-byte accounting.
Review nested fetch keys, provenance strings, parked vectors and active worker products;
count retained allocations, not temporary strings or SSO capacity already inside objects.

`Done_`, `Posted_` and `Awaiting_` are queue-owned under `QueueMutex_`; their ordering
is not an externally visible data contract. Completion order must remain deterministic
for a fixed admission order. `CacheAt_` belongs to the byte-cache category, not scheduler.

## Decision

Replace queue-owned node containers with bounded contiguous storage. Reuse `FlatMap`
only where its key/value nonthrowing contract is met and make allocation failure explicit
at the scheduling boundary. Use fixed-capacity ring storage for the retained-key windows;
do not keep `std::deque` blocks with unknowable retained capacity. Preserve the existing
separate byte-cache, DEM and scheduler categories and count each allocation at one owner.

`TileMeshes::Reply::Deferred` means that a request was not admitted and may be retried;
it is distinct from `Pending` (admitted work) and `Refused` (a source/product failure).
`OutstandingMost` bounds unfinished posted work: queued, carried or parked. Retained
completed results have separate bounded windows and must release admission slots. Duplicate admitted keys remain `Pending` without consuming another slot. A failed
`FlatMap` insertion returns `Deferred`; it must never masquerade as a repeated pending job.

`DelayedFieldsDoNotBecomeRims` intermittently fails its `FieldDropped == 0 &&
Posts < 100` dependency-admission check under the combined focused suite, then
passes alone. Record both counters and the queued/parked state on failure;
determine whether duplicate field jobs or scheduling timing causes the extra
posts. A rerun is evidence of flakiness, not resolution.

Do not count shared decoded terrain fields in the scheduler. Count a queued `Fetch` key,
completed `Result` payloads, parked job vectors and every retained queue/index allocation.
A synchronized snapshot may be momentary, but its arithmetic must be exact for the lock
state it observes.

## Implementation

1. First prove progress with capacity one after a cached completion; retain the old
   result while admitting a distinct request. Then prove dependency progress: a mesh
   occupying a slot must still obtain its required fetch/field. Reserve bounded
   dependency capacity or separate fetch and compute quotas; do not drop the bound
   or rely on endless retry. Test cold source-backed work, not only analytic shapes.
2. Inventory each `TilePool` allocation and designate ByteCache, DEM or Scheduler owner.
   Remove `TreeNodeBytes`; it is a library-layout assumption, not a contract.
3. Give queue-owned containers a direct capacity-byte contract, including nested job keys,
   result nodes/payloads and parked dependency lists. Keep result publication, eviction,
   duplicate suppression and wake-up behavior unchanged.
4. Bound retained completed-key windows and parked queues. At capacity, coalesce a repeated
   request or return a declared admission error; never overwrite a current required result.
5. Add fault-injection tests for index growth and tests that independently sum every owned
   capacity. Check queued, carrying, parked, completed and post-eviction states.

## Acceptance

- Scheduler bytes equal an independent sum for every queue state and include no host bytes.
- Duplicate fetch/mesh/field requests retain their prior result and do not allocate.
- Overload, cancellation, waiter wake-up and result eviction preserve coverage and ownership.
- The reported categories sum to `ResidentBytes`; a shared decode field appears once.
- `make format`; focused TilePool cases; queue race/cancellation cases; `make lint`.
