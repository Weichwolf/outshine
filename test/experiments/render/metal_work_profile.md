# Native Metal scenario work profile

The private injected library records existing Metal pass boundaries and submitted
work. Build Outshine and its current compilation database first. Apple stage-boundary
counters and Command Line Tools are required; the Xcode application is not.
CMake builds the existing injection source as `test_harness_metal_work_profile`,
using the shared compiler policy, Objective-C ARC and standard SHARED/PIC handling.
Configure with BUILD_TESTING=ON; `--build-dir` selects that configured build.
Python3.10+ is required, as for the other render experiments.

```sh
python3 test/experiments/render/metal_work_profile.py \
  --output "${TMPDIR:-/tmp}/outshine-metal-work" -- \
  --scenario test/outshine/data/places/Jura.scenario --view station \
  --frames 60 --angular-velocity 0,-360,0 --offline
```

Arguments after `--` go directly to `outshine benchmark`. The scenario owns its
resolution/rate/content; pass the actual source cache with `--cache-dir` when needed.
Only `--output` and `--image` belong to the profiler. `--client` chooses the binary;
`--runtime-root` chooses its working directory. Coordinates stay in caller inputs.
Product, tool and injection-library hashes/build identities remain separate.

`benchmark.json` must report a complete world, complete measurement, no unrefined
frames, one device and all ordered GPU samples. Sky-labelled commandbuffers identify
candidate frames, including preload. Their rounded native GPU duration sequence must
match the benchmark sample sequence in exactly one contiguous window. Matching uses
at most0.500001ns printing tolerance, not assumed equality of native/interposer IDs.
Missing or ambiguous matches fail; preload stays in raw records and out of summaries.
Custom plans without a sky pass remain outside this diagnostic.

- Raster vertex/fragment intervals can overlap each other and other passes; do not
  add them into a frame. Apple counter deltas are nanoseconds, not CPU mach ticks.
- Compute intervals cover their encoder pipeline set, not individual kernels.
  Declared groups/threads include padding and early returns, not actual invocations.
- Indirect arguments are copied after draws and read after completion. They must
  remain unchanged until that copy. Counts include instancing, clipped/degenerate
  triangles and hidden geometry; they are not visible triangles or vertex invocations.
- Sampling is bounded to64 encoders,65536indirect calls and eight readback buffers
  up to1MiB per command. GPU-indirect dispatches are counted; their generated
  groups/threads remain unknown, without synchronous argument readback. Missing
  timing/work stays explicit rather than zero.
- Hooks, counters, readbacks and completion logging add work. GPU times include
  instrumentation. Compare an ordinary run separately; this is never a budget gate.

`receipt.json` records commands, source/binary hashes, selected native/interposer
submission mappings, peak-frame raster/compute intervals and all raw records.
Standard output is one compact summary; detailed tables stay in the receipt.
`metal_work_profile_checks.py` checks overlap, missing work, preload exclusion,
rounding, ambiguous windows and invalid benchmark measurements. Native Metal APIs:
installed `MTLCounters.h`, `MTLRenderPass.h`, `MTLRenderCommandEncoder.h`.
Current integration/measurements: #1540/#1257/#15. Historical index-order cost
comparison: [PR776](https://github.com/Weichwolf/outshine/pull/776#issuecomment-6251118304).
