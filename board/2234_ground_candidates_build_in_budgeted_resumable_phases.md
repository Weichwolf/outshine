Type: defect
State: active
Parent: 2105
Depends: 2231
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Problem

`Grounds` synchronously prepares a world candidate, patchwork, sheets, classes, roads, earthworks,
water and GPU geometry. A repeat floor-contact preload returned success after 16.50 s despite a
15 s budget because the deadline was checked before this final operation. The existing test now
enforces wall-clock residency and exposes the violation.

## Decision

Make a ground candidate an owned phase state with explicit immutable input revision and candidate
resources. Each `Advance` has a measured bounded work budget and either retains the candidate,
reports an owned error, or publishes the complete candidate atomically. Cancellation and stale
revisions discard the candidate only after worker/GPU ownership permits it. Initial material setup
and the final footprint revision follow the same state machine; no partially built terrain or
building state reaches `Live`.

The first ground revision starts from admitted terrain/vector coverage, before the final generator
snapshot, class revision and structure bakes. Those later revisions trigger an owned rebuild.
`preload` flushes only candidates with admitted immutable inputs; frame updates advance one phase.
The floor-place control proved a model phase before this split consumes the last 15 s slack.

## Acceptance

- Deterministic phase tests cover initial build, every phase boundary, stale input, cancellation
  and publish failure; no candidate state leaks into the current world.
- The unchanged floor-contact Place passes its enforced 15 s wall-clock budget and all floor/road
  contact checks.
- Phase CPU time, queue depth and peak candidate memory are measured; lint passes.
