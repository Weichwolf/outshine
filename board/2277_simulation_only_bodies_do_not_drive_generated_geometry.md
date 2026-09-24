Type: defect
State: active
Architecture: ready
Parent: 2096
Depends:
Priority: P0
Area: engine, simulation, render, audio
Tags: ownership, instances, native-geometry

# A simulation-only body cannot become a render subject instance

## Reproduced defect

`GeneratorProductsComposeAtomically` is UNPREPARED on clean `origin/master`
`b5a85a2d4`: an audio-bound `Scenario::Body` and generator-only geometry call
`Engine::State::Draws`, which forwards every dynamic body to
`RuntimeScene::Carry`. With zero driven parts, `Carry` rejects the whole frame:
"nothing joined this picture from a file". The same failure persists after
public API renaming; the rename is not causal. The test is a valid contract:
a physics/audio body needs no renderable asset.

## Boundary and implementation

Simulation owns bodies; the renderer owns visual instances. A body does not
implicitly instantiate unrelated generated/static geometry. `Draws` must not
submit body placements when `RuntimeScene::DrivenParts()==0`; still advance
the scene and publish the body's audio snapshot. No synthetic mesh, fake body
or test-specific branch. Preserve the previous error path for a real driven
subject with an invalid placement. Do not claim this guard solves asset-to-body
binding when driven and unbound bodies coexist; native instance ownership is
WI 2150 and requires an explicit association.

## Acceptance

- Generator-only geometry plus physics/audio-only body advances, renders and
  mixes; the targeted failure path no longer reports UNPREPARED.
- A genuine driven subject still rejects invalid placement and preserves its
  previous published world. `make format`, the failing generator test, audio
  binding test, target rollback test and `make lint` pass.
- If the complete generator test exposes a further unrelated failure, retain
  it as a distinct diagnosis; never mask it by changing the test.
