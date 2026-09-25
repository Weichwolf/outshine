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
building native geometry. `BuildingMesh.cpp` also tests distance to a fixed
world anchor. Accepted tiles are reused within 64 m of their bake eye. At the
same Hockenheim final pose/time after Refined and render settle, a jump and a
paced approach differ in 5,679/921,600 PNG pixels, concentrated in 43 horizon
rows (worst channel difference 118/255). Tile 24 had identical source, DEM
and street inputs but 51 Fine footprints after a jump versus 38 after motion.
The current eye-revalidation fix prevents a false Refined result; it also left
all 4,491 paced frames unsettled and still permits different valid bakes.
Repeated tile baking every 64 m cannot be the streaming LOD architecture.

## Architecture contract

`generators/building` owns deterministic geometry alternatives keyed by OSM,
DEM, street and generator/material revision, spatial cell and explicit detail
level. The camera, focal length, frame timing and world anchor are never part
of a geometry product key. `world/ground/BuildingField` stores source-keyed
footprint semantics; its accepted input does not claim a camera-local mesh is
the only representation. Keep one source geometry model; detail levels are
derived products, not parallel importer or generator contracts.

Partition each vector tile into bounded spatial cells using stable tile-local
coordinates and assign a building by a stable source key/centroid. Bounds
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

## Implementation order

First slice: make `StructureMesher::Mesh` obey only its explicit detail level.
Remove the hidden focal/world-anchor distance gate from `BuildingMesh` and
prove Fine remains detailed at a remote world anchor while Shell stays coarse.
This does not close camera-dependent selection in `StructureBake` or this WI.
The focused anchor/detail test passes. Hockenheim 74.85 s paced capture keeps
RGB 91/88/83; p99 is 17.16 ms with 51/4,491 over-budget frames and 520.1 MiB
peak heap. Static/paced PNGs still differ in 5,719/921,600 pixels (0.621%),
only near the horizon. The remaining selection is upstream of the mesher.
Second slice: `RawTile::RequestedDetail` produces explicit Fine/Shell/Massed
products whose digest is invariant under changed eye/focal input. A source
height change alters the digest; Skyline and mid-bake detail changes reject.
The engine queue still uses automatic eye selection and needs conversion.
Residency prerequisite: `SceneResources` now stores the default identity row
as visible state and reapplies empty rows after candidate GPU restoration.
A depth-buffer fixture hides a structure piece, publishes a copied world,
confirms it stays hidden, then reveals it through the same valid handle.
Without this, hidden coarse/fine alternatives could reappear together.

1. In `StructureBake`, `BuildingField`, `StructureBuildQueue`: split semantic
   footprint acceptance from camera-local detail choice. Add stable cell/level
   product identity and source-keyed variant requests. Remove eye/focal inputs
   from the product revision only after camera independence is proven.
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
