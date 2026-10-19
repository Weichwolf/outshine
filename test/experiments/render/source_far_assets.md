# Source-owned hull / layered-field inputs (#12)

Active follow-up to [master's representation priority](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6259935181).
This delivers original source-owned controls and the first unlit layered-field
baker. The full novel-view/appearance comparison and native integration remain
open. No image-based product receives a larger guessed `SurfaceError` or opaque
coverage permission.

## Original source and native controls

The first controlled source is original ordinal 1 of cohort bbd9aab74311…,
anonymous source ID `(kind=0,id=0)`. Its two generated parts remain intact.
`subset_native_source_input` copies its original record, summary and complete
first-in-cell form bytes. Coordinates and source identity are retained. The
private local ordinal 0 is explicitly mapped to the original cohort/ordinal;
anonymous IDs are never used to deduplicate sources. Unsupported hip-backed or
non-first-in-cell subsets are rejected by this initial bounded adapter.

The native generator/probe is unchanged, with binary SHA256
`60679166753fd2cc6856524329a1eba13f59b61fa6cfdb13b63d5f5a0a6d9a33`.
Only private input copies change. Generation error 0 requests Fine with recessed
openings; generation error 512 selects native Shell. These are **asset-generation
controls**, not loosened runtime profiles or a world-LOD admission. Actual view
comparisons retain 1280×720 and the original 1044.33155 px focal length.

`gdb_read_source_mesh` reads registers/memory from that frozen x86-64/libc++ binary
at `BuildingMesh::Mesh` entry/return. It makes no inferior calls, memory writes,
patched branches or C++ changes. Empty append targets are required. All six
vector boundaries are checked against independent native JSON counts and bytes;
the original whole and cell invocations must produce identical stream hashes.
Both native/gdb runs terminate successfully, and their non-timing JSON matches.
This is a one-source control, not a complete native Source/Part/Face range export
for normal, projected and massed multi-source paths (#12/6259673828).

| Existing native variant | Triangles | Raw `Raised::UsedBytes` |
| --- | ---: | ---: |
| Fine, recessed openings | 2,518 | 226,248 B |
| Native Shell, no recessed openings | 44 | 3,440 B |

All native positions, packed normals, UV0, optional wall metres and colour
factors are available, without inferred material fields or source ownership.
The table is neither a GPU-residency measurement nor final cache-artifact size.
One source, even with these large reductions, is not a worldwide ROI result.

## Layered proposal and current decision

SIGGRAPH-first sources: [Layered Depth Images (1998)](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
and [Billboard Clouds (2003)](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
pages 1–2 including diagrams visually inspected. The prototype captures six
object-space axis directions through the same original Fine geometry. Each ray
retains up to four front-facing hits, sorted by depth. Its normal word, material
program, smooth UV fields, colour-palette index and wall/roof domain stay unlit.
Native material evaluation and dynamic world lighting are not baked into RGB.

Every stored hit retains its original triangle and nonnegative dyadic barycentric
weights. Float32 depth error and displacement transverse to the capture ray are
recorded separately. For this source the maximum measured transverse residual
is 1.714e-7 m, and depth rounding stays below 7.63e-6 m. These bind **stored
samples** to source geometry; they do not prove coverage between samples or from
a new observer. Layer overflow is explicit and tested. Original source data stay
available for every unsupported/unknown domain.

At 64×64×4 in six directions, 27,043 hits are stored from 24,576 rays; no sampled
ray overflows. Dense runtime fields alone occupy 2,064,384 B. A sparse estimate
with fields, per-ray offsets and colour palette is 666,239 B, excluding layout,
overflow/fallback/state overhead. Both exceed this source's original Fine data,
and greatly exceed native Shell. No native layered product is recommended from
this initial byte result. Capture takes about 0.20 s in Python; it is not a
device/runtime frame measurement. Work inside the R-tree/library remains separate.

The field sheet was opened: native normal/material-code fields and two diagnostic
relightings of colour factors. These are not native PBR appearance results.
The initial eight identical Fine/Shell source-orbit views include 32 m translation;
all are nonempty. Depth/difference images were opened. They contain zero lost and
one added covered pixel sample, so conservative opaque coverage is not claimed.

## Novel views: this first layered proposal is rejected

`source_far_reprojection` queries stored fields without looking up source
triangles. Each sample defines a plane through its stored depth with its stored
normal, over its capture cell. A bounded grid traversal visits intersected cells
and tests the stored layers. The proposal crops these rectangles to the original
capture bounds, with a numeric depth/Oct16 quantisation guard; this crop is **not**
a form-error or coverage certificate. Perpendicular/grazing unsupported planes
remain absent. Original Fine remains the fallback for the unvalidated product.

Without that crop, grazing wall samples produce huge extrapolated strips. The
actual opened failure image and frozen pre-crop scripts remain in temporary
evidence. That run completed four views before its bounds crossed the near plane
and it exited 1. The final cropped run completes all 24 views with exit 0.

The 24 identical 1280×720/original-focal views combine six orbit angles
(0/44/46/90/180/270 degrees), two distances (4/8 source extents) and 0/32 m
translation. Fine/Shell use actual front-facing triangle intersections at the
same pixel-centre rays. Layered queries compare the closest capture axis with
the union of compatible positive-facing axes. Light A/B changes at query time;
only colour factors and Lambert are evaluated. These are **not native PBR or
procedural facade/UV images**. Source-shader material-program disagreements and
normal differences are recorded separately. The 44/46-degree images expose the
closest-axis switch; they and a translated view were actually opened.

Counts below sum pixel samples over those 24 views; they are not unique world
points or worldwide coverage. Fine covers 292,389 samples.

| Candidate | Lost samples | Added samples | Changed material-program samples |
| --- | ---: | ---: | ---: |
| Original native Shell | 0 | 1 | 3,794 |
| Layered closest axis | 89,896 | 6,671 | 28,945 |
| Layered compatible axes | 1,772 | 10,379 | 12,109 |

The single-axis proposal loses roofs; combining axes restores many samples but
adds surfaces and still loses coverage. It is rejected for this source. This
does not reject all layered/cluster products or establish worldwide Shell ROI.
The stored source-bound samples did not imply source-bound extended rectangles.

Compatible-axis queries visit 23,335,448 ray/cell pairs and read 93,341,792
layer-depth operands, testing 23,949,202 valid planes. Depth/normal operand bytes
sum to 469,163,976 B, **not** measured cache/memory traffic or a complete byte
ledger. Texture material fetches, normal-array preparation, allocation counts and
library-internal reference costs are not included in that operand estimate.
Whole Python-process peak RSS is about 327 MiB; it is not GPU residency. No claim
of two cheap runtime triangles is made for this traversal. An independent native
source roof-patch control against triangle intersections matched 249 hits from
500 rays, with zero coverage disagreements and maximum depth error 5.7e-14 m.

Next work needs more sources/geographic controls and a more appropriate grouped
far representation, rather than sweeping this rejected per-source LDI. Continuous
near/far transitions, native material evaluation and native device costs remain
unproved. The bounded diagnostic policy retains Fine for every unvalidated case.

## Reproduction

Use `.venv` with NumPy, Trimesh, Pillow and additional `rtree==1.4.1`.
Existing original native inputs and the frozen binary are required. No build,
source download, cache write or new native feature PR is involved.

```sh
.venv/bin/python test/experiments/render/subset_native_source_input.py \
  --input "$ORIGINAL_INPUT/input.json" --source 1 --error-px 0 --output "$TMP_OUTPUT/fine"
# Repeat with --error-px 512 and a fresh output to obtain the Shell control.
# Run the unchanged native exporter on each private input; read streams through
# gdb_read_source_mesh.py at main with OUTSHINE_SOURCE_MESH_OUTPUT,
# OUTSHINE_SOURCE_NATIVE_JSON, OUTSHINE_SOURCE_EXPECTED_DETAIL (0/1),
# OUTSHINE_SOURCE_EXPECTED_OPENINGS (1/0). Use the original runtime-library env.
.venv/bin/python -m unittest discover -s test/experiments/render -p test_source_far_assets.py
nice -n 10 .venv/bin/python test/experiments/render/source_far_assets.py \
  --input "$TMP_OUTPUT/fine" --hull "$TMP_OUTPUT/shell" \
  --output "$TMP_OUTPUT/capture" --resolution 64 --layers 4
nice -n 10 .venv/bin/python test/experiments/render/plot_source_far_fields.py \
  --capture "$TMP_OUTPUT/capture" --fine "$TMP_OUTPUT/fine" \
  --hull "$TMP_OUTPUT/shell" --output "$TMP_OUTPUT/images"
nice -n 10 .venv/bin/python -W error test/experiments/render/compare_source_far_views.py \
  --capture "$TMP_OUTPUT/capture" --fine "$TMP_OUTPUT/fine" \
  --hull "$TMP_OUTPUT/shell" --output "$TMP_OUTPUT/novel-views"
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p 'test_source_far*.py'
```
