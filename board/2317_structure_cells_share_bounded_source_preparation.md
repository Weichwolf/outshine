Type: feature
State: active
Architecture: ready
Parent: 2311
Depends:
Priority: P0
Area: engine, streaming
Tags: ownership, terrain, bounded

# Structure cells share bounded source preparation

## Evidence and scope

4b54277f2 Graz: 975 cell landings, one queued cell, 1395 missing requested cells after
6144 frames. No stall: the diagnostic's empty queue counts whole tiles only. 126/128
observed HeightField pins exceed the existing 2 MiB cache threshold; mean synchronous
resolution is 8.41 ms in those observations, not a frame quantile or causal delta.
/tmp/outshine-structure-readiness-4b54277f2-Graz.log. PostsCell repeats BlocksUnder,
field assembly and digest work for the same immutable source. Raising cache/frame
limits is not the repair. Existing HeightField::Request preserves exact ordered requests;
TilePool::Field is synchronized and asynchronous. Terrain certificates survive eviction.

## Ownership and implementation

- StructureBuildQueue owns a bounded source-preparation batch for up to eight missing
  cells from one accepted tile/source, matching the existing eight-cell planner burst.
  StructureCellPlanner supplies the ordered missing requests; Advancing submits one batch.
  Keep the current four-candidate bake window and per-frame landing limits.
- Capture full accepted vector identity, street digest, span, generation, terrain scope,
  ordered HeightField request and cell/detail requests before posting. Hash equality is
  insufficient. Each bake keeps its own immutable RequestedCell/RequestedDetail.
- Heap-owned preparation state remains stable across running owner moves. A worker uses
  a bounded subset of existing immutable SourcedTerrainFields for published inputs;
  TilePool::Field handles missing-source reconstruction. Never Engine/GroundStack closures
  or borrowed mutable GroundStream/HeightSheets. Clear cancels and joins before pool/world
  destruction. No worker self-post loop and no blocking wait inside a worker.
- States: Preparing -> Ready -> Draining -> Finished; Deferred, Failed and Cancelled are
  explicit outcomes. One worker post inspects at most four requested fields; Pending or
  Deferred yields. Frame owner resumes after completion with 1/2/4/8/16-frame retry spacing
  (16 maximum), reset on actual progress; cancellation/source replacement revokes the batch.
- Preserve exact requested coordinates and order, qualification, boundary dependencies and
  source bytes. Capture only selected source/ancestor entries, not the whole-world snapshot.
  Reuse the existing ancestor-selection rule; build HeightField on the worker. Its key matches the
  complete captured receipt. Native field versus published-source resampling is an explicit
  equivalence gate, never silently accepted through matching addresses or ancestor IDs.
- Ready fields are shared by admitted bake tasks and unstarted demand in this batch.
  Do not retain a field after its batch and borrowers finish. No second persistent cache;
  remove the old cell pin from this path once batch integration passes its controls.
- Byte admission covers request metadata, preparation blocks, retained TerrainFields and
  shared HeightField once per owner. Initial preparation reservation: 8 MiB per batch,
  four batches maximum = 32 MiB; observed native fields peak about 4.23 MB. These are
  work-residency caps, not a cache allowance or an Engine-wide memory claim.
- Check metadata size before copying and field size before retaining; transferred shared
  terrain allocations remain counted in their existing owner and transfer ledger. Refuse
  oversized preparation without copying it. Account transient duplicate metadata too.
  OverBudget retains the coarse front and unmet quality; identical input must not retry
  every frame. Changing demand/source/budget can reopen it. No silent optimistic Refined.
- Landing revalidates vector/street/span/generation/scope and certificate. Unknown/Pending
  keeps valid coarse output and bounded retry. Stale content requests coherent ground
  replacement through GroundWorldCandidate; never install new building heights on old
  terrain. Source revalidation of already active fronts remains required by 2311.

## Integration boundary

Owners/files: engine/streaming/StructureBuildQueue, StructureBuildTask and a private
StructureSourcePreparation owner; StructureCellPlanner, engine/Advancing and existing
GroundWorldCandidate coordination. Ground::TilePool/HeightField keep their current data
ownership. Snapshot task bytes only after worker completion; frame never reads live vectors.
This step must remove synchronous source preparation from the runtime cell-post path.
Whole-tile bootstrap preparation is separately visible, not falsely claimed repaired.
2312's paired geometry/proof phases consume this source owner; no smaller LOD bounds here.

## Acceptance and negative controls

- Same-source multi-cell demand prepares once; independent sources never share. All eight
  cell results retain exact detail/cell/source identity, including partial completion.
- Existing pinned source versus worker reconstruction agrees on request, raster digest,
  source list, heights and terrain receipt in Graz/Wien and analytic ancestor/seam cases.
  If equivalence fails, diagnose source reconstruction before enabling the worker path.
- Pending/Absent/Refused, contention, cancellation, running moves, source/shape replacement,
  memory refusal and destruction preserve lifetime and bounded retry. Measure actual bytes.
- Restored per-cell preparation, changed-request sharing, stale landing, ignored cancellation,
  missing byte admission and a frame-thread resolver must each cause an actual test failure.
- make format; focused StructureSourcePreparation/StructureBuildQueue/StructureBuildTask,
  HeightField, BuildingField and GroundPublication; full lint/tidy/API.
- All ten Places through client, personally opened PNGs. Compare Graz/Wien preparation
  count/time, cell completion, p50/p95/p99 and CPU/GPU bytes on identical inputs. Retain
  current frame/timeout/quality limits. No claim of success from fewer calls alone.

## Evidence and implementation

4b54277f2 worker diagnostic: three Graz tiles/31 fields; native and published nodes agree
exactly, with equal raster sizes, source lists and digests. Both remain qualified. Wien had
no published candidate after 6144 frames; reconstruction equivalence there stays unmeasured.
Logs /tmp/outshine-source-equivalence-4b54277f2-{Graz,Wien}.log; Refined remained unmet.
a96116dc6: 24 focused PASS; full lint exit 2, 14 findings and one incomplete Tidy unit.
Direct includes/defaults and function boundaries corrected; complete new gate pending.

Runtime now submits up to eight missing cells together. One heap-stable preparation builds
four requested fields per worker post and shares its final HeightField with those bakes.
Exact immutable rasters are shared; ancestors are resampled. Capture, ordered requests,
block storage and copied source/certificate metadata enter admission before assembly.
Fourfold metadata reservation covers block copies, aggregate growth and transient old/new
aggregate storage; raster bytes count once per retained source plus sampled child nodes.
This is a conservative reservation for current containers, not an allocator-wide hard cap.
Full vector/request/source/shape/terrain receipt is rechecked before posting; revoked batches
cannot land. The new landing path never falls back to a synchronous height resolver.
Four preparation slots retain reservations until all borrowers finish. A 64-ticket bounded
refusal ledger spaces missing-source retries 1/2/4/8/16 frames and suppresses identical
oversized inputs while retained. No persistent field cache is added.

Format 1218 PASS, eleven focused tests PASS. Eight real mixed-detail cell products land from
one capture without frame copies; running moves, cancellation and byte refusal are covered.
Restored frame resolver, ignored admission/cancellation and stale-preparation mutants each
exit 1 again after refactoring. Logs /tmp/outshine-source-batch-tidy-*.log; full gate pending.
Missing published snapshots currently defer; TilePool reconstruction remains to integrate.
The old single-cell API/pin remains for its existing callers until this runtime gate passes.
Native fallback, full byte-ledger accounting, remaining negative controls and measured
Place improvement remain open. No reduced LOD bound or completed WI claimed.
