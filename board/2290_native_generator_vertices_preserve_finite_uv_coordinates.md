Type: defect
State: active
Architecture: ready
Parent: 2289
Depends:
Priority: P0
Area: base, generators, render
Tags: geometry, uv, facade, material

# Native generator vertices preserve finite UV coordinates

## Evidence and contract

`StoredVertex::Of` divides UVs by four and packs them as signed normalized
16-bit values; `PackedPair` clamps at [-1,1]. Thus any generator UV outside
[-4,4] becomes exactly ±4. `BuildingMesh` writes style/bay/floor codes up to
thousands through this type, then `TilePieces` would upload the clamped result.
The limitation also breaks ordinary repeated or transformed native texture
coordinates. It is not a valid engine-wide geometry contract.

`base/spatial/StoredVertex` keeps position as float3 and octahedral normal as
one 32-bit word, but stores UV as two finite float32 coordinates. The native
vertex becomes 24 bytes; this is an explicit CPU/storage trade for correct
UV values, not a second geometry model. All consumers use the typed `uv()`
accessor. Building corner welding hashes both exact float bit patterns, so
distinct facade coordinates do not collapse to one corner. The CPU-to-GPU
piece stream continues to upload float2 UVs. Reject nonfinite generator UVs
at the responsible generator/geometry boundary; do not silently clamp them.

## Acceptance

- Exact finite UV round-trip including negative, repeated, >4 and values
  representing facade style and floors; normal precision remains bounded.
- Two building corners with the same position/normal but different second UV
  components remain distinct. Mesh digest and cluster stride include all
  24 bytes deterministically; input order remains stable.
- Native building and piece suites pass. Compare Hockenheim 88-s PNG and
  generated triangle counts before/after; image may remain blank until 2289
  selects a procedural facade material. Report memory and frame cost.
- `make format`, focused tests and `make lint` pass.

## Measured step

`StoredVertex` now holds float2 UVs and remains 24 bytes; normals retain
the octahedral word. The building key compares both float bit patterns.
The generator suite's ten cases pass after updating the seam-key test; the
direct glTF client-render suite passes. Warm/offline Hockenheim at 88 s keeps
27,050 building triangles, 538 cache hits, no remote starts and an exactly
equal 1280×720 PNG. Streamed Piece CPU payload capacity rises from
16,952,892 to 18,387,272 bytes (+1,434,380); GPU piece capacity remains
67,108,864 bytes because wall UVs are not uploaded yet. A single 100-s run
has p50/p95/p99 1.925/7.922/12.759 ms and 14/6000 late frames, versus
1.953/8.074/12.738 ms and 16/6000 before. Peak process heap varies from
548.914 to 577.229 MiB, not attributed solely to these 1.4 MB of vertices.
The final PNG is pixel-identical. WI 2289 must connect UVs to a procedural
material before this representation repair can improve the image.
