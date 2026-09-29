Type: feature
State: active
Architecture: ready
Parent: 2298
Depends: 2313
Priority: P1
Area: generators, engine, render
Tags: geometry, lod, proof, bounded

# Structure variants carry conservative surface-error bounds

## Problem and verified foundation

StructureCellDetail still uses the whole-cell envelope. Shell preserves native
roof/footprint/foundation surfaces, but CPU primitive evidence alone cannot justify
smaller runtime LOD errors. Vertex samples miss a filled opening: a 3x3 m cap over a
1x1 m opening has zero corner error and 0.5 m directed surface error.

TriangleDistance supplies outward upper/lower distances; TriangleRegion encloses
rounded subdivision samples. StructureSurfaceError validates complete native streams
before publishing a coarse envelope. 2313 owns adaptive precision. These tested CPU
contracts do not prove publication, transforms or visual acceptance.

## Binding ownership and implementation order

1. generators/building owns comparison of immutable Raised wall/roof streams from
   the SAME pinned source snapshot/common frame. Preserve TriangleDistance/Region and
   StructureSurfaceError contracts; no importer types or alternate geometry model.
2. StructureBuildTask owns phases Variant -> Reference -> SurfaceProof -> Complete.
   Publish the resident coarse safety net before requesting comparison work. Reuse a
   matching pinned Fine reference when available. Full paired generation remains an
   independent validation path: measured Jura preload exceeds its budget when enabled
   per runtime cell. WI 2319 owns generator-derived bounds for fast initial selection.
   Never mutate RequestedDetail on a running bake. Separate captured source identity
   from explicit detail requests; cancellation revokes all phases and scoped views.
3. Each SurfaceProof slice has a primitive-work cap and observes stop/source generation.
   Scratch belongs to the task and participates in CPU residency accounting. Task moves
   transfer ownership and revoke source progress (2313); no worker reads released inputs. Start with
   2313's limits; measure native cell sizes, completion rate, query work and bytes first.
   2313 now covers dense cells with incremental native hierarchies and nearest-witness
   queries. Do not rebuild indices per slice or couple one tiny slice to one frame.
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
- Render all fourteen Places without vegetation through outshine-client and open their PNGs;
  the static Hockenheimring Place is included; laps remain deferred. Compare selection, silhouette, shadows,
  p50/p95/p99, CPU/GPU bytes and work/frame. CPU-only evidence cannot close this WI.

## Existing paired-worker path and remaining integration

StructureBuildTask implements Variant -> Reference -> SurfaceProof -> Complete with
private intermediate products. Output.Tile remains final completion; cancellation and
running moves preserve ownership. Work is bounded per post; exhaustion retains a complete
upper bound. Invalid, omitted or fallback geometry cannot qualify a smaller error.

Runtime proof requests remain disabled. Initial LOD must not generate a second complete
Fine scene to choose Shell. Use the existing paired path as an independent check of the
fast generator bounds from WI 2319. Reuse of resident Fine products requires exact
source/cell/frame identity and complete byte admission, including raw, meshes and scratch.

Still open: source revalidation through landing, resident error transfer, conservative
render transform/precision enclosure and visual acceptance. CPU intervals alone do not
lower runtime LOD error or establish the Place budget.
