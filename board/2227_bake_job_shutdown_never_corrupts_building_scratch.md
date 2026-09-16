Type: bug
State: active
Area: engine, generators, world
Parent: 2105
Depends: 2224
Tags: ownership, streaming, shutdown

# Bake-job shutdown never corrupts building scratch

## Problem

`ScoreEveryMeshFacesOutward` now reaches streamed-world preparation, times out after 15 s on
pending ingestion/classification, then crashes during `Engine` destruction. The macOS report is
an allocator-detected invalid/double free in this exact stack:

`Generators::BuildingScratch::~BuildingScratch` → `StructureBakes::Job::~Job` →
`StructureBakes::~StructureBakes` → `Engine::State::~State`.

`StructureBakes::Clear` waits queued task handles before destroying their `Raw`, `Output` and
`Scratch` owners, but the report proves that this ownership or the worker's scratch mutation is
not safe. The timeout must be a clean failure, never memory corruption.

## Decision

Establish whether a bake worker can still access a job after `Tasks::Wait`, whether a scratch is
leased twice, or whether `BakeStructures` corrupts a `BuildingScratch` buffer. Reproduce with a
controlled queued real bake that tears down before landing; build that test and its complete
engine/generator dependency closure under AddressSanitizer and UndefinedBehaviorSanitizer. Fix
the proven owner or bounds violation at its source. Do not retain jobs forever, skip shutdown,
or turn the timeout into success.

## Proof

- A controlled real bake is stopped before landing; Engine/StructureBakes destruction waits every
  worker and exits with no sanitizer finding, trap or leak.
- Repeated timeout/shutdown cycles and a completed landing both preserve scratch exclusivity and
  exit cleanly.
- `ScoreEveryMeshFacesOutward` no longer signals after a preload timeout. Its streaming readiness
  remains independently governed by 2105.
- Negative control that destroys a leased scratch before its task completes is caught by the
  sanitizer contract.
