# Directed rock pixel footprints

Owner: #370 / #6 / #189. The existing three noise fields and catalogue material
values are retained. Baseline source: 1a1d1d885 (ground shaders also identical to
archived a56f3e3be). SIGGRAPH2009 Sparse Gabor Noise, section5.2/Figure6/page6 was
read optically: mapping derivatives precede spectral filtering. This experiment
uses existing cubic value-noise, not Gabor convolution.

The old shader applies a global longest world footprint, with a1.04 bound for
transformed fields. The first alternative transforms both derivatives but keeps
the worst-case signal-gradient bound. It improves albedo while barely improving
normals. The selected alternative additionally uses local phase increments
`dot(noiseGradient, mappedDx/Dy)` and uniform-pixel variance `(dx²+dy²)/12` for the
existing Gaussian crevice profile. This is a local Gaussian approximation, not
exact integration of arbitrary nonlinear noise or a BRDF.

The field helper now tests its transformed squared footprint before noise work.
Unresolved fields return zero. Near fields avoid the visibility square root.
At most24 corner hashes remain; some pixels now correctly retain fields that the
old bound discarded. The global rockDetail footprint parameter is removed from
its only production caller. rockCrevice now takes explicit phase variance;
visibility and invalid wavelengths belong to the field sampler. No GPU binding,
block/asset layout, palette, source classification, lighting or geometry change.
The final sampler returns screen derivatives directly: its existing two phase
increments already are the required projected noise gradient. Composing these
avoids reconstructing and reprojecting world gradients. The new native86-case
comparison preserves scalar outputs exactly, changes derivatives by at most
1.1921e-7, and retains the original parity limits. This does not itself explain
or clear the observed Central-Park frame spike.
The independent Gaussian primitive test retains its numerical thresholds and
extends to1,345 checks over both phase axes. Disabled/unresolved cases move to
actual field-level GPU checks, where visibility is now decided.

## Reproduction

Use Python with NumPy, Matplotlib and outshine_ref; glslang, Vulkan validation,
and the existing NativeGroundLitterProbe consumer linked to a matching native
archive/header closure. Its public timestamp option comes from PR368/#336.
All outputs must be explicit system-temp paths.

```sh
PYTHONPATH=lib/src python test/experiments/render/rock_footprint.py \
  --output /tmp/outshine-rock-reference
PYTHONPATH=lib/src python test/experiments/render/rock_footprint_native.py \
  --binary /tmp/native-material --baseline-checkout /tmp/old-rock-checkout \
  --output /tmp/outshine-rock-native --profile
```

CPU comparison:86 cases, including long/sheared pixels, lattice critical points,
translation across critical points and swapped pixel axes. Point-material
integration doubles its grid from32 through at most512, requiring successive
nominal RGB change<=1e-5 and world-gradient change<=1e-4. These are numerical
convergence checks, not a rigorous global quadrature bound.

The initial86-case CPU cohort reduces normal error RMS from1.9083 to0.5553degrees,
maximum8.3251 to1.7097degrees. Some individual cases still worsen. Transformed
footprints alone give1.8835degrees RMS. Normals resolve the integrated gradient;
this does not establish filtered normal variance or integrated radiance.

The native comparison uses the unchanged versioned14-row native palette input,
its exact float32 metric records, and the actual old/current production functions
in addition to both private alternatives. Catalogue nominal RGB in the CPU study
and already attenuated native RGB are distinguished. Scalar parity must stay
below2e-4 and normal parity below0.05degrees. Current native differences are far
smaller; source/output hashes and final measured costs belong to the delivery PR.

RTX3060 component cost:115200 matched probes,32 sequential timestamped dispatches
per run. Legacy/production then production/legacy sums are1.372512/1.384288ms and
1.368896/1.375712ms. This is approximately0.043ms per dispatch, not an established
speedup or a full-frame/device budget. Timed and untimed outputs are byte-equal.
Requested private probe buffers total25807360bytes and copy12903680bytes;
these are not driver peaks. Production adds no binding, allocation or asset byte.
The helper still uses at most24 corner hashes. Mapping before culling can retain
previously discarded fields, so reduced global work is not claimed.

All86 cases run through the actual old and new GPU functions, plus both private
alternatives; maximum native normal/parity difference is0.000014664degrees.
Twelve disabled/unresolved groups are exactly neutral, and exchanged pixel axes
preserve scalar output while exchanging the corresponding screen derivatives.
The86-case PDF was optically opened. Current gallery7f69a41f0 and the dated
Koerbersee reference2026-09-07T12:40 local were also compared optically in a PDF;
large pale walls and missing conifer stands are separate unresolved deficits.

The86-case work cohort uses1320 old corner hashes versus1480 with either mapped
variant (165 versus185 active fields); the reference integrates15679488 hashes.
The mapped variants preserve more fields, rather than universally saving work.

## Public client countercheck

The regular Linux build on2a6f211c7 includes the material-independent forest
placement fix. Same executable, scenario/source/assets and render settings are
used for both shader roots; only21 Ground fragment artifacts differ. Width/height
1280×720,60 timed frames,360° in one second, only the final PNG. Camera, palette,
clock and geometry remain unchanged. Cached fresh offline preloads are below10s.
Initial source acquisition/asset construction is separate from these runs.

