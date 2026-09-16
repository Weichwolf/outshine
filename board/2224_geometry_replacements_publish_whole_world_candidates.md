Type: bug
State: active
Parent: 2191
Depends: 2223
Area: engine, render, test
Tags: geometry, ownership, state

# Geometry replacements publish whole world candidates

## Problem

`Engine::setGeometry`, `Engine::State::Stood` for pending geometry and
`Engine::State::Models` for streamed ground call `Live::SetGeometry` on the published `Live`.
That method changes its held geometry, material selection, proxy, camera state and shared renderer
products before `Build` can fail. WI 2223 makes a fresh declaration transactional, but these three
replacement paths can still leave a mixed CPU/GPU world. Current Place setup reaches the
candidate path previously aborted with `world replacement requires native world geometry`: the
provider result was not accepted as the native candidate input. The first streamed-world candidate
now rebuilds the declaration without a static mesh, restores its live resources and publishes before
the provider installs ground geometry; `GroundResourcesSurviveWorldPublication` covers that path.

## Decision

Extract a cloneable native world-input snapshot from `Live`: declaration, source geometry,
registered piece surfaces, placements, animation pose, surface overrides and scroll state. Build a
new `Live` and `SceneState` from that snapshot plus the replacement geometry. The old `Live`, audio
occlusion and renderer state remain published until the candidate succeeds. A successful nonthrowing
move replaces both owners; a failed candidate is destroyed without touching either old owner.

`Render::PieceId` and `PageId` are candidate-local renderer indices, so they must never escape as
long-lived engine resource identities. `Live` owns copyable piece/page descriptions and exposes
stable, generation-checked handles to `TilePieces`, `CrownPieces` and streaming owners. Publication
recreates candidate-local renderer resources from those descriptions, then rebinds the stable
handles. Released or superseded handles fail validation; relying on matching allocation order is
not a contract.

Do not move the published `Live` into a candidate and move it back on failure: `SetGeometry` mutates
its CPU state and such a rollback cannot prove restoration. Do not rebuild through scenario export:
native geometry and generated ground are not scenario data.

## Scope

- Public `setGeometry` replaces native geometry and audio occlusion together.
- Pending geometry entering a newly targeted world uses the candidate path and accepts its
  provider-supplied native geometry without requiring a separate static `setGeometry` call.
- Streaming ground rebuild preserves prior visible ground until the new mesh, material tables and
  placement rows are complete; superseded streaming results remain discardable.
- Completed structure bakes stage their tile pieces in the candidate and retain their bake job until
  publication succeeds. A rejected wall or roof upload publishes neither tile geometry nor its
  footprint-derived terrain input.
- `Restands` and surface-only redeclaration remain separate mutation audits.

## Proof

- `GroundResourcesSurviveWorldPublication` proves a geometryless declared world publishes its
  first streamed-world candidate and retains its height-page/lattice resources. Place preload now
  passes this boundary and separately waits for ingestion/classification (2105).
- Inject upload, material, placement and submit failures after an existing native or ground world
  rendered; old pixels, readbacks, audio occlusion, declaration and revision remain unchanged.
- Retry every rejected replacement and verify exactly one new publication.
- Reject a building roof after its wall upload and verify the candidate retains no partial tile.
- Move a camera and animated body through a native replacement; retained placements and frame state
  remain valid.
- Stream two ground revisions and reject the first after the second is current; stale work cannot
  publish. Run focused suites and `make lint`.

## Streaming owner rebinding

Publication destroys the previous `Live`. `TilePieces`, `HeightSheets` and `WorldCrowns`
retain borrowed `Live` pointers. Bake publication currently rebinds only pieces; earthwork
publication only sheets; public/pending geometry replacement rebinds neither. The next
same-frame ground/vegetation operation can dereference the retired owner. Use one nonthrowing
`Surrounds::BindLiveResources` operation after each successful replacement and before further
streaming work. Keep pool creation and bake setup outside this operation. Prove that resource
operations after replacement address the new world, including retained height-page handles.
The final ground `SetGeometry` and earlier CPU/network/height publication remain a separate
transaction gap; fixing pointer rebinding does not establish whole-ground atomicity.
