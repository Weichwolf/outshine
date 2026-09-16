Type: bug
State: active
Parent: 2191
Depends: 2223
Area: engine, render, test
Tags: geometry, ownership, state, gpu

# Geometry replacements publish whole world candidates

## Ground publication boundary

`Engine::State::Grounds` now stages sheets, ground positions/indices, network, material slots and
bounced-light albedo in one candidate. Earthworks no longer publish an intermediate world; final
geometry/classification must succeed before the renderer owner and nonthrowing CPU moves commit.
The published revision changes last. Provider requests and preparation caches remain independent.

## Existing foundation

Public and pending geometry replacement and completed structure bakes use prepared `Live` /
renderer candidates. Native geometryless declarations can prepare their first streamed world.
Owner-local piece/page handles survive recreation of renderer-local resources. Released handles
are invalidated; bounded slot reuse/generation handling still needs implementation.
All replacement paths rebind pieces, height sheets and crowns via nonthrowing
`Surrounds::BindLiveResources`; rebinding contains no worker/pool setup.
`GroundPublication` records only successfully installed revisions, supports retry after an
uncommitted request and prevents unpublished classes/buildings from reporting ready.
Ground classification GPU buffers belong to `WorldContent`; `Live` owns their CPU inputs.
Candidate rejection preserves old buffers/data, retry restores the original snapshot and a fresh
declaration starts with empty classification. Old global storage failed five GPU checks.

## Decision

`GroundWorldCandidate` owns an entire ground rebuild. It owns prepared `Live`, copied
height-sheet state, new terrain positions/indices, candidate network and building material slots.
Classification changes bounced-light albedo only on candidate `Live`. Refinement and earthworks
use candidate sheets; earthworks do not publish. Final geometry and classification uploads must
succeed before publishing renderer/Live and nonthrowingly transferring CPU products. Publish the
matching ground revision last. A local RAII owner abandons the candidate on every early return.
Request counters and provider/generator caches may advance; published-world products may not.
Do not snapshot and restore a mutated published owner. Do not rebuild via scenario export.

CPU/GPU identity remains independent of resource allocation order. GPU-, material-, placement-,
height- and class-upload errors must preserve old scene resources and permit an immediate retry.
Copy/prepare costs are explicit preparation work; no bounded-frame-time claim without measurement.

## Scope still requiring proof

- End-to-end public Engine/OSM failure after height upload: material mapping, albedo, actual
  routing graph, readbacks and rendered old pixels survive together, beyond the synthetic candidate
  fixture. Terrain topology, CPU positions/indices, network counters and revisions are covered.
- Failed structure roof after accepted wall upload publishes neither tile nor terrain input.
  `TilePieces::Hands` now removes old pieces only after both replacement uploads succeed. The
  original roof-refusal fixture lacked a base material and failed at the wall; fixed preparation
  exposes old-tile loss on former code. Old geometry/digest preservation and retry are tested.
  Complete bake-job/footprint publication still needs its independent failure proof.
- Reject stale streaming results after a newer revision is current.
- Camera/animation/native replacements retain placements and frame state.
- Piece/crown behavior after replacement needs independent coverage beyond height ownership.
- `Live::ReleasePiece` / `ReleaseHeightPage` currently retain released CPU payloads in append-only
  handle tables; later candidates copy those dead payloads. Release them and bound/reuse handle
  metadata without accepting stale handles; repeated streaming/retry must not grow retired storage.
  First release owned vector payloads while retaining invalid handle records. Add piece/page CPU
  payload capacity counters to existing optional diagnostics; test real placement, release, double
  release, stale-handle rejection and candidate reconstruction. Metadata reuse remains separate.
- `Restands` and surface-only redeclaration remain separate mutation audits.
- Public geometry/audio occlusion must publish together; no partial declaration replacement.

## Evidence

- `LateFailurePreservesPublishedGround`: actual class and geometry GPU submission failures after
  staged terrain changes preserve the old renderer owner, terrain topology and CPU publication;
  immediate retry installs staged products and revision once. Both SDL submit entry points injected.
- Whole-ground migration: Graz without vegetation is pixel-identical to the pre-change render
  (0/921600 differing pixels), PNG opened. Reference: build/shots/reference/ground-world-transaction/.

- `GroundResourcesSurviveWorldPublication`: geometryless declaration, two replacements, stable
  height-page handles and operation through rebound streaming owner. Removing rebinding fails.
- `FailedBuildsRemainEligibleForRetry`: publication state, retry, residency policy, changed
  data/projection and reset. State-contract proof, not injected whole-OSM-build failure.
- `GroundClassificationBelongsToItsWorld`: actual GPU handle/payload preservation across candidate
  upload, rejection, retry, publication and new declaration. Related generator/upload suites pass.
- Graz without vegetation renders. Visual inspection against saved historical image: 71/921600
  pixels differ on the left slope; no broad material regression. Not a controlled causal comparison
  or visual-quality approval. References remain under build/shots/reference/ground-classification-ownership/.
- For each completed step: make format, focused tests, make lint; image changes via client PNGs.
