# Native Metal encoder timing

Owner #15; native integration #328/#531. The explicit diagnostic prefix
`OUTSHINE_METAL_ENCODER_PROFILE` enables bounded stage-boundary timestamp sampling
only on devices created with GPU timing. Without the variable, no sample buffer
or trace is allocated. The output prefix must resolve inside the system temp
directory. Each device appends its identity and `.json`; its actual path is
printed once when the device's retained diagnostic session is destroyed.

The A18 Pro supports stage-boundary timestamps, but reports no draw-, dispatch-,
tile-dispatch- or blit-boundary sampling. The implementation therefore observes
existing render and compute encoders. It never splits or merges them for timing,
and does not advertise per-dispatch samples. A compute label identifies the first
pipeline in its possibly multi-dispatch encoder, not an isolated kernel duration.

Render records preserve separate vertex/fragment endpoints; compute records have
one encoder interval. They can overlap across encoders. Never add their spans or
substitute CPU encoding, fence waits or command-buffer duration for shader time.
Counter ticks stay raw, with independent GPU/steady-clock calibration pairs.
A prototype incorrectly applied the Mach timebase to already-normalized CPU
samples; its failed conversion is retained privately. The actual capture does
not assume that conversion or a fixed counter rate.

Sample buffers, metadata and their session remain owned through GPU completion.
The completion callback resolves shared counter storage after the GPU has finished;
no texture readback, extra GPU transfer, encoder or fence is added. Formatting and
file IO happen after the retained session dies. Bounds:128encoders per submission,
256completed submissions per device. Dropped records, allocation failures and
unavailable samples stay explicit. Counter collection CPU cost is measured; buffer
creation and GPU sampling overhead are not isolated or claimed free.

## Reproduction

Build the actual native checkout first. `probe` takes all compilation flags from
its current Metal compilation-database entry and the canonical native link flags.
It tests output confinement, delayed ownership,130real render encoders against
128slots, explicit dropped records and quoted/control/UTF-8 label roundtrips.

```sh
make all
make db
python3 test/experiments/render/metal_encoder_profile.py probe \
  --output "${TMPDIR:-/tmp}/outshine-metal-encoder-probe"
```

Select SCENARIO and VIEW in the calling shell/test driver. Keep normal content
and the declared 1280×720/60 profile.
The diagnostic file name comes from the printed line; pass that path as TRACE.

```sh
profile_root="${TMPDIR:-/tmp}/outshine-metal-encoders"
mkdir -p "$profile_root"
OUTSHINE_METAL_ENCODER_PROFILE="$profile_root/frame" \
  build/outshine benchmark --scenario "$SCENARIO" --view "$VIEW" \
  --frames 60 --angular-velocity '0,-360,0' --preload-seconds 120 --offline \
  --image "$profile_root/image.png" --output "$profile_root/benchmark.json" \
  > "$profile_root/benchmark.log" 2>&1
python3 test/experiments/render/metal_encoder_profile.py analyse \
  --trace "$TRACE" --benchmark "$profile_root/benchmark.json" \
  --output "$profile_root/summary"
```

Every declared frame must have one ordered, unique GPU sample from one device.
Each serial and individual elapsed time must match its actual trace submission;
matching only a sum is insufficient. Preparation and later readbacks remain in
the trace but are excluded by IDs. Tests reject swapped durations with equal
totals, missing/duplicate/reordered/cross-device samples and incomplete windows.
Historical GPUROW logs can use `--log`: exactly one complete GPUROW is required,
and the serial-ordered trace tail must reproduce its entire measured sum. A wrong
fresh-process log, mismatched windows, dropped records and unavailable samples
are refused rather than converted into timings.

## Current e2ae80985 countercheck

Four full original 1280×720/60, 60-frame/360-degree offline windows complete.
Every final PNG is byte-identical to its current Place; no unrefined frames.
Each raw trace has 61 submissions, with no dropped records/allocation failures;
all 60 measured IDs and individual durations agree with benchmark JSON.
The following means cover exactly those measured frames, including first use.

| Interval, ms | Tokyo | Wien | Koerbersee | CentralPark |
|---|---:|---:|---:|---:|
| Whole GPU submission | 14.251 | 12.375 | 15.181 | 14.006 |
| Cull/scan/compact compute encoder | 1.516 | 1.450 | 0.288 | 1.262 |
| Opaque object vertex | 6.191 | 4.410 | 0.248 | 4.557 |
| Opaque object fragment | 3.502 | 2.442 | 2.337 | 2.746 |
| Terrain vertex | 0.993 | 1.185 | 6.939 | 0.951 |
| Terrain fragment | 0.717 | 1.312 | 3.586 | 3.134 |

These intervals overlap and cannot be added or converted into exclusive shares.
GPU p99 remains 25.013/23.677/23.224/19.631 ms respectively, above 16.67 ms.
The first compute label names the cull pipeline but does not isolate it from
scan/compact dispatches in the same encoder. No sampling-overhead-free timing
or general optimisation gain is claimed. #894 continues object work reduction;
terrain vertex work remains a separate substantial cost under #12/#16.
Raw traces, commands and source association are linked from #15.

## Verification and costs on 82e7a69d7

The following original 11-encoder measurements precede the valid-cache shadow-pass
elision in #806. Current traces can contain 10 encoders. Ordinals are not stable
stage identifiers; inspect the recorded targets and the matching render catalogue
for each source revision. [The cached-shadow comparison](cached_shadow_passes.md)
records the later change without reinterpreting these historical intervals.

Current Apple A18 Pro: complete489-object build/491-unit database, all17Metal
product/dependent units analysed with actual full flags, the actual probe unit
analysed separately, format/Ruff and30real boundary checks pass. Disabled and
enabled Koerbersee both reproducec80e3641; enabled Tokyo reproducesddb9ef51.
Both are unchanged1280x720/60,60frames/360degrees/60GPUqueries with39336/2981446
ROW object triangles. ROW excludes GPU terrain; it is not total GPU geometry.
The integrated pictures therefore need no replacement by a diagnostic variant.

Two actual single-place traces retain61submissions each, including preparation.
Their final60submissions match the GPUROW sums exactly:826.540250/899.097584ms.
Means below divide the sums of60calibrated intervals by60; rows are not additive.
Encoder roles follow the current render catalogue and regular frame order.

| Measured phase | Koerbersee mean ms | Tokyo mean ms |
|---|---:|---:|
| Whole GPU submission |13.775671|14.984960|
| Opaque object vertex interval |0.265323|6.805364|
| Opaque object fragment interval |1.250597|3.670749|
| Terrain vertex interval |7.111912|1.033458|
| Terrain fragment interval |3.764061|0.705001|

The terrain-dominated view is vertex dominated; the dense city is object-vertex
heavy. This supports avoiding terrain/structure work before emission and refining
batching/selection under #12/#567. It does not prove optimal algorithms, exclusive
GPU utilisation, a speedup or a device budget. GPU maxima28.325/28.561ms remain
above16.67ms. CPU collection totals0.620/0.202ms include all61submissions; they do
not include every instrumentation cost. A separate disabled/enabled pair is
preserved, but single noisy runs do not isolate net CPU/GPU overhead.

Actual commands, source/archive/binary hashes, initial failed setup, raw counter
files, original images and receipts remain privately under
/tmp/outshine-metal-encoder-profile-82e7/ and
/tmp/outshine-metal-encoder-profile-analysis/. No current full-lint pass is claimed.
