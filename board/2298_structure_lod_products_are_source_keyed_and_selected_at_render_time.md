Type: defect
State: active
Architecture: ready
Parent: 2123
Depends:
Priority: P0
Area: generators, world, engine, render
Tags: buildings, lod, determinism, streaming, hockenheim

# Structure LOD products are source-keyed and selected at render time

## Problem and evidence

`StructureBake.cpp` uses `RawTile::Eye` to choose Fine, Shell or Massed before
building native geometry. Accepted tiles are reused within 64 m of their bake eye.
Hockenheim tile 24 had identical source, DEM and street inputs but 51 Fine
footprints after a jump versus 38 after motion. Eye revalidation avoids a
false Refined result, yet repeated whole-tile baking prevents stable streaming.

## Architecture contract

`generators/building` owns deterministic geometry alternatives keyed by OSM,
DEM, street and generator/material revision, spatial cell and explicit detail
level. The camera, focal length, frame timing and world anchor are never part
of a geometry product key. `world/ground/BuildingField` stores source-keyed
footprint semantics; its accepted input does not claim a camera-local mesh is
the only representation. Keep one source geometry model; detail levels are
derived products, not parallel importer or generator contracts.

Partition each vector tile into bounded spatial cells using stable tile-local
coordinates; assign each building by its footprint-bounds midpoint. Bounds
cover its entire footprint, including cross-cell overlap. Produce a resident
coarse product first; prepare Shell/Fine variants asynchronously on demand
from the same pinned source snapshot. Existing `Raised` and `ClusteredMesh`
remain native payloads. A cell/level product owns separate wall/roof handles;
replace only after both products and source revision validate. Old resident
geometry remains drawable until the replacement fence retires. Eviction may
drop Fine before the coarse safety net, never the only drawable level.

`render` selects exactly one resident level per visible cell from bounds,
conservative projected error and hysteresis. Selection changes index/handle,
not source identity or contact geometry. Shadows use a geometrically valid
level from the same published source revision. No renderer callback enters
the generator; streaming receives bounded detail requests as commands.
`Refined` means selected resident levels meet the declared pixel error for
the current view and no required source upgrade is pending. A coarse fallback
remains Playable and reports its error; it cannot falsely report Refined.
Keep per-frame admission, upload, GPU bytes and CPU scratch bounded.

## Verified foundation and remaining work

- `StructureMesher` obeys explicit detail without camera/world-anchor gates.
  `RawTile::RequestedDetail` bakes Fine/Shell/Massed independently of eye/focal;
  invalid or mid-bake detail changes reject. `StructureCellOf` assigns an 8x8
  tile-local cell, preserving full cross-cell/dateline bounds; an explicit cell
  request filters the bake and rejects invalid source/request IDs and mid-bake
  changes. The queue annotates raw structures; live jobs still bake whole tiles.
- `SceneResources` restores hidden instance rows after world publication.
  `TilePieces` stages multiple cell levels hidden; a complete source mask/revision
  triggers one atomic swap and retires all products of the previous revision.
  Missing cells, stale sources and failed uploads keep the legacy image visible.
  Live jobs still publish only legacy cell zero; cell scheduling remains open.
  `StructureSourceKey` covers vector, DEM set/raster, street, scale and fallback;
  candidate and live uploads retain it. Changed sources retire old variants
  only after successful upload; failure leaves the old image intact.
- `BuildingField::Footprint` now contains semantic data only. Render detail is
  retained in `BakedTile::FootprintDetails`; accepted inputs own the source cell
  mask. Camera-only changes leave the semantic revision stable.
- DEM source identities are a set: `StructureSourceKey` uses the existing
  sorted fast path and normalizes reversed/duplicate deliveries; accepted
  footprint inputs store the same canonical identity set. The raster digest
  still distinguishes changed sample values.
- Explicitly Refined Hockenheim 74.85-s still/motion captures have 46 visible
  structure tiles with identical source keys and no fallback heights. Only
  tile 24 differs in mesh digest; both runs mark its detail `automatic`. The
  opened PNGs differ in 120 pixels by more than 1/255, all around building
  edges (worst 112/255). The recorded source/DEM/street identity is the same;
  the different automatic bake is the next cause to isolate. Earlier tile-24
  bakes had 51/1337 Fine/Shell
  footprints after a jump versus 38/1350 after motion at eyes about 109 m
  apart. The 4,491-frame motion run has zero contact gaps and p99 13.22 ms.
- A 60-s lap remains roughly 3,393/3,600 frames unrefined with six ground
  candidate starts. Source ingestion and structure view detail are separate
  blockers. Source bakes survive eye/focal movement, but `Complete` still
  requires bake eyes within 64 m and restarts whole-tile refinement. Increasing
  that radius would keep the wrong camera-baked mesh longer. Stable cell/level
  residency and source publication remain P0; no readiness gain is proven.

## Implementation order

1. In `StructureBake`, `BuildingField`, `StructureBuildQueue`: retain the
   completed semantic/detail separation; give source qualification and view
   detail separate cursors and readiness results. Add stable cell/level product
   identity and source-keyed variant requests. Ground candidates wait for
   qualified semantic footprints, not a camera-local bake eye.
2. In `TilePieces`, `SceneResources`, `SubjectDraw`: publish variants with
   explicit cell/level ownership and retain coarse geometry during fine
   upload, retirement and failed replacement. Use existing cluster bounds;
   add measured simplification error rather than treating flat clusters as a
   finished hierarchy.
3. In renderer/engine readiness: choose resident level from projected error,
   hysteresis and budget. Request missing detail without blocking a frame.
   Remove the 64-m whole-tile rebake after this path passes the controls.

## Falsifiable acceptance

- A synthetic tile with near/far buildings generates identical source-keyed
  variants for two camera eyes and reversed source order; changing a source
  revision changes only affected cell products. Forced coarse violates a
  near-view pixel-error oracle; forced fine raises measured cost, not geometry
  identity. Reject a deliberately stale source before GPU publication.
- Jump, slow drive, rapid crossing and backtrack select legal variants with
  no hole, duplicate, roof/wall mismatch, stale shadow or unbounded queue.
  Contact and navigation products keep their IDs throughout.
- Fresh same-build Hockenheim 74.85 s static/paced final PNGs match after
  Refined/settle; opened decile frames have no giant silhouette change. Report
  p50/p95/p99, over-budget frames, peak CPU/GPU bytes, uploads and work per
  frame against the current trace. Run focused suites, `make format` and
  `LINT_JOBS=2 make lint`.
