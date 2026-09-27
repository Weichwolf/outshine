Type: defect
State: done
Architecture: ready
Parent: 2195
Depends: 2268
Priority: P0
Area: client, capture, khronos
Tags: groundless, readiness

# Groundless scenario run captures without world readiness

## Evidence

`outshine-client run --rows` sends every scenario through `Shots::Draw`.
`PlaceCamera::MeasureFrames` then waits up to 6144 frames for
`Engine::settled(Refined)`, which is documented to stay false without requested
world tiles. Three groundless Khronos scenarios consequently fail before image
comparison. `Engine::beginCapture` already accepts assembled groundless scenes.

## Contract and ownership

`src/client/PlaceCamera` owns the timed still-capture loop. Check the declared
ground capability once per capture. Only a scenario with ground waits for
Refined world quality and reports world blockers. Every scenario still advances
and renders at least 120 measured frames, starts a capture and writes a PNG.
Do not change `Engine::settled`: it reports world-streaming readiness, not
general scene readiness. Do not add artificial terrain to vendor scenarios.

## Falsifiable acceptance

- A groundless scenario importing a visible glTF through `run --rows` exits
  successfully and writes a nonempty PNG. Its world readiness remains false.
- A declared-ground scenario with unavailable ground still refuses capture
  after its readiness budget; this change cannot bypass terrain publication.
- The three formerly blocked Khronos captures reach image comparison.
- Focused client test, `make format`, and `LINT_JOBS=2 make lint` pass.

## Result

`run` captures a groundless imported asset after 120 measured frames. Its world
readiness remains false; declared-ground captures still require Refined. The
groundless client test and the existing offline-terrain refusal cover both sides.
DirectionalLight, PointLightIntensityTest and SheenWoodLeatherSofa now reach the
oracle comparison. Their image agreements are 37.6338%, 20.2521% and 97.5468%;
none passes. Visual inspection shows mismatched sphere light gradients, missing
square panel corners behind the point lights, and sofa edge/detail differences.
These are separate render/material defects, not capture-readiness failures.
