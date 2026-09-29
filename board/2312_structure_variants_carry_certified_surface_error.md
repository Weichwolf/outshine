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

## Verified task ownership

29610fade: running/partial/final moves PASS; three ownership controls FAIL (exit 1).
Five focused cases, full lint 256/256 tidy units, zero findings, 32 guards/API docs PASS.
Logs /tmp/outshine-build-task-move-names-{focused,queue,full-lint}.log; process exit 0.

## Next step after dense-cell proof: paired worker phases

Owner: StructureBuildTask.h/.cpp, StructureBuildQueue.cpp and StructureBake.h. Keep task
lifecycle separate from heap-owned phase state; running moves preserve worker pointees.
Queue supplies an immutable qualified source/cell/detail proof request. Eye-selected
whole-tile detail cannot supply one cell bound. Variant/Fine use the same captured raw
geometry, HeightField and anchor. Create a separate Fine RawTile on the worker before
starting its own bake progress. Never rewrite a started variant/detail request. Initially
build the reference locally; cache reuse waits for exact source/cell/frame identity.

Output.Tile currently means final completion: Poll/Resume uses its absence. Keep the
variant private through Reference/SurfaceProof or the queue will land it and release inputs
prematurely. The published coarse front remains usable. One post performs at most four
64-structure ranges OR one 128-work proof slice; transitions add no second full slice.
Check stop before transitions/steps. Use 2313 limits: 131072 queries, 4096 regions, 0.02 m
uncertainty. Exhaustion preserves complete coverage; cancellation rejects late output.
Proof failure retains the valid variant with an explicit failure result and no smaller
error. Invalid geometry/source cannot become optimistic zero. No mutable owner callbacks.

BakedTile carries optional native interval, reference/variant detail and cell identity.
Queue rechecks the complete captured source receipt at landing. Renderer selection stays
unchanged in this step. Account Fine raw copy, reference geometry, both bake progress
buffers and proof scratch; production admission must reserve their configured maximum.
HeapBytes cannot read live mutable worker state; publish counters through completion.
Production proof requests stay disabled until byte limits/accounting are implemented.

Acceptance: paced native Fine/Shell/Massed cells, immutable requests/common anchor, real
cancellation and running/partial moves in EACH phase, exactly-once output, exhausted/failed
proof and byte admission. Early Output.Tile, rewritten Fine request, lost phase ownership
and ignored cancellation controls must actually FAIL. Format; StructureBuildTask/Bake/
SurfaceRefinement/BuildQueue suites; full lint/tidy/API. Source revalidation (2311), resident
transfer and GPU/visual acceptance remain open; CPU intervals alone cannot lower LOD error.

Real-cell probe now blocks useful integration: all three Graz Fine/Shell pairs exhaust
2313 seed capacity without queries. Complete its dense-cell hierarchy first; retain caps.
