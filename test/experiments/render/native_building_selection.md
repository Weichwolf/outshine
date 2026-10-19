# Native selection across cache partitions

Refs #567/#15/#12. This diagnostic calls the unchanged native building producer
against complete original cached parents and their original selected views.
It supplies the bounds, shell-error and actual command inputs missing from the
Python resident-page comparison (`outshine_research`, `930392b72`). No product
algorithm, cache schema, render contract, source or error budget changes.

Analytic surface transfer for coverage is in `native_building_surface_inputs.md`.

## Input and observer contract

`export_native_building_selection_inputs.py` reads one SQLite transaction in
read-only mode. It copies the original index, all existing per-cell shape blocks
and the specified original geometry artifact carrying the view. Package CRC,
bounded decompression and exact package extents are checked; SHA256 checksums
remain in each manifest. It fetches and regenerates nothing. Missing inputs fail.

`export_building_selection.cpp` uses the product decoders and the original view's
eye/optional ECEF position and pixel-error projection. It requires automatic
detail selection. Every original shape is assigned exactly once in index order,
and the production `CompatiblePreparedSurface` check validates it against its
index header. Missing, duplicate or incompatible forms fail. The cluster size is
an explicit input; the current runtime assigns128 (`render/Shape.h`,
`StructureBuildQueue.cpp`). A mismatched setting is not a controlled comparison.

The original `BuildingMesh` supplies source bounds and shell errors. A delegating
mesher records actual `Mesh` and `Project` calls from `BakePreparedStructures`;
it does not imitate the selection algorithm. The observer records every original
source index, ID, kind and cell; unknown bounds/errors remain null. Projected
groups retain their original source indices, chosen detail and opening flags.
An ordinary aggregated mass has a null source pointer: do not assign it an
invented individual ID. Failed individual mesh calls retain the native reason.
Footprint detail lists follow the selected original source order.

Each complete parent is baked once as a whole and once per occupied cell, with
the same inputs and projection. Whole emission is a **diagnostic control only**:
it is never published and does not restore oversized whole cache products.
The unchanged64MiB cache-product limit remains binding. Whole scratch/output may
exceed that limit; those diagnostic allocations are not an admitted runtime path.

## Actual native counterexample

Intel i5-1135G7, native candidate82e7a69d7, original local cache. Three complete
parents match the research cohorts; this is not identity with the original
8,068-source Tokyo failure or master's new Metal cache population.

| Parent prefix | Sources / cells | Whole triangles | Cell triangles | Whole / cell projection calls |
|---|---:|---:|---:|---:|
| bbd9aab743 | 457 / 36 | 461,942 | 467,717 | 1 / 11 |
| 6d41e63e73 | 1,754 / 60 | 190,760 | 211,743 | 1 / 33 |
| 035e2150a3 | 4,141 / 61 | 10,478 | 34,638 | 1 / 48 |

These are actual fallback triangle counts, not GPU-selected triangles, draw calls
or frame times. Cluster index references are exactly three times these counts in
these cases; every stored DAG level must be interpreted separately in general.
The whole/cell source-level detail distributions are identical: Fine/Shell are
107/350,86/1,668,40/4,101. Unsupported counts are unchanged at1,7,40; their sources
are retained, and this experiment does not repair their geometry.

Individual mesh-call counts are1/214,7/261,40/180 for whole/cells. Thus the real
partition affects projection grouping and work even when source detail classes
stay equal. This does not isolate the16-source threshold from cross-cell
projection/visibility effects. Merely removing that threshold is not an accepted
fix. Research must compare a partition-independent plan before native adoption.

Bounds and shell errors are known for456,1,747,4,101 sources; unsupported unknowns
remain explicit. All original6,352 source records appear once in each whole and
combined-cell footprint metadata set. They are not all necessarily rendered.

All157 generated cell triangle counts independently match the original cached
artifacts. Six native negative controls reject missing/duplicate cells, misplaced
forms and corrupted index/view bytes, and preserve an existing output unchanged.

Observer JSON allocation, diagnostic prepasses, clustering and host contention
are included in this tool. Its wall times and `HeapBytes` are not isolated CPU
costs, allocation counts, process peaks, resident GPU bytes or a device budget.
No runtime image changed. The already checked native candidate's world images
remain the rendering control; this diagnostic makes no new visual quality claim.

## Reproduce and remaining work

Requires the existing cache, Python with system libzstd/pkg-config and the native
C++ build with nlohmann-json. No new package was required on the measured host.
Build the C++ executable using the complete current generator compilation flags
and declared native archives/libraries, compiling both `export_building_selection.cpp`
and `building_capture_inputs.cpp`. Actual archived-cell capture pairs are described
in `native_building_capture_inputs.md`. A shortened ad hoc compiler invocation
does not reproduce the checked configuration. Run focused clang-tidy against
that exact experiment entry in the compilation database, including its header.

```sh
python3 test/experiments/render/export_native_building_selection_inputs.py \
  --cache "$asset_db" --view-key "$existing_view_key" \
  --cluster-triangles 128 --output "$fresh_temp_inputs"
nice -n 10 "$native_selection_exporter" \
  "$fresh_temp_inputs/$parent/input.json" "$fresh_temp_result"
```

Inputs and outputs must be explicit fresh system-temp paths outside the checkout.
The native output must not already exist. Complete input manifests, native source
commit, compiler/link commands, terminal run/analysis receipts and original
reports live at `/tmp/outshine-engine-building-selection-evidence-82e7a69d7/` on
the measured host; GitHub #567 links the canonical delivery. These paths are
evidence locations, not product or fixed-test dependencies.

Next: independent Python selection/grouping comparison, source-complete bounded
native planning, then resident batching with complete material/render-state
compatibility and fence-safe lifetime. Changes, movement, cancellation, contact
publication and the original Tokyo overflow remain required counterexamples.
Actual world images, CPU/GPU, load and memory gates remain open.
