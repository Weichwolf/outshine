# Bounded continuous surface witnesses (#12)

This completes the already running mesh comparison. Further proof refinement
and mesh parameter sweeps are parked per [master's updated priority](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6259935181):
compare substantially smaller source-owned hulls with layered depth/normal/material
products, retaining dynamic lighting. No native implementation is recommended
from this comparison.

`directed_bound` bounds complete triangle point sets, including interiors.
For each source patch it proposes one target triangle and three target points
with nonnegative integer barycentric weights summing to `2^30`. If source patch
vertices `p_i` and those target points `q_i` satisfy `|p_i-q_i| <= e`, every
`sum(a_i*p_i)` has a point `sum(a_i*q_i)` in that **same convex target triangle**
within `e`. Longest-edge bisections partition source triangles exactly, using
dyadic source weights with depth at most 24. The maximum over a complete set of
patches bounds the directed surface distance. Both directions must complete.

Nearest-triangle searches and Trimesh closest-point calculations only propose
witnesses. The convex integer weights remain valid even after failed/NaN
proposals. Outward intervals compute the distances; `check_bound` independently
reconstructs the entire partition and checks every published witness using exact
rational squared distances. Missing leaves, unsigned overflow, false exact
matches and incomplete proofs cannot publish a bound. Budget/depth exhaustion
is **inconclusive**, not proof that the candidate violates the tolerance.
The result concerns positions, independently of winding or material semantics.

SIGGRAPH-first basis: [Simplification Envelopes (1996)](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf)
and [Garland/Heckbert (1997)](../../../doc/references/geometry/siggraph/1997-quadric-error-metrics.pdf),
PDF page 2 visually inspected. The convex witness derivation here is our own;
the proposal generator is [fast-simplification 0.2.0](https://pyvista.github.io/fast-simplification/).
Its quadric metric or `lossless` option is not accepted as a maximum-error proof.
There is no global optimality claim or equivalence to the envelope algorithm.

## Actual completed comparison

All 5,752 original level-0 cluster ranges partition the same 714,098 triangles
from 157 SHA-bound native products/314 streams used by [1e3aaef84](surface_parent.md).
These remain three Vienna cohorts at one original eye, not three places or a
complete world. The original product/stream/index ranges and cell catalogs are
retained; they do not establish per-triangle source ownership. The native
source/part/multi-source boundary is [confirmed separately](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6259673828).

0.5 m grid and 50% QEM proposals are tested at 1280×720, original focal length,
one pixel, the entire 64 m half-extent observer box and arbitrary rotation.
The final outward `projection_error` test is authoritative. The grid's existing
affine proof is reused when adequate; the new proof is attempted otherwise.
Every incomplete or unsuitable proposal retains its complete original range.

| Recipe | Selected triangles | Incremental reduction |
| --- | ---: | ---: |
| Whole-stream grid, previous comparison | 712,275 | 0.255% |
| Cluster grid, existing affine proof | 707,381 | 0.941% |
| Same grid plus convex proof | 707,317 | 0.950% |
| Cluster QEM plus convex proof | 714,072 | 0.00364% |
| Best accepted grid/QEM plus original fallback | 707,293 | 0.953% |

The stronger proof adds only 88 triangles of reduction over the cluster affine
baseline. It does not justify a native mesh-parent pipeline. There are five
completed additional grid pairs and two QEM pairs; complete directed proofs
audit 1,656 rational corner witnesses/552 leaf patches plus 1,605 exact triangle
matches, including first directions whose reverse proof remains incomplete.
Partial proof leaves never constitute an accepted parent.

These are **incremental** errors relative to already selected cached meshes.
Any existing Fine→Shell/parent error must be added outward before a total LOD
decision. Actual single-source control: cohort bbd9, cell 4, original source
ordinal 36 (anonymous ID 0), wall stream 42→20 triangles. Its incremental bound
is 0.4649047723 m/0.91723 px. Adding its native 0.25 m Shell bound gives
0.7149047723 m and 1.41369 px using its original envelope: no one-pixel Fine→new
parent admission. A missing reference error must not silently become zero.

The 24 actual depth pairs include rotation and 32 m translation. Sixteen are
nonempty, eight partial-cohort views are empty. There are 25 lost and two added
covered pixel samples; the viewed addition crop explicitly prevents an opaque
coverage claim. Normals, UVs, colours, material compatibility, shadows, collision,
native resource/LOD publication and world/device budgets remain unproved.

## Costs and reproduction

The compact-input run takes 411.69 s, peak RSS 173,052 KiB. It performs
158,868,096 corner witness tests and 13,263,764 KD queries; rational audits are
separate. Identified repeated target-triangle arrays materialize 72 bytes per
corner test, 11,438,502,912 B cumulatively, not peak memory. A separate 128→64
QEM proof sample peaks at 3,210,716 traced bytes/907 live traced blocks and ends
inconclusively. Tracemalloc does not count all library-native allocations.

Compacting referenced input vertices preserves their original order. All 11,504
non-timing comparison records and all 314 compound mesh bit patterns match the
pre-compaction run. Two identified Float64 position copies fall from
42,232,482,768 to 72,668,544 cumulative bytes, plus an 18,167,136 B Float32 gather.
The grid construction total falls from 51.85 to 23.64 s. These Python costs are
not native timings, GPU vertex invocations, cache-residency savings or an FPS claim.

Use the existing `.venv` with NumPy/SciPy/Trimesh/Pillow and the additional
`fast-simplification==0.2.0` package. Explicit fresh system-temp outputs are required.

```sh
.venv/bin/python -m unittest discover -s test/experiments/render -p test_convex_surface_bound.py
nice -n 10 .venv/bin/python test/experiments/render/compare_convex_parent.py \
  --inputs "$INPUT_SNAPSHOT" --output "$TMP_OUTPUT/comparison" --max-tests 15000
nice -n 10 .venv/bin/python test/experiments/render/plot_convex_parent.py \
  --inputs "$INPUT_SNAPSHOT" --comparison "$TMP_OUTPUT/comparison" \
  --output "$TMP_OUTPUT/images"
```
