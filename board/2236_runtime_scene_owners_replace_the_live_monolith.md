Type: refactor
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: engine, render, content, test
Tags: ownership, architecture, state

# Runtime scene owners replace the Live monolith

## Evidence

Live.h/cpp currently combine declaration, imported asset posing, CPU mesh/material
storage, resident handles, terrain pages, lighting/atmosphere, camera, render history,
submission, readback and PNG filesystem IO. Their 599/1684 lines locate the audit;
size is not the defect. Resource access by CrownPieces and HeightSheets forces unrelated
systems to depend on the whole object. Splitting member definitions across files would
leave the same coupling and is not acceptance.

## Ownership decision

- Engine runtime scene coordinator: current native world identity, revision and complete
  replacement transaction. Reuse WorldCandidate/GroundWorldCandidate and SceneState;
  do not add a second commit owner. Final name RuntimeScene replaces Live after migration.
- src/render/scene/: scene resource bindings for mesh instances, material slots and height
  pages; this owner belongs to existing WorldContent and its candidate lifetime. CPU data
  retained for reupload belongs with the binding, not a second authoritative world mesh.
  Expose typed generation handles and bounded batches, not a reference to Live.
- src/render/: submitted pose/camera history and camera-relative frame data. WI 2191 owns
  last-successful-submit semantics; extraction must preserve that contract, not advance
  history on Pose/Measure. SceneState/FrameResources keep GPU completion ownership.
- src/world/sky/: physical sky/sun evaluation; render converts values into lighting inputs.
  Scenario translation supplies settings at the boundary, not atmosphere evaluation.
- src/content/ and simulation: native asset/instance/animation state under WI 2150.
  Import documents remain temporarily owned by the adapter until that consumer migrates.
- src/engine/: public screenshot orchestration over renderer readback; PNG encoding/IO
  stays outside runtime scene and renderer state. Preserve public saveScreenshot errors.

## Current boundary

`Render::SceneResources` owns piece and height-page sources, generation handles, registered native
materials, terrain bindings, ground classification and candidate restoration. `SceneRenderer`
owns camera binding and submitted pose history. `world/sky` owns atmosphere integration/cache.
`FrameCapture` owns readback, PNG encoding and screenshot IO. TilePieces, CrownPieces, WorldCrowns
and HeightSheets use these owners directly; none reaches Live. Candidate failure and publication
preserve complete active resources. Native Geometry remains beside resolved material slots because
its images back borrowed texture pixels. `UiSession` owns declared surfaces, layout, painting,
scroll state and transactional renderer publication; `Live` only forwards UI input and replacement.

## Next complete slice

`Live` still owns subject import/playback, surface resolution, base subject material preparation,
render-plan selection and scene orchestration in 425/1130 lines. Inspect every remaining member by
owner. Move base subject material preparation beside the native content asset without duplicating
`SurfaceTable`; preserve declaration rollback and borrowed image lifetime. Then rename the reduced
coordinator and `Live.{h,cpp}` to `RuntimeScene`; migrate all callers/tests in one step and leave no
alias or compatibility header. A name-only move before those responsibilities are separated does
not satisfy this slice.

## Acceptance

- [x] Consumers compile against the extracted owner without Live/EngineHeld includes.
- [x] Public A -> failed B -> retry B proves pixels, handles and terrain resources remain
      coherent; old owner destruction cannot clear the successor's resources.
- [x] Stale handle after release/reuse is rejected; failed bulk instance update is atomic.
- [x] Repeated replacement has bounded retained bytes and unchanged completed-frame pixels.
- [x] Focused SceneResources lifetime suites and make lint pass; the wider public suite retains
      three exact texture-repeat failures owned by WI 2179.
- [x] UI declaration, scroll and renderer replacement are owned transactionally by UiSession;
      focused surface failure and pointer-input suites plus make lint pass.
- [ ] RuntimeScene owns only coordination; the base subject material owner has focused rollback/
      lifetime tests, public render behavior is unchanged, and make lint passes.
