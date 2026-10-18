The full 105-source Transport -> 65 Buildings -> four whole SITE plants sequence
now passes the spatial experiment handoff in #770. It preserves the old flat
8192-volume rejection, all 8308 physical and 15 logical transport records, the
original 394 habitat parts and the unchanged 289094-vertex/549948-triangle Pinus
prototype. This completes the actual larger consumer check left by #779/#785;
it does not integrate a shared library/native/world API or improve plant form.

Inputs and reconstruction are those of
[prepared growth](stair_prepared_growth_comparison.md) and
[bounded occupancy](stair_occupancy_candidate_comparison.md). The real additional
three-source group is unchanged. Old report hashes and receipts remain intact.
The source/installed producer and complete readers below ran at
`b7d2f5ac39fd475aacc7d4b4e104240fda18a7de`, containing handed-off Transport
`2f47e58e6`, the integrated #785 priority, and current master `0e4656f6d`.

`CandidateQuery` retains bounded numeric selection before affine construction.
The plant adapter reserves budget for every actual building body and keeps those
parts separate, including multiple bodies with the same source owner. It never
deduplicates physically distinct building parts by owner ID. The concrete query
and incomplete-result contracts were explicitly accepted by both consumers in
#770/6250785442 and #770/6250954547. Complete empty selections now explicitly
return zero narrowphase rows/candidates/conflicts/seconds with `metadata_*` work
separate; an unresolved selection raises instead of reporting empty free space.

The XY query is priority only. It computes actual polygon distances after bounded
numeric selection and returns an error on incomplete work. The old whole-volume
reader independently reconstructs all original physical/logical partitions and
uses their complete geometry for proposal distances and conflict checks.
The default flat numeric distance ordering is retained: the handed-off equal-pool
comparison measured 6.616 s versus 14.277 s for product-group pruning, despite
11.836 M versus 5.404 M facet tests. The latter selected more geometry candidates
(276764 versus 80243). Those are prior single observations, not a new speedup.
The prebuilt full-geometry oracle's ~0.214 s query time excludes its expensive
all-volume preparation and is not a valid flat-capacity runtime replacement.

SIGGRAPH-first basis remains [OBBTree (1996), Figure 1/page 7](../../../doc/references/buildings/siggraph/1996-obbtree-hierarchical-interference-detection.pdf)
for progressive conservative rejection, and
[Plant Ecosystems (1998), Figure 1/page 4](../../../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf)
for separation of distribution, form and instancing. Both figures were inspected
optically during takeover. No OBBTree/SAT/ecosystem algorithm or foreign code is
copied. NumPy, Shapely/GEOS and bounded caller-owned functools LRU remain the tools.

The 1024 canonical proposals are preserved exactly. `--order boundary` calls the
unchanged `site_order.priority` control from #777, then runs all existing terrain,
root, height-band, whole-envelope and actual SITE checks. The canonical proposal
pool remains nearest-first; `budget.selection` explicitly records its subsequent
caller-owned permutation. This is not a new ecological distribution/default.
Four plants pass after four of 128 allowed attempts, using eight conflict calls.
The ledger retains 330 deferred habitat parts (324 explicit proposal-capacity,
six unsupported habitat classes), 60 proposed but unrequested parts, four sampled
parts and 1020 unrequested sites. No deferred part is declared collision-free.

Fresh Source and isolated Installed processes use the same complete inputs.
Every prototype file, complete proposal file and habitat ledger is byte-identical;
all SITE fields and work counters are exact after excluding only the four named
`root_support_query`/`whole_body_query` transport/building `seconds` per plant.
The fresh wheel is SHA256
`86e7c5b587429f52278b2ff022c9a808abd848aa46055ef00162c94d79688306`;
the Installed process has that target instead of `lib/src` on PYTHONPATH.
Both fresh independent readers re-emit the entire prototype, rebuild the full
proposal set, check its permutation/attempt prefix, and replay actual SITE and
all complete transport/building conflicts. Both pass 105/65/394/4 counts.
Rehashed semantic CLI mutations moving a root by 0.01 m or duplicating a proposal
both terminate 1 without a successful receipt. Source and Installed each pass
61 focused occupancy/precision/candidate/distance/whole-plant/priority controls.

Costs are Linux VIRUS/i5-1135G7, Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0,
GEOS 3.14.1, shared host including native compile/lint work. Producers have normal
priority and one BLAS/OMP thread; tests/render/profiling run nice 10. The two normal
producer observations after imports are 15.152/18.017 s, including input reads,
query preparation, prior form validation/reuse, all proposals/SITE and export.
One additional fresh whole subprocess including imports/export is 14.961 s wall,
14.700 s user, 0.227 s system, 230944768 B peak RSS; terminal 0 with the same
verified semantic output. These single observations are not a regression study,
native warm-load budget or hardware performance admission.

