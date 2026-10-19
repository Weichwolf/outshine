# Local-bound opportunities in the actual cell capture prefix

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), native producer
context [#567](https://github.com/Weichwolf/outshine/issues/567).
The [cost audit](building_capture_costs.md) identifies source tracing in actual
cell captures as the CPU hotspot. This experiment measures a new cell-specific
opportunity; it does not transfer the earlier Whole-only 37-million figure.

## Count the work before any source hit can prune it

For each original capture ray, native `TraceClosest` visits its sorted DFS
prefix before reaching the first source whose `SupportsProjection` is true.
Earlier unprojectable leaves return without tracing. Until that first eligible
callback, maximum remains infinity: no closer source hit has occurred.
On the successful original capture groups, `RequireShapes` then succeeds and
the existing `BuildingSurface::Trace` visits every original part.

That prefix is therefore guaranteed original work, independent of which later
surface eventually wins. `building_capture_prefix.py` replays just this prefix
to count a lower bound. **Stopping there is solely a diagnostic.** Production
must continue its full closest-hit traversal. First eligible is not necessarily
the nearest box or visible source; a focused counterexample tests that error.

The candidate remains the [outer source-local bound](building_trace_bounds.md):
after existing `RequireShapes`, reuse the already computed local ray and return
empty before the part loop only when the outer union proves no intersection.
The 0.25 m source margin in these inputs is an explicit outer margin, never
opacity. No source, mesh, cache cell or original 64×64 patch region is removed.
Unknown bounds delegate to the original loop. Keep every original part/tie/face
decision on the admitted path and preserve shared-block errors before rejection.

## Use the actual native trees and first callbacks

`gdb_capture_prefix_inputs.py` reads tree input boxes, output nodes and order
from the existing #801 normal exporter at Build entry/return. There is no new
native build or product implementation. The reader is restricted to the exact
x86-64/libc++ ELF SHA-256
`012d9aa8c525e98f46466d99ba30f8c5273dd4040fb012b92f5733e2979a7d8d`.
The expected/vector/node/optional layouts are verified against this binary,
bounded before reads and rejected for unknown storage. These are diagnostic
ABI details, not an engine API or portable cache format.

The Python tree reference uses NumPy sorting instead of copying nth_element.
The native centre comparator breaks equal-centre ties by local source index,
uniquely fixing every child set; midpoint and axis tie rules remain identical.
All **135 actual native node/order arrays are byte-identical** to the reference.
All input boxes match the original source slots and metadata. All original
source IDs, errors and representations remain in the independent native output.

Up to four first actual `Source::Trace` callbacks per tree are additionally read
with their ray, complete frame/bounds header, minimum/maximum and optional return.
All **540** are uniquely identified and match the replayed first source, with
minimum zero and maximum infinity. The local union rejects **524 actual native
misses**, retains all **12 actual native hits**, and conservatively retains four
further misses. No observed hit is rejected. A separate positive ABI control
matches Along/Normal/Part/local Side to the original #801 surface export;
fresh construction is matched by full frame and unique ray, independently of
runtime cached-header boxes. Complete native export fields remain unchanged.

The existing 65,156 native ray controls / 64,980 positive hits and actual winning
part/point checks remain the separate broader bound controls. These observations
do not replace a full native candidate capture/mesh/framebuffer replay.

## Complete original cell grids, guaranteed prefix only

The three Wien parents retain 4178×4178 per cube face, Flensburg/Koerbersee
4715×4715. Every original admitted cell pixel is enumerated, with original
projection membership and 64×64 boundaries. Whole trees are verified but Whole
work is excluded from this table. Koerbersee has no Project call.

| Parent | Original cell rays | First eligible callbacks | Original part checks in prefix | Part checks retained by source union | Retained by separate part boxes |
| --- | ---: | ---: | ---: | ---: | ---: |
| bbd9aab743 | 3,899,392 | 2,449,931 | 4,900,221 | 1,499,100 | 804,512 |
| 6d41e63e73 | 7,021,184 | 4,456,365 | 9,276,025 | 3,564,335 | 1,912,004 |
| 035e2150a3 | 942,080 | 160,733 | 273,062 | 67,799 | 45,197 |
| a169a8d0d6 | 12,773,568 | 8,442,329 | 14,381,303 | 5,130,428 | 3,381,247 |
| 87b5710776 | 0 | 0 | 0 | 0 | 0 |
| Total across these input runs | 24,636,224 | 15,509,358 | 28,830,611 | 10,261,662 | 6,142,960 |

The source union exposes **18,568,949 geometrically avoidable part-loop checks**
in this guaranteed prefix, at the cost of 15,509,358 extra union tests. It can
return empty before parts in 10,023,100 first callbacks. This does not mean the
Source::Trace function entry or existing local transform disappears when the
guard is placed inside that function.

Separate part boxes expose 4,118,702 more avoided part checks but require
13,321,253 additional box tests compared with one source union. These counts
compare work, not native cost: a part check includes different roof/wall work;
the compiler may inline it. Known bounds suffice for every counted first source
in these inputs. Later callbacks are uncounted and may have different costs or
selectivity. Neither table percentages nor sampled CPU shares are a speedup.

Logical box payload is 48 B/source for the union, versus 48 B/part for separate
boxes. Native layout, construction, cache locality, ownership, allocations,
copies and peak memory remain to be measured. Do not sum input-cohort payloads
as one world's residency. The replay's 65,536-ray batch / 32-level pending array
is bounded diagnostic scratch, not a proposed new production allocation.

## Controls and native comparison contract

Nine focused controls cover unique centroid ties, equal-entry right-first order,
unprojectable leaves, sorted DFS versus globally nearest-box order, zero axes,
tangency, root misses, infinite native entries versus nullopt, corrupted native
bytes, input ownership and scalar/batch consistency. The existing independent
triangle-image fixture retains all geometry and exact depth/RGB at three eyes;
it is an analytic control, not an integrated world image.

For a bounded native candidate:

- Keep `RequireShapes` and all shared-block validation/error identity first.
- Derive/publish bounds with ready immutable shapes, using original rings,
  holes, Bottom/Top, source frame and current MinimumHeight. Do not rebuild them
  per ray or share a stale bound across changed geometry/height revisions.
- Reuse the original local ray, keep the original hierarchy and complete part
  loop on admitted/unknown rays. Fine remains fully meshed as before.
- Compare all original cell captures, hits/patches/emitted products and runtime
  images, separately from Whole. Include near/skew/tie and shared-error cases.
- Measure added bounds work/storage and eliminated part work alongside native
  CPU, allocation/copy/peak and full world/device gates. Compare the simple union
  with the separate-part alternative before retaining a more expensive path.

## Sources and reproduction

SIGGRAPH-first context: Wald et al., *Embree*, TOG/SIGGRAPH 2014,
[local primary paper](../../../doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf).
Page 3, including primitive representations and single-ray BVH traversal, was
actually viewed. This is the existing native binary traversal, not an Embree
implementation or an adoption of published performance numbers.

Run the SHA-pinned reader under GDB at main with
`OUTSHINE_CAPTURE_PREFIX_JSON` pointing to a fresh private result, using the
same secured native environment as the [cost audit](building_capture_costs.md).
Then:

```sh
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/measure_building_capture_prefix.py \
  --input "$original_input_json" --trace "$original_native_trace" \
  --surfaces "$original_native_surfaces" --trees "$native_snapshot_json" \
  --native-result "$same_run_native_output" --output "$fresh_temp_result"
```

Commands, terminal receipts, immutable reader snapshot, original ABI controls,
all five counted results and private image controls are under
`/tmp/outshine-research-cell-prefix-844dd770a/`; in particular `native-captures/`,
`verified/`, `verified-runs.json`, `summary.json` and `positive-abi-verification.json`.
Original source packets remain in the secured #800/#801/geographic inputs.
The reader/counter refuse failed, mismatched or unverified input; they do not
invent a fallback count. Native candidate equivalence/performance, full
hierarchical demand before cell emission and world/A18/Iris gates remain open.
