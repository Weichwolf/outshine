# Query admission in the existing building capture

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), native cost/input
context [#567](https://github.com/Weichwolf/outshine/issues/567). This follows the
[weak Shell-seed result](shell_wall_selection.md) by inspecting the actual native
Whole/Project consumer. It does not remove a building or change capture density.
The complete source-selection, native-output and device gates remain open.

## Existing consumer and two different contracts

`BuildingSurfaceCapture.cpp::OccupiedRegions` projects non-Fine projectable
source bounds into a cube face and admits intersecting **64×64 blocks**.
`VisibleFaces` traces every pixel of each block through the complete source BVH.
Fine sources must remain possible occluders in those traces. The existing
`EmitSurfaces` always meshes Fine in full; Fine captures are not consumed.

`compare_cube_capture_masks.py` uses the **original #800 successful projection
groups**, including their original per-source representations, both Whole and
per-cache-cell. This is the real native grouping, unlike an unculled roof-only
control. The independently decoded schema-8 raw surface **Bounds_** are the BVH
bounds; they are distinct from padded SourceEnvelopeBounds. All 9,409 original
source identities/cells/nullable envelopes/errors match the native traces.
No Fine meshes are inputs to the mask. Existing cache groups and products stay
unchanged; this is capture/build work, not a measured frame optimization.

Two masks have different guarantees:

- **All projectable source bounds:** outside their union, no source-box query
  should reach a tracing callback. Empty samples, including the absence of Fine
  samples, remain unchanged. Native arithmetic still needs its own verification.
- **Non-Fine demand only:** rays outside that union can only supply an empty or
  ignored Fine capture. Used non-Fine samples and their patch rectangles are
  preserved for valid finite samples. This does **not** prove identical failure
  behavior: skipped Fine preparation can change RequireShapes error order, and
  an invalid Fine sample can change the global InvalidSamples fallback.

Master has explicitly approved omitting unused Fine-only captures and changed
their error coupling in
[#12/6257012164](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6257012164).
Unused invalid Fine captures must not globally invalidate valid non-Fine
sources. Actually consumed invalid samples still need safe source/region
fallback; required Fine-source validation must occur before its full mesh,
with errors/source IDs preserved. This is an approved target policy, not proven
native equivalence or adoption. Keep the original 64×64 region boundaries.
No new native build or exporter is requested by this study.

## Representation and bounded work

`cube_capture_mask.py` projects represented input bounds with exact dyadic
rationals, preserves the existing one-pixel padding and original admitted block
set, and ORs half-open rectangle rows into UInt64 words. Unknown bounds admit the
whole face. An eye inside/on a box admits every direction, including zero-distance
RayEntry hits. A positive near depth ≤1e-6 m also conservatively admits the whole
face; it is a numeric fallback, not a changed world tolerance.

This guard matters: at subnormal distances, Float64 slab division can report a
hit that the exact ray misses. The independent oracle exhibits that case; it is
not dismissed by a relative epsilon. Exact and scalar-native-style slab checks
also cover all six face orientations, seams, tangent/behind boxes, odd grids,
translated coordinates and observer contact.

Caps: dimension 1..8192, original coordinate magnitude ≤1e7, and one million
rectangle/block-pair visits per mask. Exceeding the work limit restores **all
original block rays**; unknown work never becomes an empty world. Bounds,
source membership, representation and eye are current inputs. These masks
certify no observer region or temporal reuse; recompute after relevant changes.

No opacity, Hausdorff-to-coverage inference, hull fill or source deletion is
involved. Scalar bit scans can enumerate set bits; a native implementation and
its arithmetic/cost behavior are not supplied here.

## Original-input comparison

Wien inputs retain dimension **4178×4178 per cube face**; Flensburg/Koerbersee
retain **4715×4715**. These are existing capture grids, not changed render
profiles. The three Wien parents share one camera. The other two use their own
cached cameras; park-edge and worldwide-load acceptance remain open.

| Original parent | Sources | Original Whole rays | Non-Fine-demand rays | All-source-bound rays |
| --- | ---: | ---: | ---: | ---: |
| bbd9aab743 | 457 | 3,146,368 | 2,448,686 | 3,146,368 |
| 6d41e63e73 | 1,754 | 5,117,696 | 4,381,140 | 5,046,521 |
| 035e2150a3 | 4,141 | 155,648 | 79,962 | 79,962 |
| a169a8d0d6 | 3,043 | 9,957,817 | 8,224,898 | 9,957,817 |
| 87b5710776 | 14 | 0 | 0 | 0 |

Non-Fine-demand Whole ray reductions are 22.17/14.39/48.63/17.40 percent. Strict
all-source masks save nothing in the two prominent near-city cases. Koerbersee
has no projection call, so there is no capture saving to claim.

Original per-cell ray totals → non-Fine demand totals:
3,899,392→2,977,242; 7,021,184→4,927,070;
942,080→198,848; 12,773,568→9,028,647; Koerbersee 0→0.
These are admission counts, **not proportional native speedups**. Empty-BVH
queries and full shape traces have different costs.

Ten terminal comparisons cover five inputs and both modes, 135 distinct accepted
original projection groups and **1,620 face/mode checks**. Every UInt64 bit is
compared with an independent byte-per-pixel rectangle union; all agree. Native
Float64 rectangles and exact padded rectangles agree on these inputs. This is
not a native tracing/output replay or a universal compiler arithmetic proof.

## Measured preparation and memory

`measure_cube_capture_masks.py` compares existing coarse-block preparation,
additional dense pixel masks and UInt64 row masks, on each largest Whole face.
24 fresh serial normal-priority processes run coarse/word/dense/dense/word/coarse
under existing interactive and nice10 native-analysis load. Metadata/rectangle
acquisition is identical and outside the instrumented measurement.

| Parent | Coarse tracked peak B | Word tracked peak B | Dense tracked peak B | Word / dense initialized payload B |
| --- | ---: | ---: | ---: | --- |
| bbd9aab743 | 60,584 | 329,679 | 1,692,453 | 191,968 / 1,535,744 |
| 6d41e63e73 | 98,928 | 744,967 | 4,037,629 | 470,528 / 3,764,224 |
| 035e2150a3 | 37,560 | 67,351 | 200,445 | 19,456 / 155,648 |
| a169a8d0d6 | 80,680 | 538,839 | 2,856,621 | 331,440 / 2,651,520 |

Word payload is eight times smaller than dense on these measured faces, but is
**additional memory versus the existing coarse path**. Instrumented Python
word preparation takes 0.120–0.393 s and is about twice as slow as dense
(0.057–0.182 s); coarse is 0.010–0.041 s. No winner for native CPU cost follows.
Tracked live blocks are not total allocation calls. NumPy payload allocations,
zeroed bytes and logical OR/fill read/write bytes are counted separately; those
logical bytes are not measured memory-bus traffic or native allocation volume.

Most importantly, the existing complete PlaneSurfaceSample grids remain: the
logical 8-byte surface code plus two three-double vectors total 56 B/sample.
Masking rays alone neither removes their full-grid initialization nor the later
patch scan. Maximum 64×64 sample payload stays 229,376 B per block; aggregate
allocation/init volume across Whole and cells is unchanged. Original source
arrays are not copied by the mask operation. Kernel VmHWM since exec is reported
separately, includes earlier acquisition and instrumentation, and is not used as
an isolated mask peak or an inherited ru_maxrss estimate.

## Viewed emission fixture and actual policy counterexample

`cube_capture_fixture.py` traces three finite planar sources, one Fine in front
of two non-Fine sources. It recomputes admission at three eyes, preserving the
native **per-64×64-block** greedy patch boundaries. All used non-Fine codes and
points, then their patch rectangles and actual emitted positions, are identical.
Both full-visible-face and patch-emission arms render unchanged 1280×720 RGB and
depth. Their 12 images were actually opened. This is an analytic consumer
fixture, not a native building/runtime gate.

Queries fall from 36,864 to 7,082/7,082/7,754. Non-Fine visible pixels remain
28,512–40,055 in the full-face arm and 28,083–40,364 in the patch arm. Both arms
emit the same number of triangles before/after (6 and 18 respectively).

The fixture then deliberately corrupts one skipped Fine point. Original patch
validation reports **InvalidSamples**, while admission leaves a valid empty
sample there. Non-Fine sample equality therefore does not prove the original
global failure policy. This counterexample remains explicit rather than being
turned into a passing native-equivalence claim. It is now an expected difference
under master's explicit target policy. Normal finite-input render checks,
the preserved old failure case and the still-required native consumed-error
fallback/validation proof are separate results.

## Decision and next useful bound

There is no native adoption or time/memory/device acceptance from this study.
Non-Fine demand masking has an explicit consumer policy, but its native
fallback/validation behavior is still unverified. Strict empty-ray masking has
little benefit in the larger original workloads.

An additional read-only diagnosis explains this: bbd9 Fine sources **447/448**
and Flensburg **2880/3042** have global axis-aligned bounds containing the eye.
Every admitted ray therefore reaches these source leaves regardless of nearer
positive hits. Yet the eye is outside every corresponding original local part
box (1/3 and 2/1 shapes respectively). Global AABBs are loose for these rotated,
thin local volumes. A preliminary clipped-box projection gives no extra saving
on the five inputs; observer-containing boxes still cover all directions.
The [source-local outer-bound comparison](building_trace_bounds.md) now checks
all original native hits and enumerates the full original Whole-grid work for
these four sources. Its union bound **after RequireShapes** preserves
preparation errors and the original part loop.
It is a geometric avoidance opportunity, not a measured native improvement or
an opaque proxy. It requires no change to the global sample-error policy.
The [native cost audit](building_capture_costs.md) further distinguishes this
Whole-only opportunity from the actual cell path; its current native priority
cannot be inferred from the Whole count.

## Sources and reproduction

SIGGRAPH-first reference: Wald et al., *Embree: A Kernel Framework for Efficient
CPU Ray Tracing*, TOG/SIGGRAPH 2014,
[local primary paper](../../../doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf),
pages 2/3 and their system diagram actually opened. Its coherent-ray/BVH
discussion motivates examining traversal work; this reference implements neither
Embree packets nor its SIMD kernels and adopts no published speedup factor.
Rectangle OR and the projection/patch invariants above are independently checked.

```sh
PYTHONPATH=lib/src nice -n 10 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p 'test_cube_capture_mask.py'
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/compare_cube_capture_masks.py \
  --input "$original_input_json" --trace "$original_native_trace" \
  --output "$fresh_temp_result" --verify-bitmaps
# Repeat with --include-fine-in-demand for the stricter variant.
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/cube_capture_fixture.py --output "$fresh_temp_images"
```

All original inputs, SHA-bound snapshots, terminal commands and private images:
`/tmp/outshine-research-capture-spans-c72e6030d/`, especially `verified/`,
`mask-costs/` and `fixture-tiled/`. The source originals remain in the #800/#801
evidence directories and the previously secured geographic packets. No new
images are published, no cache is written, and no native product is changed.
