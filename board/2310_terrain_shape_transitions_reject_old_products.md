Type: defect
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: world, streaming
Tags: terrain, ownership, revisions

# Terrain shape transitions reject old products

## Evidence and existing capability

TilePool::RunJob calls TerrainTiles::Shapes only for a nonempty Kind.
After shaped terrain is disabled, a reused worker can still generate the old shape.
TerrainTiles clears its stitched cache on shape change; GroundStream has another
stitched cache, and queued/done jobs currently have no shape-revision contract.
GroundStream exposes usable partial heights; missing boundaries remain unqualified.

## Ownership and implementation

- TilePool owns the current declaration and a terrain-scope revision. A shape
  transition includes empty Kind; identical declarations must not invalidate.
- RunJob always installs the complete declaration, including empty Kind.
  Compare all numeric fields and seed; geometry-relevant changes advance revision.
- Field/mesh jobs capture their scope at admission. A stale completion may release
  its own reservation but cannot replace or erase a newer job with the same key.
  Stale done products are not returned as Ready. Retry uses the new declaration.
- GroundStream caches attach the terrain scope and invalidate before lookup after
  a transition. Sampled coarse/fine tiles obey the same scope. A borrowed old field
  remains immutable; its validity for future publication is explicitly revoked.
- Owners/files: world/ground/TilePool.h/.cpp, TerrainLoader.h/.cpp and
  tiles/TerrainTiles.h/.cpp. Keep algorithms out of engine orchestration.
  No format types or new public API. Unknown source remains missing, never zero.
- No unbounded waiting, stale-work drain on the frame thread or global revision.
  Preserve cancellation, bounded queues and independent raw provider bytes.

## Current implementation

Worker declarations include empty Kind. Terrain-scope changes invalidate query
stitched/sampled caches and building bake revisions. Admissions own unique IDs;
old results/dependants cannot release replacements. Held meshes validate scope/owner;
LRU eviction releases only its completed product. Physical waiting work is bounded.
Controlled IO barriers cover Field/Mesh and same-scope cancel/repost. Removing
release-owner or cached-scope validation fails the independent fixture; both restored.
24 focused regressions and full lint pass: 251 tidy units, zero findings.
Public documentation/repository guards pass. GPU transition render remains unverified
in the prior sandbox; this WI stays active until that visual integration evidence exists.

## Falsifiable acceptance

- CPU fixture: provider -> shape A -> shape B -> empty Kind on the same worker;
  check analytical heights and source origin/revision for each transition.
- Equal declaration preserves a resident product; altered amplitude/seed/focus
  invalidates it. Raw provider data is reused when returning to empty Kind.
- Repeat through GroundStream, including sampled tiles and stitched cache hits.
  Keep an old shared field: bytes remain unchanged, current lookup yields new scope.
- Deterministically pause an old job, switch shape, admit a new job, release old:
  old completion cannot publish, drop the new reservation or unblock Ready falsely.
  Exercise both Field and Mesh; no timing-dependent sleeps as race specification.
- Negative controls: restore nonempty-only update and remove stale-scope check;
  the corresponding independent tests fail. No GPU needed for ownership tests.
- make format; focused TerrainTiles/GroundStream/TilePool suites;
  SDKROOT="$(xcrun --show-sdk-path)" LINT_JOBS=2 make lint.
  Render the declared terrain transition through outshine-client when GPU access works.
