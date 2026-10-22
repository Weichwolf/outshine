# Bounded alternatives for observed transport ports

Owner: [#1003](https://github.com/Weichwolf/outshine/issues/1003).
`mvt_port_alternatives.search_ports` compares combinations from the complete
geometric profiles queried from the retained native crop. It does not create
cross-sections, infer source-face ownership or replace unobserved boundaries.

The input graph and body generation remain separate, following the pipeline in
[Chen et al., SIGGRAPH 2008, page 2, Figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
That page was inspected visually. The finite search below is an experiment of our
own, not an algorithm or performance claim attributed to that paper. Existing
[queries and their limits](mvt_stair_native_crosscuts.md) remain unchanged.

## Contract

Each target has a lower and upper boundary outside its explicitly padded request.
Every alternative is an existing complete native profile on the original axis.
The selector rechecks original sources, access/layer semantics, observed contacts,
other constructions, unknown endpoints, source count and total axis-span limits.
A failed alternative does not change those contracts or make an endpoint free.

The unchanged qualifier constructs the whole replacement with its local 3 m
prior budget and axis/crossfall model, audits original source coverage, actual
Float32 profiles and all fixed native contacts, checks closed-body vertex links,
and builds the shared reference-field/navigation/terrain products. A solver result
alone cannot admit an alternative. These products remain Python references;
native navigation stations, outside splitting/packing, terrain and Places are open.

`span_order` sorts the explicitly affordable catalog by total target-axis span,
then deterministic index order, and stops at the first qualified combination.
`outer_then_shrink` first tries the outer observed combination, then each boundary
in source/lower/upper order, testing inward candidates nearest-first with other
boundaries fixed. It retains each qualified improvement. A rejected outer trial
falls back to `span_order`; it never implies that inner combinations are invalid.
One-pass shrinking can miss a better combination requiring several simultaneous
boundary changes. A two-island test demonstrates this limitation explicitly.

The default caps are 128 examined combinations and 16 full qualifier calls.
The hard configurable maxima are 4096 combinations and 256 qualifier calls;
existing source/profile/geometry limits also apply. Sorted enumeration refuses
an oversized catalog before materializing its Cartesian product. Shrinking needs
no Cartesian product. Duplicate combinations are not qualified twice.
If the cap interrupts refinement, the last fully qualified body is returned with
`valid=true, reason=work_budget`; with no qualified body the result is absent.
This is a count bound, not a hard wall-clock deadline. Existing cooperative
per-fit solver time limits do not bound all preparation or process execution.

## Reproduction and evidence scope

Requires the existing research environment (NumPy, SciPy/HiGHS, Shapely and the
reference-library dependencies); no new package or native code is introduced.
Run from the checkout with `PY` pointing to that environment:

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/transport "$PY" \
  test/experiments/transport/compare_mvt_port_alternatives.py \
  --strategy outer_then_shrink --max-fits 64 --image \
  --output "${TMPDIR:-/tmp}/outshine-port-alternatives-shrink"
```

Use `--strategy span_order` for the control and a separate output directory.
`--trace` measures a separate instrumented run. Reports retain all selections,
full qualifier audits, original input hashes, aggregate LP rows/nonzeros and input
array bytes, retained body-array bytes, Python traced peak and whole-process RSS.
Solver input bytes are accumulated buffers presented to LP calls, not a measured
copy count or total allocator traffic. Tracemalloc excludes opaque native solver
allocations; process RSS includes imports. Timings include native queries, catalog
preparation, selection, all attempted fits, body construction, audits and consumers;
imports, serialization and optional plotting are outside that timed phase.

The original eleven profiles yield 4×3×2×2 = 48 combinations. An exhaustive local
control finds three qualified combinations. In this catalog both search strategies
select the same asymmetric 52.343370 m region; the outer diagnostic used 64.343370 m.
The selected body has 372 vertices/740 faces versus the outer body's 414/824.
Both searches produce byte-identical body arrays and shared consumer products.
Shrinking uses seven full qualifier/LP calls versus 39 for sorted enumeration.
This is evidence for this input and these unchanged local physical constraints,
not global optimality, a world-admission result or a native/frame-time benchmark.
Targeted tests cover the actual body, finite caps, unknown boundaries, bad station
binding, catalog permutation/duplicates, failed outer alternatives and local minima.

On the i5-1135G7/shared Linux host, three fresh normal-priority runs per method
measured 4.459/4.368/4.406 s for sorted enumeration and 1.596/1.602/1.559 s for
shrinking (medians 4.406 vs 1.596 s). Initial runs overlapped targeted nice-10 tests.
These are Python generation-phase times, not native device acceptance.

| Measured work | Span order | Outer then shrink |
|---|---:|---:|
| Full qualifier / LP calls | 39 | 7 |
| Aggregate LP rows | 106169 | 19898 |
| Aggregate LP nonzeros | 199393 | 37865 |
| Accumulated solver-input array bytes | 3284080 | 621836 |
| Separate Python traced peak bytes | 2256946 | 2245739 |

All eight fresh measured/traced bodies and consumer outputs were byte-identical.
The selected body's arrays total 49584 bytes; maximum native contact residual is
2.14e-14 m over 33 checked points, with 18 risers and no Float32 face reversal.
Original source coverage residuals are at most 2.12e-15 m² and maximum prior error
is 2.719193 m within the unchanged local 3 m recipe. Process peaks were about
111 MB for both methods. This does not establish a meaningful peak-memory saving;
opaque allocations and actual copy traffic remain unmeasured.