The full search performs 1418 XY distance queries, 11836046 transport facet tests,
80243 selected geometry candidates and 53781 logical-materializer invocations.
The latter include bounded-cache hits, not that many newly constructed physical
planes; cache totals are 54101 hits/149 misses/149 retained facets. Building-tree
internal traversal and GEOS internal copy counts remain unmeasured. The prior
whole form links 400 files/9966946 compressed bytes, copies zero prototype payload
bytes, and materializes zero organ-array bytes in the producer. Logical complete
vertex/index payload is 13537632 B, excluding leaf masks and other metadata.

Separate whole fresh-process Memray 1.20.0 (Python allocator events enabled,
native stacks disabled) captures 61309231 allocation requests including 7405
zero-byte requests, 8841074292 cumulative requested bytes and 177333082 B tracked
heap peak. The existing positive-size record reader counts 61301826 requests;
its byte total agrees with `memray stats`. The profiled product has identical
whole semantic output. Tracked heap/RSS are not additive; cumulative requested
bytes are not copied or simultaneously live bytes. Preparation still dominates
work and remains a candidate for measured simplification, not native porting by
transliteration. Raw profiles are retained; profiled timings are not normal times.

Two complete 1280x720 Eevee diagnostics were actually opened. They import the
unchanged 96771-triangle terrain/transport/architecture body and four separate
whole plant objects, totalling 2296563 triangles. Exact per-object topology,
Float32 positions, supplied normals and face colours are checked by the existing
renderer; the existing per-object limits and 16 samples remain. Whole camera,
content and before/after axes match. llvmpipe/LLVM19.1.7, Blender 5.2.2: 24.932 /
76.753 s and process peaks 2179944/3099672 KiB. This is a diagnostic detail stream,
not appropriate distant LOD or an Iris Xe/A18 application measurement.
`test/scripts/pixels.py` finds 21/921600 changed pixels, 13 above 1/255, maximum
7/255. The whole tile remains pale and sparse; no substantial visual gain is
claimed. Float32/subpixel plant organs may collapse. Images remain private.

Reproduce from the checkout using explicit fresh system-temp outputs, the pinned
original inputs above and `.venv` with reference transport/places/solids/assets
dependencies. Optional diagnostics use bpy 5.2.2 and Memray 1.20.0. Set
`PYTHONPATH=lib/src:lib/tests:test/experiments/transport:test/experiments/buildings:test/experiments/vegetation:test/experiments/assets:test/experiments/render`,
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. `ROOT` below is the preserved source
input root `/tmp/outshine-transport-resume-1f10d55eb`; `RESULT` is a fresh output.

```sh
.venv/bin/python -W error -m stair_candidate_plants \
  --buildings "$ROOT/candidate-buildings105" \
  --building-receipt "$ROOT/candidate-buildings105-reader.json" \
  --prototype "$ROOT/outshine-plants102" \
  --prototype-receipt "$ROOT/plants102-reader.json" \
  --order boundary --output "$RESULT/source"
.venv/bin/python -W error -m stair_candidate_plants_verify \
  "$RESULT/source" --output "$RESULT/source-reader.json"
```

For Installed, build/install a fresh lib wheel to an isolated target and replace
only `lib/src` in PYTHONPATH. The saved `proposals.json`, full ledger, every
prototype file and all named SITE fields allow an exact structural comparison
with only the four query durations excluded. Run the ten control modules listed
in the delivery PR for #770 with `python -W error -m unittest ... -q`.
The existing `stair_plant_display` takes the verified product and matching
`candidate-buildings105-display/after-scene.json`; render its before/after scenes
through `render_transport_research`, with EGL surfaceless/llvmpipe and unchanged
16 samples. `memray run --trace-python-allocators -o ...` wraps the same producer
in a separate fresh process; `memray stats --json` reads the retained trace.

Private source/installed/profiled/timed products, original full reader receipts,
semantic countercases, costs, raw trace, pixel report and actual diagnostic pair
are on VIRUS at `/tmp/outshine-research-plants105-0e4656f6d/`. Required source inputs
and original receipts remain protected at the preceding pinned paths.
No shared Plan/Reservation/native cache, source identity or former flat limit
changes. Common library migration belongs to #50/#47; native/product integration,
global Float32/frame correctness and worldwide transport completeness remain
#48/#717/#434. This closes the finite larger consumer experiment, not those goals.
