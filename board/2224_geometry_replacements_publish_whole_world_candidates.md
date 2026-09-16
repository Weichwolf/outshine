Type: bug
State: active
Parent: 2191
Depends: 2223
Area: engine, render, test
Tags: geometry, ownership, state, gpu

# Geometry replacements publish whole world candidates

## Remaining defect

`Engine::State::Grounds` still changes active height sheets, ground positions, network,
material indices and bounced-light albedo before water/material/geometry preparation finishes.
`ApplyGroundEarthworks` publishes a partial world; final `Live::SetGeometry` then mutates it.
A late failure can leave mixed CPU/GPU products even though the published revision stays old.

## Existing foundation

Public and pending geometry replacement and completed structure bakes use prepared `Live` /
renderer candidates. Native geometryless declarations can prepare their first streamed world.
Stable generation-checked piece/page handles survive recreation of renderer-local resources.
All replacement paths rebind pieces, height sheets and crowns via nonthrowing
`Surrounds::BindLiveResources`; rebinding contains no worker/pool setup.
`GroundPublication` records only successfully installed revisions, supports retry after an
uncommitted request and prevents unpublished classes/buildings from reporting ready.
Ground classification GPU buffers belong to `WorldContent`; `Live` owns their CPU inputs.
Candidate rejection preserves old buffers/data, retry restores the original snapshot and a fresh
declaration starts with empty classification. Old global storage failed five GPU checks.

## Decision

Use one `GroundWorldCandidate` for an entire ground rebuild. It owns prepared `Live`, copied
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

- Whole-ground rejection after height upload and before final geometry publication: old terrain,
  network, material mapping, albedo, revision, readbacks and pixels survive together.
- Failed structure roof after accepted wall upload publishes neither tile nor terrain input.
- Reject stale streaming results after a newer revision is current.
- Camera/animation/native replacements retain placements and frame state.
- Piece/crown behavior after replacement needs independent coverage beyond height ownership.
- `Restands` and surface-only redeclaration remain separate mutation audits.
- Public geometry/audio occlusion must publish together; no partial declaration replacement.

## Evidence

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
