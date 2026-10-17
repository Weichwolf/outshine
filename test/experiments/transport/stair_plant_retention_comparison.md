# Stable vegetation sites after transport updates (#646)

The actual Vienna 65→77-source transport update leaves all four previously verified
Pinus sites physically valid. Re-running global proposal priority selected only one
within the same 1024 proposals/128 attempts. Retaining verified source/SITE products
first restores four without increasing the four-plant demand or either work cap.
This is an experiment consumer update, not a new vegetation recipe or native API.

The prior receipt, artifact hashes, physical array ABI, definition and seed are
validated before reuse. Matching source geometry/properties, world frame, original
canonical SITE region, root XYZ and placement contract are required. Current terrain
contact and whole transport/building clearance are checked again. Invalidation has
an explicit reason; new proposals fill only remaining capacity. Local terrain queries
are capped at 2048 faces per source. Whole current terrain loading is still performed.

## Verified current result

Producer `0971b29579d24eef45c59a7dc0cb99f7f9b3dd3d`, report SHA256
`6a5c5c9b1bb48934096ab7ab8fba31eb19cb2ea4664b5946708f618ed89b12cc`:
four unchanged source/SITE IDs and root positions, four attempts, zero proposals.
Source and installed global readers independently re-emit the full prototype,
replay SITE and scan whole transport/building reservations; both pass.
18 affected source and 18 installed controls pass, including changed sources,
frames, definitions, terrain, placement and overhead obstacles, capacity/refill,
world growth, corrupted artifacts and inconsistent ABI. Ruff passes.

400 prototype files (9,966,946 compressed payload bytes) are hardlinked; zero
payload bytes are copied and no organ arrays are materialized by the producer.
Hashing, ZIP header decoding and filesystem operations still cost work.
The complete mesh has 289,094 Float64 vertices and 549,948 uint32 triangles:
13,537,632 logical mesh-array bytes, excluding the Boolean leaf-face mask.

## Equal-result comparison

`stair_plant_retention_compare` compares the same four fixed prior sites and same
cached form/current reservations. Global terrain union runs first, local support
second. Every result field and query work counter is exact after removing only
wall-clock `seconds` from the two query telemetry dictionaries. The initial strict
comparison failed solely on those measured durations; the counterexample is retained.

| Method | Selection seconds | Python-traced peak bytes | Accepted |
|---|---:|---:|---:|
| Global union | 4.214787 | 5,951,497 | 4 |
| Local source support | 1.430520 | 768,202 | 4 |

Local queries return 301 facets across four sources, versus 30,782 input facets for
the global union. Source normalization and SITE graph preparation are included;
input loading, prior artifact validation and physical form emission are excluded.
This single ordered run uses tracemalloc on a loaded WSL2 four-vCPU i5-1135G7 host;
rendering and other agents' analysis run concurrently. Native allocations are not
fully traced. Process-wide peak RSS is 206,553,088 bytes, not a per-method peak.
No same-result speedup is inferred from the former one-plant proposal run, and no
A18/RTX/runtime or general affine geometry admission is claimed.

Private inputs, fresh exports/readers, timing counterexample and comparison output
are retained in `/tmp/outshine-transport-sites-7f4a55d67/`; original prior inputs
remain at the paths pinned by the producer report. Whole private 1280×720 rendering
is in progress. Allocation-event/copied-byte accounting beyond the explicit prototype
reuse counters, final image review and PR delivery remain open in #646.
