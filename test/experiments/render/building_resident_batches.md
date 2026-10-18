# Bounded resident streams from native building cells

Python research for [#567](https://github.com/Weichwolf/outshine/issues/567) /
[#15](https://github.com/Weichwolf/outshine/issues/15). Existing cache cells can be
packed into fewer bounded storage pages while preserving every geometry byte,
index and cluster. **These page counts are not measured native batches/draws.**
No renderer, GPU contract, cache format, source admission or LOD is changed.

## Actual inputs and the partition-dependent admission finding

The local cache was opened read-only in one SQLite transaction. Original package
CRCs, artifact SHA256 checksums/input keys, complete array extents and supported
wire schemas are checked. Frozen inputs contain 147 actual schema-6 building
bases and three unambiguous complete sets of schema-10 cell products. Same-parent
cells share the recorded input view/detail; duplicate cells or mixed views fail.
This is a local cache cohort, not equivalence to master's current Metal producer
or the original failed 8,068-source Tokyo product.

The 147 bases contain 150,637 stable source records in 4,850 occupied cells.
2,044 cells contain fewer than 16 sources, covering 11,722 source records.
At native candidate `82e7a69d7`, `StructurePlanSelection::SelectProjected` requires
16 distant sources per invocation. Thus these cells cannot independently qualify
even if all their sources are distant. This is a necessary eligibility ceiling,
not actual far classifications, omitted geometry or a measured triangle loss.
Known bounds, shell errors and actual selection commands need a native export
before changing the LOD decision. Merely removing the threshold is not accepted.

## Packing contract and independent preservation checks

[Haar/Aaltonen, SIGGRAPH 2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
page 14, was actually opened. Its persistent instance data and state-based draw
grouping motivate separating cache products from resident storage. This reference
proves storage compatibility only; the complete native material/render-state key
is not present in these artifacts and must be agreed with the renderer owners.

The ten streams are walls/roofs' stored vertices, fallback indices, cluster
records and clustered indices, plus optional wall colours and material metrics.
Vertices retain their exact position/UV/packed-normal bytes in the original
parent ECEF-offset frame. Index rebasing changes only vertex offsets; cluster
rebasing changes only `First`. Bounds, parent/self errors and every DAG level
remain unchanged. Optional attributes stay in separate page groups; no defaults
are fabricated. Per-source key/cell/range mappings remain disjoint and complete.
Recovery subtracts offsets and compares every source byte; independent known
triangle/cluster controls and deliberate attribute/index/identity/range corruption
check that aliases, holes and omissions cannot pass through a roundtrip alone.

The tested page cap is 16,777,216 payload bytes, independent of the unchanged
64-MiB cache-product limit. It is an experiment parameter, not a device budget or
chosen GPU layout; alignment, resource overhead and reserves remain additional.
All source cells must fit before allocation. The smaller 1-/4-MiB alternatives
reject oversized near cells explicitly rather than silently omitting sources.

| Parent prefix | Sources / cells | Original stream bytes | Pages at 16 MiB | Clustered triangle references |
|---|---:|---:|---:|---:|
| bbd9aab743 | 457 / 36 | 48,688,348 | 7 | 467,717 |
| 6d41e63e73 | 1,754 / 60 | 23,884,012 | 5 | 211,743 |
| 035e2150a3 | 4,141 / 61 | 2,814,184 | 3 | 34,638 |

Largest original cell stream payloads: 14,544,176 / 10,234,520 / 114,364 bytes.
Largest resulting page: 16,485,076 bytes. The far cohort also fits four 1-MiB
pages or three 4-MiB pages. These are not optimal packing or native call counts.
The products already report 1 / 7 / 40 unsupported meshes: this research preserves
their source metadata and existing geometry, and does not repair those omissions.
The aggregate counters do not identify the affected source IDs individually.

## Copy alternatives and measured costs

Intel i5-1135G7, Linux x86_64, Python 3.13 and the main `.venv`. Ordinary medians
use three repetitions at normal priority; separate one-pack Memray runs include
Python allocators at nice 10. Concurrent host work was possible. Both methods use
the same immutable inputs, page groups, rebasing and validation. Timing excludes
parsing, validation, IO, uploads and frames.

`slice` assigns into a bytearray slice; `view` writes through a writable buffer
view, avoiding the source-sized temporary allocation. Both still copy the whole
output. Their 150 page-stream files are bit exact, also to the original viewed
producer. Allocation count increases with the small view objects; timings are
mixed. This is an allocation-volume improvement, not a universal timing winner.

| Parent | Median ms slice → view | Allocations slice → view | Allocated bytes slice → view | Traced peak live bytes slice → view |
|---|---:|---:|---:|---:|
| bbd9aab743 | 23.970 → 19.333 | 8,423 → 9,191 | 98,053,749 → 49,480,289 | 53,448,108 → 48,738,483 |
| 6d41e63e73 | 8.025 → 8.941 | 12,237 → 13,524 | 48,592,215 → 24,899,690 | 25,911,054 → 23,935,261 |
| 035e2150a3 | 2.710 → 2.448 | 11,756 → 13,084 | 6,350,221 → 3,730,735 | 2,868,710 → 2,858,349 |

The explicit destination copy is the original stream byte count above, followed
by 11,239,972 / 5,088,724 / 832,664 bytes of index/cluster-offset writes. Output
buffers are zero-initialized. Allocation traffic is not total memory traffic;
Python/internal copies, zero-page handling and native upload copies are not all
measured. Packing adds host storage and work; a native path should target final
resident destinations directly where its ownership contract permits it.

Full comparison process peaks are 148,279,296 / 147,755,008 bytes for slice/view,
from Linux `/proc/self/status` VmHWM since exec. They include original inputs,
packed outputs and verification scratch, not engine/GPU memory. A preliminary
`ru_maxrss`-only report falsely suggested 2,217,603,072 bytes. On this host a small
64-MiB parent-allocation control reproduces the problem: a fresh default-spawned
child reports 77,188 KiB via getrusage versus 9,112 KiB via its own VmHWM; forced
fork reports 73,748 versus 9,112 KiB. Original receipts remain as counterexamples.
The final report exposes both readings; Darwin reports bytes but does not claim
to exclude a startup/fork floor independently. Unknown hosts stay unmeasured.

## Actual images, remaining native work and receipts

Three complete input cohorts were rasterized before/after at 1280×720 in four
yaw directions each. All twelve depth pairs are exact; eight directions contain
geometry, four are empty outside their partial cohort. The near forward image
is a full-frame close wall. All 24 panels were actually viewed in a local contact
sheet. This is pixel-centre opaque level-0 CPU depth, not lighting/material,
MSAA/alpha, near-plane clipping or a complete native world. Attribute preservation
comes from the separate byte checks. No runtime image improvement is claimed.

The native consumer still needs its complete state/binding key, per-cell view
validity and visibility, bounded decode/upload, edits/movement, cancellation,
old/new revision lifetime and fence-safe retirement. Storage pages must not force
whole-page visibility or collapse source/contact ownership. Existing GPU cluster
selection must consume all preserved ranges. Measure actual CPU encoding, native
calls, GPU work, total resident/peak bytes and Places before adopting the scheme.
The original Tokyo overflow and A18/Iris/whole-world budget gates stay open.

Receipt root: `/tmp/outshine-research-projection-b208e03e8/`.
`frozen-inputs/manifest.json` pins all original bases and cell artifacts.
`whole-study/` holds the original packed bytes actually consumed by the image
comparison; `final-slice/` and `final-view/` hold the final paired cost reports.
`final-traced-{slice,view}/` hold six allocation traces/stats. `depth-{near,middle,far}/`
and `depth-all-pairs.png` retain the images and exact-depth receipts; private
preliminary reports retain the contaminated RSS reading.

Input/initial byte/image producer: `053d0fdfc`, frozen in
`/tmp/outshine-building-batches-053d0fdfc`. Final alternative/cost producer:
`54a982845`, frozen in `/tmp/outshine-building-batches-54a982845`.
All 150 final page streams were independently hash-compared to the viewed
producer. The later renderer change corrects only the scope text from two to
four views. Final controls: 10 packing tests, 9 existing scalar/vector raster
controls, five integrity negatives, twelve CLI/path checks, six-file format/lint,
functions ≤120 lines and `make lint-docs` pass. No native gate was replaced.

## Reproduce

Requires NumPy, system libzstd/pkg-config and optional Memray; no new package was
installed. Use the main `.venv/bin/python`, absolute in a detached tree. Export
requires the existing local cache and full parent keys from the input manifest;
it never fetches or regenerates missing products. Outputs must be fresh explicit
system-temp directories outside the checkout.

```sh
.venv/bin/python test/experiments/render/test_building_batches.py
.venv/bin/python test/experiments/render/export_building_batch_inputs.py \
  --cache "$asset_db" --parent "$parent_a" --parent "$parent_b" \
  --parent "$parent_c" --output "$study_dir/inputs"
.venv/bin/python test/experiments/render/compare_building_batches.py \
  --inputs "$study_dir/inputs" --output "$study_dir/view" --copy-method view
nice -n 10 .venv/bin/python test/experiments/render/compare_building_batches.py \
  --inputs "$study_dir/inputs" --output "$study_dir/view-traced" \
  --copy-method view --track-pack
nice -n 10 .venv/bin/python test/experiments/render/render_building_batches.py \
  --inputs "$study_dir/inputs" --packed "$study_dir/view" \
  --parent "$parent_a" --output "$study_dir/depth-a"
```

Repeat comparison with `--copy-method slice` and distinct outputs; render the
other two parents likewise. `python -m memray stats --json -o result.json
allocations.bin` reads each separate trace. Shared cache paths and private inputs
are not product dependencies. No new PR or production activation accompanies
this research handoff.
