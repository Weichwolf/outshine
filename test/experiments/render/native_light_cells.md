# Native world-light producer and renderer connection

Refs #448, goal #190; shared backend transition #48/#531. This delivery connects
the world-cell method to the native renderer. It does not admit the complete
world at 1920×1080/60, establish Metal/A18 execution, or solve dense overlap.
The SIGGRAPH 2012 and HPG 2012 sources and earlier alternatives are recorded in
#448 and `local_light_lists.md`. The world-fixed comparison is in
`local_light_world_build.md` and Draft #656.

## Product contract

Up to 16 sources retain the uniform-array path. Larger sets use original source
IDs, Light64 and a world-fixed 32 m hash index. No source or per-cell list is
truncated. Directional, range-zero and non-normal reciprocal-range sources remain
global. Spatial construction has explicit storage, scratch and sphere-box work
limits; insufficient spatial capacity falls back to complete enumeration. If
even that representation cannot fit, publication fails and retains accepted data.

Light64 is 64 B/align16 with Tint/Place/Beam/Cone at 0/16/32/48. Finite positions
use signed 128 m cells plus local FP32 metres, not camera-dependent source data.
Fragment SSBOs are set2/binding24 (Light64) and set2/binding25 (raw uint index).
The index header has eight words: global offset/count, table offset/mask,
position-cell offset, up-rounded maximum finite range as FP32 bits, source count,
and version1. Each hash slot has biased signed xyz and ID offset/count; ID lists
are sorted. Position cells have exactly three raw int32 words per original ID.
There is no relative-position cache in this format.

Fragment set3/binding4 is a 48 B/align16 frame: PositionCell at0, PositionLocal at16,
LookupCell at32. PositionLocal may round to128. The lookup remainder is therefore
`PositionLocal - 32*(LookupCell - 4*PositionCell)`, including that rounded endpoint.
Unrepresentable lookup keys select the complete sum before lookup arithmetic;
an unrepresentable position origin is refused. The conservative boundary query
may visit eight neighbours and deduplicates original IDs. The current FP32 margin
has counterexample controls, not a universal error proof.

Opaque and transparent draws share one immutable source generation. Three slots
bound in-flight generations; polling, failed admission and aborted command
recordings preserve the accepted snapshot without waiting. A submitted frame
records the last reader. Device replacement also waits for retirement through
nonblocking admission. Identical sources allocate, rebuild and upload nothing.
Changes currently rebuild the bounded index; incremental spatial editing is not
claimed. Camera movement changes only the frame uniform. Spatial pipelines are
prepared when the larger source set is declared, and shared material parameters
and existing BRDFs remain unchanged. The Sun owner uses its original uint ID bits
in the existing Count.z word on the spatial path.

## Producer choice and checks

The initial flat assignment sort needed about16.84 ms cold and3,537,648 B scratch
for the retained8192-source fixture. Count/prefix/scatter avoids sorting all
177,033 assignments: it sorts occupied cell keys and visits finite footprints
twice,442,206 sphere-box tests. The initial local/Earth pair took6.668/6.336 ms,
with491,520 B peak scratch and1,658,448 B packed payload. These are Linux/i5-10400
diagnostics under shared load, not a device or frame-budget admission.

`native_light_cells.cpp` calls the product producer with typed source parameters.
`native_light_cells.py` independently checks exact packed bytes for local/Earth
small, street, dense and overlap cases, empty data and directional unused range.
Three additional cases mix ordinary finite, directional, zero-range and positive
subnormal reciprocal ranges at local/Earth origins, then use only subnormal
ranges. The product keeps those non-normal ranges global. The adapter expresses
that classification as zero range only in its private legacy-grid input copy;
it does not rewrite actual Light64 output or the shared older grid contract.
All thirteen actual producer cases pass on frozen `bdd54c01c`; the original ten
retain identical source/index/Light64/position bytes. Before the correction,
all three new cases failed the independent packed-word comparison. Their exact
commands, binary/source hashes and original negative are retained under
`/tmp/outshine-research-light-reference-bdd54c01c/` on the research host. This is
CPU index/identity evidence, not a universal GPU-bound or device/light-image gate.
The module tests cover original IDs, finite supports, complete fallback,
refusals, frame layout and rounded/overflowing camera coordinates. The separate
native-device test exercises unchanged data, three occupied generations,
unsubmitted readers, abort/retry, admission failure and device replacement.

`native_light_scene.cpp` uses the actual RuntimeScene/SceneRenderer, validation
enabled, at64×64. Its reference sums separate captures of every group of at most
16 sources through the retained uniform-array renderer. It checks all RGB values
against2% maximum relative error (1e-5 floor) and0.5% normalized RMS, and requires
nonzero reference energy. Cases include only the last source active, perspective
and orthographic cameras at a rounded world-cell boundary, mixed17/1024/8192
point/spot/directional/range-zero inputs, overlap, MR, coat, sheen, iridescence,
mapped anisotropy, back-lit thin transmission, alpha blending and glass.
It saves actual/reference linear floats in an explicit external directory.
The original public `local_light_limit.cpp` remains the separate Engine API probe.

## Reproduction and open acceptance

Use the normal declared Render compiler flags for the producer/module test, and
the declared Engine flags and matching native archive/shader package for the
scene experiment. Required native packages are those of `make all`; Python needs
NumPy. Keep compiler, source commit and load with the receipts.

```sh
python3 test/experiments/render/native_light_cells.py --binary /tmp/native-light-cells --output /tmp/outshine-light-native-reference
mkdir -p /tmp/outshine-light-native-scene
/tmp/native-light-scene /tmp/outshine-light-native-scene
```

Complete Places, dynamic jitter/motion/edits, terrain/patch consumers, Sun-atlas
ownership, exact runtime shader layouts, full lint and frame/memory budgets must
be checked separately. The shared Check.h closure remains owned by #666; its
findings are not suppressed or removed from this delivery's affected test runs.
