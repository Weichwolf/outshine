# Native direct-light list comparison

Refs #448; goal #190. This is a controlled native raster comparison, not a
production renderer, shared binding, public SDK or runtime light-limit change.
The existing stage port #328 remains the sole renderer transition.

The exporter preserves real Vienna axis IDs/XY/DEM priors from #455 and authors
lamp placement/photometry, receiver planes and mixed light kinds explicitly.
Cases retain 1920×1080 and complete receiver planes at 25/250/500/5,000 m. Mixed
controls include spots, two directions and an unbounded point; the Sun shadow
owner retains its world index rather than its ordinal in the global list.
All input hashes, source IDs and declared test recipes are recorded.

The shader preparer copies the actual `subjectLighting.glsl`, changing only
enumeration, original-ID shadow lookup and optional diagnostic support counting.
The material type and BRDF/attenuation body remain actual production sources.
Eight prescribed material stripes, black environment and a fixed shadow texel
isolate direct response. Textures, Coverage/Thin composition, independent BRDF
correctness, source emission recipes and full framepipeline acceptance remain
outside this control. Native 3af55ff02 headers/archive/shader tables are a
coherent immutable producer; all 1,787 hashes were checked after restart. Current
source/header migrations do not relabel it as a current-engine build.

## Concrete private layout

- set 2/24: existing 64-byte Light records in original world-index order.
- set 2/25: uint32 words. Header16 contains global-index offset/count, cell-header
  offset and depth-edge offset, all in words. Each cell stores an 8-byte absolute
  index offset/count, followed by 4-byte world IDs and FP32 depth-edge bit patterns.
- set 3/4: 64-byte frame: uvec4 grid at 0, vec4 depth at 16, view-Z row at 32,
  viewport at 48. Complete camera coverage, no opaque-depth truncation.
- set 3/1: existing 1344-byte Lighting. Private camera scaffold3/5 is 80 bytes,
  not a proposed runtime field. Shadow sample stays2/0.

Tiles are 64 px; log uses 24 cells, with edges 0.1..4096 m and an infinite final
cell. Range 0/directions are global. Overflow replaces partial lists with all
world IDs, preserving every contribution at an explicitly high cost. No sparse
GLSL enters the SDL-root path; the audit and shared Metal/runtime contracts stay
in #448. The 16 MiB index cap is an experiment control, not a device budget.

## Native evidence, Linux / RTX 3060

Diagnostic controls: 17/2539/8192 lights, 1024-light overlap, forced 4096-byte index
overflow, turn and orthographic views. Every tested support count matches the
full sum. RGB is bit-identical except overlap accumulation order, which stays
within per-component max(1e-7,abs(reference)*1e-5); no tolerance was relaxed.
Every plane is nonempty. Counts include attenuation/angular/atlas support before
material-facing tests. No general FP32 culling or temporal/history proof follows.

Two actual negative controls fail: keeping only world IDs<16 loses the 17th
far light (672 RGB-component failures /224 support mismatches); assigning the
Sun atlas by list ordinal produces 49,766,400 /16,588,800 failures. The initial
incorrect per-cell-count cap did not test world-ID truncation and was replaced;
its passing output is not evidence for the intended negative.

Render-only GPU timestamp medians, five alternating repetitions, counter and
validation off. Upload/readback are separate; these are not full-frame times:

| Case / view depth | Full sum ms | Tiles64 ms | Log64 ms |
|---|---:|---:|---:|
|8192 /25 m|236.7410|5.8365|1.3859|
|8192 /250 m|234.2240|5.8352|1.0321|
|8192 /500 m|235.3637|5.7276|1.4852|
|8192 /5000 m|235.3198|5.7911|0.4335|
|1024 overlap /250 m|46.2822|54.9636|47.9707|
|1024 overflow /250 m|46.3599|54.4164|54.6871|

Log64 is the next native construction candidate for distributed lights. It is
not uniformly best:17 lights favour tiles in this control, and overlap/overflow
remain above the whole-frame budget. Report each depth; a median across depths
would hide the expensive overlapping plane. A measured small-light path and a
valid solution for pathological overlap remain required by #448.

Dense logical records: 524,288 B lights;323,068 B tile lists;691,744 B log lists.
The comparison intentionally holds both lists. Its explicit own API requests
are 3 storage buffers + 3 staging buffers + 1 shadow staging, then 12 readback
buffers; 2 textures, 1 sampler and 2 pipelines. Source payload memcpy/GPU upload
is 1,539,104 B; 12 RGBA32F captures copy 398,131,200 B separately from timing.
Each draw supplies 1,488 uniform bytes. These are code-derived own-request/data
counts, not total backend/driver allocations, DRAM traffic or measured peak.
Native CPU construction and a separate copy/rebase candidate are now compared
in `local_light_build.md`. Upload, complete allocation/copy/peak and device
budgets remain open; neither experiment is a production runtime implementation.

## Reproduction and remaining work

Requires Python 3/NumPy, glslang, and a matching compiled native consumer. The
consumer arguments are vertex-SPIRV, brute-fragment-SPIRV, listed-fragment-SPIRV,
input directory, output directory, repetition count 1..16 and validation 0/1.
`export_local_light_native.py` prepares inputs; `prepare_shaders` in
`local_light_native_shaders.py` prepares strict private includes. Compile the
fragment with LOCAL_LIGHT_LISTED=0/1 and LOCAL_LIGHT_COUNTER=1 for diagnostics,
0 for timing. Negative variants use LOCAL_LIGHT_FIRST16 or LOCAL_LIGHT_WRONG_OWNER.

`check_local_light_native.py --input <capture-dir> --output <private.json>`
checks actual images; add --without-counter only for the corresponding timing
shader. `plot_local_light_native.py --input <results-dir> --output <private.pdf>`
needs Matplotlib. Four PDF pages were visually opened, including original
negative crops and per-depth cost. Full original RGBA32F captures remain local.
Exact commands/exit codes/hashes are linked from PR #458; no images go to doc/.

All four diagnostic SPIR-V variants also translate to MSL2.3 offline. Reflection
retains Light stride64, uint stride4 and1344/64/80-byte uniform blocks with
the declared offsets. This is not Metal compilation/execution or acceptance
of the full material resource layout.

Own source/layout analysis findings are corrected. Default focused analysis
under the current policy still fails on 14 pre-existing Check.h diagnostics;
no analyzer guard is disabled or full clean claim made. Shared #603 owns that
harness decision. Native GPU construction/runtime, jitter/motion/light updates,
transparent composition, complete Places, Metal and A18 remain open. The
restart bundle in #448 preserves earlier originals; new outputs remain in /tmp.
