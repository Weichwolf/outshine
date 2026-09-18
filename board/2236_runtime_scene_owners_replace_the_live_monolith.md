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

## First complete implementation slice

1. Extract Live's Piece/HeightPage storage, free lists and source byte accounting into
   the scene resource owner inside WorldContent. Move existing algorithms unchanged;
   use generation handles and keep data alive until their last renderer use completes.
2. Route PlacePiece/ReleasePiece/SetPieceInstances and height-page operations through that
   owner. Migrate TilePieces, CrownPieces and HeightSheets to its narrow concrete API;
   no generic service locator, callback per field or compatibility Live reference.
3. Preserve atomic replacement: prepare candidate bindings/resources, validate, then use
   the existing complete-world commit. A rejected candidate cannot release active handles.
   Preserve transferred handle identities across candidate publication as existing tests require;
   only released/reused slots invalidate their old generation. Do not redefine that contract.
4. Migrate mirrored internal tests and reaches declarations in the same commit. Remove
   migrated Live fields/methods. Then take the next owner above as a separate full slice.

Before implementation activate this WI in its own commit. Files: src/engine/Live.{h,cpp},
WorldCandidate.h, TilePieces.*, CrownPieces.*, HeightSheets.* and src/render/SceneRenderer.*.
Reuse SceneState/WorldContent from 2223; move ResourceHandle.h to the owning lower tier; no dependency on its remaining
proofs. Renderer cannot include engine headers. WI 2237 uses this same resource boundary;
it must not invent a competing one. No new threads or algorithm changes in this slice.

`Render::SceneResources` now owns piece CPU sources, resident IDs, generation slots, free-list
reuse and atomic instance batches inside `SceneRenderer::WorldContent`. Candidate creation copies
only source state, then rebuilds candidate GPU residents; rejection leaves the published owner
untouched and publication moves the complete owner. Live delegates piece operations and no longer
stores piece slots. The same owner now holds height-page sources and handles, terrain-tile bindings
and ground-grid parameters. It translates native `TerrainTile` bindings into GPU instances and
restores the complete terrain resource set inside a world candidate. `HeightSheets` depends on
`SceneRenderer` instead of `Live`; all migrated height-page and terrain forwarding methods and
state have been removed from `Live`. `TilePieces`, `CrownPieces` and `WorldCrowns` use
`SceneRenderer` directly and no longer include or store Live.

## Registered piece material decision

`SubjectMaterial` contains borrowed pixel pointers, so copying resolved slots without their native
`Geometry` source is invalid. The next slice moves registered piece material sources, resolved slots
and registered surface-to-slot mapping together into WorldContent. Candidate copying clones each
source and resolves pointers against the clone. Publication uploads the base subject table first,
then appends registered slots and publishes native and registered mappings independently. Any
resolve, material upload or piece upload failure abandons the whole candidate; it must not append to
the active table. SceneResources then exposes one registration operation returning a registered
surface index. CrownPieces uses that operation plus piece handles and no longer includes Live.

Implemented: SceneResources retains each native Geometry beside its resolved slots, clones and
re-resolves sources for candidates, appends subject and glass materials with rollback, and restores
registered mappings before resident pieces. Live supplies only its base material table and owns no
registered sources or indices. The existing native-growth pixel proof verifies that registered
images and handles survive candidate publication while base slot indices change.

Frame readback validation, float-buffer selection, PNG encoding and filesystem output now live in
the engine FrameCapture boundary over SceneRenderer. Public Engine methods call it directly; Live
no longer exposes readback, settling or screenshot IO. The implementation shares Framing.cpp with
the public orchestration because separating the two archive members creates a Mach-O static-archive
pull cycle; FrameCapture.h remains the narrow internal contract.

Ground classification words and palette now share SceneResources ownership with height pages,
grid and terrain tiles. Candidate copying and one RestoreGroundResources operation rebuild the
complete ground GPU state. Live retains neither classification sources nor a forwarding API; its
ownership proof moved beside SceneResources.

Do not let SceneResources duplicate Live's complete import-facing SurfaceTable. The base table is
an input to scene publication until its separate extraction; registered generated materials are an
owned extension with a distinct index domain. SubjectDraw keeps the two mappings separate. Retain
the native Geometry sources because their images back SubjectTexture pointers and candidate rebuilds.

## Acceptance

- [x] Consumers compile against the extracted owner without Live/EngineHeld includes.
- [x] Public A -> failed B -> retry B proves pixels, handles and terrain resources remain
      coherent; old owner destruction cannot clear the successor's resources.
- [x] Stale handle after release/reuse is rejected; failed bulk instance update is atomic.
- [x] Repeated replacement has bounded retained bytes and unchanged completed-frame pixels.
- [ ] make format; make suite SUITE=outshine/include/Outshine; focused resource lifetime
      suites under test/outshine/src/render/scene/SceneResources (PieceHandlesSurviveWorldReplacement,
      PieceInstanceBatchRejectsPartialUpdates, GroundResourcesSurviveWorldPublication); make lint. Use existing client captures for
      before/after PNG checks; record existing WI 2219 failures separately, never mask them.
