# Native light-list construction

Refs #448, goal #190; native raster comparison in Draft PR #458. This standalone
C++23 experiment measures the CPU work missing from that comparison. It does not change
the renderer, shared bindings, shader audit or production 16-light refusal.

## Method and retained semantics

Three builders use the same camera, original 64-byte probe light records,
finite-range spheres, full-screen 64 px cells and either one depth interval or
24 logarithmic intervals with an infinite tail:

- `sort`: enumerate candidate cells, count accepted pairs, sort by cell/world ID.
- `scatter`: enumerate the same pairs, count/prefix/scatter in original-ID order.
- `hierarchy`: before the same scatter, test groups of up to 8 by 8 cells in
  each depth interval. The parent AABB encloses every tested leaf AABB, using
  the same light-far clamp. Rejecting the parent avoids all enclosed leaf tests.

The hierarchy changes work, not the accepted lists. It retains exactly the
flat builder's leaf predicate and original-ID accumulation order. This is a
CPU candidate comparison, not a derivation of a globally optimal hierarchy or
a general FP32 fragment-culling proof. The existing SIGGRAPH/HPG basis and
remaining production culling requirements are owned by #448.

Direction lights remain global even if their unused range field is positive;
point/spot lights with range zero are global too. The Sun keeps its world ID.
Overflow replaces partial lists with all world IDs. The configured index cap
does not cap temporary pairs, vector capacity, all-light fallback or total memory.

`local_light_build.py` prepares the seven original mixed-light cases, then
compares every generated CSR byte with the Python builder and every copied/
rebased light byte with independent NumPy FP64 subtraction/FP32 conversion.
The additional direction-range case changes the unused field and must retain
the original lists. Invalid CLI values and nonfinite/malformed parameter
semantics must return 2. These are private generated controls, not changed pins.

Camera facts are 16 little-endian FP64 values: row-major basis, eye, half extents,
near and depth extent. A separate four-word grid stores pixel width/height,
perspective flag and tile size. FP64 depth edges are explicit. The CLI rejects
values outside its stated experiment domain: 1920×1080, tile64, at most24
intervals, coordinates/parameters of magnitude at most1e8, positive scale/range
reciprocals at least1e-8. The final positive infinite depth edge is required.

## Measured result, Linux / Core i5-10400

Counter-free warm CPU build times on the six-core development host:

| Case | Sort p50 ms | Scatter p50 ms | Hierarchy p50 /p99 ms |
|---|---:|---:|---:|
|small|0.7120|0.2993|0.3007 /0.3122|
|street|17.0024|14.8735|2.1625 /2.6353|
|dense|55.7415|49.3934|5.9324 /6.0184|
|overlap|92.6892|34.6731|33.0903 /34.1425|
|overflow|0.0457|0.0460|0.0440 /0.0483|
|turn|23.4454|21.6306|2.1157 /2.1642|
|ortho|0.4464|0.2192|0.2341 /0.2954|

For8192 lights, leaf tests fall from2,550,756 to188,706 plus64,112 parent
tests, preserving all148,424 assignments and exactly the same691,744-byte CSR.
This avoids90.09% of the AABB tests. It does not remove true overlapping work:
the1024-light overlap still costs33.09 ms. Ortho and the small case favour
flat scatter; a runtime policy and GPU construction remain separate work.

Dense own PMR cold peak is3,194,696 B; warm live payload
is2,886,832 B with zero allocation requests. Cold growth
retains2,097,156 logical element bytes. The separate copy/rebase
candidate needs0.0627 ms p50, copies524,288 source bytes and
rewrites24,576 position components. These are not full application/driver costs.

Observed dense hierarchy p50 with hot counters is6.0340 ms versus5.9324 ms
without. These are separate development-load runs; do not subtract that
difference as a precise causal overhead estimate. Primary timings exclude
hot counters and use matching output hashes; all raw samples are retained.

All42 case/grid/method comparisons and two direction-range controls pass per
profile;12 invalid-input controls are rejected. Normal, counter-enabled and
fully ASan/UBSan/libc++-hardened profiles retain matching CSR/repack outputs.
All56 original raster input files match the earlier actually executed GPU
controls byte-for-byte. No unnecessary new GPU capture or scene claim follows.

## Measurement scope

All three native translation units compile without Engine objects, shaders or
GPU dependencies. Measurements use one cold construction plus100 unchanged-
capacity rebuilds, without input IO in the timer. The CLI reports every sample;
CPU p50/p95/p99 are empirical statistics for this development load. They are
not full-frame timings or A18 measurements. Sanitized timings are separate.
Hot work/growth counters are compiled out with `LOCAL_LIGHT_BUILD_COUNTER=0`
for the primary timing matrix. A separate default-counter matrix supplies the
work counts and observed diagnostic cost; both must retain the same output
hashes. Allocator-boundary counters remain enabled and unchanged warm runs do
not allocate. Output formatting stays outside both timers.

The copy/rebase kernel performs one full Light64 memcpy and FP64 subtraction
followed by FP32 conversion of three position components. Source fixture
positions were already FP32-quantized. This measures a candidate arithmetic/
copy kernel, not actual FP64 ECEF acquisition, the complete current
`SubjectDraw::PackedLights` loop, its photometric/trigonometric packing, GPU
upload, or safe runtime publication. Original world descriptions must retain
their real precision in production.

All builder vectors use a counted PMR resource: allocation requests, payload
bytes, live payload and peak payload are actual allocator-boundary counts.
Input/stream objects, call stacks, allocator metadata, OS/driver memory and
residency are outside those counts. Vector growth reports retained logical
element bytes separately; sort swaps and physical DRAM traffic are not measured.
The copy/rebase kernel separately reports source memcpy bytes and rewritten
position components. Unchanged-capacity warm rebuilds must allocate zero times.

## Reproduction

Compile `local_light_build{,_geometry,_main}.cpp` with C++23, optimization,
`-ffp-contract=off`, no exceptions and strict warnings. No external library is
needed beyond the standard C++ library. The driver requires Python/NumPy:

```sh
python test/experiments/render/local_light_build.py \
  --binary /tmp/<private>/light-build --output /tmp/<new-private>/current \
  --repeats 101
python test/experiments/render/local_light_build_report.py \
  --input /tmp/<timing>/results.json --counts /tmp/<counted>/results.json \
  --output /tmp/<private>/build.pdf
```

The C++ CLI is `input-folder output-prefix sort|scatter|hierarchy tiles|log
index-bytes repeats`. Output prefixes must name existing private directories.
The Python driver requires a fresh external output directory. Retain input
hashes, complete stdout/exit codes and exact compiler/source identity with the
comparison. ASan/UBSan and libc++ hardening instrument all three owned units;
their complete matrix is a correctness control, not a performance result.

Native GPU construction, complete production repack/upload, jitter and changing
light/camera generations, runtime publication/lifetime, transparent composition,
Places and Metal/A18 remain open. CPU list identity permits reuse of matching
previous native raster controls; it does not imply those remaining acceptances.
