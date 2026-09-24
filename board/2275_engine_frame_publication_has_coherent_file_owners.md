Type: debt
State: open
Architecture: ready
Parent: 2139
Depends:
Priority: P1
Area: engine, audio, render
Tags: ownership, naming, runtime

# Engine scene setup, measurements and audio snapshot have separate file owners

## Evidence

`src/engine/Telling.cpp` still contains `EnsureRuntimeScene`,
`PublishResourcePayloadMeasurements`, `PublishFrameMeasurements`,
`PublishAudioSnapshot` and `IsAudioOccluded`. The filename is now false, and
those methods have three owners: scene lifetime, frame telemetry and the audio
snapshot. `src/engine/RuntimeScene.cpp` belongs to `outshine::Core` and owns
render-scene implementation; engine orchestration does not belong there.

## Binding split

- Move `EnsureRuntimeScene` to `src/engine/SceneRuntimeIntegration.cpp`. It
  remains `Engine::State` coordination of renderer replacement and pending
  world products. Preserve the old scene on failed replacement and release
  pending geometry only after success.
- Move `PublishResourcePayloadMeasurements` and `PublishFrameMeasurements`
  to `src/engine/FrameMeasurements.cpp`. Their `Published.Places` order,
  guards and heap tag remain stable. The frame-publication timer continues to
  include the following audio snapshot handoff at the same tick boundary.
- Move `PublishAudioSnapshot` and `IsAudioOccluded` to
  `src/engine/AudioScenePublication.cpp`. The double-buffer sequence and
  thread owner do not change here; WI 2130/2212 owns any later audio-thread
  migration. `Core::AudioOcclusion` remains the acoustic geometry helper.
- Remove `Telling.cpp` entirely. Update the one authoritative source inventory,
  reaches and test build profiles; no forwarding implementation or duplicate
  state. Only move code and private includes necessary for each owner.

## Acceptance

- Scene creation/replacement failure preserves its prior state. One successful
  update publishes one measurement round and one audio snapshot in the original
  order. Audio source binding and camera behavior match the previous commit.
- `make format`, focused public Engine target/camera/audio/diagnostics tests,
  `make test-client-render` and `make lint` pass. No public API changes or
  changed PNG bytes are expected; investigate any difference rather than
  accepting a new reference.
