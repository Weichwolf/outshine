# Metre coordinates for native facade materials

Owner: [#306](https://github.com/Weichwolf/outshine/issues/306), goal #189.
This comparison chooses an explicit producer coordinate as the next integration
input. It changes no product shader, asset/cache format, PieceMesh or renderer.
The masonry filter and its cost comparison remain in #315 / `masonry_filter.md`.

## Native raster alternative

The existing lit vertex is used unchanged after #327. A diagnostic fragment
tests whether `bay / length(surfaceGradient(bay))` and the analogous storey
expression can supply physical metres without another vertex stream. Its
reference projects the actual metre-valued vertex position onto the declared
wall axes. This reference does not use a UV derivative or inferred scale.

The existing 160×96 native facade consumer renders six views, orthographic and
perspective, for both inferred and explicit coordinates: 24 real GPU captures,
840 native checks, Vulkan synchronization validation, RTX3060. The fixtures
reuse the actual packed material encoding, native tangent frame and independent
projected-quad/coverage guards from #317. Endpoint bays are integers. The two
methods use identical positions, transforms, viewport and physical reference.

The explicit alternative temporarily uses the existing Float2 UV0 input for
metres, with the same production vertex and ordinary interpolation. This is a
controlled interpolation comparison. It does **not** demonstrate simultaneous
packed facade metadata plus UV1 or an implemented Piece-/cache-/stage path.

| Case | Inferred maximum U/V error | Explicit maximum U/V error |
|---|---|---|
| Near perspective | 0.516 mm / 96.554 mm | 0.0011 mm / 0.0058 mm |
| Far perspective | 5.829 mm / 201.355 mm | 0.0306 mm / 0.1832 mm |
| Bay 255, near perspective | 33.090 m / 40.921 m | 0.1221 mm / 0.1221 mm |

All explicit cases satisfy the 2e-4 m comparison bound, 2% of the
recipe's 10 mm mortar width. Camera-shift and tile-anchor cases also pass.
The inferred alternative fails even in the near one-bay case: finite float
differences perturb the estimated scale, which then multiplies the absolute
coordinate. This rejects this particular reconstruction; no universal lower
bound on all implicit mapping methods is claimed. Gables with constant bay
coordinates also cannot recover a horizontal metric from that derivative.

`plot_facade_metric.py` plots the actual captured coordinate errors and height
profiles into a local PDF. It is optically inspected, not a lit masonry image.

## Actual native producer

`NativeFacadeSource.cpp` calls the real BuildingSurface preparation, Trace and
VertexAt, and BuildingMesh at all four native representation rungs. Four
declared rectangular generator fixtures cover several geographic frames,
including a long facade; they are not captured Place/OSM features.

For each source hit, the proposed U is distance along the original source edge;
V is height above the shape seat and foot. Coordinates are formed in the source
frame before float storage. The Python replay independently checks U using
Euclidean distance from the original edge endpoint and V using source height.
Both the queried local point and source endpoints are retained in the raw log.

116 source hits, including 20 actual gable hits, and 32 actual meshes pass 276
native checks. Source-coordinate error is at most 0.0531 mm; stored coordinates
are identical under the tested 500 m change of tile anchor. Gable bay U is zero
while the proposed metric U remains meaningful. This does not yet prove an
emitted coordinate stream across mesh welding, cache roundtrips or LOD switches.

Potential additional Float2 storage is eight bytes per wall vertex. Actual Fine
mesh counts project 12,096 / 328,064 / 12,928 / 12,928 additional bytes for these
four fixtures. These are calculated stream sizes, not measured new allocations,
physical GPU traffic or device peaks. Their current complete mesh UsedBytes are
46,704 / 1,250,448 / 49,872 / 49,872 bytes, including roofs and indices. Optional
per-piece streams must cover the piece's vertices, including unused material
classes; a brown wall does not imply only its vertices incur residency costs.

## Reproduction and remaining contract

Build `test/harness/render/NativeFacadeSource.cpp` with the normal project C++
flags and generator includes against a matching regular production archive;
link GEOS and SFCGAL >=2.3. It has no graphics/backend dependency. Then run:

```sh
python3 test/experiments/render/facade_metric_source.py \
  --binary /tmp/native_source --output /tmp/outshine-material-metric-source
PYTHONPATH=lib/src python3 test/experiments/render/facade_metric_raster.py \
  --binary /tmp/native_facade --output /tmp/outshine-material-metric-raster
PYTHONPATH=lib/src python3 test/experiments/render/plot_facade_metric.py \
  --input /tmp/outshine-material-metric-raster
```

GLSLANG, SPIRV_CROSS and Vulkan validation/driver paths follow the existing
facade raster workflow. Original inputs, outputs, commands, archive hashes and
terminal status stay in the system-temp experiment directories named in the PR.
Concurrent host work precludes an isolated timing claim. No new texture or pass
has been introduced. Existing consumer proofs are reused; the new C++ source
consumer receives formatting and focused clang-tidy checks.

The concrete next contract is an optional explicit material-metre stream from
the native generator/cache through PieceMesh, using existing UV1/location6 for
procedural facades. Generic Geometry already supports UV1. Performance owns
the shared renderer port #328 and is explicitly addressed under #306; no
parallel Stage implementation or agreement inferred from silence. Producer
welding, gable continuity, source-colour priority, actual UV1 input, cache and
LOD/camera raster checks precede activating #315. Master owns lit Places,
Metal/A18, integrated cost checks and picture publication.