Optically opened full-image and identical crop PDFs show a small local change:
more resolved crevice response on Koerbersee's rock faces; the pale large walls,
geometric shore teeth and missing conifer forest are still visible deficits.
Central Park remains visually close. No general webcam-quality claim follows.
Final screen-derivative composition preserves the component's scalar values;
its small floating-point gradient differences require their own final captures.

The frame gate is still RED. Koerbersee's initial paired p99 is9.5564/9.4264ms;
the final screen variant is5.4926ms. Central Park initially gives12.9970/25.7638ms,
reverse order20.4776/14.9547ms, and screen/baseline21.0365/19.3087ms. Each failure
is at frame index1, beyond the unchanged16.67ms60Hz budget. No extra warm frames
or raised threshold are used. Component GPU timestamps do not clear this gate.

A private, unchanged-call SDL timing shim identifies synchronous submit wall time
on that frame:27.633862ms old,24.289410ms new. These instrumented runs are
diagnostics, not acceptance or GPU execution measurements. Pipeline creation is
earlier, and the fence-wait calls do not contain this spike. Evidence is reported
to the submission owner under #328/6204546022 and its follow-up. No parallel
renderer/SDL repair is included. A native submission/frame countercheck remains
necessary before accepting this production change. Master owns Metal/A18 and
publication; no image belongs under doc/ from this experiment. The declared
global limestone assumption remains unchanged and is a separate issue.

## Native countercheck with the supplied immutable package

The source topic is synchronized to master36be5afa3 at421fc62b9; the three rock
production sources and primitive test remain byte-identical to62a3e340b/4a423e9a1.
The provided native5ea6f823b and SDL2fe8da2ca packages each retain their own matched
Main/Heap/core/header closure. Their C++ snapshot predates #400; this is explicit,
not represented as current-master C++ execution. No private old Main/Framing object
is combined with a mutable new Device archive. The native baseline client is the
provided byte-identical relink, without #416's independent producer correction.

All shared shader sources match that package except the three intended rock files.
The21 Ground fragments are rebuilt against it;471 other shader/include files are
byte-equal.42 before/after reflection checks pass and retain identical resource
kinds/sets/bindings;21 changed artifacts cross-compile to MSL2.3. This is source
portability, not Metal execution. Previous independent CPU/native primitive and
filter checks remain valid for their unchanged sources; no full-lint repeat.

First10second attempts on Koerbersee/CentralPark did not reach frames. A separate
complete offline Koerbersee preparation took117.4s preload plus2.9s standing:
48LOD and two deformed-terrain products were written. It was not a full warm-cache
hit. Only after this successful preparation were fresh10second attempts made;
all three still failed before frames, in sheet-mesh with zero source terrain fields.

Separate120second image preparation then gives the actual warm-hit comparison,
1920×1080/60, unchanged fill0.6/scenario/camera/clock/sight/source products:

| Consumer | Standing/preload s | CPU p50/p95/p99 ms | Frames/over | GPU mean/max ms |
|---|---|---|---|---|
| Native baseline | 2.6/34.8 | 5.88/8.05/12.34 | 60/0 | 8.912/12.369 |
| SDL baseline | 2.5/11.5 | 1.31/2.07/10.61 | 60/0 | unavailable |
| Native rock filter | 2.7/43.5 | 5.52/12.35/12.62 | 60/0 | 9.937/15.228 |

Standing/preload are rounded to0.1s by the human client report. The full warm
readiness target includes both; it is red on every consumer. Native timestamps
are60/60 available each, zero unavailable; total query CPU0.023/0.024ms. No fence
wall time is labelled GPU execution. Native launch loads are5.20/5.33 under
concurrent foreign work; these are single pairs, not a speedup or isolated
regression claim. Both native hit paths read the133.00MiB region,49building bases,
49LOD products and nine prototypes, with zero source bytes and no new products.
Reported geometry is23500 triangles/zero bare tiles;60frames use the unchanged
client360degree turn. Heap peaks522MB do not establish driver/device memory.

The native and SDL baseline PNGs are byte-identical697fd186. Enabling the rock
filter giveseb215015:456216 of2073600 pixels differ,189916 above1/255, worst40/255,
mean RGB0.371626318/255. All four PDF pages were optically opened: baseline parity,
native pair, identical224pixel crop (1632,352)-(1856,576), dated webcam. The crop
is selected by largest RGB-error sum in96pixel windows/32pixel steps. It shows
more resolved local rock crevices; large pale surfaces, poor vegetation and
shore geometry remain dominant deficits. No broad webcam-quality claim.

CentralPark's longer preparation stops in the unmodified provided C++ with
"road profiles need more room than junction slopes can provide". Source comparison
finds Corridors.h/.cpp, CorridorCrossings.cpp and CorridorHeights.cpp older than
86a2176de/#400; this is not claimed as a current-master regression. Tokyo initially
lacks camera terrain height offline; ordinary online source acquisition passes
that stage and reaches the same old road-profile failure. No camera/source/profile
reduction is used to obtain a shot. #47/6208605285 requests the next immutable
native package with current-master fixes; no duplicate renderer or full build.
Light's later #439 solar writer/sampler/512width transition must also stay together.

Exact frozen source/package/binary/shader hashes, commands, original first attempts,
separate preparation/warm runs, actual images/PDF and paired resource reflection
are under `/tmp/outshine-material-rock-current-421fc62b9/`. Current Koerbersee
frames pass their budget; complete warm loading and the two mandatory density
counterchecks remain open. #386 stays Draft for these concrete checks. I merge
it myself after acceptance; no prior Master review is required.
