# Source-local outer bounds before building part tracing

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), original native
inputs/cost context [#567](https://github.com/Weichwolf/outshine/issues/567).
This follows the [capture-mask comparison](cube_capture_masks.md).
Python establishes a useful
geometric rejection opportunity and retains original native hit controls;
native tracing/output, time, memory and device acceptance remain open.

The [subsequent native cost audit](building_capture_costs.md) limits its priority:
none of the accepted original per-cell projection groups contains these four
expensive sources. They are fully meshed in their cells. The 36.9 million-call
opportunity therefore applies to the diagnostic Whole path; it does not prove
a saving in the current cell path. Do not prioritize native adoption from that
number alone. Source-complete planning before cell emission remains the goal.

## Concrete cause and placement contract

The existing hierarchy uses source bounds in the parent ECEF frame. Wien-bbd9
sources 447/448 and Flensburg sources 2880/3042 have bounds containing the eye,
although their original local part boxes do not contain it. Every admitted
capture ray therefore reaches those leaves at entry distance zero, even after
a nearer positive hit. `BuildingSurface::Trace` currently visits every part.

`building_trace_bounds.py` constructs an outer union in the existing source
frame. It uses original outer/inner ring XY extents, Bottom, roof deck and Top.
Flat roof deck already includes rise; other supported roofs add rise once.
The original source's declared shell error supplies an explicit outer margin:
0.25 m in these inputs. This margin is not an opacity or coverage certificate.
The union contains each padded part box, including holes as bound inputs.
Unknown/nonprojectable/unsupported geometry delegates to the original trace.

The proposed consumer order is mandatory:

1. Run existing `RequireShapes`, including shared-block validation and errors.
2. Reuse the local origin/direction already computed by `BuildingSurface::Trace`.
3. Test the outer source union over the original minimum/maximum interval.
4. For an admitted or unproved ray, execute the entire original part loop.

Only a proven empty ray skips the expensive part loop. Source/part ordering,
equal-distance decisions, faces, normals, source IDs and emitted geometry stay
with the original consumer. No part BVH, opaque proxy, source removal, capture
mask or global sample-error change is required for this candidate. Native
ownership, construction/reuse, bounded storage and destruction belong to the
consumer; repeatedly rebuilding the union per ray would defeat its purpose.

Bounds use outward-rounded endpoints; slab subtraction and division also use
outward intervals. Tangency, parallel rays, finite maximum distances and
subnormals have independent rational-oracle controls. Nonfinite/unproved ray
state delegates to the original path. Batches are capped at 65,536 rays, parts
at 4,096 and supported local coordinate magnitudes at 1e7. This establishes
conservative tests for represented local rays, not a proof about every native
roof/intersection implementation or compiler transformation.

## Original native controls

The verifier binds all 9,409 source identities/cells/envelopes/errors and source
frames to the original schema-8 inputs and #800/#801 native outputs. Across five
parents, 9,308 projectable sources and 12,284 parts have supported bounds; 101
nonprojectable sources retain the original path.

All 65,156 original native control rays are checked. All 64,980 native hits pass
both the source union and the specific winning part box. Their actual native
hit points also lie inside the corresponding local part bounds. Zero native
hits or winning parts are rejected. The 176 native misses are retained: this
deliberately conservative result is not a replacement for actual tracing.

These are original native controls, not a newly implemented Python building
tracer. A full native capture replay remains necessary, including near/skew
planes, ties and arithmetic behavior on the target compiler/platform.

## Full original Whole-grid work, limited to four sources

The verifier enumerates the complete original admitted Whole capture grids for
the four eye-containing sources. Wien retains 4178×4178 per cube face;
Flensburg retains 4715×4715. Cache cells, membership and capture density stay
unchanged. The other three parents have no applicable source in their accepted
Whole projection calls; no saving is attributed to them.

| Parent / source | Parts | Original part calls, lower bound | After source union, upper bound | Separate part boxes, upper bound |
| --- | ---: | ---: | ---: | ---: |
| bbd9 / 447 | 1 | 3,146,368 | 256,868 | 256,868 |
| bbd9 / 448 | 3 | 9,439,104 | 770,805 | 145,220 |
| a169 / 2880 | 2 | 19,915,634 | 0 | 0 |
| a169 / 3042 | 1 | 9,957,817 | 4,562,073 | 4,562,073 |
| Total, these four sources only | | 42,458,923 | 5,589,746 | 4,964,161 |

Original source leaves contain the eye, so their part calls cannot be pruned by
an earlier positive hit. Candidate counts use maximum=+infinity, conservatively
retaining rays a nearer hit could additionally exclude. The difference is a
36,869,177-call **geometric avoidance opportunity**, not a measured native
speedup or a whole-world cost total. Original/native numerical false hits still
need the actual consumer replay before adoption.

Separate part boxes avoid only 625,585 more part calls while adding 16,250,553
box tests compared with one union test per original source query. The union is
therefore the simpler first native probe. Its logical Float64 payload is 48 B
per source, versus 48 B per part for separate boxes. Actual ownership overhead,
allocation/copy counts, construction cost, residency and peak memory are
unmeasured natively; no free-memory claim follows from these logical sizes.

## Viewed independent image and error controls

`local_trace_bound_fixture.py` traces every triangle of a rotated thin cuboid
with an independent Moller–Trumbore consumer. Its global AABB contains each eye;
the local outer union does not. All original 12 triangles remain. Three eyes
each render 1280×720 before/after; all six PNGs were actually opened together.
Nearest triangle IDs, depth and RGB agree exactly for every pixel.

| Pose | Original expensive queries | Guarded expensive queries | Visible pixels |
| --- | ---: | ---: | ---: |
| 0 | 921,600 | 349,920 | 171,360 |
| 1 | 921,600 | 349,200 | 171,360 |
| 2 | 921,600 | 393,120 | 186,480 |

This simple analytic image demonstrates the mechanism, not native full-world
appearance. Immutable geometry validation precedes filtered batches. Ten
focused unit tests separately check RequireShapes/shared-block error identity
before even a proven-empty guard, delegation of original hit/error objects,
unmodified inputs and scalar/batch local-frame consistency. The capture-mask
tests and the explicitly changed Fine-sample policy remain separate controls.

## Reproduction and remaining acceptance

SIGGRAPH-first context: Wald et al., *Embree: A Kernel Framework for Efficient
CPU Ray Tracing*, TOG/SIGGRAPH 2014,
[local primary paper](../../../doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf).
Pages 2/3 and the system diagram were actually viewed. This experiment examines
bound/traversal work; it implements no Embree SIMD/packet kernel and adopts no
published speedup factor. The conservative interval recipe is checked directly.

```sh
PYTHONPATH=lib/src nice -n 10 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p 'test_building_trace_bounds.py'
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/verify_building_trace_bounds.py \
  --input "$original_input_json" --trace "$original_native_trace" \
  --surfaces "$original_native_surfaces" --count-whole-rays \
  --output "$fresh_system_temp_result"
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 \
  .venv/bin/python test/experiments/render/local_trace_bound_fixture.py \
  --output "$fresh_system_temp_images"
```

Original packets and inputs remain in the secured #800/#801 evidence and
geographic input directories. Commands, source/input SHA-256s, all five terminal
exit-0 receipts and per-parent results are under
`/tmp/outshine-research-capture-spans-c72e6030d/local-bounds-final/`.
`local-fixture/` holds the six images and exact comparison report beside it.
Nothing is published to an image gallery or written to native caches.

Any later native comparison needs the same full original inputs, RequireShapes/errors and
part ordering; actual old/new capture hits, patches, emitted products and
framebuffer; union construction/storage, expensive calls, allocations/copies,
peak memory and CPU. Compare against unchanged global bounds and separate part
bounds. Full-world hierarchical demand, park-edge/geographic counterexamples,
movement, shadows/reflections/interactions and A18/Iris 1280×720/60 including
p99 remain open under #12. This bounded probe does not complete that feature.
