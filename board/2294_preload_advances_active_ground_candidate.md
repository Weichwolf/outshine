Type: defect
State: active
Architecture: ready
Parent: 2285
Depends:
Priority: P0
Area: engine, streaming
Tags: startup, structure, scheduler, publication

# Preload advances an active ground candidate despite stale published footprints

## Evidence

An offline Hockenheim still at 74.850 s has 538 cache deliveries, zero
provider starts and all requested terrain/vector tiles arrived. Refined
preload times out at 120 s. A sample puts 1722/2305 main-thread samples in
`FinishesPreload` -> `BuildingField::IngestedWithin`; the candidate remains at
`network` although its worker reports `done` after 1416 advances. Old published
footprints are at revision 4, current footprints at 21. `FinishesPreload`
checks the old publication's `StructuresReady` before calling `Grounds`, so
it cannot collect the finished worker and publish its replacement.

The same run spent 116.4 s in structure waits with zero wake signals when
`AwaitSlice` waited merely for a nonempty candidate queue. Restricting that
wait to running tasks reduces structure-wait time to 18.5 ms but cannot fix
the stalled publication alone. Both conditions require correction.

## Contract and ownership

`Engine::State::FinishesPreload` always advances an already-started ground
candidate, independent of readiness of the previous publication. It may keep
the existing gate for starting a new candidate while old structures finish.
`StructureBuildQueue::AwaitSlice` waits only for active worker tasks; completed
but unlanded entries must return immediately. Preserve ordered landing,
revision checks, cancellation and bounded candidate admission. Do not add a
Hockenheim path or extend timeouts to conceal stalled work.

## Falsifiable acceptance

- In a test with an old incomplete publication and a current candidate whose
  network worker has finished, preload collects the worker and progresses to
  a new publication. Existing Playable preload and invalid-budget contracts
  remain valid; timeout reports the actual candidate phase.
- Offline Hockenheim at 74.850 s reaches `settled(Refined)` within the 120 s
  diagnostic limit, with no provider starts. Two fresh pixel probes agree at
  the same camera and quality. If work remains slow, capture its phase and
  open a separate measured performance WI.
- Focused candidate/route tests, `make format`, `LINT_JOBS=2 make lint` pass.
