# Native road package reuse under movement

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915), research.
This isolates package amplification and temporal reuse on the actual unchanged
[native corridor/junction corpus](native_road_parts.md). It is a Python cost
experiment, not a runtime page producer, visibility solution or device admission.

The complete corpus is packed as one geometry package, as 256 m cells, or as those
cells recursively split along their longest centroid extent with a 65,536-byte
target. Whole native emissions have one owner; no triangle cutting, profile
resampling, geometry simplification or altered source/port identities. One
indivisible 68,656-byte emission exceeds the adaptive target and remains explicit.
The material table is one separate shared package. Package counts including that
table are 2 / 86 / 437. A native emission with zero geometry retains its metadata
and requests no geometry/material payload when it alone is needed.

The private databases use the existing AssetCache schema version 3, including
assets/bounds/packages, offsets, CRC and independent Zstd level-1 frames or raw
bytes. This is **not** native asset serialization: bounds are diagnostic local ENU,
payloads are the exact corpus streams, and hashes.json is verification metadata.
Full source/port/plan metadata remains in the fixed oracle. Future native page
headers, complete contact metadata IO and an ECEF consumer remain unmeasured;
these databases must not be installed into the application cache.

## Demand and lifetime

Each trace has 36 steps: start/hold, 16 moves, hold, return and hold. The corridor
route follows the longest captured core interval, about 652.78 m. Two further
traces translate 300 m through the densest core centroid cell at 150 m and 500 m
query radii. These are declared synthetic planar ROI demands, **not recorded camera
visibility**. Core fine AABB tests and whole incident-junction closure are offline
oracles. Every selected junction has its original complete incident payloads,
gates, planes, cuts and widths checked by the shared verifier (#927).
Outer halo references remain references; this is not a complete functional world.

The worker resolves metadata once, maps required atom IDs to package IDs, then
loads `wanted - resident - pending`. Pending deduction/cancellation is a unit-tested
set operation only; this serial replay has no asynchronous pending jobs. Already
loaded packages and the material table are shared. LRU evicts only unneeded
packages before loading; an insufficient retention target is reported without
dropping required content. No SQL query or payload read is needed for a hold.
Identities are qualified by the frozen archive; they are not global source IDs.

The serial CPU model assumes previous consumers have finished. GPU fences,
parent/child publication, delayed misses, stale jobs, horizon/occlusion validity,
independently prepared render/function/simulation LODs and actual device IO remain
native work. A DB diff must follow package/content versions and all independent
demands; a view change does not invalidate unchanged assets. A ready coarse parent
cannot retire merely because a finer child was requested.

## Measured example, 2026-10-20

Input archive SHA-256: `5aafb7b17b2820135b45ed26ca57a7e48d575e84ee6262ead81fc438713810fb`.
Linux/i5-1135G7, Python 3.13.5, zstandard 0.25.0, fresh worker processes with warm OS
file cache. Other compilation/native probes were running. Times are diagnostic;
none establishes native SSD latency, Metal/Vulkan time, p99 or a world memory budget.
The monolithic comparison contains **this 18,610,944-byte geometry corpus**, not
the full current prepared terrain/road/water package. Each layout has identical
geometry plus the same 2,391-byte material table.

The 2 MiB parameter is a decoded retention target for this comparison, not an
application budget. Decimal MB below. Reads are cumulative compressed payload
bytes; peak is resident payload plus the currently coexisting input/decode output.
It excludes interpreter, SQLite, allocator, metadata and codec scratch; those are
included in the separately observed Linux VmHWM process peak.
The measured worker peaks span 45.77–72.32 MiB. The separate fine-oracle preparer
took 5.30 s and peaked near 150 MiB: 8,358 atoms, 368,832 bound vertices scanned,
592,488 seed AABB tests and 108,586 selected contact-leg checks. These are explicit
offline costs, not the intended runtime coarse selection. A separate full-layout
audit also reads all pages offline against the original 8,359 geometry/material
records; those validation reads are outside the demand replay counters.

| Trace | Layout | Read MB | Known payload/decode peak MB | Requests | Steps whose required residency exceeds target |
|---|---|---:|---:|---:|---:|
| Corridor, 150 m | Monolithic | 5.743 | 24.356 | 2 | 36/36 |
| Corridor, 150 m | Fixed | 0.188 | 0.558 | 9 | 0/36 |
| Corridor, 150 m | Adaptive | 0.125 | 0.371 | 13 | 0/36 |
| Dense, 150 m | Monolithic | 5.743 | 24.356 | 2 | 36/36 |
| Dense, 150 m | Fixed | 1.845 | 4.765 | 12 | 36/36 |
| Dense, 150 m | Adaptive | 1.287 | 2.883 | 118 | 26/36 |
| Dense, 500 m | Monolithic | 5.743 | 24.356 | 2 | 36/36 |
| Dense, 500 m | Fixed | 2.490 | 7.854 | 23 | 36/36 |
| Dense, 500 m | Adaptive | 2.100 | 6.528 | 184 | 36/36 |

All holds have zero payload requests/reads/decodes. The monolithic layout also has
zero moving/return reads after its first load, while retaining the whole corpus:
delta requests alone do not prove a small working set. In the dense trace, fixed
and adaptive layouts reread 4 and 35 packages on return under the 2 MiB target.
With an explicitly separate 8 MiB retention comparison both have zero return reads;
dense-trace cumulative reads become 1.255 MB and 0.890 MB. This does not repair the
failed 2 MiB cases. All layouts exceed that target in the wide demand case.

The adaptive layout reduces bytes but increases request count substantially. Both
spatial layouts remain native measurement candidates; no global/device winner is
selected. Unneeded atoms co-loaded inside a needed page remain charged. Excluded
pages have no BLOB read; tests observe the actual SQLite statements. Every loaded
atom, including co-loaded atoms, is hashed against original bytes without a payload
slice copy. CRC, decompression and hashing work are explicit; verification is
separately timed. Blob materializations, decode allocations, known join/copy bytes,
selection/total CPU time, metadata resolution, OS read bytes and process RSS are
reported. Internal SQLite/codec/interpreter copies and allocation counts are not
fully instrumented; GPU upload/copies are absent. Warm OS physical reads were zero.

## Reproduce

Requires existing Python packages NumPy and zstandard; no vendored dependencies,
network acquisition, source-cache mutation or native build.

```sh
.venv/bin/python -B -W error::ResourceWarning -m unittest discover -s test/experiments/transport -p 'test_native_road*.py'
.venv/bin/python -B test/experiments/transport/native_road_page_replay.py --output /tmp/outshine-research-road-pages-run
.venv/bin/python -B test/experiments/transport/native_road_page_replay.py --database /tmp/outshine-research-road-pages-run/adaptive --trace /tmp/outshine-research-road-pages-run/trace-dense.json --budget-bytes 8388608 --output /tmp/outshine-research-road-pages-run/adaptive/retained-dense.json
```

Use a new output directory. Offline source verification, fine bounds, contact
closure, packing and index construction are charged in the preparer report and
must never be mistaken for free runtime coverage or the intended direct far cook.
Nine new controls plus six upstream controls cover ownership, corrupt data despite
a valid CRC, missing contacts, pending-versus-ready, stillness/return/shared material,
raw-versus-Zstd accounting, empty emissions and insufficient retention.

The [Clipmaps](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
and [GigaVoxels](../../../doc/references/geometry/i3d/2009-gigavoxels-ray-guided-streaming.pdf)
principles motivate spatial reuse and demand before payload work (source review
#922/#919). Historical HDD/separate-VRAM assumptions and published timings are not
transferred to SSD/UMA. This experiment leaves #915's full LOD/coverage/producer and
#10/#12's moving-world admission open.
