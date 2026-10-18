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

## Precision conditions and exact V1 scaling

V1 fixes position cells at128m and lookup cells at32m. The shader uses the
compile-time ratio4 and binary scale1/32, rather than dividing a runtime uniform
before an integer conversion or `floor`. The 48-byte frame and its width metadata
remain unchanged. `FrameUniformsOf` derives its origin from the same
`-PreViewTranslation` used to place geometry; it is not a separate camera origin.

The [Vulkan operation-precision contract](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#spirvenv-op-prec)
allows2.5ULP for single-precision division. One representable result below4 is
3.999999761581421, which truncates to3. With a nonzero position cell, using that
ratio corrupts the carry by the position-cell index, not by a small FP32 delta.
`native_light_bounds.py` reproduces two lost positive-contribution tail IDs using
actual native18-source hash bytes at opposite Earth origins and a permitted-error
arithmetic model. Exact division and the fixed integer ratio retain them; a null
origin is the unchanged positive control. This is not a measured Iris failure.
The generated `lightCellBounds` must contain no `OpFDiv`; checking only the GLSL
spelling or a driver that happens to divide32 exactly would not close the gap.

The following bound is a conditional derivation for Vulkan's non-relaxed FP32
operation contract, including adjacent-result rounding and denormal flushing.
It is a review draft, not a completed common Metal/Vulkan acceptance. Metal
currently compiles fragments with `MTLMathModeRelaxed`; master has explicitly
not confirmed a fixed ULP bound for that mode or its default fast library
functions. No backend precision policy is changed by this shader correction.

Let e=2^-23, d=2^-52, n=2^-126, P=max(abs(p)), R be the producer's up-rounded
maximum finite radius, and H=P+R+128. The hash path requires the computed
M=16e*(P+R+128) to be less than16m. Thus H<2^24. Source-specific r is the
actual double reciprocal used by the CPU footprint builder; r<=R. Directional,
zero-range, non-normal reciprocal, budget-fallback and unrepresentable lookup
cases enumerate their complete original source sets and do not need this bound.

1. **Integer and split domain.** Signed128m position cells lie in[-2^30,2^30),
   so source-minus-origin fits int32. A valid32m lookup key constrains the origin
   position cell to[-2^29,2^29); multiplication by4 and its subtraction from the
   lookup key are exact, normally giving a carry in0..3. FP64 underflow at a
   zero-equivalent origin may also select carry-1 or4; the packed local is then
   the exact0/128 endpoint. In every case rounded local-minus32*carry remains
   in[0,32]. The implementation checks key addition before
   forming the final keys, and falls back when more than two cells per axis are
   needed. The accepted hash query therefore has at most eight cells.
2. **Query enclosure.** Correctly rounded basic operations may choose either
   adjacent result; use e as a conservative relative bound, with additive n for
   a flushed result. The positive margin sum has two additions and exact binary
   scaling, giving M>=16e*(1-e)^2*H. Even allowing reassociation of the three local
   coordinate terms, the local/query endpoint error is bounded by4e*P+512e m.
   Multiplication by1/32 is exact outside the negligible denormal range; flushing
   in that range is absorbed by the additive allowance. Using e*H<2, the queried
   cell union encloses an infinity-norm cube around the same reconstructed world
   point Q with half-width at least e*(12P+15R+1024). This deliberately retains
   slack instead of relying on a rounded endpoint lying in its nominal cell.
3. **Emitter reconstruction.** Write Z=128*(sourceCell-originCell)+sourceLocal-
   roundedOriginLocal. Integer conversion, source-local packing and either fused
   or unfused reconstruction give per-axis error no greater than
   (3e+6e^2)*abs(Z)+384e*(1+4e)+16n. For a contributing source, solving the same
   inequality with abs(Z)<=P+length(decoded-p)+error bounds that error by4e*H.
   This covers source-local rounding and different valid associations; the origin
   packing term is common to Q and Z and must not be counted as a different origin.
4. **Actual range predicate.** The shader uses `dot(offset,offset)*I*I`, where I
   is the actual packed reciprocal, not a geometric square-root comparison. For
   D=length(decoded-p)>=2^-40m, positive sum-of-squares evaluation gives
   square>=(1-10e)*D^2, including flushed intermediate products. Since H<2^24,
   I>2^-25, so the first scaled product is normal in this case. A positive finite
   attenuation requires the computed reach to be below1; the two scaled products
   then imply D<=(1+10e)*r, including the double reciprocal rounding. If D is
   smaller than2^-40m, the source centre itself is inside the much larger query
   enclosure. No false negative is inferred from an idealized range-only test.
5. **CPU membership.** In the non-tiny case a point of the producer sphere can
   be reached from Q with infinity displacement at most4e*H+10e*r. The query
   enclosure leaves at least e*(8P+R+512) further slack. Choose the point inside
   radius r*(1-32d): its extra displacement is below32d*R<e m, far below that
   slack. The CPU's three nonnegative squared distances have at most a
   (1+d)^5 overestimate; the inward point therefore survives comparison with the
   rounded r*r. The outward `nextafter` footprint bounds include its cell.
   Its original ID is consequently present in a queried span. Sorted IDs and
   deduplication preserve exactly one visit; total-span overflow enumerates all.

Outstanding review must check these operation assumptions against actual emitted
SPIR-V and each target's permitted optimizations, including the contribution
predicate's exceptional values. Random sweeps, the permitted-division negative,
and actual runtime images are supporting controls, not a substitute for that
review or a Metal precision contract. No radius, budget or RGB tolerance changes.

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
The optional `motion` mode uses two independent synchronized renderer histories.
It compares every nonzero declared source against the uniform renderer while
retaining the original zero-intensity sources only on the spatial side. Thirty-six
camera states per opaque/alpha/glass material include two16-pose camera rings,
perspective/orthographic switching, signed32/128m boundaries, rounded local128,
colour/kind/range/position changes and18->16->18 source transitions. Camera binding
uses `RuntimeScene::Advance`; it must not insert an uncounted `Draw` before the
captured frame. Both actual plans must retain TemporalResolve and complete exactly
one native frame per pose, so the same Halton history is exercised on both sides.
The existing per-pixel2% and0.5% RMS limits and nonzero-energy requirement remain.
The scalar stage-duration counter is not a temporal-pass counter: temporal work
is executed within the display path and its catalogue executor is null.
Displayed RGBA8 readbacks are saved and their absolute byte differences logged
separately. A close displayed image does not turn a failed temporal HDR comparison
into a pass; current residual cases remain in the experiment exit status.
The original public `local_light_limit.cpp` remains the separate Engine API probe.

## Reproduction and open acceptance

The bounds regression additionally takes the actual compiled shader:

```sh
python3 test/experiments/render/native_light_bounds.py \
  --binary /tmp/native-light-cells \
  --shader build/shaders/groundLitSpatial-000.frag.spv \
  --output /tmp/outshine-light-bounds
/tmp/native-light-scene /tmp/outshine-light-motion motion
```

Create the motion output directory beforehand; the bounds command creates its
explicit external directory and refuses to overwrite one. The previous shader
artifact must fail the no-division check even when its native driver happens to
return an exact quotient. A motion run reports both HDR and displayed errors.

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
