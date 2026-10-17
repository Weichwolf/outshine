# Camera-independent local-light control

Refs #448 / #190. This private experiment compares a persistent world-cell
index with the camera-dependent lists in `local_light_native.md`. It changes
neither product bindings nor the current 16-light limit. #328 remains the sole
native renderer transition. No additional SDL_GPU implementation is introduced.

## Method and private data

Finite-light spheres populate world-fixed cells in FP64. Sorted original IDs
survive within each cell; directions and range-zero lights remain global.
The Python reference compares sorted cells/binary lookup with a power-of-two
open-address hash table, at 16/32/64/128 m cell widths. Empty tags and complete
three-coordinate keys resolve collisions; probes are bounded by table capacity.
This is ordinary hashing, not a perfect hash or a constant-two-read claim.

The motivation follows Lefebvre/Hoppe, SIGGRAPH 2006,
[Perfect Spatial Hashing](https://www.hhoppe.com/proj/perfecthash/), including
sparse-domain validity and the static-occupancy limitation. All ten PDF pages,
figures, equations and memory comparison were inspected optically. The actual
choice here uses measured lookup work, payload and complete native images.

Position encoding is independent of lookup width: Light64 stores the unchanged
photometry/kind/beam/cone/inverse range, with place.xyz local to a 128 m cell.
An additional int32 xyz cell coordinate costs 12 B/light. The shader subtracts
integer camera cells before FP32 conversion, then uses FMA with local positions.
Directions ignore position. No camera-dependent light-record upload is needed
by these prescribed camera poses. The Python reference and native producer
below share this encoding; productive runtime packing/publication remains open.

Private storage 2/25 has a five-u32 header: global offset/count, slot offset,
hash mask or sorted-cell count, emitter-cell coordinate offset. Each slot is
five u32: sign-biased xyz key, absolute ID-word offset and count. Original IDs,
global IDs and emitter xyz cells follow. An optional three-float position cache
follows those cells, adding 12 B/light/variant. All offsets count words.
Light64 stays at 2/24. The existing 64 B frame at 3/4 now privately means:

- Grid.xyz: signed camera cell bits. Grid.w: sorted flag in bit 0, position-cache
  word offset in bits 1..31. This is an experimental encoding, no product ABI.
- Depth: 128 m position quantum followed by camera-local xyz.
- ViewZ: prescribed translation xyz and yaw for the motion control.
- Viewport: lookup width, unused zero, full 1920×1080 dimensions.

Original 1344 B Lighting and private 80 B receiver frame remain unchanged.
The optional 64-thread compute pass refreshes only camera-dependent positions;
the native driver accepts its origin token only after successful completion.
Static tests dispatch twice for two independent index variants, across 60 draws;
the prescribed moving sequence dispatches 47 times. They retain one upload and
never rebuild the world index. This synchronous offline probe does not establish
runtime in-flight slots, bounded residency, retry/backpressure or dirty updates.

## Correctness and actual images

The 32 Python scene/width/lookup cases verify packed IDs against cell contents
and independently required spheres at receiver samples and source extrema.
Invalid inputs, positive unused directional range and complete overflow are
checked separately. A per-light million-cell work bound or exceeded assignment
budget falls back to every original ID; there is no partial/truncated list.
The 16 MiB assignment cap is not a cap on slots, total peak or a device budget.
The private position domain is explicit; general FP32 culling is not proved.

Native 17/8192/overlap/overflow controls use the frozen coherent 3af55ff02
headers/archive and actual BRDF sources, as in #458. Cached and per-fragment
rebasing produce byte-identical complete RGBA32F captures. Each listed result
passes the unchanged full-sum RGB/support comparison. Prescribed movement and
Earth translation also retain all 12 image bytes. The original eight-argument
driver mode retains all 12 old 17-light images; exporter refactoring preserves
all input bytes for both 17 and 8192 lights.

Independent GPU coordinate checks cover 32 cases ×24,576 axes: origin and
Earth-size translations, four position quanta, and 30/60 Hz steps across cell
boundaries. A predeclared gamma4 bound includes arithmetic, local quantization
and direct-reference rounding. Maximum error to FP64-subtracted/FP32 reference:
0.1221 mm. A wrong-camera-cell kernel exceeds its bound by about 32 m.
A genuinely stale pose cache produces 56,618 RGB failures /38,098 support
mismatches. Neither check tolerance nor support definition was relaxed.

## Cost and choice

RTX 3060, 1920×1080, five repetitions/depth for distributed lights, three for
overlap/overflow, alternating method order within a run. Counter and validation
off. GPU timestamps include any coordinate dispatch plus draw, excluding
upload/readback, CPU construction and the rest of the frame. Shared desktop,
two foreign nice compilers and overlapping own Python reference work; no fixed
clocks, isolation or universal speedup claim. Static 8192-light medians:

| View depth m | Hash16 per-fragment ms | Hash16 cached ms | Hash32 per-fragment ms | Hash32 cached ms |
|---|---:|---:|---:|---:|
|25|2.2584|1.9234|2.7066|2.2900|
|250|0.9779|0.8816|1.1544|1.0317|
|500|0.6278|0.5832|0.7288|0.6578|
|5000|0.5168|0.5093|0.4217|0.4151|

Moving Hash32 cached: 2.3014/0.8872/0.6078/0.4274 ms. Same-width Hash32 versus
sorted32: 2.2804/1.0209/0.6588/0.4107 versus 2.4289/1.1687/0.7922/0.5180 ms.
Cached Hash64: 3.2082/1.6217/1.0551/0.3882 ms; Hash128:
4.8961/2.6976/1.6153/0.3911 ms. Large cells save bytes but increase near work.
Overlap/250 m remains red at 43.3168 ms; full-list overflow at 60.6076 ms.

Dense payload, including keys/IDs/emitter cells/pose cache, excluding the
524,288 B Light64 records: Hash16 4,228,340 B, Hash32 1,232,452 B,
Hash64 508,596 B, Hash128 278,732 B. Hash32 has 5,944 occupied cells,
177,033 assignments and 221,103 reference sphere-box tests. Sampled hash slot
probe p50/p95/p99/max = 1/3/6/10; sorted32 =13/13/13/13, with 1,023,652 B.
These are logical own payloads, not complete backend allocation/DRAM/peak.
Both comparison variants coexist; the per-variant 98,304 B position region is
already counted. Readback is separate: twelve full captures copy 398,131,200 B.

Hash32 plus reusable GPU positions is a next native producer candidate, balancing
near cost against Hash16's 3.43× index payload. Camera-independent reuse may avoid
the previous 5.93 ms camera-list CPU rebuild. Native production/edit costs follow
below; productive upload/publication savings remain unmeasured.

## Native producer and actual edits

`local_light_world_build.{h,cpp}` now implements the same products natively.
A PMR ordered cell map retains per-cell sorted original IDs. A light edit
enumerates only its previous/new support, merges those cell sets and changes
their difference. Colour, point/spot-kind and global-only edits require no
spatial tests. Cell-local Light64 and int32 coordinates are generated in C++;
sorted/hash serialization retains the exact Python bytes. Overflow preserves
the complete original-ID domain until an explicit rebuild. The experiment has
fixed IDs/count; insertion/removal and active-world residency remain open.

Forty normal and forty ASan/UBSan/container-hardened cases match all word,
Light64 and coordinate bytes: four scenes, four widths, both lookups, Earth
translation, empty input, full overflow and positive unused directional range.
Twenty-eight successive edits per profile cover colour, point/spot kind,
movement, finite range, unbounded point, direction and finite return. Eleven
invalid-input/record/update invocations per profile are genuinely rejected.
Four affected C++ units pass focused LLVM22 with the unmodified policy; the
separate raster harness's 14 Check.h findings remain open.

An additional actual far emitter, rather than an unilluminating test position,
passes the same seven edits in both native lookup producers. Its native bytes
feed the existing GPU consumer directly. Baseline plus seven edited states
all pass the unchanged full-sum/support comparison; moving/range edits visibly
change the far receiver, and unbounded/directional states affect all planes.
Eight fresh offline processes use complete uploads. They do not establish
runtime partial uploads, asynchronous publication or in-flight update safety.

Five normal-priority native sequences on the shared RTX-host CPU, work counters
off but PMR allocation tracking retained, give these median CPU times:

| Operation /8192 lights, Hash32 | Build/edit ms | Complete pack ms |
|---|---:|---:|
|Cold build|11.862235|0.532524|
|Colour|0.000527|0.297743|
|Move one|0.006616|0.258464|
|Finite range|0.014427|0.613327|
|Make global|0.015612|0.251134|

The move performs 54 sphere-box tests/18 membership changes instead of the
full 221,103 tests/177,033 assignments. Cold own allocation requests:40,410;
move:8; colour/point-kind:0. Own tracked producer peak is3,864,312 B cold and
3,870,812 B across those edits. It includes original/local Light64 arrays,
coordinates, PMR map/ID capacities, footprint scratch and packed words;
caller-owned file input, IO and driver allocations are outside this count.
Unchanged contents need no Build/Update call; forcing Build again still
recreates map nodes and is not a zero-allocation frame path.

Every Pack still zero-fills the complete word payload and rewrites its
slots/IDs/coordinates, including after colour-only edits. CLI dumping copies
words +64N Light64 bytes +12N coordinate bytes after the timed operation.
Those copies and full producer packing remain explicit costs; no dirty-range
GPU upload saving is claimed. Input reads/capture writes are outside timings.
Concurrent reference/analysis jobs and normal desktop load remain part of the
measurement; these are five trend samples, not p99/full-frame/device acceptance.

The producer checker is `local_light_world_build.py --binary <native>
--output <private> --repeats 2`. Build its main/core with the existing
`local_light_build.cpp` and `local_light_build_geometry.cpp` memory-helper
closure, C++23, `-ffp-contract=off`, no exceptions and warnings as errors.
`LOCAL_LIGHT_BUILD_COUNTER=0` disables work counters for timing. Sanitizers add
ASan/UBSan and libc++ extensive hardening. `local_light_world_updates.py --help`
documents the existing native raster control inputs. The report's --producer
option adds the native edit/cost page; all four pages were optically inspected.

## Reproduction and open gates

Requires Python/NumPy, Matplotlib for PDF, glslang, and the coherent native
consumer/toolchain from #458. `local_light_world_check.py --output <private>`
runs reference comparisons. `local_light_world_native.py --output <private>
--scene dense --width 16 32 --methods hash hash --raster-root <checkout>` exports
inputs and strict private shader copies. Compile its `shaders/world.frag` with
LOCAL_LIGHT_LISTED=0/1, LOCAL_LIGHT_COUNTER=0/1, WORLD_MOTION=0/1 and
WORLD_PRETRANSFORMED=0/1. Compile `local_light_world_prepass.comp` with the same
motion flag, WORLD_PRETRANSFORMED=0 and the generated/includes shader paths.
The original driver arguments still work; append compute-SPIRV, motion0/1 and
stale0/1 to enable the cached path. Actual image checking remains
`check_local_light_native.py`; use --without-counter only for counter-free shaders.
`local_light_world_coordinates.py --help` documents the separate coordinate
consumer. `local_light_world_report.py --input <evidence> --output <private.pdf>`
generates the three-page report; all pages were optically reviewed.

Five representative fragment/compute variants translate to MSL2.3 and reflect
64/80/1344 B uniform blocks, Light stride64 and u32 stride4 offline. This is
neither Metal execution nor full material-resource acceptance. Strict native
build/format pass; focused LLVM22 remains red solely on the same 14 shared
Check.h findings (#603), with no suppression or harness-policy change.
Shared product ABI/lifetime agreement, productive dirty upload, active-world
residency, transparent composition, full Places, Metal/A18 and total budgets stay
open. The original coherent archive is not relabelled as current-engine proof.
