# Native surface interpolation counterexamples

`native_interpolation.py` compiles its C++ probe with the complete native
`StagePipelines.cpp` compilation record, links that checkout's product archive,
and executes unchanged product shaders. A CPU reference transforms the original
Float32 vertex inputs in Float64 and computes perspective interpolation at a
strictly interior pixel. The velocity comparison retains two Half ULPs plus
eight Float32 epsilons; this numerical diagnostic bound is not scene admission.

| Case | Vertex shader | Geometry | Pipeline |
|---|---|---|---|
| `original` (default) | `lit-200-1` | 166 vertices, 77 valid triangles in 346 slots | No culling, GreaterEqual |
| `tinted-backface` | `lit-210-1` | 236 vertices, 109 valid triangles in 24,343 slots | Backface culling, Greater |

Both use `lit-010-100`, the original PBR/material calculation, constant dummy
textures, 1280×720, and one indirect instance. Index/instance offsets are zero. The second case includes its exact per-vertex
colour factors with a normalised zero colour offset; that field is unused in
the original case.
These draws omit world streaming, other passes, animation and postprocessing.
They make no FPS, loading, memory-budget, image-quality or worldwide claim.

## Run

Build the native archive, shader package and `compile_commands.json` in a matching
native checkout first. Keep private output in the system temporary directory:

```sh
python3 test/experiments/render/native_interpolation.py \
  --checkout /path/to/native-checkout \
  --case tinted-backface --iterations 1024 \
  --output /tmp/outshine-interpolation-tinted-COMMIT
```

`--shader-root /path/to/shader-checkout` selects a separate frozen shader package.
Use it only when that package's CPU resource/layout contracts match the archive;
the report records both checkout commits, dirty state and shader hashes. This
permits comparison of the original and packed shader interfaces with unchanged
native C++ code. `--device-name` rejects an unexpected actual GPU. Darwin defaults
`MTL_DEBUG_LAYER=1`; each diagnostic creates a validation-enabled native device.

Exit 0 means the chosen pixel agrees throughout that run, 6 means an observed
GPU/reference disagreement, and 2 means an invalid or incomplete diagnostic.
A report containing a failed verdict/exit 6 is a counterexample, not a passing
renderer gate. Inspect the terminal report and validation output together.

## Fixed inputs

The original fixture remains byte-for-byte unchanged. The new fixture lives in
`test/outshine/data/render/interpolation/tinted-backface/`; its manifest preserves
the source commit, original draw/vertex IDs, retained primitive IDs and hashes.
Its five little-endian parameter words select target triangle, pixel X/Y, index
count and flags: 1=tinted, 2=backface culling, 4=strict Greater depth. Missing
parameters retain the original case's defaults. No product scenario contains
these fixed camera coordinates or test names.

The long leading index padding is intentional. Removed triangles repeat a valid
target vertex, so they introduce no out-of-range fetches. Shortening that padding
suppressed the observed defect in the reduced Vulkan probe. This is a bounded
reduction, not a proof of a minimum input or a padding threshold. `indices.bin.gz`
decodes to the exact pinned 292,116-byte index input in the run's temporary
directory. Both encoded and decoded sizes/hashes are checked; no test creates or
updates an oracle. Stored inputs including provenance are approximately 20 KB.

## Observed scope

On Iris Xe/Mesa 26.1.6, #911's packed interface fixes the original strict case but
leaves this tinted counterexample. The full-world failure was initially absent
from a reduced draw that used None/GreaterEqual instead of the actual
Back/Greater state. Matching the actual state exposed the same native velocity
failure without world assembly. Separate controls show Back/GreaterEqual also
fails, while both depth functions were healthy with no culling in the tested
reduction. Dummy textures retain the failure. These controls do not establish a
driver/compiler/host cause and do not justify weakening product culling.

Actual Metal/A18 results for each source commit remain separate evidence. A
healthy isolated run or a healthy Metal result does not dismiss an erroneous
Vulkan world capture. Current decisions and device evidence belong to
[#900](https://github.com/Weichwolf/outshine/issues/900), implementation to
[#907](https://github.com/Weichwolf/outshine/pull/907).
