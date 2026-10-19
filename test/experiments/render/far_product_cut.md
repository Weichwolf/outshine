# Source-complete far cuts before cell emission (#12)

This is the bounded box-product baseline for the current
[source-complete hierarchy priority](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6258747087).
It supplies a checked source cut and a negative ground-view ROI result.
It does not supply an adopted native parent product or world/device acceptance.

## Input and output

`FarProductHierarchy` consumes the entire original source-envelope table,
explicit known flags and explicit geometric candidate eligibility. Cache-cell
IDs are not inputs. The reused `BoundsHierarchy` provides the median-split
source permutation and joint node bounds. Each cut entry either names one
geometric box candidate or delegates its complete member range to the original
plans. Unknown sources have separate original addresses. No source ID is
replaced, deduplicated or reassigned to a new cache cell.

Budget exhaustion delegates pending complete ranges, rather than losing their
members or waiting. Protected sources never enter a proxy. Actual shadow,
interaction, reflection and contact demands must provide those protection flags.
Original logical sources and collision data remain unchanged. The output is a
planning reference, without a new cache, export or scene-publication protocol.
The cut owns an immutable observer-box snapshot and its projection profile;
`contains_view` rejects reuse outside that region or after a profile change.
The original frozen hierarchy/source state must also remain valid; no new
persisted source-revision protocol is inferred.

The candidate is the twelve-triangle surface of a joint source AABB. It is
explicitly a **geometric diagnostic candidate**, with original-source membership.
This geometry recipe differs from native `DescribeStructureMass`; no native
massing equivalence is claimed. The input declaration must establish nonempty
source geometry contained by each supplied envelope. Material, topology,
silhouette/coverage and supported runtime-product contracts remain separate.
A box cannot certify opaque source coverage or hide other sources.

## Conservative projection rule

SIGGRAPH-first basis: [Simplification Envelopes, Cohen et al., 1996](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf).
PDF page 2, including the definitions and model assumptions, was inspected
visually. It motivates an explicit two-sided geometric error; this baseline
uses the much looser joint box diameter, rather than implementing that paper's
surface simplification. The projection inequality below is derived here.

Let `B` enclose source and candidate point sets and `V` enclose all allowed eye
positions. Let `D` bound the diameter of `B`, `d` be their minimum box distance,
`f` the symmetric focal length in pixels and

```
kappa = hypot(width/(2*f), height/(2*f))
c = sqrt(1 + kappa*kappa)
```

For any orthonormal view orientation, take any `p,q` in `B`, with `p` projecting
inside the viewport. Then `|p_xy/p_z| <= kappa`, hence `p_z >= d/c` and
`q_z >= d/c-D`. With `delta=q-p`,

```
|project(q)-project(p)|
  = f * |delta_xy - (p_xy/p_z)*delta_z| / q_z
 <= f*c*D / (d/c-D).
```

The denominator must exceed the near distance; otherwise the bound is infinite.
It holds throughout `V` and through 360-degree rotation for the declared profile.
All distance, diameter and scalar operations round outward. Source/proxy
enclosure, orthonormal view and arithmetic precision remain actual consumer
requirements; this is not a native Float32/FMA or rasterization proof.

Simple `f*error/distance` can underestimate perspective displacement off axis.
The independent counterexample uses `p=(640,0,1000)` and
`q=p+(0.99,0,-0.6336)`, `f=1000`, viewport 1280×720: the Euclidean estimate is
below one pixel while horizontal displacement exceeds 1.3 pixels. The new bound
contains it. No native test specification or error budget is relaxed here.

If a parent box encloses a child box, its diameter cannot shrink and its distance
to `V` cannot grow. The rule's upper bound is therefore monotone upwards. A
leaf failing this particular acceptance rule prevents every enclosing parent
from passing it. This says nothing about the actual minimum approximation error
or whether a better surface representation could meet the budget.

## Actual source comparison

The unchanged native #801 packets provide all 6,352 source records from the
three complete Vienna cohorts, including duplicate/anonymous IDs at their
original record addresses. There are 48 unknown bounds. Experimental candidate
eligibility follows supported nonempty shapes, zero minimum height and no
footprint holes: 6,215 eligible records; the other 137 always retain plans.
This declaration does not establish opacity or native product readiness.

