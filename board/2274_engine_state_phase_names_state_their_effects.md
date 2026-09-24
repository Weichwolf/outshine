Type: debt
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P1
Area: engine
Tags: naming, ownership, runtime

# Engine phase names state the work they perform

## Evidence and boundary

`Engine::State` still exposes `Stood`, `Watches`, `Asks`, `Composes`, `Bakes`,
`Tells`, `Grows`, `Carries` and `Models`. These verbs reveal neither owner nor
side effect. This slice fixes `Stood`, `Watches`, `Asks` and `Tells`; the parent
WI retains the remaining names. `Tells()` publishes both measurements and an audio snapshot;
`Stood()` creates or replaces the runtime scene; `Watches()` resolves and applies
the active camera. A caller cannot reason locally about failure or cost from
those names. The work is internal to `src/engine/`; the public API must not grow.

## Contract and implementation

- Rename `Stood` to `EnsureRuntimeScene`, `Watches` to `UpdateActiveCamera`
  and `Asks` to `RequestTerrainCoverage`. Their verified bodies create/replace
  the runtime scene, resolve/apply the camera and request visible terrain tiles.
  Preserve their existing `bool` failure propagation and error strings.
- Split `Tells` at its actual owner boundary: frame measurements and the audio
  snapshot are separate operations. Preserve the current publication order and
  measured interval; do not silently move audio to render time or alter the
  double-buffer handoff. Name the metrics operation `PublishFrameMeasurements`;
  `PublishAudioSnapshot` already exists and stays in
  its current thread until WI 2130 changes ownership deliberately.
- Rename definitions, declarations, callers, filenames where the file has one
  coherent owner, tests and metric labels together. No compatibility aliases,
  forwarding facades, broad search-replace or function splits justified only
  by line count. `EngineHeld.h` remains internal; public method names change
  only with a separate API contract.
- Where a method mixes owners, first extract the smallest real boundary with
  explicit inputs, output and failure propagation. Keep frame work bounded;
  a rename alone does not claim an ownership defect fixed.

## Falsifiable acceptance

- A call-site audit confirms that each renamed method communicates its side
  effects and whether failure can occur. No references to the four old method
  names remain in engine declarations or calls.
- Failure injection at scene creation and camera resolution preserves the
  previous error and published state. Update publishes one audio snapshot and
  one measurement round at the same tick boundary. No duplicate publication.
- `make format`, focused engine/public API tests, `make lint`, and affected
  client render run on the same commit. Do not claim that the remaining broad
  phase verbs are resolved by this slice.
