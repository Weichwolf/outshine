Type: feature
State: active
Architecture: ready
Parent: 2298
Depends: 2313
Priority: P0
Area: generators, engine, render
Tags: geometry, lod, proof, bounded

# Structure variants carry conservative surface-error bounds

## Problem and verified foundation

StructureCellDetail still uses the whole-cell envelope. Shell preserves native
roof/footprint/foundation surfaces, but CPU primitive evidence alone cannot justify
smaller runtime LOD errors. Vertex samples miss a filled opening: a 3x3 m cap over a
1x1 m opening has zero corner error and 0.5 m directed surface error.

At 5de351d40: 45 focused tests and full lint pass, 255/255 tidy units, zero findings.
TriangleDistance uses outward convex interpolation for upper estimates and support
planes for lower estimates. The independent Decimal-160 experiment covered 6000
queries, permutations, binary scales and collapsed targets. TriangleRegion encloses
rounded midpoint/interior samples. StructureSurfaceError validates complete native
index runs before exposing a resumable coarse envelope. 2313 owns adaptive precision.
These are native CPU proofs, not publication, transform or visual acceptance.

## Binding ownership and implementation order

1. generators/building owns comparison of immutable Raised wall/roof streams from
   the SAME pinned source snapshot/common frame. Preserve TriangleDistance/Region and
   StructureSurfaceError contracts; no importer types or alternate geometry model.
2. StructureBuildTask owns phases Variant -> Reference -> SurfaceProof -> Complete.
   Publish the resident coarse safety net before requesting comparison work. Reuse a
   matching pinned Fine reference when available; otherwise generate it on the worker.
   Never mutate RequestedDetail on a running bake. Separate captured source identity
   from explicit detail requests; cancellation revokes all phases and scoped views.
3. Each SurfaceProof slice has a primitive-work cap and observes stop/source generation.
   Scratch belongs to the task and participates in CPU residency accounting. Task moves
   transfer ownership and revoke source progress (2313); no worker reads released inputs. Start with
   2313's limits; measure native cell sizes, completion rate, query work and bytes first.
   Brute-force target search and 4096-region initialization can exhaust on dense cells.
   Add a conservative nearest-surface acceleration only if measurements justify it;
   its pruned regions must supply valid lower bounds, not sampled guesses.
4. BakedTile owns optional geometric error plus source/detail/product identity. Move it
   through queue landings and TilePieces resident metadata without losing provenance.
   Validate pinned generation, vector/street inputs and terrain certificate at activation;
   equality of a hash alone is insufficient. No error metadata in footprint/navigation.
5. render owns resident selection. Use a smaller bound only after independent native
   and transfer controls pass. Enclose instance scale/transform and render-coordinate
   precision; otherwise keep the whole-cell guard. Unknown, expired or mismatched proof
   keeps a valid fallback and cannot claim Refined. Material/lighting error is separate.

## Invariants and error flow

- Every exposed upper certificate covers both COMPLETE surfaces. The maximum bounds
  Hausdorff distance. Lower evidence uses minima over ALL target triangles and sample
  deviation; upper evidence includes region radius and outward arithmetic.
- Four enclosed child triangles replace one parent only as a complete transaction.
  Exhaustion retains a proved complete upper bound, never partial coverage or zero.
- Empty/nonempty mismatch, invalid indices and nonfinite/unrepresentable arithmetic
  are expected failures. Failed proof does not delete valid resident geometry.
- Source changes/cancellation reject late proof; instance/world/contact IDs do not
  change when the renderer switches detail. Shadows use a valid source-matching level.

## Acceptance

- Analytical opening/planes, concavity, pitched roofs, sloped foundations, long walls,
  permutations, large anchors and native Fine/Shell/Massed pairs enclose known errors.
- Source/detail mismatch, exhausted budgets, cancellation, copy/move and staged transfer
  cannot expose optimistic error. Zero/radius/source/forced-near-Massed controls FAIL.
- make format; focused StructureSurfaceRefinement/StructureSurfaceError, BuildingMesh,
  StructureBake, StructureBuildTask, StructureBuildQueue, TilePieces and StructureCellDetail
  suites; full lint with clang-tidy and public API guards.
- Render same-build static/paced Hockenheim through outshine-client, open PNGs and
  compare selection, silhouette, shadows, p50/p95/p99, CPU/GPU bytes and work/frame.
  Current GPU/visual acceptance is unverified; prior sandbox failures do not establish
  today's availability. Do not close this WI with CPU-only evidence.

## Active step: transfer BuildTask ownership

Existing defaulted moves copy Handle/Running while transferring payload and stop owner.
A running source then fails destruction and cannot stop safely. Current deque production
constructs before posting; no production move/crash has been observed. This base-owner
repair is independent of 2313's adaptive comparison and precedes the phase expansion.
Owner: StructureBuildTask.h/.cpp. Explicit moves transfer all stable heap payload, stop
owner and handle; source becomes Empty/kNoTask. Empty permits stop/join/poll/destruction,
never Start/Resume. Assignment requires a nonrunning destination; self-move preserves work.
Worker closures already capture stable pointees, not the task object's address. Move may
therefore transfer a Running owner; Clear/destruction still requires the sole owner Join.
Preserve Ready, partial Completed, Running and final output/progress ownership. Fixture
holds a real worker in its mesher while construction/assignment move; only destination
stops/joins. Move partial completion before Resume; verify exactly-once output/payload
release. Restored default moves must produce actual FAIL, not a timeout or build failure.
Acceptance: make format; StructureBuildTask/StructureBuildQueue suites; full lint/tidy/API.
No change to geometric errors, publication or today's renderer-selection contract.

Direct implementation check: 20 checks PASS with real blocked workers. Restored default
moves, shared source stop ownership and lost destination stop ownership each give actual
FAIL (exit 1), not SIGNAL/BUILD/TIMEOUT. Format: 1203 files, zero errors. Logs:
/tmp/outshine-build-task-move-{controls,correct,default-moves,shared-source-stop,
lost-destination-stop}.log. bc33d18d4: official BuildTask 1 and queue 4 PASS;
logs /tmp/outshine-build-task-move-{focused,queue}.log. Full lint running; phases still open.