Shared and cell-isolated trees receive the same inputs, 1280×720, each packet's
original focal length, a one-pixel budget and an observer box with 64 m half
extent. Offsets of 10/50/100 km along local geocentric up are additional declared
poses, not substitutions for the ground scenario. All 24 cuts independently
cover every original address exactly once: 50,816 checked addresses across
repeated poses/methods, without losses or duplicates within any cut.
Cell-isolated means this same Python predicate on partitioned inputs; it is not
the native `SelectCell`/Whole algorithm. Cut descriptors are not draw calls.

| Cohort source count | Height offset | Shared proxy nodes / members | Cell-isolated proxy nodes / members |
| ---: | ---: | ---: | ---: |
| 4,141 | 0 and 10 km | 0 / 0 | 0 / 0 |
| 1,754 | 0 and 10 km | 0 / 0 | 0 / 0 |
| 457 | 0 and 10 km | 0 / 0 | 0 / 0 |
| 4,141 | 50 km | 928 / 2,052 | 817 / 1,883 |
| 1,754 | 50 km | 130 / 286 | 138 / 299 |
| 457 | 50 km | 5 / 11 | 6 / 14 |
| 4,141 | 100 km | 1,007 / 3,576 | 965 / 3,523 |
| 1,754 | 100 km | 373 / 1,038 | 348 / 1,001 |
| 457 | 100 km | 19 / 45 | 24 / 54 |

The direct leaf checks also find **zero eligible leaf boxes** passing the rule
at the original eyes or at +10 km. By the monotonic argument, merely changing
the bounding-box hierarchy cannot produce a ground-view candidate under this
same rule. This is a useful failure of the box recipe, not evidence against
source-complete hierarchies. Cell boundaries also do not define an optimal
spatial grouping; the median trees differ in both directions in these examples.

At the original eyes shared queries perform 12,605 node tests, versus 12,451 for
the isolated trees, with no admitted proxy. Retained NumPy array payload across
the three trees is 1,465,384 versus 1,454,296 bytes. Those figures include the
reference's Float64 bounds, 80-byte nodes, source order/position and eligibility
prefixes; they are not a proposed native storage format. Diagnostic geometry
copies 864 returned bytes per twelve-triangle box, separately counted after
selection. Original-plan geometry, SourceAsset decode and native allocations
are not avoided or measured by this JSON reader.

The complete verification process peaks at 236,395,406 tracked heap bytes and
545,140 KiB RSS, including Python, JSON, both tree sets, reports and tracking.
100,808 tracked **live** allocations are not cumulative allocation events.
No native CPU/GPU or frame-time inference follows. Eight focused tests cover
budget/unknown/protected fallbacks, exact address coverage, projection and
inside/near cases, rotation/observer sampling, stale-domain/profile rejection
and generated-box containment.

## Reproduction and next requirement

```
PYTHONPATH=lib/src .venv/bin/python -m unittest discover \
  -s test/experiments/render -p test_far_product_cut.py
PYTHONPATH=lib/src nice -n 10 .venv/bin/python \
  test/experiments/render/compare_far_product_cut.py \
  --surfaces <original-#801-packet.json> <more-packets.json> \
  --output /tmp/outshine-research-far-cut-<commit>-comparison
PYTHONPATH=lib/src .venv/bin/python test/experiments/render/plot_far_product_cut.py \
  --report <comparison/report.json> --output /tmp/outshine-far-cut-diagnostic.png
```

The local report binds input/source hashes and retains original source-catalog
hashes and proxy membership. The local scientific plot illustrates projection
and geometric candidate counts; it is not a rendered Outshine world image.
No images are published. The current native world, lighting, coverage and device
gates remain unchanged and open.
Dependencies are the existing NumPy/`outshine_ref` environment and Matplotlib
for the optional scientific diagnostic; no package installation is introduced.

Decision: do not implement a ground-view box-product pipeline or tune another
bounding-box hierarchy for this recipe. The missing parent input is a **source-
complete non-box product with a tighter certified source-to-parent and parent-
to-source error**, plus its own coverage/appearance/validity contract. A source's
`ShellErrorM` is not a merged-parent error or an opacity certificate. Reuse the
checked source-address cut and safe fallback semantics when that product exists.
The existing native `StructureBuildTask` already has a source-key-qualified
`StructureSurfaceRefinementTask` for reference/variant geometry. A read-only
schema-10 inventory of all 157 existing frozen building products finds **no
stored surface-error interval** in these inputs. That distinguishes an available
bound-producing procedure from a parent certificate available before selection.
Source selection before payload decode, generation, upload and cache-cell
emission remains the actual #12/#10 objective.
