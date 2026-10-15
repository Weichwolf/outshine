# Native wall metres through the building asset and UV1

Issue #335, parent #306, goal #189. This is the producer/cache/consumer input
for the selected masonry filter, not activation of the masonry material.
The rejected derivative reconstruction and explicit-coordinate comparison are
owned by PR #333 and `facade_metric.md`.

## Contract

`Raised::WallMaterialM` is empty when absent. When present it contains exactly
`2 * WallCorners.size()` finite floats, interleaved U/V in metres. U measures
distance along the source wall edge, starting at its first endpoint. V measures
height above `SeatM + FootM`. The same frame serves walls, gables and split
surface patches. Massed envelopes use their own simplified edge. No globally
continuous perimeter phase across different edges or curved facets is claimed.

Recipe `WallVariant == 6` supplies these coordinates, except glazing. Explicit
source colours retain priority. Negative roof/trim/plinth codes and earlier
non-masonry vertices receive defined unused zero pairs if an asset also contains
masonry. Assets containing no masonry keep an empty stream. Roof vertices keep
their existing layout. The weld key includes both metric values; rollback,
used/capacity accounting and the structure digest include the additional stream.
`StoredVertex` remains 24 B. Building producer version is 19; structure artifact
schema is 10. Older schema hits are rejected and rebuilt through the existing
cache path.

The agreed generic `PieceMesh::Uv1` is an optional `span<const float>`:
empty means absent; otherwise UV0/Textured plus exactly two finite values per
vertex are required. `SceneResources` owns a deep copy and restores it with the
other piece sources. `SubjectDraw` selects the existing UV1 stream/layout by
actual data presence, even without an image using UV1. Generic authored UV1
retain their own meaning. Only the native producer defines its coordinates as
metres. There is no temporary Float2 adapter copy or new SDL GPU operation.
Performance owns the subsequent native renderer port in #328.

## Reproduction

Build the regular production archives with the repository toolchain and link
`test/harness/render/NativeWallMetresProbe.cpp`, `NativeFacadeProbe.cpp` and
`PieceUv1Probe.cpp` with the same source/header revision. The first probe needs
the building/generator includes, shared Check harness, liboutshine, SFCGAL and
GEOS. The piece probe uses the normal render link closure, SDL3 and the built
production shaders. It runs CPU validation by default; `--gpu` additionally
checks actual residency and source restoration on the existing Vulkan path.
Use the existing native GPU diagnostic environment for validation/readback.

```sh
python test/experiments/render/wall_metres_raster.py \
  --source-binary /tmp/outshine-material-wall-metres/NativeWallMetresProbe \
  --raster-binary /tmp/outshine-material-wall-metres/NativeFacadeProbe \
  --output /tmp/outshine-material-wall-metres/raster
```

The raster script compiles the actual production `litVertex.glsl` with
`LIT_UVS=2`. Cached native wall vertices and their real UV1 tail pass through
location 6 alongside the unchanged packed UV0 metadata. The diagnostic fragment
compares them to an independent source-edge projection of the actual rendered
position. It checks two tile anchors, two crops, orthographic/perspective views
and camera shifts, requiring full coverage and maximum error below 0.2 mm.
The local comparison PDF must also be opened optically. This is an offscreen
coordinate test, not a lit Place or a device frame-budget measurement.

`NativeWallMetresProbe` checks real native generation at Fine/Shell/Massed/
Skyline, actual gables and split-patch phase, explicit source colour, append and
rollback, whole-buffer and streaming cache roundtrips, finite/complete data,
resident caps and checksum-valid old-schema rejection. Its declared geographic
fixture is synthetic, not a claim about an acquired OSM building.
`PieceUv1Probe --gpu` overwrites the external source array after placement,
reads back the original values, destroys the original scene and restores the
owned copy. A neighbouring legacy piece retains its UV0-only layout.

## Costs and limits

Logical coordinate payload and encoded growth are exactly 8 B per covered wall
vertex. The measured fixture uses 2,240 Fine wall vertices (17,920 B used,
32,768 B capacity), 52 Shell vertices (416/512 B), and 20 Massed/Skyline vertices
(160/256 B). The shared scratch corner key grows from 16 to 24 B, including
non-masonry keys. Vector capacity, owned source copies and residency must be
counted separately; logical payload is not measured VRAM or process peak.
The three-vertex piece copies 24 additional source bytes and allocates a 24 B
UV1 buffer in the measured mixed-layout case. Actual run commands, terminal
receipts, native pixel errors, regression counts and lint attribution are in
the delivery PR rather than duplicated here.

Masonry shading, relief/aging, curved-edge seam appearance, integrated Places,
Metal execution, A18 Pro memory and frame budgets remain with #306/#328 and the
main integration. No thresholds or reference images are regenerated.
