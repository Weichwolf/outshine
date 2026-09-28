Type: defect
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: world, streaming
Tags: terrain, ownership, revisions

# Terrain shape transitions reject old products

## Problem and implemented contract

RunJob previously installed Shapes only for nonempty Kind; a reused worker could
retain old shaping after disable. Stitched caches and queued/done jobs lacked a
shared shape-scope contract. TilePool now owns the declaration and terrain scope.
Every job installs the COMPLETE declaration, including empty Kind; identical numeric
fields/seed do not invalidate. Shape changes invalidate stitched/sampled query caches.

Admissions have unique ownership IDs and captured scope. A stale completion may
release only its reservation, cannot erase a replacement or report Ready. Held
meshes validate scope/owner; LRU eviction releases only its completed product.
Old borrowed bytes remain immutable while future publication eligibility is revoked.
Physical pending work stays bounded; cancellation cannot drain arbitrary work on frame.
Partial stitched boundaries stay usable but never qualify Fine or enter complete caches.

## Evidence and remaining work

At 5de351d40: terrain/ownership suites are part of 45 focused PASS; full lint passes
255/255 tidy units, zero findings. Independent controlled IO barriers cover Field/Mesh,
same-scope cancel/repost, A -> B -> empty declarations, cache reuse and old handles.
Historical owner/scope mutations cause actual FAIL; tests and history preserve them.
Remaining: render shape transitions through the public client/API with new-scope
publication and retained old view until replacement. No current GPU acceptance.

## Owners, errors and acceptance

- world/ground/{TilePool,TerrainLoader,tiles/TerrainTiles} own state/cache algorithms.
  Engine consumes source scope; it does not compute terrain or invent global epochs.
  Unknown/refused provider input stays missing. Raw provider bytes are independently reusable.
- Alter amplitude/seed/focus: invalidate; repeat declaration: retain. Return to empty:
  preserve raw-source origin and analytical height. Exercise stitched/sampled cache hits.
- Pause an old job, change scope, admit replacement, release old. Neither Field nor Mesh
  may publish stale data or release the replacement. Timing sleeps are not race oracles.
- Restore nonempty-only updates or remove owned-release/scope guards: independent tests FAIL.
- make format; focused TilePool/TerrainLoader/HeightField/TerrainTiles suites; full lint.
  Run declared terrain transitions via outshine-client, open PNGs and report source/scope,
  holes, streaming work and frame costs. 2311 owns raster-free source certificates.
