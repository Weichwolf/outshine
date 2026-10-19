# Non-box surface parents with continuous geometric witnesses (#12)

This experiment supplies a geometry-only parent recipe and a conservative
perspective test. It does not supply a source-complete native parent product,
coverage/material acceptance, selection before decode, or a device result.

## Procedure and proof

SIGGRAPH-first basis: [Simplification Envelopes (1996)](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf)
and [Garland/Heckbert (1997)](../../../doc/references/geometry/siggraph/1997-quadric-error-metrics.pdf).
The latter's PDF page 2 was visually inspected, including the contraction
diagrams and vertex-clustering discussion. It describes grid dependence and
quality limitations. This is a vertex-clustering baseline with an additional
explicit witness, not QEM or the published envelope algorithm. QEM's approximate
metric alone would not prove our two-sided maximum displacement requirement.

`cluster_parent` receives a complete indexed position stream. Only referenced
vertices participate in a grid; each cluster chooses an original encoded
position. Unused vertex-buffer entries cannot expand the reference enclosure.
Each original triangle gets exactly one of these witnesses:

1. Its unchanged original triangle is retained.
2. Its affine image is retained, or is an oriented duplicate of a retained image.
3. Its affine image is a point/segment whose endpoints are vertices of one
   retained image triangle. That entire convex image is therefore present.

A collapsed image without the third witness retains its original triangle.
This includes an entire isolated component collapsing into one grid cell.
Three distinct collinear image vertices also retain the original triangle;
there is no unproved segment-containment inference. Normal interval tests reject
uncertain degeneracy and large normal reversal before image retention. Opposite
windings are separate duplicate keys. These checks do not prove topology,
watertightness, view-dependent facing equality or attribute compatibility.

For original vertices `p_i` and their chosen encoded images `q_i`, every point
of a triangle has barycentric weights `a_i >= 0`, `sum(a_i)=1`. Its affine image
has the same weights. Thus

```
|sum(a_i*p_i) - sum(a_i*q_i)| <= max_i |p_i-q_i| <= e.
```

The witnesses ensure **all** affine image points exist in the parent, including
images of deleted triangles. Conversely, every parent triangle is either an
original or the affine image of an original triangle. Both directed continuous
surface distances are bounded by `e`. This proof concerns full triangle point
sets, not just sampled vertices. The maximum corner displacement is computed
with outward Float64 intervals on the represented input positions. Output
positions reuse original encodings; no extra Float32 placement is presumed.

`check_parent` independently audits every original/output triangle witness and
corner displacement. Tests additionally use rational squared distances and an
independent Trimesh point-to-triangle oracle, including interior samples. The
samples supplement the continuous proof; they do not replace it.

## Perspective and complete fallback

`projection_error` reuses the derivation in [far_product_cut.md](far_product_cut.md)
with the independently proved `e` replacing the enclosure diameter:

```
pixel_error <= f*c*e / (d/c-e),  c=sqrt(1+(width/(2*f))^2+(height/(2*f))^2).
```

The enclosure still contains reference **and** parent; `d` is its minimum
distance to the whole observer box. The denominator must exceed the near plane.
This test holds through arbitrary orthonormal rotation within that box. Near
uncertainty, invalid input, a failed witness or exhausted offline triangle budget
retains the original product. A geometric bound does not guarantee conservative
opaque coverage, pixel equality, openings or unchanged shading.

The source-complete catalog cut from `far_product_cut` remains unchanged. This
experiment retains original product/stream/cell catalog identities and original
triangle addresses. It **does not** turn a cell's source-ID list into ownership
of individual triangles or use these mesh parents to remove source records.
No unknown/unsupported source is declared represented by a parent here.

## Measured decision

All 942 stream/grid comparisons audit all their reference triangles: 2,142,294
triangle witnesses in total, with no unrepresented image. The same 714,098
original triangles are used at every grid size. With 1280×720, the original
1044.33155 px focal length, a one-pixel error limit and a 64 m half-extent
observer box around the unchanged saved eye:

| Grid cell (m) | All candidate triangles | Geometry-selected triangles | Reduction after bound |
| ---: | ---: | ---: | ---: |
| 0.125 | 710,749 | 713,446 | 0.091% |
| 0.5 | 512,305 | 712,275 | 0.255% |
| 2 | 254,364 | 710,321 | 0.529% |

At 0.5 m the unconstrained count falls by 28.258%, but almost all that gain is
ineligible under the whole-product maximum error and distance bound. This
measurement does **not** justify a native grid-parent pipeline. The far-only
035e cohort has more admissible geometry reduction; it must not stand in for
the dense near cohorts. Do not loosen the pixel budget or omit nearby sources.
The next useful comparison needs smaller source-owned spatial parents and a
tighter surface/appearance bound, rather than more whole-product grid settings.

