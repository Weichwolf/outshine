Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: world, ground, streaming
Tags: ownership, candidate, regression

# A ground snapshot releases pending reservations behind the watermark

## Problem and evidence

A Feldkirch local-sight client capture aborts in
`BuildingField::SnapshotAccepted`: `copy.Taken_ != copy.Accepted_`.
`TileWatermark::Advance` removes every reserved tile from `Ahead_` when the
scan passes it, even if its structure bake has not yet been accepted. The
snapshot's `ReleaseUnaccepted` only scans `Ahead_`, so it cannot release such
reservations. The source field remains valid; the copied candidate does not.

## Contract

`BuildingField` owns accepted tile IDs and immutable products. `TileWatermark`
owns scan/admission state. On a candidate snapshot, retain exactly accepted
and explicitly skipped tile IDs, clear every other reservation, and rewind
the scan if any reservation was removed. Replaying admission from the source
features must skip accepted/skipped IDs and retry every pending tile. Leave
the source watermark and reservations untouched. `Takes()` must count only
accepted tiles in the copy. No assertion removal or forced readiness.

## Acceptance

- Reserve two tiles, advance the scan past both before accepting either,
  accept one out of order, then snapshot. Source still owns both; the copy
  owns one and offers the unaccepted tile to `Next`. It reports incomplete
  coverage until that tile has a product. This must fail on the old code.
- Existing BuildingField/TileWatermark cases and Feldkirch local-sight client
  capture pass. Run `make format`, focused suites and `make lint`.
