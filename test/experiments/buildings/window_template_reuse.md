# Reuse existing templates within one detail update

[#1073](https://github.com/Weichwolf/outshine/issues/1073), under #1052/#33.
The explicit-ID detail manager split the old batch recipe into individual closed
windows. `opening_details.expand` reused frame/casing/sill templates only within
one call, so every entering window rebuilt an identical template. This change
passes an optional template builder through the existing helpers and shares those
same raw components within one update. No geometry algorithm or asset format changes.

## Scope, lifetime and fallback

`WindowDetails.update` first checks admitted **new** windows for repeated width/height
pairs. The window kind, ring method and sill choice are fixed by this existing
recipe. If there is no repeated pair, no shared context is created. Otherwise an
ordinary `functools.lru_cache(maxsize=16)` wraps the existing template constructor.
Its complete key remains kind, width, height, method and sill choice. It is lazy;
existing resident windows and capacity fallbacks request no template or geometry.

The context is local to the update and explicitly cleared in `finally`, including
partial failures. Unchanged demand allocates no template context, and departing
products retain the previous ownership rules. At most sixteen raw templates are
retained by the context; evicted variants are rebuilt normally. There is no global
history, persistent cache, asset DB or native/frame dependency. Factory-produced
raw arrays are read only by placement/concatenation; output draw arrays own their
frozen storage and do not retain template arrays. Existing callers without a
builder keep the original within-call reuse and numerical recipe.

All geometry, closed parts, normals, UVs, material values, stable identities and
complete coarse exterior remain unchanged. Shadows, logical floors/rooms, function,
physical state and general LOD/radiance policy retain their original owners.
This is generator work; it does not propose running GEOS in the native frame path.

## Matched costs

Scientific producer `55edd94e536489daab24652925fd890233e3f053`, based on #1053
`97862e716` containing Master `ee7bbd775`. The baseline manager is loaded from that
exact Git object and its source SHA recorded, using the same recipe dependencies.
Three fresh normal processes alternate method order; a fourth traces allocations.
Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0/GEOS 3.14.1, Linux/i5-1135G7 shared host,
one BLAS/OMP thread. Measured starting 1-minute host loads were 8.72/9.07/9.52.
These are Python generator trends; actual work avoided is the stronger evidence.

Seven explicit diagnostic states select eight windows each on three original
sources. The same 14/17/8 entering patches are built by both managers; unaltered
resident references are reused. The existing original batched full recipe remains
a separate cheaper control in #1053; these numbers compare only singleton managers.

| Source | Template builds, original/shared | Seven-state ms, original/shared median | Raw template arrays built, original/shared B |
|---|---:|---:|---:|
| Original two-court cascade |14/7|62.85/41.92|93520/46760|
| Original concave paired-wing |17/11|78.63/58.61|113560/73480|
| Original compact cascade |8/2|38.69/17.93|53440/13360|

Individual totals span 61.45–72.16/41.90–60.72 ms, 70.21–89.39/52.66–72.64 ms
and 29.40–41.74/14.01–19.43 ms respectively. Coarse construction is unchanged and
separately costs 194–257 /263–328 /124–154 ms across these normal processes;
coarse arrays occupy 1507594/1733052/617438 B. Logical plan preparation is separately
reported. Cold first-update times are also recorded; no frame-budget admission.

Final resident detail arrays are identical: 249088/222400/249088 B. Newly generated
frozen detail buffers likewise stay 435904/472600/249088 B over the sequence. This
is work avoidance, not smaller retained products. Shared incremental traced detail
peaks are 324432/301236/324208 B versus 309160/278800/309216 B without sharing:
roughly 15–22 KB more temporary Python-visible storage buys fewer template builds.
The normal raw template has 6680 B in these cases, before Python container overhead.
Raw construction-byte totals omit other allocation/copy traffic and native GEOS heaps.
Whole-process RSS 133.3–140.1 MB includes both methods, imports, plans and diagnostics;
it is not an application/device memory budget or per-method residency measurement.

A constructed 21-window control gives every window a different width, then requests
all/repeat/empty/all. Both methods build 42 templates and produce the same fields;
the final path creates no shared context. Its 187.84–207.74/137.73–208.11 ms totals
are noisy shared-host controls, not an algorithmic speedup. A preceding always-cache
control retained unused templates and raised its traced peak from 757178 to 860510 B.
The no-repeat bypass removes that cost; final peaks are 757266/741014 B, with allocator
variation. Twenty different keys followed by the evicted first key independently
check the sixteen-entry bound and exact ordinary reconstruction after eviction.

## Correctness and actual picture

Fourteen targeted tests pass on this frozen source (78.452 s). Eight unchanged
#1072 extent-consumer tests also pass (12.394 s), loaded from that PR's frozen Git
objects with the current detail implementation. This verifies the shared Python
transition; no native execution is implied.

All 2936 original windows from five provider footprints × two form plans are checked
against unshared raw parts and every draw field. The original eight detail tests
still compare against the complete old batch recipe, closed volume/edge ownership,
full coarse hull, capacity, motion, releases and rollback. New checks cover local
context lifetime, collection of raw buffers, failure clearing, sixteen-key eviction,
all-distinct/single-entry bypass, and original ring/bars/window/entrance/terrace/sill
expansion variants. No snap, tolerance change or warning suppression.

A fresh saved-product reader compares 432 array fields across both methods and all
four final processes, including dtype/shape and exact words, with original-source
opens forbidden and generator modules absent. All agree. The actual
source/camera/material/draw inputs also match the original #1053 Near rendering
exactly. Its original image/report/mesh hashes were checked and the 1920×1080,
16-sample Eevee picture was opened again. Producer remains `7cfdaedb3`, software
llvmpipe: this is a verified reuse of that picture, not a new rendering or GPU proof.
It shows the same frame, pane and sill; no improved glass/interior/world appearance
is claimed and no images are published.

## Reproduce and remaining work

Existing `.venv`, no new packages. Set `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`
and `PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets`.
Run `python -B -W error -m unittest test_window_detail_demand test_window_template_reuse -q`.
On frozen sources run `compare_window_templates.py --output <fresh-system-temp>`,
repeat twice (one `--shared-first`) and separately `--trace --shared-first`.
The fixed baseline Git object must be present. Local source reproducer, raw reports,
source/mesh/image hashes, reader and receipt remain linked from the PR.
A preliminary driver setup failure had no measurement window; its original-plan
camera dependency is corrected, with compact failure evidence retained.

Native adoption, asynchronous generation, scheduling, DB/IO/upload, device memory,
full world quality and both target devices remain open. #1072's stored extent query
is a separate optional consumer, with unchanged IDs/draw contracts. No independent
full native gate or general rendering/LOD acceptance follows from this experiment.

SIGGRAPH-first context: [Procedural Modeling of Buildings, 2006](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
page 4/Figures 5–8, read and visually inspected for reusable components in separate
scopes. The source recipe already implemented this; no new grammar or instancing
representation is claimed.
