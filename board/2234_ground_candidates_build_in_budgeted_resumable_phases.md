Type: defect
State: active
Parent: 2105
Depends: 2231
Priority: P0
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Problem

`Grounds` synchronously prepares a world candidate, patchwork, sheets, classes, roads, earthworks,
water and GPU geometry. A repeat floor-contact preload returned success after 16.50 s despite a
15 s budget because the deadline was checked before this final operation. The existing test now
enforces wall-clock residency and exposes the violation.

## Architecture decision

`GroundCandidateBuild` is a move-only engine-thread state machine. It owns an immutable input
revision, candidate CPU/GPU products, phase-local cursor and cancellation state. Its phases are
`Prepare`, `Patchwork`, `Classify`, `Routes`, `Earthworks`, `Water`, `Geometry`, `Validate` and
`Publish`. Each `Advance(budget)` consumes a bounded measured work unit and returns `Pending`,
`Ready`, `Rejected` or `Cancelled`; only `Publish` transfers the complete candidate to `Live`.
The active world is read-only throughout all earlier phases.

Worker and GPU completions return to the owning phase with their input revision. A stale or
cancelled candidate retains its owners until final worker/fence completion, then releases itself
without changing the active world. The state machine records phase CPU time, pending queue depth
and candidate CPU/GPU requested bytes without periodic logging or frame-path allocation.

The first candidate starts after admitted terrain/vector coverage. Its completed phases retain only
terrain, class and material products; class/footprint revisions therefore still restart a finished
candidate. WI 2224 supplies the separate candidate `BuildingField` and `TilePieces`: bake planning,
landing and mesh handoff occur there before publication, while the active world remains unchanged.
This WI owns the remaining bounded scheduling, phase timing and peak-memory proof; it must not
reintroduce active-world mutation to shorten preload.

## Implementation order

1. **P0, after 2231:** Extract the existing synchronous stages into the named phase owner without
   changing their algorithms or publication boundary. Add one analytical phase-boundary test first.
2. Continue the longest measured stage with a cursor. A phase that cannot state a finite unit of
   work is not admitted; identify and split it before claiming a budget.
3. Add stale-input, cancellation, GPU-submit failure and retry tests using production candidate
   operations. Then enforce the unchanged floor-contact and Lattice deadlines with phase telemetry.

## Acceptance

- Deterministic phase tests cover initial build, every phase boundary, stale input, cancellation
  and publish failure; no candidate state leaks into the current world.
- The unchanged floor-contact Place passes its enforced 15 s wall-clock budget and all floor/road
  contact checks.
- Phase CPU time, queue depth and peak candidate memory are measured; lint passes.
