# Native persistent light-cell producer

Refs #448, goal #190. This is a standalone CPU producer/reference experiment,
not an engine renderer or a fix for the product's current 16-light limit.
Its words are consumed by the private native GPU comparison in Draft #656.
Product bindings, lifetime, active-world residency and dirty GPU uploads remain
unagreed/unimplemented; #328 remains the only native renderer transition.

`local_light_world.py` is the camera-independent reference: finite spheres
populate world-fixed 16/32/64/128 m cells. Directions/range-zero lights are
global. Sorted original IDs and all contributions survive bounded-work or
assignment-budget fallback. Sorted lookup and ordinary open-address hashing
share a five-u32 slot representation, with complete three-coordinate keys.
This is not perfect hashing; source and initial method comparison are in
[Draft #656](https://github.com/Weichwolf/outshine/pull/656).

Light64 retains photometry/kind/beam/cone/range; its position becomes local to
a 128 m cell, supplemented by int32 xyz. The packed words retain explicit keys,
absolute offsets/counts, original IDs, global IDs and those coordinates, plus
a zero-initialized 12 B/light region for the private GPU relative-position test.
Signed coordinate limits and budgets are explicit; no general FP32 culling
proof, implicit device budget or dynamic source-ID lifecycle follows.

The C++ producer uses PMR ordered cells with sorted ID vectors. It retains
original/local Light64, coordinates and bounded per-light footprint scratch.
An edit compares old/new footprints, changing only their set difference.
Colour, point/spot and global-only edits require no spatial search. Overflow
keeps every original ID until explicit Build. IDs/count stay fixed in this
control; source insertion/removal/residency are separate work.

## Measured result and limits

8192 lights, 32 m hash, five independent normal-priority native sequences on the
shared Linux host. Work counters off; PMR allocation tracking retained. IO/dumps
are outside timestamps. Reference/analysis/desktop load and unfixed clocks apply.

| Operation | Build/edit median ms | Full pack median ms |
|---|---:|---:|
|Cold|11.862235|0.532524|
|Colour|0.000527|0.297743|
|Move one|0.006616|0.258464|
|Range|0.014427|0.613327|
|Become global|0.015612|0.251134|

Counted move:54 sphere-box tests/18 membership changes instead of full
221,103/177,033. Cold allocation requests40,410; move8; colour/point-kind0.
Own producer peak3,864,312 B cold /3,870,812 B over edits includes both Light64
arrays, cells, PMR capacities, scratch and packed words. Caller input, IO and
driver are excluded. This is not total process/device peak or frame acceptance.

Packing now retains the current words until cell membership, global membership,
Build, or lookup method changes. Photometry and point/spot changes update Light64
without rewriting the index. A changed 128 m position cell patches only its changed
coordinate words while the index is valid. Changes before the first Pack, batches,
full fallback, empty input and rejected requests retain the same full-pack bytes.
The original IDs and binary layout are unchanged; no dirty GPU-upload contract is
introduced. Build and membership changes still perform the complete original pack.
The caller dumps words +64N local-light +12N coordinate bytes after timing.
Static use needs no Build/Update call; a forced rebuild recreates map nodes.

## Reproduction and evidence

Build `local_light_world_build_main.cpp`, `local_light_world_build.cpp`,
`local_light_build.cpp` and `local_light_build_geometry.cpp` with C++23,
`-O2 -ffp-contract=off -fno-exceptions -Wall -Wextra -Wpedantic -Wshadow -Werror`.
Only standard C++ libraries are required; the last two units provide the
existing tracked-memory helper. NumPy is required by the Python checker:
`local_light_world_build.py --binary <native> --output <system-temp> --repeats 2`.
Output must be external. `LOCAL_LIGHT_BUILD_COUNTER=0` disables work counters;
allocation tracking remains. Sanitizers add ASan/UBSan, frame pointers and
libc++ extensive hardening. The CLI takes source-Light64 file, output prefix,
width, hash/sorted, assignment-byte cap, translation xyz, repeats, optional
update file. Updates contain original u32 ID plus16 FP32 words per record.

Normal and ASan/UBSan/hardening each pass40 exact native/reference cases plus
28 sequential updates and11 actual input rejects. Cases cover four scenes,
four widths, both lookups, Earth translation, empty input, full fallback and
directional positive unused range. Four affected C++ units pass default LLVM22.
Additional tail-emitter bytes feed eight actual GPU full-sum/support controls
in Draft #656, with visible movement/range changes; four PDF pages inspected.
Those fresh offline processes prove neither runtime publication nor Metal/A18.
No shared Check.h is included by this producer; the separate raster harness's
14 findings remain with #603. Exact commands/terminal logs and original inputs
are linked from the delivery PR; no images are published by this experiment.

## Retained packing comparison (#448)

`local_light_world_pack_compare.py` compares two native producer executables against
exact output hashes after every edit. Six normal-priority pairs alternate before/after
order on the shared Linux i5-10400 host; both use Clang/libc++21, O2 and work counters
off. PMR allocation tracking and constant-time word-store accounting remain enabled.
The unchanged full-pack control comes from commit0ce1957c2. Dumps/hashes are outside
the measured build/edit/pack intervals. Concurrent desktop/render/compiler load and
unfixed clocks apply; no GPU time or overall frame improvement follows.

| Operation | Full pack median ms | Retained pack median ms | Retained index word stores |
|---|---:|---:|---:|
|Cold|0.462787|0.465724|539450|
|Colour|0.190122|0.000048|0|
|Point/spot|0.189644|0.000032|0|
|Move one|0.191447|0.221257|539450|
|Range|0.481809|0.496923|539744|
|Become global|0.193297|0.190232|539414|
|Direction while global|0.188520|0.000045|0|
|Become finite|0.186912|0.198030|539450|

All 12 sequences produce identical original binary outputs at all eight states.
The 8192-light colour edit retains the1,232,452 B index payload and avoids539,450
logical word stores (2,157,800 B, including original zero-fill and scatter overwrites;
source-level accounting, not measured DRAM traffic).
This concerns CPU index packing only: Source/Local Light64 still update, no transfer
or GPU residency saving is claimed. Membership changes retain the complete pack;
their measured times show no general gain. Both versions retain identical PMR
allocation counts and tracked peaks:3,864,312 B cold,3,870,812 B maximum over edits.
The Builder object grows from368 to384 B (Linux x86_64); object/stack, caller input,
dump files and driver are outside those PMR peaks.

The existing independent NumPy reference passes40 cases across four widths and two
lookups, including Earth translation, empty and overflow;28 sequential edits and11
actual input refusals remain checked. `local_light_world_pack_checks.cpp` adds196
checks in both normal and ASan/UBSan/libc++ extensive-hardening builds: reused index,
coordinate-only global/fallback edits, multiple changes before packing, lookup
switches, fresh Build, pre-Pack edits, invalid requests and exact fresh-rebuild bytes.
Five affected/dependent C++ units pass the unchanged focused LLVM22 policy. No shared
Check.h dependency or new renderer ABI is added; Draft #656 owns earlier actual GPU
controls for these unchanged output bytes. Actual runtime upload, transparent-world
rendering, Metal execution, A18 and the product's16-light refusal remain open.

For the comparison, build both versions with `-DLOCAL_LIGHT_BUILD_COUNTER=0`, then run
`local_light_world_pack_compare.py --before <full> --after <retained> --output <temp>`.
Build the state checks by substituting `local_light_world_pack_checks.cpp` for the
existing CLI main in the four-unit command above. Full argv, hashes, terminal status,
original first-pair dumps and the optically inspected one-page PDF live in the PR's
linked evidence; repeated identical output copies are removed after hash comparison.
