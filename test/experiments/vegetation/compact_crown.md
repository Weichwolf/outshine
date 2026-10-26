# Compact continuous original crown parameters

Owner [#721](https://github.com/Weichwolf/outshine/issues/721), after
[host demand](crown_demand.md) and [shared leaf carriers](leaf_patches.md).
The native world comparison still holds90000 individual leaf matrices for a
whole visible original crown. This Python experiment factors that original
placement algebra into host parameters and two shared metamer tables. It is
not a native instance/asset/shader format or a complete crown LOD.

## Source, factorization and fidelity

The six unchanged versioned inputs in `data/crown682/` provide5472 skeleton
nodes,4301 hosts and90000 leaves. Original source catalog/material identity:
`62a49f3c918384dba7633ec6e13a3f48560c828df682c0745cea7e0e8115015d`.
Keep each original host origin/frame, continuous PCG64 phase, ID offset and
unchanged original variant list. The actual counts are20 or21 leaves/host.
For each count, cache the original circular-arc centre/tangent/lateral/normal
values at the same metamer parameters. These tables depend on the existing
recipe, not a representative host phase. Compilation calls the original scalar
stem-frame path and draws exactly the original phase once/host; no leaf poses,
Shoot.detail or Blade.mesh are needed.

The bounded evaluator computes `phase + index*golden_angle`, original radial
and .3-aligned leaf directions, original Z-hint normal frames and .004m petiole
offsets. Matrix products retain the original operation order. Original uint64
IDs and uint8 variants are copied from source offsets; no new inventory or
physical leaf area is introduced. Default batches hold at most256 host groups;
capacity up to100000 leaves is checked before local pose evaluation/output
allocation. Empty demand and empty inventories produce complete empty arrays.
Saved Float64 analytic host bounds permit camera selection without a skeleton,
original recipe reconstruction or source file read. The saved variant catalog
contains original variant indices; blade geometry and procedural material remain
separate shared inputs, bound by source identity.

Frozen producer167df4157 compares every output against the independent original
`CrownShoots.group` path, retaining one source group at a time. All90000
Float64 ID/variant/origin/frame/anchor rows are **bit-identical**, including
signed-zero bits. Complete four-field output SHA256:
`96e7b4d4e87ee03f5beccd82c04bc6e69333f00cb8cd519ce8977695d1f16f30`.
This is observed equivalence on the measured NumPy/platform, not a promise
about every BLAS/compiler or a future native implementation.

Float32 is a separately labeled approximation: host transforms, continuous
phases and common arc values are rounded to Float32; analytic bounds stay
Float64. The CPU evaluator uses Float32 arithmetic and trigonometric functions,
not an actual GPU shader or its fast-math contract. IDs/variants remain exact.
Maximum actual component errors: origins1.90710µm, anchors1.85849µm,
frame1.07594e-5. Maximum origin distance1.90765µm; maximum basis-column angular
error1.14179e-5rad. Predeclared limits are10µm positional component and1e-4rad
basis angle. The separate Float32 frame path checks positive/negative Z poles,
near-pole fallback and finite orthonormality. That does not admit every future
recipe near a discontinuous frame choice.

## Storage, CPU work and transient memory

| Complete compact product array field | Float64 bytes |
|---|---:|
| Original host IDs |17204|
| Original uint64 ID offsets |34416|
| Host origins |103224|
| Host frames |309672|
| Continuous phases |34408|
| Original leaf variants |90000|
| Petiole scalar |8|
| Shared20/21 arc tables |3936|
| Two original Float64 bound arrays |206448|
| Total |799316|

Float32 total573692B, including the same206448B bounds. Trusted uncompressed
experimental NPZ files are802734/577110B. Python container overhead and native
alignment/index/descriptor/upload costs are additional. This is91.54% less
retained array payload than the original full9450000B Float64 pose table,
while also including host bounds. It does not shrink the latter table if a
consumer unnecessarily retains both representations. The native world owner's
11520128B matrix layout is different; no native saving is claimed from Python.

Compilation takes1.011s on this run, including phase/stem preparation and
bounds. The same prebuilt bounds and camera query are used inside each timed
path below. Three uninstrumented repetitions; memory tracing is a separate
call. Both evaluators produce the same four output fields. Compact64/32 CPU
expansion is a reference consumer, not a required per-frame matrix rebuild.

| View | Leaves | Original median s | Compact64 s | Compact32 s |
|---|---:|---:|---:|---:|
| Whole |90000|3.4197|.0779|.0974|
| Inside |14231|.5252|.0132|.0117|
| Edge |28026|1.1797|.0433|.0230|
| Away |0|.0016|.0015|.0014|

Original and compact64 output hashes agree in every case. The whole output
still occupies9450000B; Float32 output5130000B. Whole-call traced peak is
9547969B original /11507581B compact64 /6273773B compact32. Inside-call peak
is1563136/3438084/1841188B. Vectorization reduces Python loop work but increases
temporary Float64 arrays. Full verifier process peak82907136B includes source,
both parameter products and independent controls. Positive live allocation
counts in the JSON are not cumulative allocation counts.

The versioned `batch_costs` comparison repeats16/64/256/1024 host limits with
identical hashes. Whole traced peaks are9708819/10059795/11463699/17079299B;
inside1639514/1990490/3394314/4978978B. Corresponding whole medians on the
shared host are .1747/.1258/.1079/.1088s. Default256 avoids the much larger1024
peak without promising an optimal host-independent batch size.64 is a useful
lower-memory alternative; no image quality or inventory changes with this knob.

Two terminal0 Valgrind3.24 DHAT processes compare identical cold source decode,
imports and one full build/query; compact additionally compiles its parameters.
Both output the same source/output hashes above.

| Whole-process tracked native heap statistic | Original | Compact64 |
|---|---:|---:|
| Allocated blocks |513146|133886|
| Cumulative allocated bytes |384839667|311914106|
| Live heap at global peak, bytes |25705099|26687070|
| Tracked heap reads, bytes |1976874302|1308824234|
| Tracked heap writes, bytes |774598195|577809864|

These are not query-only allocations, Python arena suballocations, stack/GPU
traffic, total system bandwidth or process RSS. Instrumented wall times are not
used as ordinary performance numbers. The larger heap peak remains explicit.
Normal timing runs use normal priority; heavy profiling/rendering uses nice10.
Four-CPU WSL host shared with Blender/other-agent work; NumPy2.5.4, single
BLAS/OMP thread. Variability prevents ranking Float32 versus64 by these times.

## Finished parameter replay and actual images

Fresh f6a7b5b16 readers load only the trusted finished parameter file, with
source-load/build/RNG/original-group callbacks forbidden. Full/inside/edge/away
hashes agree with the respective producer, including Float32's own output.
Decode7.48ms Float64/5.15ms Float32; current-process `/proc/self/status` VmHWM
70406144/64126976B. `ru_maxrss` is already1508913152B at reader entry and stays
there, inconsistent with the current process-image high-water counter. Both
are retained in the reports; its initial value is not attributed to compact
decoding. A separate fresh check records70410240B via both counters. No precise
cause of that initial counter discrepancy or general cache-load budget is claimed.

All three actual2560×1440 PNGs were saved and opened. Same inside camera as
the parent demand study: eye(0,0,12),target(2,0,12),vertical FOV60°,clip.1/30000m.
The reader uses the same connected32 geometry/alpha and diagnostic leaf-box
rejection/front ordering. Original comparator considers90000 leaves; compact
readers evaluate the same14231 demanded leaves from finished parameters, with
build/RNG/original-group callbacks forbidden. Constant-colour coverage isolates
placement; material/normal shading, shadows and wind are not tested by it.

| Actual RGB8 image result | Compact64 | Compact32 |
|---|---:|---:|
| Mean channel error |0|.00678404|
| Maximum channel error |0|5|
| Different pixels |0|50622|
| Different fraction |0|.0137321|
| Accepted under predeclared image limits |Yes|No|

Float64 must be pixel-identical and is. Float32's mean stays below.1 channel
units, but its1.373% changed pixels exceed the unchanged.5% limit. Thus small
geometric error alone does **not** admit this Float32 consumer. All three views
look closely matched, with the same conspicuous fan/repetition pattern; that
optical impression does not overrule the declared quantitative rejection. The
source of the many low-amplitude changed pixels is not yet isolated.

Producer167df4157 exits1 after the final assertion. It saved the three PNGs but
asserted before writing its in-memory scene/render timing report. A separate
readback records actual image hashes/differences; missing per-case costs and
render RSS remain unavailable, not reconstructed from log timestamps. The
renderer now saves metrics before raising; three new CPU image controls retain
rejected metrics, enforce exact Float64 pixels and refuse different profiles.
That diagnostic correction is tested on the saved images, not called a new
heavy-render success. EEVEE5.2.2/software llvmpipe,32 samples,nice10, shared
host; no native GPU/RTX/A18 evidence or material/forest/LOD quality admission.

## Reproduction and remaining scope

Use `.venv`, TMPDIR=/tmp, PYTHONDONTWRITEBYTECODE=1, one BLAS/OMP thread,
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
Run `compact_crown_probe.py --inputs test/experiments/vegetation/data/crown682
--output /tmp/<fresh-proof>`. The report now includes the same versioned batch
comparison; frozen167df4157's main evidence and f6a7b5b16's batch supplement
are separate runs, not one timing population.

Run `compact_crown_reader.py --parameters /tmp/<fresh-proof>/compact64.npz
--expected-source <identity-above> --output /tmp/<reader>.json`, likewise32.
Use the parent connected study's `connected32` templates, then
`render_compact_crown.py --inputs test/experiments/vegetation/data/crown682
--parameters /tmp/<fresh-proof> --templates /tmp/<connected-study>/connected32
--output /tmp/<fresh-render>`. Set LIBGL_ALWAYS_SOFTWARE=1 and XDG_CACHE_HOME/
MESA_SHADER_CACHE_DIR explicitly under/tmp. Profiling uses
`valgrind --tool=dhat --dhat-out-file=/tmp/<profile>.json --log-file=/tmp/<profile>.log
.venv/bin/python test/experiments/vegetation/compact_crown_profile.py
--inputs test/experiments/vegetation/data/crown682 --method original` or compact.
All diagnostic outputs stay under/tmp; no PNG is versioned.

Thirteen new controls plus the previous86 pass both source and installed modes;
the completed96-control runs precede the final99-control sets. The
installed run reuses the verified PR707 wheel; all212 package Python files
equal this focused tree. No lib/, native, shared shader or production asset
format changes. The permanent branch's independent Draft202 work is separate.
The NPZ loader is a trusted experimental round-trip, not an untrusted-file
validation framework or a proposed production cache manager.

Local SIGGRAPH1995 Weber/Penn *Creation and Rendering of Realistic Trees*,
pages120/124(PDF2/6), was optically inspected: explicit stem parameters/local
frames and one source representation interpreted at varying detail. This
experiment factors its own existing CrownShoots algebra; it does not copy that
paper's leaf-removal/LOD policy or claim to reproduce SpeedTree internals.
Source: `doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf`.

Two-stage world-root/local-instance GPU execution, actual bindings/layouts,
residency, native cache integration and hardware gates belong to integration.
The full visible crown still has90000 leaves; triangle/overdraw cost, direct
coarse forests, natural shoot/species architecture, material/shadow/wind
response and moving LOD transitions remain unfinished. This study preserves
the existing visibly repetitive fan-shaped crown rather than correcting it.
