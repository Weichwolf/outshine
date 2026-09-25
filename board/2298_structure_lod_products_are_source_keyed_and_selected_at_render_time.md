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
  changes. Live scheduling posts bounded pinned cell variants, stages them
  hidden and activates only complete source selections.
- `SceneResources` restores hidden instance rows after world publication.
  `TilePieces` stages multiple cell levels hidden; a complete source mask/revision
  swaps atomically with whole tiles in both directions and retires old products.
  Resident queries support planning; same-source fallback refresh retains hidden cells.
  Missing cells and failed uploads keep the legacy image visible.
  `StructureSourceKey` covers vector, DEM set/raster, street, scale and fallback;
  cell landings recheck current DEM and street inputs before publication.
  Changed sources retire old variants only after successful upload.
- `BuildingField::Footprint` contains semantic data only. Render detail stays
  in `BakedTile::FootprintDetails`; whole-tile landings carry source cell masks,
  full envelopes and maximum heights into acceptance; camera-only changes leave the semantic revision stable.
- DEM source identities are a set: `StructureSourceKey` uses the existing
  sorted fast path and normalizes reversed/duplicate deliveries; accepted
  footprint inputs store the same canonical identity set. The raster digest
  still distinguishes changed sample values.
- Fresh Hockenheim 74.85-s still/motion `refined` PNGs were opened: before
  source-keyed cell selection, 120 building-edge pixels differed by >1/255
  (worst 112); after it, only 6/921600 differ, each by 1/255. An eye-near
  legacy tile no longer certifies `refined`; contact remains gap-free.
- A false `refined` gate caused 120-s timeout and >84000 repeated DEM field
  jobs: readiness must not prepare data. Posting and activation validate live
  source; readiness compares resident revisions. Eight-request tile bursts and
  a single <=2-MiB height pin with stale-landing eviction cut Hockenheim motion
  field jobs 16361 -> 6097 and still `refined` time 9.57 -> 8.22 s at the same
  16-MiB stitched-field budget. The opened still/motion PNGs differ by at most
  1/255 with zero contact gaps. Motion p50/p95/p99 is 3.33/8.63/12.44 ms,
  peak heap 688 MiB: both p50 and bytes regress against 2.48/8.38/12.39 ms
  and 592 MiB. All 4491 paced frames remain unsettled. A 32/64-MiB cache
  reduced jobs further but worsened memory and p50; global pinned heights
  caused stale-source churn and was rejected. Resident identity skips DEM rebuild
  on hits; misses run the full check. Static refined 8.22 -> 6.41 s; motion
  p50/p95/p99 2.32/9.06/11.76 ms, 670 MiB, 6874 jobs (was 6097), 4491 unsettled
  frames, zero gaps, identical PNG. Eviction revision and tighter bounds remain open.

## Implementation order

1. Give `GroundStream`/`TilePool` a cheap revision also valid after cache eviction.
   Resident identity already avoids live DEM rebuild on hits; misses still rebuild.
   Keep stale-source rejection and bound pinned bytes. Measure cold/warm
   startup, moving-camera p50/p95/p99, jobs/frame and memory on target hardware.
2. Store certified simplification displacement per variant. Select with
   projected error and hysteresis; replace the whole-cell bound only after
   forced-coarse negative controls prove the tighter criterion.
3. Admit visible cell detail by bounded priority. Keep the coarse resident
   safety net while Fine streams; reject stale source/failed upload without
   replaying legacy camera-local whole-tile geometry.

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
