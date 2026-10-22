# Reused ready BVH intervals (#1101)

The complete stored attributed-prism consumer in #1094 evaluated both child boxes
for packet ordering, discarded their hit masks, then evaluated each box again
when popping it. This removes that repeated work without changing the hierarchy,
product, clipping, packet ordering, tie policy or material/fallback evaluator.
The same ready compact product is used by both measured consumers. This is a
local Python traversal improvement, not a new global aggregation or native path.

## Method and correctness argument

The root box is tested once per existing 256-ray packet. A stack record stores
only child-hit ray indices and their already computed Double entry distances.
On pop, entry <= current closest depth replaces another box division/slab test.
Child order still uses the original minimum-entry rule, including its original
packet-wide ordering; no ordering heuristic is changed in this comparison.

For fixed ray and box, let n be the existing clamped entry, f its exit, and d0
the closest depth at scheduling. The old test is nonparallel-miss AND n <= f
AND n <= d0. A queued hit has already established these conditions. Closest depth
can only decrease to d1 <= d0; testing n <= d1 on pop is therefore equivalent to
repeating the old box test. Equality remains inclusive because another primitive
at identical depth may have a smaller ID. The primitive intersection and its
existing depth/ID tie comparison are unchanged. Array filtering preserves ray
order; the original plane/field computations receive the same leaf packets.
This argument assumes the existing immutable finite ray/box inputs and valid
hierarchy; it adds no malformed-product or opacity/temporal-history contract.

No records are added to the stored asset and loading is unchanged. Separate stack
index/entry arrays do introduce gathers and distance storage. Per queued index
this host stores 8-byte index plus 8-byte distance. The existing balanced median
builder has depth <= ceil(log2(N)); for N <= 65536 and 256 rays, at most 17 pending
packets imply a 69,632-byte bound for queued index/entry array payload. This is
not a bound for Python objects, active/transient arrays, output arrays, libraries
or the entire consumer. It does not apply to arbitrary externally supplied trees.

## Complete consumer measurements

Linux x86_64, Python 3.13.5 existing environment, one BLAS/OpenMP thread, normal
priority under shared development load. Data/product producer 92fcd89aa from
#1094, algorithm producer 42fc95e15. Three fresh processes per path, alternating
order, query identical 6,600 actual static20/moving23/roof-view rays. Each includes
BVH, all plane clipping, complete original fallback and every material field.
The old tracer is extracted unchanged from 15f41cb3f and explicitly selected;
merely importing an old attribute module would accidentally use the new tracer.
Source/recipe rebuilding and diagnostic rasterization are outside both queries.

| Median | Original traversal | Cached traversal |
| --- | ---: | ---: |
| Ready load, ms | 15.49 | 13.52 |
| Complete query, ms | 2,798.05 | 1,941.66 |
| Box-ray tests | 559,636 | 283,118 |
| Primitive-ray tests | 15,532 | 15,532 |
| Plane-ray pairs | 117,679 | 117,679 |
| Analytic material / fallback queries | 1,554 / 300 | 1,554 / 300 |
| Maximum pending ray-index references | 1,242 | 567 |

Query time decreases 30.61%, a 1.441× local speedup. The loader did not change;
its observed timing difference is not claimed as a loading improvement. Outputs
still occupy 607,200 field bytes, and material-record operands remain 281,163 B.
The unchanged geometry operand model decreases 30,628,256 to 17,355,392 B. Additional
cached-entry/depth comparisons number 157,899, or 2,526,384 B of declared Double pairs;
these must not be omitted when discussing the new cost. Counts are not measured
DRAM traffic, cache misses or copied bytes across all operations.

Pending cached index/entry arrays peak at 9,072 B in this case. Their explicitly
created stack gather arrays total 2,526,384 B over the query. That counter covers
only scheduled index/entry array writes, not all NumPy gathers/copies/allocations.
The old pending-reference counter double-counts aliased sibling index arrays;
1,242 references are not an old unique-allocation measurement.

Ordinary raw process peak RSS ranges 125,534,208–125,878,272 B old and
125,816,832–126,033,920 B new. Separate tracemalloc runs report 8,054,027 B versus
8,046,063 B peak traced allocations and 127,676,416 B versus 127,807,488 B raw RSS.
These do not establish a meaningful total-memory improvement. Traced query times
9.48/7.60 s are kept separate from normal timing. Cumulative allocator/copy traffic,
GPU work, device times and whole-world p99 remain unmeasured.

## Regression and retained image scope

Six focused controls cover a cached entry exactly equal to the winning depth,
pruning a previously scheduled farther box, divergent/opposite and parallel rays,
empty/root-miss packets, exhaustive all-primitive controls for incoherent
integer rays, and recovery of conservative compact extents including fallback-only
products. The existing independent explicit-polyhedron/original-triangle,
material and compact-fallback controls remain unchanged. All 24 tests pass.

The first replay reported 216,512 samples in the original-source ROI. The verifier
now also recovers conservative primitive boxes from BVH leaves when compact
products omit duplicate boxes; the full-extent replay reports all 122 poses and 448,936 exactly equal numeric
samples. Its
old/new comparison includes all depths, source/part/plane/fallback identities and
category/coordinate/metre/normal/RGBA fields, with explicit offscreen rows. The
known native reference corner/coverage failures remain part of the replay; this
change does not repair them or weaken any tolerance.

The ready product and geometry/material/diagnostic-renderer files are unchanged.
#1094 owns the actual 21 lit image pairs; matched ready-product and image hashes are
retained rather than rerendering identical inputs as new visual evidence. A further
6,600 representative rays and both known-failure raw arrays compare byte-for-byte
with the previous output, including dtype/shape/signed-zero bits. The day
comparison was reopened during this work. That finite neutral-shader evidence
remains subject to #1094's native filter/material/caster/transition limitations.
No new images are published. Do not infer native performance from the speedup.

## Sources and reproduction

SIGGRAPH 2014 Wald et al., *Embree: A Kernel Framework for Efficient CPU Ray
Tracing*, existing `doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf`,
sections 5.1.2–5.1.4 / pages 3–4 read and optically inspected: ray/packet traversal and
active-lane tradeoffs. No Embree code, ISA kernels, new hierarchy/index or dynamic
packet/single-ray switching are introduced. Existing solid/field sources and
contracts remain owned by `roof_prisms.md` and `attributed_prisms.md`.

Use `PYTHONPATH=lib/src:test/experiments/render`, one BLAS/OpenMP thread and the
existing `.venv` Python with `-B -W error`. Extract the own legacy tracer and
attribute module with `git show 15f41cb3f:test/experiments/render/<module>.py` into
system temp. Run:

```
python -B -W error -m unittest test_primitive_bvh_intervals test_roof_prisms \
  test_attributed_prisms test_prism_lighting
python -B -W error test/experiments/render/bench_prism_consumer.py \
  --product <ready-product.npz> --rays <saved-origins-directions.npz> \
  --traversal-reference <legacy_primitive_bvh.py> --output <old-report.json>
```

Omit `--traversal-reference` for the current consumer; add `--trace` only in a
separate process. Existing `prepare_prism_lighting.py` reproduces the ready
product from versioned fixture inputs. `verify_prism_lighting.py` accepts its
usual fixture/product/legacy/output arguments plus `--legacy-product` pointing
to the same compact product and `--legacy-traversal` pointing to the old tracer.
Outputs must be fresh explicit system-temp paths. Local evidence:
`/tmp/outshine-research-prism-intervals-15f41cb3f/`. No new dependencies/native code.