At 0.5 m, 408 triangles remain original because a safe image/collapse witness is
missing; 201,030 collapsed images have explicit surviving convex witnesses.
The 763 duplicate images remain geometry-only eliminations: different source
materials/UVs at the same positions must not be discarded without their contract.

The depth comparison uses the same original geometry, both saved and 32 m
translated eyes, and four diagnostic yaw directions per cohort. These directions
are defined by the cohort centroid, not asserted to be the original scenario's
camera orientation. Sixteen of 24 pairs are nonempty; eight views of partial
cohorts are empty and provide no preservation evidence. Across the nonempty
pairs, 24 covered pixel samples are lost and none gained. Shared covered samples
can change depth by up to 373.99 m at disocclusions. The viewed before/after and
difference images therefore demonstrate why point-set proximity is not a
coverage or depth-equality certificate. This is not a current Place/world gate.
An independent direct perspective check of 346,103 visible corner pairs stays
below 0.68550 px, including the moved views. The lost-sample crop was also viewed;
the geometry bound is not being treated as a guarantee of equal raster coverage.

The full comparison took 95.12 s including independent audits, with process peak
RSS 243,276 KiB under concurrent CPU work (`nice -n 10`). Individual offline bake
totals over the three cohorts were 14.53/12.36/11.96 s for the three grid sizes.
For the largest original stream (124,241 triangles), the separate 0.5 m traced
allocation run peaked at 158,463,534 B, with 7,134 live traced blocks at capture.
Its two identified corner gathers copy 8,945,352 B each; witness arrays occupy
5,040,149 B. These are Python construction costs, not native asset-cache bytes,
cumulative allocation counts, frame costs or a device budget proof.

## Native inputs and integration boundary

The input is the unchanged SHA-bound native cache snapshot: 157 products,
314 nonempty wall/roof streams and 714,098 triangles from three Vienna cohorts.
All cohorts share one saved eye; these are not three geographic places or a
complete world. Native level-0 ranges independently partition each stream.
The proposed parent changes only diagnostic positions/indices; original
normals, UV0, optional wall metres and colours remain in the frozen inputs.
Their semantics are not certified or included in geometry-only byte savings.

The actual current `StructureSurfaceErrorTask` on `origin/master` was inspected:
its per-corner distance to one target anchor provides a valid coarse bound, not
this affine witness's tighter bound. No persisted surface-error certificate is
present in any of these 157 frozen products. No native error gate is relaxed or
claimed executed for a newly created parent. This witness needs a checked native
equivalent or another independently conservative native surface-distance bound.

For a source-complete pre-emission parent, the missing minimal input is the
original source index/part and selected detail/openings mapped to output index
ranges **before** cluster packing, bound to the actual emitted product. This
allows building spatial parents across cache-cell boundaries while retaining
unknown/failed sources as original references. Source/part material seams and
the permitted appearance approximation must accompany that grouping. Per-source
`ShellErrorM` alone proves neither a merged-parent bound nor opacity. Contacts,
collision, shadows/reflections and stream publication remain separate consumers.

## Reproduction

Use the existing `.venv` with NumPy, Pillow and Trimesh; no new Python package is
required. Explicit outputs must be fresh system-temp directories.

```sh
.venv/bin/python -m unittest discover -s test/experiments/render -p test_clustered_surface_parent.py
.venv/bin/python -m unittest discover -s test/experiments/render -p test_far_product_cut.py
.venv/bin/python test/experiments/render/compare_surface_parent.py \
  --inputs "$INPUT_SNAPSHOT" --output "$TMP_OUTPUT/comparison" --cell-m .125 .5 2
.venv/bin/python test/experiments/render/plot_surface_parent.py \
  --inputs "$INPUT_SNAPSHOT" --comparison "$TMP_OUTPUT/comparison" \
  --output "$TMP_OUTPUT/images" --cell-m .5
```

The report binds input manifest and code hashes. `--allocations --limit N`
separately records traced live allocation blocks/current/peak; this is not a
cumulative allocation count or all native allocator traffic. Work records count
corners, normal tests, edge inserts, collapses and retained triangles. Identified
gather bytes, witness payload, geometry payload and process peak RSS are separate;
the listed arrays are not a claim to account for every scratch copy/allocation.
Python bake times include offline construction only and are not native runtime
or GPU estimates. Cached parents must load as finished assets after selection.
