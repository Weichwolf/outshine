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
- Prior opened Hockenheim still/motion PNGs differ at only 6/921600 pixels by 1/255;
  this selection-consistency baseline does not validate the new Shell geometry.
- Readiness must never prepare data. WI2311 proves bounded certificates, zero-allocation
  checks and validated activation. The raster budget stays 16 MiB; larger caches/pins
  regressed costs. Prior motion: 3.33/8.63/12.44 ms, 688 MiB versus 2.48/8.38/12.39 ms,
  592 MiB. New Hockenheim PNG/performance integration remains unverified.
- Shell now reuses native footprint/roof/foundation surfaces and omits secondary
  details. Eight BuildingMesh/StructureBake cases and full lint pass. The independent
  roof-profile test fails before the fix; explicit box/flat controls and sloped-ground
  contact pass. Geometry counts: pitched 12x16 m: Fine 84 -> Shell 72 triangles;
  flat 24x30 m: 88 -> 76. These are geometry counts, not a frame-time claim.
  Fresh render PNGs and certified approximation error remain open.

## Implementation order

1. 2310 fixes shape transitions; 2311 owns bounded source-dependency certificates;
   invalidate on new provider data, shape change or an arriving neighbour.
   `Forever` bytes can evict and partial stitches exist: without a valid
   certificate, fully resolve. Missing seams no longer enter stitched caches;
   Late-neighbour oracle: two failures before fix; focused tests/full lint pass.
   Copies/shares/resampling/GroundBlock retain missing boundaries; partial heights
   cannot qualify Fine. Nine tests/full lint pass; baseline failed three checks.
2. Shell now reuses ring/roof/foundation surfaces rather than boxing every part.
   It drops door recesses,
   roof plant/chimney details; Massed stays boxed. BuildingMesh owns the change and
   keeps native payloads and footprint semantics unchanged. WI2312 owns certified
   displacement per variant; bound both surface directions, including roof/overhang,
   foundation/contact and filled concavities. Vertex samples cannot certify. Keep the
   whole-cell bound until proof exists. Independent concave/roof/contact fixtures and
   forced-coarse controls precede tighter projected selection and hysteresis;
   source/variant keys and logical collision/navigation contracts remain unchanged.
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
