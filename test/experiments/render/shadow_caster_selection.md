# Conservative index selection for shadow atlas strips

Owner: #1048. The existing toroidal atlas already preserves unchanged texels.
The remaining measured work was drawing whole batch index ranges for thin strips.
This step selects original index intervals before native vertex processing. It
changes no atlas resolution, shadow LOD, shader, GPU binding or resource format.

The primary reference is the local SIGGRAPH 2015
[GPU-driven rendering pipelines](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
slides 11 and 13, inspected as text and images: coarse instance selection followed
by cluster-bound rejection before geometry processing. This implementation keeps
selection on the CPU and uses the existing conservative source/Float32 contract
from #819 and [clustered_shadow_bounds.md](clustered_shadow_bounds.md).

## Geometry and lifetime contract

`ShadowCasterSelection` qualifies complete, ordered, triangle-aligned coverage of
the actual batch index stream by its resident jobs. Jobs must name the owning
batch and stay within its range. Gaps, invalid ranges, unavailable placements,
unqualified multiple-instance batches and absent jobs retain the original draw.
Unknown individual spheres retain their original intervals. All supplied LOD
intervals participate; this step makes no independent shadow LOD decision.

Current packed instance transforms produce conservative world spheres using the
existing guarded affine transform, outward rounding, shear bound and unknown
fallback. The four actual light-patch side planes select intervals. Depth-clamped
casters are never rejected by Z or by the main camera. Selected touching or
overlapping intervals merge before drawing, avoiding duplicate source indices.
Index buffer, model slot, material/alpha data and other draw state stay unchanged.
The reported shadow draw count follows actual encoded packets, including zero
when every qualified interval is outside and multiple disjoint retained packets.

Bounds are prepared after accepted table/placement preparation and reused while
those inputs are unchanged. Table changes invalidate selection; failed uploads
cannot publish newly prepared bounds. An accepted full mesh qualifies its supplied
cluster bounds. Any vertex `SetVertexInput` invalidates main-subject bounds until a new
qualified mesh arrives; independent immutable pieces retain their own contract.
The real native pose regression moves vertices into a light tile from outside
their old spheres, verifies full draw coverage, then replaces the mesh/bounds and
verifies fine selection becomes available again.

A complete rebuild currently scans all resident jobs. The exported Tokyo input
has 8,214 batches, 50,330 jobs and 8,214 placements. Of those batches, 8,213 have
qualified coverage; 2,082 lacked a whole-batch bound before this change. The
largest measured original packet had 744,501 indices and 1,939 clusters. Source
world class is not inferred from its model slot or material grouping.

## Work reference and measured native result

A diagnostic snapshot and actual patch matrices were exported from the native
renderer without changing its GPU commands. No vertex pose occurs in that input
window. The standalone Python reference reproduces the observed original object
work exactly, then predicts the native selected work exactly:

| Same 60-frame moving window | Original | Selected |
|---|---:|---:|
| Object instance-weighted index requests | 1,769,282,133 | 16,749,348 |
| Terrain instance-weighted index requests | 21,765,888 | 21,765,888 |
| Total requested indices | 1,791,048,021 | 38,515,236 |
| Median requests/frame, all shadow geometry | 23,277,022.5 | 649,662 |
| Submitted patches | 193 | 193 |
| Frames updating shadows | 60/60 | 60/60 |
| Median updated atlas pixels/frame | 6,144 | 6,144 |

Object requests decrease 99.05%; total requests decrease 97.85%. These are actual
submitted index requests, not unique vertices or measured shader invocations.
The same observer joins every unique timed/ended successful GPU submission with
zero dropped records, unknown pipelines or indirect payloads. The reference
examines 4,938,987 cluster bounds across the initial and measured patches.
Precomputing world bounds transforms 50,330 clusters once per rebuild instead of
transforming again for each patch test; Python timing is not native device timing.

On Intel i5-1135G7/Iris Xe, Clang22/libc++, complete bounds preparation allocates
1,540,664 bytes: batch ranges and transformed spheres. First preparation is
6.599 ms; nine subsequent full rebuilds have median 6.162 ms and maximum 7.140 ms.
Unchanged-frame reuse avoids this preparation. These are measured fixture costs,
not a scene RSS/UMA peak claim or a guarantee for arbitrary dynamic workloads.
Whole-table invalidation can still rebuild unrelated bounds; finer update
propagation remains a later measured optimization, not an implied zero cost.

Original, selected, selected-with-observer and selected-with-full-atlas-redraw
moving PNGs are byte-identical, SHA256:
`57e427d31a806e0e20433fc9701b49c27941826faace2a866212115236ffa412`.
The full-redraw control forces every region to redraw without preservation while
keeping projection, scene and quality. A post-window native readback also compares
all 16,777,216 D32Float values, actual regional uniforms and the base light matrix.
Original cached, selected cached and forced-full snapshots are identical, including
scroll origins. There are 16,777,076 positive depth texels and no nonfinite or
out-of-range values. Depth payload SHA256:
`c4757b856e364aee8c92e2654dbe618b80d71d77be4d89ae26d87059b28b4f03`.
The extra diagnostic GPU readback is excluded from performance/RSS claims.
Actual city images are inspected locally;
no product documentation images are published.

The first native source comparison on parent 3c8a20cb6 reduces moving GPU median
30.690 to 20.124 ms and GPU p99 51.346 to 30.049 ms. This pair ran under its recorded
host load and is not isolated timing admission. CPU p99 remains 91.389 ms in the
selected arm. All 60 moving frames remain unrefined (#1045), and 60 fps remains
unproven. A fresh current-parent 69a22b70a pair (including both new cache translation units)
reports GPU median 28.942 to 19.190 ms, GPU p99 41.780 to 25.779 ms and CPU p99
77.517 to 66.474 ms; both images match the same hash. Its static selected run is
world-complete/unrefined0 and matches the original static PNG. The subsequent
0b21eadae parent changes only documentation/images and ancestry, with identical
product/build/shader sources. The linked PR records exact hashes and gate scope.
Metal/A18 timing and the moving-shadow receiver telemetry question remain open.

## Reproduction

Private inputs, raw matrices, before/after images, full compilation databases,
source hashes, link maps and terminal receipts live in:

- `/tmp/outshine-engine-shadow-caster-selection-3c8a20cb6/`
- `/tmp/outshine-engine-motion-profile-493877d6e/runtime/`

`NativeShadowCasterWork.cpp` reads the diagnostic snapshot and measures actual
native preparation with the same source implementation. It is a CPU cost probe,
not a GPU benchmark. Build with the product's full CDB flags and native archive.
The snapshot has four little-endian uint64 counts, followed by each batch's eight
uint32 fields and four Float32 bound fields, the flat uint32 job table, the flat
Float32 sphere table and sixteen current Float32 values for each placement.

The Python reference uses only the standard library:

```sh
python3 test/experiments/render/shadow_caster_work_reference.py \
  --casters "$inputs/casters.bin" --patch-log "$inputs/patches.log" \
  --initial-patches 4 --output "$scratch/work.json"
```

`--initial-patches` explicitly excludes preparation patches from measured work.
The reference rejects logs containing a changed vertex pose: one snapshot cannot
establish correct work for changing vertex revisions. Native interval, unknown,
placement and pose tests cover those separate contracts. Master owns current
Metal/A18, broad integration/Places and final default-renderer admission.

## Remaining Iris work after selection

The existing private Vulkan work probe (#756) builds with all six current native
source anchors on ab0af74da. A fresh offline process repeats the same 60-frame
moving profile on Iris Xe. Pipeline statistics are supported; timestamps have
36 valid bits and a 52.08333206176758 ns period. All 1,209 records in 61 completed
traces validate, including preparation. Exactly 60 sky-bearing traces join the
benchmark's 60 unique device/submission pairs by individually identical native
submission durations, with no ambiguous match, omitted capture or cancelled trace.
The final image matches the original moving hash above and was opened locally.

| Measured interval | Records | Median ms | Actual median work per record |
|---|---:|---:|---|
| Opaque geometry, all observed pipeline sets | 60 | 12.051 | 2,597,123.5 vertex and 1,969,124 fragment invocations |
| Terrain parent pass | 60 | 2.441 | Parent statistics deliberately absent; children measured separately |
| Subject index compaction | 60 | 1.455 | 6,442,240 compute invocations |
| Shadow raster pass | 60 | 0.214 | 649,668 IA vertices and 211,326 vertex invocations |

Intervals can overlap and must not be added into a frame breakdown. The shadow
IA count includes patch-clear geometry; it is not the object-only observer total.
The opaque interval does not separate vertex from fragment execution, so it does
not establish which shader stage limits that pass. Terrain-parent and absent-stage
counters remain null rather than invented zeros. Instrumented GPU frame median is
19.957 ms and p99 53.464 ms; this is not an uninstrumented performance comparison
or admission. All 60 frames remain unrefined. CPU hook scopes total 14.389 ms across
52,640 calls, with another 4.929 ms for cold teardown output; added GPU query cost
is uncalibrated. The result shifts the next GPU investigation to opaque geometry
and their representation/selection, rather than assuming shadows still dominate.

Exact build/source hashes and commands: `/tmp/outshine-engine-iris-pass-work-ab0af74da/probe/build.json`.
Validated records, unique joins and summaries: `analysis.json` alongside `analyze.py`
in that directory. Original benchmark report, trace log, load and PNG remain under
`/tmp/outshine-engine-motion-profile-493877d6e/runtime/moving-iris-pass-work-ready/`.
These are explicit diagnostics on the selected source, not a new whole-engine gate.
