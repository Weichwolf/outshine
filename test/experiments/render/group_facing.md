# Normal groups inside original clusters (#12)

This extends [the static-facing comparison](static_facing.md). The reference
uses the same original meshes, quantized eight-byte cone, exact facing oracle,
original bounding spheres and safe runtime-state fallbacks.

## Why change the granularity

On all 714,098 building triangles at the three saved eyes, the exact oracle
allows only 2/0/4 triangles to disappear by removing entire original clusters.
Thus **18 index references is an upper bound for whole-cluster facing removal
on precisely these inputs and cameras**, even with an exact replacement for
the cone. This is not an optimality claim about other cameras or clusterings.
All original triangle normals are provably nondegenerate in this input set.

A 24-cluster-per-cohort pilot compares contiguous 128/32/16/8/4/1-triangle
ranges, with original cluster spheres and no reordering. Building eight-triangle
ranges remove 0/0/24 triangles from 2,682/3,072/2,954 pilot triangles; storing
one cone per triangle recovers almost the exact backface count but costs eight
bytes and a cone query per triangle. Contiguous splitting is therefore rejected
as the next building candidate. The pilot chooses 24 evenly spaced indices in
the original `(model, cluster)` enumeration for each cohort; no camera tuning.

The relevant primary reference remains the local [SIGGRAPH 2015 course](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
PDF pages 18–20, now inspected visually, separate cluster selection, triangle
masks and index compaction while keeping instance draws. Pages 22–24 informed
the earlier static-mask comparison. Normal-bin assignment below is our reference
variant; the paper does not prove this variant's quality or costs.

## Partition, bound, select

`group_facing.py` compares two deterministic partitions **within each original
cluster**, without moving its indices or splitting its batch:

- `faces6`: dominant absolute geometric-normal component, plus its sign;
- `octants8`: three component-sign bits.

Only occupied groups exist. Approximate normals choose membership, not
visibility: each group's cone is independently bounded against its actual
triangle positions by the existing outward-arithmetic implementation. A bad
axis, degeneracy or unsupported state disables exclusion for that group.
Each group stores a 128-bit mask of its triangles' **original** positions.
Groups partition the original positions. Start with all positions retained;
clear a group's membership only if its cone proves every member back-facing.
Consequently a removed position belongs to a group whose complete exclusion
is proved. The resulting bitmask filters the original index stream in its
original order; source/vertex/material references and existing batch/job
grouping remain unchanged. No per-group draw or new native cluster is proposed.

Original-cluster frustum/occlusion and the same whole-cluster sphere/inside
fallback still apply. A narrow group's sphere is not introduced to bypass the
requested inside-cluster fallback. Eligibility and camera conventions remain
those of the previous comparison. Shadow/reflection consumers need their own
view query; logical/collision geometry stays complete.

## Explicit candidate cost model

One possible encoding has an eight-byte cluster directory `(firstGroup,count)`
and, per occupied group, eight cone bytes plus sixteen membership bytes. A
flat array of six uint words uses 24 bytes per group; a struct with 16-byte
alignment may need 32. Both totals appear in the report. A six-group cluster
therefore costs at most 152/200 bytes including directory; eight groups at
most 200/264. A short final cluster still pays the full membership mask.
These are additional candidate payloads, not established native cache layouts.

The two reserved words of the current 48-byte GPU cluster table might hold
the directory, if both native consumers agree on bit-preserving access. That
would reuse those eight physical table bytes, rather than add eight. Group
arrays, cache version, bindings, lifetime and transfer remain undecided. This
experiment changes none of them and adds no export pipeline.

An eligible outside query reads the cone of each occupied group. It reads a
membership mask only for a proved rejected group, clears four words and writes
one 16-byte retained-triangle bitmap per original cluster. Counting retained
bits supplies the existing cluster count/prefix scan. For partial clusters,
stable index compaction additionally tests triangle bits and computes ranks;
the report budgets at most four 32-bit popcounts per triangle. Full keep/reject
can retain the existing fast paths. A possible global visibility scratch array
is 16 bytes per original cluster. Partial compaction must also read its bitmap;
physical loads, caching, shared memory and barriers require native measurement.
Candidate output-index writes are twelve bytes per retained triangle, as in
the earlier compaction model. No original geometry allocation or index reorder
is needed; native allocation events and copy bandwidth remain unmeasured.
Payload can be shared by instances of the same mesh, but queries and retained
visibility are per instance and consuming view. This prototype/cell comparison
does not establish their multiplicity in the native audit.

Python intentionally calls the shared complete cone/mask reference, including
unused static masks. Its time and process RSS describe this verification run,
not a proposed GPU instruction sequence or native asset-build cost. The ledger
counts the proposed cone/membership accesses separately; it does not convert
them into milliseconds or hardware vertex-shader invocations.

## Reproduction and acceptance boundary

```
.venv/bin/python -m unittest discover -s test/experiments/render -p 'test_*facing.py'
nice -n 10 .venv/bin/python test/experiments/render/compare_group_facing.py \
  --buildings /path/to/existing/frozen-inputs \
  --transport /path/to/existing/resident102 \
  --output /tmp/outshine-research-group-facing-<commit>
```

Both partitions run against every original cluster at its saved building eye,
six near/far axis directions, eight cube corners and centre. The exact oracle
is shared across variants for each camera; unknown-state checks really KEEP.
Independent scalar dyadic tests cover random geometry, the full 360-degree
orbit, seams, near/far, original-position membership, safe fallbacks and costs.
At fixed eye, all yaw orientations have identical facing selection. Four
near-clipped 1280×720 depth views per selected actual mesh compare original,
six-group and eight-group selection. These use explicit tight diagnostic
poses; saved-eye quantitative results are separate. Image selection callbacks
reuse the earlier rasterizer and preserve its old default comparison path.

The output report retains source hashes, input-manifest hashes, original
provenance, reference batch groups, query/copy/metadata counts, Python time and
process RSS. Actual runtime material/cull/motion/instance state is still absent;
native camera/transform arithmetic and complete per-triangle building source
ownership remain unproved. All reported removal is conditional. PBR, alpha,
shadows, world coverage, LOD, native GPU time and both device gates remain open.

## Complete Vienna comparison

All three complete cache cohorts use the same Vienna location (48.233362,
16.411041), not three independent geographic cases. On their 157 products,
714,098 triangles and 5,752 original clusters, saved-eye results are:

| Parent prefix | Original index references | Six-group retained | Eight-group retained |
| --- | ---: | ---: | ---: |
| bbd9aab74311 | 1,403,151 | 753,663 (46.288% removed) | 732,060 (47.827%) |
| 6d41e63e7328 | 635,229 | 399,099 (37.172%) | 376,497 (40.731%) |
| 035e2150a3c1 | 103,914 | 89,085 (14.270%) | 87,939 (15.373%) |

Combined, six groups remove 42.032% of index references and 41.675% of summed
cluster-local unique referenced vertex IDs. Eight groups remove 44.149% and
43.637%. The earlier unbounded static masks remove 3.649% at these same eyes;
whole-cluster exact facing can remove at most 18 index references. These are
conditional pre-frustum/occlusion geometric counts, not native VS invocations.

| Complete Vienna ledger | Six groups | Eight groups |
| --- | ---: | ---: |
| Occupied groups | 30,755 | 31,999 |
| Packed payload including directories (bytes) | 784,136 | 813,992 |
| Payload with 32-byte group alignment (bytes) | 1,030,176 | 1,069,984 |
| Saved-eye directory/cone/membership read bytes | 501,616 | 530,184 |
| Saved-eye cone queries | 30,626 | 31,871 |
| Bitmap writes (bytes) | 92,032 | 92,032 |
| Partial-compaction triangle bit tests | 666,739 | 665,097 |
| Rank popcounts, stated upper bound | 2,666,956 | 2,660,388 |
| Retained index write bytes | 4,967,388 | 4,785,984 |

Original index writes are 8,569,176 bytes. Reusing the directory words in the
existing GPU table would subtract 46,016 bytes from the incremental resident
payload above; cache encoding remains a separate decision. Visibility scratch
is 92,032 bytes for these original clusters. Bitmap reads during partial
compaction and actual native scheduling/bandwidth remain additional costs.

The 24 transport bodies add 19,288 triangles and 163 reference ranges. Their
six-group payload is 10,640/13,752 bytes (packed/aligned), versus eight-group
18,056/23,640. Across far axis cameras, removals are 16.772%/15.850%; at cube
corners 39.819%/38.479%. Transport therefore does not share a universal eight-
group advantage. There is no saved native transport eye or native cluster proof.

The full two-variant comparison completed 259,934 cluster-camera checks with
zero visible/tangent removals. Python geometry verification summed 574.40 s
under `nice -n 10`; peak process RSS was 201,920 KiB, including Python, oracle,
retained reports and raster scratch. Four selected original/cone-group depth
triples at four yaw views are unchanged. The pre-existing default renderer
also produced twelve byte-identical PNGs against commit `a66a79f3f`.

Six groups are the lower-work baseline candidate; eight groups remain a
comparison for the dense-city tradeoff. Neither has measured native net benefit
or dominates every case. Agree native eligibility, camera/transform precision,
directory/group encoding and bitmap/compaction lifetime before an implementation
or device A/B; preserve current frustum, occlusion, batches, source/material
association and all consumers. The source-complete pre-emission hierarchy and
far products remain the larger #12 objective.

## Existing geographic counterexamples

Read-only links combine the existing `geographic-products` artifact manifest
with its original SHA-checked basis files; no native export/cache generation.
The original geographic manifest and missing-cell lists remain in the linked
input manifest. Körbersee supplies all eleven expected form cells (444 triangles,
22 clusters); Flensburg supplies **39/64** expected cells (42,152 triangles,
364 clusters). Twenty-five missing Flensburg cells are not evidence of emptiness.

At saved eyes, six/eight groups remove 30.631%/47.973% of Körbersee index
references and 21.933%/23.456% of the **available** Flensburg references. Summed
cluster-local unique vertex removals are 28.555%/45.824% and 20.015%/21.485%.
These results do not establish complete Flensburg coverage or world performance.
The additional run completes 23,830 two-variant checks without false removals,
including repeated original transport controls with exactly equal numeric
results. It takes 39.66 s of Python geometry work and 95,776 KiB peak RSS.
The two geographic runs therefore total 283,764 variant checks; this includes
6,846 repeated transport checks, rather than that many independent new inputs.
Tight Körbersee and small-transport views supplement the automatic mesh images;
all compared diagnostic depths remain unchanged. No reference image is published.

Per [master's priority decision](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6258747087),
the ROI comparison stops here. This checked reference is parked without native
implementation; source-complete hierarchy/far products before cell emission
take priority over further facing variants or Whole-capture diagnosis.
