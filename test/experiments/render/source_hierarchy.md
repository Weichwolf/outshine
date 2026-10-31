# Static source bounds before fine vertex projection

Owner: outshine_research, #1866 / #1797. Existing finite native candidate: #1861.

`source_hierarchy.py` owns immutable original position/index arrays and per-part
triangle groups. Preparation copies geometry once and records group boxes,
original face ordinals, root bounds, coordinate support and up to six actual
extremal source vertices. A changed source requires a new prepared object.
Query returns the same original ordinal set, complete-model status and Whole
fallback reason as `source_raster_candidates.select` in the checked cases.
No attribute, winding, position or material binding changes. No new SourceIDs.

## Choice and model

Existing `ClusterCook.cpp` sorts triangle centroids by 10-bit-per-axis Morton
codes, stably by original order, per material part. Native renderer leaves use
128 triangles. The fixed original dump has no exported cluster/ordinal mapping;
the Python Morton control follows that convention, not a captured native table.
Even favorable boxes built from all fine projected triangles retain every vertex
for the 128-triangle Morton control. Those controls are not accelerators.

The selected reference uses four-triangle groups in original per-part order.
This retains existing CAD locality in this input. Eight-triangle and native-like
128-triangle Morton controls remain in the comparison; no native defaults change.

Coarse boxes propagate affine center/extents through the actual uploaded static
placement, shift and view matrices. Binary64 guards expand the arithmetic.
Positive absolute norms bound the existing FP32 seven-operation error model over
the whole source box. A conservative factor two expands only coarse work; final
fine support and raster guard are unchanged. A uniform quotient bound includes
the maximum per-triangle vertex uncertainty used by the existing fine selector.
Unknown root bounds delegate to the unchanged full selector.

Actual extremal source points can prove that the original projection would hit
an unsupported clip plane. That returns the whole ORIGINAL set immediately;
it does not discard an offscreen source. Static subnormal coordinates or unknown
upload arithmetic likewise preserve the existing Whole result. Invalid dynamic
layouts/limits retain the old validator/error path. A cooperative fine pair-limit
failure returns all original faces, including previously skipped groups.

Leaves whose expanded screen rectangles contain no viewport center skip fine
work. Surviving triangles gather only their referenced original position rows;
the existing selector decides final primitive support. When more than 75% of
triangle references survive, the full path avoids a nearly complete gather.
This work choice does not relax any error limit or alter the candidate contract.
Metadata batches hold at most512 boxes by default. All geometry remains loaded.

## Frozen comparison

Measured source `d17fbc98a923f5086817fb9b52092064969a1018`, Linux i5-1135G7,
NumPy2.5.4, one BLAS/OMP thread, normal measurement priority. Concurrent work
included nice10 builds; load3.31/6.15/7.49. Seven grouped timing rounds and a
separate Memray1.20.0 trace per case. Earlier runs under load7.58 and10.39 show
the same bounded-query trend; absolute times are not isolated device admission.

Full19 operands /1558 sources /38476 triangles /78374 vertices remain input.
Four native pose/guard comparisons and three12-query synthetic motion sequences
match all candidate IDs and Whole statuses/reasons. Each turn samples0…330° in
30° steps; translation reaches(1100,275,-2200)m in12 steps. Synthetic words are
not native upload captures. Four original ordinal streams are byte-identical to
the existing #1863 output; original attributes and native consumer are unchanged.
The #1853/6536055636 finite A18 HDR/depth/union images therefore remain applicable.

| Guard / query | Full ms | Four-triangle ms | Fine vertices | Full/four scoped peak B |
| --- | ---: | ---: | ---: | ---: |
| 0 / near | 72.097 | 34.734 | 18932 | 8256743 /4645811 |
| 0 / side | 73.656 | 46.973 | 31513 | 8448599 /5598559 |
| 1/256 / near | 69.796 | 34.458 | 19356 | 8285639 /4678883 |
| 1/256 / side | 73.877 | 46.688 | 32096 | 8494295 /5643227 |

Four-triangle build120.646ms,654079 allocation events,122183042 requested bytes,
6259022B scoped peak. Held payload2287276B includes1863912B original numeric
geometry and423364B metadata. Eight-triangle build70.733ms/2152624B held;
Morton128 build39.763ms/2026372B held. Full preparation amortizes after about
four to five repeated bounded queries here; source loading is separate.
Preparation explicitly copies1863912B original geometry into immutable storage;
metadata construction has additional allocations/copies measured by the trace.

Guard0 near/side query allocations9904→18206/19113; requested bytes
147047125→67263295 /147648828→94817084. Known gather copies458200/769980B;
unique/sort internals and physical cache traffic are not that copy counter.
Fine triangle rows8172/13912. Position pages228/230 and230/230; index pages
224/226 and225/226. These are logical4096-byte pages of joined numeric arrays
from offset zero. Fewer projected rows do not establish fewer DB/cache reads.

| Twelve-query sequence | Full ms | Four-triangle ms | Whole fallbacks | Full/four scoped peak B |
| --- | ---: | ---: | ---: | ---: |
| 360° samples | 393.465 | 336.601 | 7 /7 | 10574468 /10599670 |
| Same turn, translated origin | 377.434 | 337.205 | 7 /7 | 10521524 /10546726 |
| Progressive translation | 891.672 | 496.237 | 0 /0 | 8344235 /4880096 |

Turn peaks include retained outputs/work records and are slightly higher.
Whole fallbacks keep all content. Eight-triangle timing is slower than four in
these sequences; Morton128 adds header work without saving bounded-pose vertices.
This is not a universal grouping/optimality result or a native/GPU frame gain.

## Reproduction and acceptance

Use project `.venv` with existing NumPy/Memray dependencies. Set
`PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`.
Run `python test/experiments/render/compare_source_hierarchy.py --output /tmp/outshine-source-hierarchy-<commit>`
from clean measured sources. Fresh output contains report.json and31 traces.
Focused tests include independent rational affine-box checks, random reflected
frames, both guards, metadata chunk invariance, immutable ownership, original
parts, thin/overlapping/reversed/degenerate faces, source-wide fallback, unreferenced
vertices, subnormals, invalid limits, empty demand and complete turn/translation.
39 focused cases passed; Ruff/format, lint-docs and diff-check passed.

Own #1320 acceptance: Python research hierarchy and work selector, owned immutable
geometry lifetime, borrowed stable upload words during each synchronous call.
No new runtime/DB format, cache/queue manager, GPU ABI, shader or native hierarchy.
All source geometry is resident; no avoided IO, physical UMA/residency, live World,
shadow/simulation admission, hardware raster precision or p99 proof. The original
runtime parent remains until its independent consumers qualify the representation.

Primary starting point: local SIGGRAPH2015 GPU-Driven Rendering Pipelines,
pages12 and14 inspected optically: cluster grouping and earlier coarse CPU
selection are distinct steps. Current native owners are `ClusterCook.cpp`,
`TriangleBvh.cpp` and #1523. The reference adds no native implementation.
