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
`Scratch` owners, but `StructureBakes` had no destructor and therefore never called it when
`Surrounds` was destroyed. Its queue was released while `Tasks` still ran a bake. The timeout
must be a clean failure, never memory corruption.

## Decision

`StructureBakes` destruction calls `Clear` before its owning `Tasks` member is destroyed, so every
queued job reaches `Wait` before any job owner is released. Keep the controlled queued real-bake
shutdown case and AddressSanitizer/UndefinedBehaviorSanitizer coverage: they must distinguish a
future scratch bounds violation from the repaired lifetime race. Do not retain jobs forever, skip
shutdown, or turn the timeout into success.

## Proof

- A controlled real bake is stopped before landing; Engine/StructureBakes destruction waits every
  worker and exits with no sanitizer finding, trap or leak.
- Repeated timeout/shutdown cycles and a completed landing both preserve scratch exclusivity and
  exit cleanly.
- `ScoreEveryMeshFacesOutward` reaches its 15-s preload timeout without a new crash report or
  signal after destructor-driven `Clear`; its streaming readiness remains governed by 2105.
- A controlled real bake is stopped before landing and runs under ASan/UBSan to isolate future
  scratch corruption from task lifetime.
- Negative control that destroys a leased scratch before its task completes is caught by the
  sanitizer contract.
