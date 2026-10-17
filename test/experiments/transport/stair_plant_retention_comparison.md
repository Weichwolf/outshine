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
remain at the paths pinned by the producer report. Both old and fresh whole private 1280×720 images were opened. Camera, profile and
16 samples match. Fresh Blender5.2.2/llvmpipe rendering completed in201.358s;
image SHA256 `6c12aaf4baffc6e3d6010882df23a8df0184d85a5c4bfd6055f75061531e8a5b`.
The existing `test/scripts/pixels.py` reports seven changed pixels, worst channel
change6/255, bounding box[579,485,581,487]. This is stable placement preservation,
not a substantial whole-tile visual improvement or integrated world acceptance.

Separate Memray1.20.0 traces on `c4e713b9b` capture Python allocator events and
intercepted native allocations (native stack traces disabled), with tracemalloc
also active; profiled timings are not independent performance evidence:

| Method | Allocation events | Cumulative allocated bytes | Tracked peak bytes |
|---|---:|---:|---:|
| Global | 6,033,239 | 786,247,398 | 23,745,396 |
| Local | 913,942 | 86,140,086 | 1,082,282 |

These are allocations, not copied-byte counts or complete OS/device memory.
Explicit compressed-prototype payload copies are zero on this filesystem;
other internal copies remain unmeasured. Both profiled results satisfy the same
four-site field/counter comparison. General affine XY/Float32 safety, whole-world
coverage and native integration remain separate open work in #568/#434.
Raw profiles, readers, pixel report and render receipt are retained in the private
root above. `make lint-docs` passes its AGENTS-only scope; no engine gate is implied.
